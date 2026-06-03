# **🌦️ MetAI Server — Backend API & Monitoring Dashboard**  

This directory contains the modern, full JavaScript-stack replacement for the legacy Python-based server. It provides a robust backend for ingesting LoRaWAN weather sensor data, comparing embedded AI predictions against real-time Open-Meteo ground truth, and visualizing the results in a responsive, real-time dashboard.

<p align="center">
	<img src="../Images/Web_server.png" alt="MetAI Dashboard" width="80%" />
</p>

## **Architecture**

The server is split into two main services, orchestrated via Docker Compose:

- **API (`/api`)**: Built with Node.js, Express, and `better-sqlite3`. It handles incoming LoRaWAN uplinks, fetches real-time weather conditions from the Open-Meteo API, merges out-of-order sensor and prediction payloads, calculates model accuracy, persists records locally in SQLite, and streams high-volume time-series data to a remote InfluxDB instance.
- **Dashboard (`/dashboard`)**: A modern, dark-themed React 18 + Vite application styled with Tailwind CSS. It uses Recharts for data visualization and listens to Server-Sent Events (SSE) from the API for real-time, zero-latency UI updates without manual polling.

## **Data Flow**

1. **TTN (The Things Network)** receives the LoRaWAN uplink from the STM32U545 device.
2. **Node-RED** subscribes to the TTN application and forwards the decoded JSON payload via HTTP POST to the API's `/uplink` endpoint. *(See [Node-RED Flow](../Images/NodeRED_Flow.png))*
3. The **API** immediately queries the **Open-Meteo API** for current ground-truth conditions at Thonon-les-Bains, France.
4. The API merges the sensor data with the ML prediction (which may arrive slightly earlier or later), evaluates the match, updates the local SQLite database, and writes a formatted line-protocol payload to **InfluxDB**.
5. The API broadcasts an SSE `update` event.
6. The **Dashboard** receives the event, fetches the latest aggregated stats and historical data, and instantly re-renders the KPIs, accuracy gauge, and time-series charts.

## **Prerequisites**

- Docker & Docker Compose
- Node.js 20+ (only required for local development without Docker)

## **Deployment (Recommended)**

The easiest way to run the entire stack is using Docker Compose from the root of this `Serveur` directory:

```bash
docker-compose up --build -d
```

This will start both services in the background:
- **API**: `http://localhost:8000`
- **Dashboard**: `http://localhost:8501`

To view logs for troubleshooting:
```bash
docker-compose logs -f api
docker-compose logs -f dashboard
```

## **Local Development**

If you prefer to run the services natively for active development:

### **1. API**
```bash
cd api
npm install
npm run dev
```
*(The API will start on `http://localhost:8000`)*

### **2. Dashboard**
```bash
cd dashboard
npm install
npm run dev
```
*(The Vite dev server runs on `http://localhost:5173` and automatically proxies `/api` requests to `http://localhost:8000`)*

## **API Endpoints**

| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `POST` | `/uplink` | Ingests raw weather sensor data from the device. Triggers Open-Meteo fetch and InfluxDB write. |
| `POST` | `/prediction` | Ingests the embedded ML model's weather prediction. Merges with existing sensor data if needed. |
| `GET` | `/uplinks?limit=200` | Retrieves recent merged uplink records from the local SQLite database. |
| `GET` | `/stats` | Returns aggregated model accuracy metrics and sensor averages. |
| `GET` | `/influx_history?limit=200` | Proxies a SQL query to the remote InfluxDB instance to fetch raw time-series history. |
| `POST` | `/reset` | Clears all historical uplink data and resets the accuracy counter to zero. |
| `GET` | `/events` | Server-Sent Events (SSE) stream for real-time dashboard updates. |

## **Data Storage**

- **SQLite**: Local relational storage for quick lookups, recent uplinks, and aggregated stats. Persisted across container restarts via the `./data/metai.db` Docker volume.
- **InfluxDB**: Remote time-series database (`http://192.168.0.79:8181`) configured to handle high-volume historical sensor and prediction data for long-term charting.

---
*Université Savoie Mont-Blanc — Licence Électronique et Systèmes Embarqués et Télécommunications (ESET) — 2025/2026*
