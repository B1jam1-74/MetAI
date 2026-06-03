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

// Server-Sent Events (SSE) clients
const sseClients = new Set();

const broadcastUpdate = () => {
  sseClients.forEach(client => {
    client.write('data: update\n\n');
  });
};

const WMO_FR = {
  0: "Ciel dégagé", 1: "Peu nuageux", 2: "Partiellement nuageux", 3: "Couvert",
  45: "Brouillard", 48: "Brouillard givrant", 51: "Bruine légère", 53: "Bruine modérée",
  55: "Bruine dense", 61: "Pluie légère", 63: "Pluie modérée", 65: "Pluie forte",
  71: "Neige légère", 73: "Neige modérée", 75: "Neige forte", 80: "Averses légères",
  81: "Averses", 82: "Averses fortes", 85: "Averses de neige", 95: "Orage",
  96: "Orage avec grêle", 99: "Orage avec grêle forte",
};

const wmoToFr = (code) => WMO_FR[code] || WMO_FR[Object.keys(WMO_FR).reduce((prev, curr) => Math.abs(curr - code) < Math.abs(prev - code) ? curr : prev)];

const logRecord = (receivedAt, temp, hum, press, pred, act, match) => {
  const timeStr = new Date(receivedAt).toLocaleTimeString('fr-FR', { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false });
  const t = temp !== null && temp !== undefined ? Number(temp).toFixed(1) : 'N/A';
  const h = hum !== null && hum !== undefined ? Number(hum).toFixed(1) : 'N/A';
  const p = press !== null && press !== undefined ? Number(press).toFixed(1) : 'N/A';
  const prediction = pred || 'Inconnu';
  const actual = act || 'Inconnu';
  const status = match ? '✅' : '❌';
  
  console.log(`${timeStr} | ${t} | ${h} | ${p} | Pred: ${prediction.padEnd(17)} | Act: ${actual} ${status}`);
};

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
    
    if (real.real_temp !== null && real.real_temp !== undefined) {
      const fields = [`real_temp=${real.real_temp}`];
      if (real.real_humidity !== undefined) fields.push(`real_humidity=${real.real_humidity}`);
      if (real.real_pressure !== undefined) fields.push(`real_pressure=${real.real_pressure}`);
      if (real.real_condition !== undefined) fields.push(`real_condition="${real.real_condition}"`);
      fields.push(`match=${match}`);
      line += `,${fields.join(',')}`;
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

  const exactMatch = db.prepare('SELECT * FROM uplinks WHERE device_id = ? AND received_at = ?').get(data.device_id, data.received_at);
  
  let existing = exactMatch;
  if (!exactMatch) {
    existing = db.prepare(`
      SELECT * FROM uplinks 
      WHERE device_id = ? 
        AND temperature IS NULL
        AND ABS((JULIANDAY(?) - JULIANDAY(received_at)) * 86400) < 15
      ORDER BY received_at DESC 
      LIMIT 1
    `).get(data.device_id, data.received_at);
  }

  const real = await fetchRealWeather();

  const finalTemp = data.temperature_deg_c;
  const finalHum = data.humidity_percent;
  const finalPress = data.pressure_hpa;
  const finalPredClass = existing ? existing.predicted_class : null;
  const finalPredFr = existing ? existing.prediction_fr : null;
  
  const finalRealTemp = real.real_temp;
  const finalRealHum = real.real_humidity;
  const finalRealPress = real.real_pressure;
  const finalRealWmo = real.real_wmo_code;
  const finalRealCond = real.real_condition;

  let match = existing ? existing.match : 0;
  if (finalPredFr && finalRealCond) {
    const a = finalPredFr.toLowerCase();
    const b = finalRealCond.toLowerCase();
    match = (a.includes(b) || b.includes(a)) ? 1 : 0;
  }

  logRecord(data.received_at, finalTemp, finalHum, finalPress, finalPredFr, finalRealCond, match);

  if (existing) {
    db.prepare(`
      UPDATE uplinks SET 
        temperature = ?, humidity = ?, pressure = ?, 
        predicted_class = COALESCE(?, predicted_class), 
        prediction_fr = COALESCE(?, prediction_fr),
        real_temp = ?, real_humidity = ?, real_pressure = ?, real_wmo_code = ?, real_condition = ?, match = ?
      WHERE id = ?
    `).run(finalTemp, finalHum, finalPress, finalPredClass, finalPredFr, finalRealTemp, finalRealHum, finalRealPress, finalRealWmo, finalRealCond, match, existing.id);
  } else {
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

  const influxData = {
    device_id: data.device_id,
    received_at: data.received_at,
    temperature_deg_c: finalTemp,
    humidity_percent: finalHum,
    pressure_hpa: finalPress,
    predicted_class: finalPredClass,
    prediction_fr: finalPredFr
  };
  writeInflux(influxData, { 
    real_temp: finalRealTemp, 
    real_humidity: finalRealHum, 
    real_pressure: finalRealPress, 
    real_condition: finalRealCond 
  }, match);
  
  broadcastUpdate();

  res.json({ status: 'ok', real_weather: { real_temp: finalRealTemp, real_condition: finalRealCond }, match: Boolean(match) });
});

app.post('/prediction', async (req, res) => {
  const data = req.body;
  if (data.device_id !== 'metai') {
    return res.json({ status: 'ignored', reason: 'unknown device' });
  }

  const exactMatch = db.prepare('SELECT * FROM uplinks WHERE device_id = ? AND received_at = ?').get(data.device_id, data.received_at);
  
  let existing = exactMatch;
  if (!exactMatch) {
    existing = db.prepare(`
      SELECT * FROM uplinks 
      WHERE device_id = ? 
        AND predicted_class IS NULL
        AND ABS((JULIANDAY(?) - JULIANDAY(received_at)) * 86400) < 15
      ORDER BY received_at DESC 
      LIMIT 1
    `).get(data.device_id, data.received_at);
  }
  
  // If sensors arrived first, use their real weather. Otherwise, fetch it now.
  const real = (existing && existing.real_condition) ? {
    real_temp: existing.real_temp,
    real_humidity: existing.real_humidity,
    real_pressure: existing.real_pressure,
    real_wmo_code: existing.real_wmo_code,
    real_condition: existing.real_condition
  } : await fetchRealWeather();

  const finalPredClass = data.predicted_class ?? (existing ? existing.predicted_class : null);
  const finalPredFr = data.prediction_fr ?? (existing ? existing.prediction_fr : null);
  
  let match = existing ? existing.match : 0;
  if (finalPredFr && real.real_condition) {
    const a = finalPredFr.toLowerCase();
    const b = real.real_condition.toLowerCase();
    match = (a.includes(b) || b.includes(a)) ? 1 : 0;
  }

  const temp = existing ? existing.temperature : null;
  const hum = existing ? existing.humidity : null;
  const press = existing ? existing.pressure : null;

  logRecord(data.received_at, temp, hum, press, finalPredFr, real.real_condition, match);

  if (existing) {
    db.prepare(`
      UPDATE uplinks SET 
        predicted_class = ?, prediction_fr = ?, 
        real_temp = COALESCE(?, real_temp), real_humidity = COALESCE(?, real_humidity), 
        real_pressure = COALESCE(?, real_pressure), real_wmo_code = COALESCE(?, real_wmo_code), 
        real_condition = COALESCE(?, real_condition), match = ?
      WHERE id = ?
    `).run(finalPredClass, finalPredFr, real.real_temp, real.real_humidity, real.real_pressure, real.real_wmo_code, real.real_condition, match, existing.id);
  } else {
    // Prediction arrived first; insert partial record
    db.prepare(`
      INSERT INTO uplinks (device_id, received_at, temperature, humidity, pressure,
        predicted_class, prediction_fr, real_temp, real_humidity, real_pressure, real_wmo_code, real_condition, match)
      VALUES (?, ?, NULL, NULL, NULL, ?, ?, ?, ?, ?, ?, ?, ?)
    `).run(
      data.device_id, data.received_at, finalPredClass, finalPredFr, 
      real.real_temp, real.real_humidity, real.real_pressure, real.real_wmo_code, real.real_condition, match
    );
  }

  // Ensure InfluxDB gets the complete record (prediction + sensor data)
  if (temp !== null && finalPredFr !== null) {
    const influxData = {
      device_id: data.device_id,
      received_at: existing ? existing.received_at : data.received_at,
      temperature_deg_c: temp,
      humidity_percent: hum,
      pressure_hpa: press,
      predicted_class: finalPredClass,
      prediction_fr: finalPredFr
    };
    writeInflux(influxData, { real_temp: real.real_temp, real_condition: real.real_condition }, match);
  }

  broadcastUpdate();

  res.json({ status: 'ok', match: Boolean(match) });
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
  const query = `SELECT time, "temperature_C", humidity_pct, "pressure_hPa", weather_label, real_condition FROM weather_sensor ORDER BY time DESC LIMIT ${limit}`;
  const url = `http://192.168.0.79:8181/api/v3/query_sql?db=metai&q=${encodeURIComponent(query)}`;
  try {
    const response = await axios.get(url, { timeout: 5000 });
    // InfluxDB v3 often returns { data: [...] } or similar, ensure we pass an array
    const responseData = Array.isArray(response.data) ? response.data : (response.data.data || []);
    res.json(responseData);
  } catch (e) {
    console.error(`[InfluxDB] query failed:`, e.message);
    res.json([]);
  }
});

app.get('/events', (req, res) => {
  res.setHeader('Content-Type', 'text/event-stream');
  res.setHeader('Cache-Control', 'no-cache');
  res.setHeader('Connection', 'keep-alive');
  res.flushHeaders();
  
  sseClients.add(res);
  req.on('close', () => {
    sseClients.delete(res);
  });
});

const PORT = process.env.PORT || 8000;
app.listen(PORT, () => console.log(`API listening on port ${PORT}`));
