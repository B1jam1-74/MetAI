import express from 'express';
import cors from 'cors';
import Database from 'better-sqlite3';
import axios from 'axios';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import fs from 'fs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

const app = express();
app.use(cors());
app.use(express.json());

const DB_PATH = process.env.DB_PATH || '/data/metai.db';
const INFLUX_URL = process.env.INFLUX_URL || 'http://192.168.0.79:8181/api/v3/write_lp?db=metai';
const LAT = 46.366;
const LON = 6.4791;

const WMO_FR = {
  0: "Ciel dégagé", 1: "Peu nuageux", 2: "Partiellement nuageux", 3: "Couvert",
  45: "Brouillard", 48: "Brouillard givrant", 51: "Bruine légère", 53: "Bruine modérée",
  55: "Bruine dense", 61: "Pluie légère", 63: "Pluie modérée", 65: "Pluie forte",
  71: "Neige légère", 73: "Neige modérée", 75: "Neige forte", 80: "Averses légères",
  81: "Averses", 82: "Averses fortes", 85: "Averses de neige", 95: "Orage",
  96: "Orage avec grêle", 99: "Orage avec grêle forte",
};

const wmoToFr = (code) => WMO_FR[code] || WMO_FR[Object.keys(WMO_FR).reduce((prev, curr) => Math.abs(curr - code) < Math.abs(prev - code) ? curr : prev)];

// Initialize DB
fs.mkdirSync('/data', { recursive: true });
const db = new Database(DB_PATH);
db.exec(`
  CREATE TABLE IF NOT EXISTS uplinks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    received_at TEXT NOT NULL,
    temperature REAL,
    humidity REAL,
    pressure REAL,
    predicted_class INTEGER,
    prediction_fr TEXT,
    real_temp REAL,
    real_humidity REAL,
    real_pressure REAL,
    real_wmo_code INTEGER,
    real_condition TEXT,
    match INTEGER
  )
`);

const fetchRealWeather = async () => {
  try {
    const res = await axios.get('https://api.open-meteo.com/v1/forecast', {
      params: {
        latitude: LAT,
        longitude: LON,
        current: ['temperature_2m', 'relative_humidity_2m', 'surface_pressure', 'weather_code']
      },
      timeout: 5000
    });
    const c = res.data.current;
    const wmo = parseInt(c.weather_code, 10);
    return {
      real_temp: c.temperature_2m,
      real_humidity: c.relative_humidity_2m,
      real_pressure: c.surface_pressure,
      real_wmo_code: wmo,
      real_condition: wmoToFr(wmo)
    };
  } catch (e) {
    return { real_temp: null, real_humidity: null, real_pressure: null, real_wmo_code: null, real_condition: null };
  }
};

const writeInflux = async (data, real, match) => {
  try {
    const tsNs = BigInt(new Date(data.received_at.replace('Z', '+00:00')).getTime()) * 1_000_000n;
    let line = `weather_sensor,device_id=${data.device_id} temperature_C=${data.temperature_deg_c},humidity_pct=${data.humidity_percent},pressure_hPa=${data.pressure_hpa},weather_class=${data.predicted_class},weather_label="${data.prediction_fr}"`;
    
    if (real.real_temp !== null) {
      line += `,real_temp=${real.real_temp},real_humidity=${real.real_humidity},real_pressure=${real.real_pressure},real_condition="${real.real_condition}",match=${match}`;
    }
    line += ` ${tsNs}`;

    await axios.post(INFLUX_URL, line, {
      headers: { 'Content-Type': 'text/plain; charset=utf-8' },
      timeout: 3000
    });
    console.log(`[InfluxDB] write OK: ${line}`);
  } catch (e) {
    console.error(`[InfluxDB] write failed:`, e.message);
  }
};

app.post('/uplink', async (req, res) => {
  const data = req.body;
  if (data.device_id !== 'metai') {
    return res.json({ status: 'ignored', reason: 'unknown device' });
  }

  // Check if we already have a record for this device and timestamp
  const existing = db.prepare('SELECT * FROM uplinks WHERE device_id = ? AND received_at = ?').get(data.device_id, data.received_at);
  const real = await fetchRealWeather();

  // Determine final values (prefer new data, fallback to existing)
  const finalTemp = data.temperature_deg_c !== undefined ? data.temperature_deg_c : (existing ? existing.temperature : null);
  const finalHum = data.humidity_percent !== undefined ? data.humidity_percent : (existing ? existing.humidity : null);
  const finalPress = data.pressure_hpa !== undefined ? data.pressure_hpa : (existing ? existing.pressure : null);
  const finalPredClass = data.predicted_class !== undefined ? data.predicted_class : (existing ? existing.predicted_class : null);
  const finalPredFr = data.prediction_fr !== undefined ? data.prediction_fr : (existing ? existing.prediction_fr : null);
  
  // Use freshest real weather if sensor data is provided, otherwise keep existing
  const finalRealTemp = data.temperature_deg_c !== undefined ? real.real_temp : (existing ? existing.real_temp : null);
  const finalRealHum = data.temperature_deg_c !== undefined ? real.real_humidity : (existing ? existing.real_humidity : null);
  const finalRealPress = data.temperature_deg_c !== undefined ? real.real_pressure : (existing ? existing.real_pressure : null);
  const finalRealWmo = data.temperature_deg_c !== undefined ? real.real_wmo_code : (existing ? existing.real_wmo_code : null);
  const finalRealCond = data.temperature_deg_c !== undefined ? real.real_condition : (existing ? existing.real_condition : null);

  // Recalculate match if we have both prediction and real condition
  let match = existing ? existing.match : 0;
  if (finalPredFr && finalRealCond) {
    const a = finalPredFr.toLowerCase();
    const b = finalRealCond.toLowerCase();
    match = (a.includes(b) || b.includes(a)) ? 1 : 0;
  }

  if (existing) {
    // Update existing record
    db.prepare(`
      UPDATE uplinks SET 
        temperature = ?, humidity = ?, pressure = ?, 
        predicted_class = ?, prediction_fr = ?, 
        real_temp = ?, real_humidity = ?, real_pressure = ?, real_wmo_code = ?, real_condition = ?, match = ?
      WHERE id = ?
    `).run(finalTemp, finalHum, finalPress, finalPredClass, finalPredFr, finalRealTemp, finalRealHum, finalRealPress, finalRealWmo, finalRealCond, match, existing.id);
  } else {
    // Insert new record
    db.prepare(`
      INSERT INTO uplinks (device_id, received_at, temperature, humidity, pressure,
        predicted_class, prediction_fr, real_temp, real_humidity, real_pressure, real_wmo_code, real_condition, match)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `).run(
      data.device_id, data.received_at, finalTemp, finalHum, finalPress,
      finalPredClass, finalPredFr, finalRealTemp, finalRealHum, finalRealPress,
      finalRealWmo, finalRealCond, match
    );
  }

  // Write to InfluxDB only when sensor data arrives to avoid duplicate partial writes
  if (data.temperature_deg_c !== undefined) {
    const influxData = {
      device_id: data.device_id,
      received_at: data.received_at,
      temperature_deg_c: finalTemp,
      humidity_percent: finalHum,
      pressure_hpa: finalPress,
      predicted_class: finalPredClass,
      prediction_fr: finalPredFr
    };
    writeInflux(influxData, { real_temp: finalRealTemp, real_condition: finalRealCond }, match);
  }

  res.json({ 
    status: 'ok', 
    real_weather: { real_temp: finalRealTemp, real_condition: finalRealCond }, 
    match: Boolean(match) 
  });
});

app.get('/uplinks', (req, res) => {
  const limit = parseInt(req.query.limit, 10) || 200;
  const rows = db.prepare('SELECT * FROM uplinks ORDER BY id DESC LIMIT ?').all(limit);
  res.json(rows);
});

app.get('/stats', (req, res) => {
  const row = db.prepare(`
    SELECT
      COUNT(*) AS total,
      SUM(match) AS correct,
      ROUND(AVG(match) * 100, 1) AS accuracy_pct,
      ROUND(AVG(temperature), 2) AS avg_temp,
      ROUND(AVG(humidity), 2) AS avg_humidity,
      ROUND(AVG(pressure), 2) AS avg_pressure
    FROM uplinks WHERE device_id = 'metai'
  `).get();
  res.json(row || { total: 0, correct: 0, accuracy_pct: 0, avg_temp: 0, avg_humidity: 0, avg_pressure: 0 });
});

app.get('/influx_history', async (req, res) => {
  const limit = parseInt(req.query.limit, 10) || 200;
  const query = `SELECT * FROM weather_sensor ORDER BY time DESC LIMIT ${limit}`;
  const url = `http://192.168.0.79:8181/api/v3/query_sql?db=metai&q=${encodeURIComponent(query)}`;
  try {
    const response = await axios.get(url, { timeout: 5000 });
    res.json(response.data);
  } catch (e) {
    res.json({ error: e.message });
  }
});

const PORT = process.env.PORT || 8000;
app.listen(PORT, () => console.log(`API listening on port ${PORT}`));
