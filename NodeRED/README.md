### **Node-RED Integration**  

A Node-RED flow acts as the middleware between The Things Network (TTN) and our local backend services. It subscribes to the TTN application uplink topic, processes the decoded JSON payload, and routes it appropriately. This keeps the network stack modular and decoupled from the STM32 firmware. 

The flow operates in two main branches based on the LoRaWAN `f_port`:

1. **TTN Uplink Subscription**: An MQTT node listens for incoming messages from the device (`lora-e5`) on the TTN broker.
2. **Historical Data Downlink (Port 1)**: When sensor data arrives on port 1, the flow immediately queries the local InfluxDB for weather data from 3 hours and 6 hours ago. It then formats this historical context and triggers an HTTP POST to the TTN Downlink API (port 11) to send the data back to the device.
3. **Data Routing & API Forwarding**:
   - **Port 1 (Sensor Data)**: Parses the temperature, humidity, and pressure, then forwards it via an HTTP POST to the local backend API (`/uplink`).
   - **Port 12 (Model Prediction)**: Parses the ML prediction payload (predicted class, confidence percentage, and French label) and forwards it via an HTTP POST to the backend API (`/prediction`).
4. **Debug Nodes**: Scattered throughout the flow to allow real-time monitoring and payload inspection during development.

<p align="center">
    <img src="../Images/NodeRED_Flow.png" alt="Flow NodeRED" />
</p>
 
**Note on the network side:** Routing, cloud dashboards, and persistent storage fall outside our electronics/embedded specialty, so we kept the network stack intentionally minimal and modular. In practice, Node-RED already emits a complete, structured JSON payload, so the ingestion layer can be swapped without touching the STM32 firmware. We currently run a lightweight Docker setup with a FastAPI service for `/uplink` and `/prediction` ingestion, plus a Streamlit dashboard for visualization. Adding another backend (e.g., TimescaleDB) or frontend (e.g., Grafana) is therefore mostly a wiring and configuration task.  

<p align="center">
    <img src="../Images/Web_server.png" alt="Web page of the server" />
</p>

The server receives uplinks from Node-RED, stores each sample in a local database, fetches real weather from Open-Meteo for comparison, and serves both raw history and aggregate metrics to the dashboard. This provides a full end-to-end pipeline (Sensor → TTN → Node-RED → Database → API → Dashboard) while keeping deployment simple and reproducible with Docker Compose.  