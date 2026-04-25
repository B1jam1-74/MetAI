# **🌦️ MetAI — Embedded Weather Prediction with AI on STM32U545**  
**Université Savoie Mont-Blanc** — Licence 3 ESET  
 *Maram Mezlini & Benjamin Avocat-Maulaz*  

## **Introduction**  
MetAI is an embedded AI project that runs a weather prediction model directly on an ultra-low-power STM32U545 microcontroller. Using onboard sensors (temperature, humidity, pressure), the system infers the current weather condition locally — no cloud compute required — and transmits the result over LoRaWAN for remote monitoring. The project demonstrates that meaningful AI inference can coexist with strict energy budgets, making it relevant for battery-operated or energy-harvesting IoT nodes.  

## **Part 1 — Hardware**  
### **STM32U545 — The Microcontroller**  
The brain of the project is the **STM32U545**, a member of STMicroelectronics' ultra-low-power  **STM32U5** family. Key characteristics relevant to this project:  
| | |  
|-|-|  
| **Feature** | **Value** |   
| Core | Arm Cortex-M33, up to 160 MHz |   
| Flash | 1 MB |   
| RAM | 786 KB (SRAM1 + SRAM2 + ICACHE) |   
| Supply voltage | 1.71 V – 3.6 V |   
| Low-power modes | Stop 0/1/2/3, Standby, Shutdown |   
| Neural-ART Accelerator | Hardware MAC units for AI inference |   
| Development board | NUCLEO-U545RE-Q |   
| | |  

The U545's **Neural-ART Accelerator** is what makes on-device AI inference viable at milliwatt-level power: it offloads the multiply-accumulate operations of the neural network from the CPU, dramatically reducing inference time and energy per prediction.  

![alt text](Images/U545.jpg)

### **Extension Board**  
The U545 board is connected to an IKS01A3 extension board. It carries all kind of sensors such as :  
- **temperature (°C)**
- **relative humidity (%)**
- **barometric pressure (hPa)**
The three inputs fed to the AI model.  

![alt text](Images/IKS01A3.png)

### **LoRa-E5 module** 
Handles the LoRaWAN radio link (see Part 3). 

![alt text](Images/LoRa-E5.png)

## **Part 2 — AI Models**  
Both models were trained in Python (TensorFlow/Keras) on historical meteorological data sourced via [Meteostat](https://meteostat.net/ "https://meteostat.net/"), using a weather station near Chambéry, France. They take three scalar inputs:  
| | |  
|-|-|  
| **Input** | **Unit** |   
| Temperature | °C |   
| Relative humidity | % |   
| Barometric pressure | hPa |   
### **Model A — Binary Rain Classifier**  
The first and simpler model answers a single question: **will it rain?** It outputs a sigmoid probability and is thresholded at 0.5 to produce a binary label. This model is extremely lightweight and was used as a baseline to validate the full pipeline (sensor → inference → LoRaWAN → TTN → Node-RED).  
### **Model B — Multi-class Weather Classifier**  
The second model extends the output to **12 weather classes**, enabling a richer description of conditions. After training and quantization, it is converted to a TFLite FlatBuffer and deployed on the U545 using  **STM32Cube.AI** with the Neural-ART runtime.  
The 12 predicted classes are:  
| | | |  
|-|-|-|  
| **#** | **French label** | **Description** |   
| 0 | Clair / ensoleillé | Clear sky |   
| 1 | Peu nuageux | Mostly sunny |   
| 2 | Partiellement nuageux | Partly cloudy |   
| 3 | Nuageux / couvert | Overcast |   
| 4 | Pluie | Rain |   
| 5 | Averses | Showers |   
| 6 | Neige | Snow |   
| 7 | Neige légère / averses de neige | Light snow / snow showers |   
| 8 | Pluie et neige mêlées | Sleet |   
| 9 | Orage | Thunderstorm |   
| 10 | Brouillard / brume | Fog / mist |   
| 11 | Vent fort | Strong wind |   
| 12 | Orage violent | Severe thunderstorm |   
The firmware reads the argmax of the softmax output and encodes both the class index (predicted_class) and its French label (prediction_fr) into the LoRaWAN uplink payload.  
## **Part 3 — LoRaWAN**  
### **What is LoRaWAN?**  
**LoRa** (Long Range) is a spread-spectrum radio modulation developed by Semtech, designed for low-power, long-range communication (up to tens of kilometres in open terrain).  **LoRaWAN** is the MAC layer protocol built on top of LoRa that defines how devices connect to a network of gateways and route packets to an application server. Its key properties for embedded IoT are:  
- Very low transmit power (typically 14–20 dBm)  
- Extremely low device power budget — devices can run for years on a battery  
- Star-of-stars topology: end-nodes → gateways → Network Server (e.g. TTN) → Application Server  
### **Payload Encoding and TTN Decoding**  
On the STM32U545, the uplink payload is encoded as a compact binary frame carrying:  
1. Temperature (signed 16-bit, ×100 for two-decimal precision)  
2. Humidity (unsigned 8-bit, integer %)  
3. Pressure (unsigned 16-bit, integer hPa)  
4. Predicted class index (unsigned 8-bit)  
On **The Things Network (TTN)**, a JavaScript *Payload Formatter* (uplink codec) decodes this binary frame back into a structured JSON object:  
{  
  "device_id": "metai",  
  "received_at": "2026-04-22T14:29:10.467Z",  
  "temperature_deg_c": 25.84,  
  "humidity_percent": 39.6,  
  "pressure_hpa": 981,  
  "predicted_class": 5,  
  "prediction_fr": "Averses"  
}  
   
### **Node-RED Integration**  
Once decoded by TTN, the data is forwarded to a **Node-RED** flow that performs an HTTP POST to a [Request Baskets](https://rbaskets.in/ "https://rbaskets.in/") endpoint. This makes the payload immediately inspectable from a browser, as shown in the screenshot below, and provides a convenient webhook URL that any downstream service can subscribe to.  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAADUlEQVR4nGP4//8/AwAI/AL+p5qgoAAAAABJRU5ErkJggg==)  
**Note on the network side:** Routing, cloud dashboards, and persistent storage fall outside our electronics/embedded speciality, so we kept the network stack intentionally minimal. That said, since the full structured JSON is already being POSTed by Node-RED, plugging in a database (InfluxDB, TimescaleDB) or a dashboard (Grafana, Datacake) is straightforward and requires no firmware changes.  
## **Part 4 — Power Consumption**  
### **Why It Matters for AI**  
Neural network inference is inherently compute-intensive. On a general-purpose server, a single forward pass through even a small model draws hundreds of milliwatts. Bringing AI to the edge means this cost must be paid by a battery or a small energy harvester — so minimizing inference energy is critical.  
The U545's Neural-ART accelerator helps significantly: by executing MAC operations in dedicated hardware rather than running them on the Cortex-M33, inference completes faster and at lower energy per operation than a pure software implementation.  
### **Measured Power Budget**  
Our system operates at **1.8 V** supply and draws approximately  **3 mA** in the active sensing + inference phase, corresponding to:  
$$P = V \times I = 1.8,\text{V} \times 3,\text{mA} = \mathbf{5.4,mW}$$  
Between acquisitions, the MCU enters a low-power Stop mode, bringing average consumption well below the active peak.  
### **Measuring Power Consumption with the LPM01A**  
STMicroelectronics' **X-NUCLEO-LPM01A** (Power Shield) is the reference tool for accurate current measurement on NUCLEO boards. The procedure is as follows:  
5. **Hardware setup:** Remove the IDD jumper on the NUCLEO-U545RE-Q and connect the LPM01A in series on the 3.3 V / VDD supply rail using the dedicated headers.  
6. **Software:** Install  **STM32CubeMonitor-Power** on the host PC and connect to the LPM01A over USB.  
7. **Configure acquisition:** Select the appropriate current range (µA for Stop mode, mA for active mode). Set the sampling rate to at least 10 kHz to capture inference spikes accurately.  
8. **Run:** Flash the firmware, trigger a measurement session, and observe the current waveform. STM32CubeMonitor-Power integrates the waveform to give average current, peak current, and total charge (µAh) per acquisition cycle.  
9. **Identify phases:** You will see distinct phases — sensor read (~ms), Neural-ART inference (~ms), LoRa TX burst (~hundreds of ms at higher current), and Stop mode (µA baseline).  
## **Part 5 — Conclusion**  
This project was an introduction to on-device artificial intelligence in a constrained embedded context. Building and training the models highlighted how much representational power even a small dense network can have — the multi-class classifier reaches solid accuracy using only three sensor inputs. Deploying that model on a Cortex-M33 with a hardware neural accelerator, and watching it produce correct predictions at 5 mW, made the energy argument for edge AI very concrete.  
At the same time, the project reinforced that **energy consumption is a first-class constraint** in embedded AI, not an afterthought. Every design choice — quantization, model depth, duty cycle, supply voltage — has a direct impact on battery life. Future work could explore INT8 quantization (vs the current float32 baseline) and adaptive duty cycling to extend autonomy further.  
## **Repository Structure**  
MetAI/  
├── Notebooks/              # Jupyter notebooks — data prep, training, evaluation  
│   ├── binary_rain_model.ipynb  
│   └── multiclass_weather_model.ipynb  
├── Firmware/               # STM32CubeIDE project for NUCLEO-U545RE-Q  
│   ├── Core/  
│   ├── X-CUBE-AI/          # Generated Neural-ART runtime files  
│   └── ...  
├── NodeRED/                # Node-RED flow export (JSON)  
├── TTN/                    # TTN payload formatter (JavaScript)  
└── docs/                   # Images and schematics  
   
## **Dependencies**  
| | |  
|-|-|  
| **Tool** | **Purpose** |   
| STM32CubeIDE | Firmware development |   
| STM32Cube.AI / STEdgeAI | Model conversion and deployment |   
| TensorFlow / Keras | Model training |   
| Meteostat | Historical weather data |   
| The Things Network | LoRaWAN network server |   
| Node-RED | Payload forwarding |   
| STM32CubeMonitor-Power | Power consumption measurement |   
*Université Savoie Mont-Blanc — Licence Électronique et Systèmes Embarqués et Télécommunications (ESET) — 2025/2026*  
   
