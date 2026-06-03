# metAI Server (JavaScript Stack)

This project replaces the previous Python-based server with a modern, full JavaScript stack for improved performance, maintainability, and a beautiful user interface.

## Architecture

- **API (`/api`)**: Node.js + Express + `better-sqlite3` + `axios`. Handles incoming LoRaWAN uplinks, fetches real-time weather from Open-Meteo, compares predictions, stores data in SQLite, and writes to InfluxDB.
- **Dashboard (`/dashboard`)**: React + Vite + Tailwind CSS + Recharts. A modern, responsive, dark-themed dashboard that auto-refreshes every 30 seconds, displaying KPIs, accuracy gauges, time-series charts, and historical data.

## Prerequisites

- Docker & Docker Compose
- Node.js 20+ (for local development)

## Running with Docker (Recommended)

```bash
docker-compose up --build -d
```

- API will be available at: `http://localhost:8000`
- Dashboard will be available at: `http://localhost:8501`

## Local Development

### API
```bash
cd api
npm install
npm run dev
```

### Dashboard
```bash
cd dashboard
npm install
npm run dev
```
*(The Vite dev server proxies `/api` requests to `http://localhost:8000`)*

## API Endpoints

- `POST /uplink`: Receive weather sensor data.
- `GET /uplinks?limit=200`: Get recent uplinks from SQLite.
- `GET /stats`: Get aggregated model accuracy and averages.
- `GET /influx_history?limit=200`: Proxy query to InfluxDB for raw history.

## Data Storage

- **SQLite**: Stored in `./data/metai.db` (persisted via Docker volume).
- **InfluxDB**: Writes to the configured InfluxDB instance (`http://192.168.0.79:8181`).
