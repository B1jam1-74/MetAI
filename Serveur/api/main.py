from fastapi import FastAPI
from pydantic import BaseModel
from typing import Optional
import sqlite3, os, requests

app = FastAPI(title="metAI API")
DB_PATH = "/data/metai.db"

# ── WMO code → French label (grouped to match model classes) ──────────────────
WMO_FR = {
    0:  "Ciel dégagé",
    1:  "Peu nuageux",
    2:  "Partiellement nuageux",
    3:  "Couvert",
    45: "Brouillard",
    48: "Brouillard givrant",
    51: "Bruine légère",
    53: "Bruine modérée",
    55: "Bruine dense",
    61: "Pluie légère",
    63: "Pluie modérée",
    65: "Pluie forte",
    71: "Neige légère",
    73: "Neige modérée",
    75: "Neige forte",
    80: "Averses légères",
    81: "Averses",
    82: "Averses fortes",
    85: "Averses de neige",
    95: "Orage",
    96: "Orage avec grêle",
    99: "Orage avec grêle forte",
}

def wmo_to_fr(code: int) -> str:
    # Match closest code
    if code in WMO_FR:
        return WMO_FR[code]
    # Fallback: find nearest
    closest = min(WMO_FR.keys(), key=lambda k: abs(k - code))
    return WMO_FR[closest]

# ── DB ─────────────────────────────────────────────────────────────────────────
def get_db():
    os.makedirs("/data", exist_ok=True)
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def init_db():
    conn = get_db()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS uplinks (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id       TEXT NOT NULL,
            received_at     TEXT NOT NULL,
            temperature     REAL,
            humidity        REAL,
            pressure        REAL,
            predicted_class INTEGER,
            prediction_fr   TEXT,
            real_temp       REAL,
            real_humidity   REAL,
            real_pressure   REAL,
            real_wmo_code   INTEGER,
            real_condition  TEXT,
            match           INTEGER  -- 1 if prediction_fr matches real_condition, else 0
        )
    """)
    conn.commit()
    conn.close()

init_db()

# ── Open-Meteo ─────────────────────────────────────────────────────────────────
# Coordinates: Thonon-les-Bains
LAT = 46.37
LON = 6.48

def fetch_real_weather():
    try:
        r = requests.get(
            "https://api.open-meteo.com/v1/forecast",
            params={
                "latitude": LAT,
                "longitude": LON,
                "current": [
                    "temperature_2m",
                    "relative_humidity_2m",
                    "surface_pressure",
                    "weather_code"
                ]
            },
            timeout=5
        )
        c = r.json()["current"]
        wmo = int(c["weather_code"])
        return {
            "real_temp":      c["temperature_2m"],
            "real_humidity":  c["relative_humidity_2m"],
            "real_pressure":  c["surface_pressure"],
            "real_wmo_code":  wmo,
            "real_condition": wmo_to_fr(wmo)
        }
    except Exception:
        return {
            "real_temp": None, "real_humidity": None,
            "real_pressure": None, "real_wmo_code": None,
            "real_condition": None
        }

# ── Schema ─────────────────────────────────────────────────────────────────────
class Uplink(BaseModel):
    device_id:       str
    received_at:     str
    temperature_deg_c: Optional[float] = None
    humidity_percent:  Optional[float] = None
    pressure_hpa:      Optional[float] = None
    predicted_class:   Optional[int]   = None
    prediction_fr:     Optional[str]   = None

# ── Endpoints ──────────────────────────────────────────────────────────────────
@app.post("/uplink")
def receive_uplink(data: Uplink):
    # Ignore everything that isn't metai
    if data.device_id != "metai":
        return {"status": "ignored", "reason": "unknown device"}

    real = fetch_real_weather()

    # Soft match: check if prediction_fr is contained in real_condition or vice versa
    match = 0
    if data.prediction_fr and real["real_condition"]:
        a = data.prediction_fr.lower()
        b = real["real_condition"].lower()
        if a in b or b in a:
            match = 1

    conn = get_db()
    conn.execute("""
        INSERT INTO uplinks
            (device_id, received_at, temperature, humidity, pressure,
             predicted_class, prediction_fr,
             real_temp, real_humidity, real_pressure, real_wmo_code, real_condition,
             match)
        VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)
    """, (
        data.device_id, data.received_at,
        data.temperature_deg_c, data.humidity_percent, data.pressure_hpa,
        data.predicted_class, data.prediction_fr,
        real["real_temp"], real["real_humidity"],
        real["real_pressure"], real["real_wmo_code"], real["real_condition"],
        match
    ))
    conn.commit()
    conn.close()

    return {"status": "ok", "real_weather": real, "match": bool(match)}

@app.get("/uplinks")
def get_uplinks(limit: int = 200):
    conn = get_db()
    rows = conn.execute(
        "SELECT * FROM uplinks ORDER BY id DESC LIMIT ?", (limit,)
    ).fetchall()
    conn.close()
    return [dict(r) for r in rows]

@app.get("/stats")
def get_stats():
    conn = get_db()
    row = conn.execute("""
        SELECT
            COUNT(*)                          AS total,
            SUM(match)                        AS correct,
            ROUND(AVG(match) * 100, 1)        AS accuracy_pct,
            ROUND(AVG(temperature), 2)        AS avg_temp,
            ROUND(AVG(humidity), 2)           AS avg_humidity,
            ROUND(AVG(pressure), 2)           AS avg_pressure
        FROM uplinks WHERE device_id = 'metai'
    """).fetchone()
    conn.close()
    return dict(row)
