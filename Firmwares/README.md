# 🔧 Firmwares — STM32U545 Firmware Projects

This directory contains all three **STM32CubeIDE** firmware projects for the MetAI embedded system.  
Each project targets the **NUCLEO-U545RE-Q** development board and is built with a **Makefile** (GCC arm-none-eabi toolchain), so it can be compiled from VS Code or any terminal — no IDE required.

---

## Summary

- [Projects Overview](#projects-overview)
- [Directory Structure](#directory-structure)
- [Prerequisites](#prerequisites)
- [Build Instructions](#build-instructions)
- [Flash Instructions](#flash-instructions)
- [Project Details](#project-details)
  - [test — Board validation](#test--board-validation)
  - [model_IA — First AI model](#model_ia--first-ai-model)
  - [implementation_gros_model — Full project](#implementation_gros_model--full-project)
- [Peripherals & Pin Configuration](#peripherals--pin-configuration)
- [Firmware Architecture](#firmware-architecture)

---

<a id="projects-overview"></a>
## Projects Overview

| Project | Description | AI Model | LoRaWAN | Sensors |
|---|---|---|---|---|
| `test` | Basic board validation (LED, UART, button) | ❌ | ❌ | ❌ |
| `model_IA` | First AI inference test on STM32 | ✅ (`rain_model.tflite`) | ❌ | ❌ |
| `implementation_gros_model` | **Complete project** — sensors + AI + LoRaWAN | ✅ (`meteo_multiclasse.tflite`) | ✅ | ✅ HTS221 + LPS22HH |

---

<a id="directory-structure"></a>
## Directory Structure

```
Firmwares/
├── test/                          # Basic board validation firmware
│   ├── Core/
│   │   ├── Inc/                   # Header files (main.h, hal_conf.h, …)
│   │   └── Src/                   # Source files (main.c, syscalls.c, …)
│   ├── Drivers/                   # STM32U5 HAL + CMSIS + BSP
│   ├── Makefile                   # Build system
│   ├── test.ioc                   # STM32CubeMX project file
│   └── STM32U545xx_FLASH.ld       # Linker script
│
├── model_IA/                      # First AI model firmware
│   ├── Core/
│   │   ├── Inc/
│   │   └── Src/
│   ├── Drivers/
│   ├── X-CUBE-AI/                 # AI runtime + generated network files
│   │   ├── App/                   # app_x-cube-ai.c, network.c, …
│   │   └── constants_ai.h
│   ├── Middlewares/ST/AI/         # STEdgeAI runtime library
│   ├── Makefile
│   └── model_IA.ioc
│
└── implementation_gros_model/     # Complete MetAI project (final firmware)
    ├── Core/
    │   ├── Inc/
    │   └── Src/
    │       ├── main.c             # Main application + LoRa + duty-cycle loop
    │       ├── hts221_read_data_polling.c   # Humidity/temperature sensor driver
    │       ├── lps22hh_read_data_polling.c  # Pressure/temperature sensor driver
    │       └── …
    ├── Drivers/
    │   ├── STMems/                # ST MEMS low-level register drivers
    │   └── STM32U5xx_HAL_Driver/
    ├── X-CUBE-AI/
    │   ├── App/                   # Generated AI inference code
    │   └── constants_ai.h
    ├── Middlewares/ST/AI/         # STEdgeAI runtime library
    ├── Makefile
    └── implementation_gros_model.ioc
```

---

<a id="prerequisites"></a>
## Prerequisites

| Tool | Purpose | Notes |
|---|---|---|
| `arm-none-eabi-gcc` | C compiler for Cortex-M | v12+ recommended |
| `make` | Build system | GNU Make |
| **STM32CubeProgrammer** | Flash binaries | or use `make flash` |
| **STM32CubeIDE** *(optional)* | IDE / debug | Only needed for `.ioc` editing |

> **Toolchain path** — If your `arm-none-eabi-gcc` is not in `$PATH`, pass the directory with `make GCC_PATH=/path/to/bin`.

### Install arm-none-eabi-gcc (Ubuntu/Debian)
```bash
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi
```

### Install STM32CubeProgrammer
Download the installer from [st.com](https://www.st.com/en/development-tools/stm32cubeprog.html) and install it.  
The Makefile assumes the CLI is at:
```
/opt/st/stm32cubeclt_1.20.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI
```
Override it at compile time if your path differs:
```bash
make flash STM32_CUBE_PROGRAMMER=/path/to/STM32_Programmer_CLI
```

---

<a id="build-instructions"></a>
## Build Instructions

All three projects use the same Makefile workflow.

### 1. Navigate to the target project
```bash
cd Firmwares/implementation_gros_model   # or model_IA / test
```

### 2. Build (debug, with optimisation `-Og`)
```bash
make -j
```
Artifacts are placed in the `build/` subdirectory:
```
build/
├── implementation_gros_model.elf
├── implementation_gros_model.hex   ← used by STM32CubeProgrammer
├── implementation_gros_model.bin
└── implementation_gros_model.map
```

### 3. Clean
```bash
make clean
```

### Compiler flags summary

| Flag | Value | Description |
|---|---|---|
| `-mcpu` | `cortex-m33` | Cortex-M33 core |
| `-mfpu` | `fpv5-sp-d16` | FPv5 single-precision FPU |
| `-mfloat-abi` | `hard` | Hardware floating-point ABI |
| `-Og` | — | Optimise for debugging |
| `-specs=nano.specs` | — | Newlib-nano (smaller libc) |
| `-u _printf_float` | — | Enable `%f` in `printf` |

---

<a id="flash-instructions"></a>
## Flash Instructions

### Option A — `make flash` (build + flash in one step)
```bash
make flash
# or with a custom programmer path:
make flash STM32_CUBE_PROGRAMMER=/path/to/STM32_Programmer_CLI
```
This connects over SWD at 4 MHz, writes the `.hex`, verifies, and resets the board.

### Option B — STM32CubeProgrammer GUI
1. Connect the NUCLEO-U545RE-Q over USB (ST-LINK).
2. Open **STM32CubeProgrammer** → select **ST-LINK** → connect.
3. Go to **Erasing & Programming** → browse to the `.hex` file (either from `build/` or from `Binaries/`).
4. Click **Start Programming**, then **Reset**.

### Option C — Flash a prebuilt binary from `Binaries/`
If you do not want to compile, pre-built `.hex` files are in the root `Binaries/` folder:
```
Binaries/
├── Final project/implementation_gros_model.hex  ← complete AI + LoRaWAN project
├── First AI model/model_IA.hex                  ← first AI model test
└── Simple board test/test.hex                   ← basic board validation
```

### Serial monitor
After flashing, open a serial terminal at **115 200 baud** (e.g. `minicom`, `picocom`, PuTTY) on the NUCLEO virtual COM port to read runtime logs:
```bash
picocom -b 115200 /dev/ttyACM0
```

---

<a id="project-details"></a>
## Project Details

---

### `test` — Board validation

**Purpose:** Minimal "hello world" for the NUCLEO-U545RE-Q.  
Validates that the board boots correctly, the green LED toggles on button press, and UART prints work.

**Main loop behaviour:**
- Prints `"Welcome to STM32 world !"` on startup over COM1 (115 200 baud).
- Toggles the green LED and prints `"Hello, World!"` each time the USER button is pressed (interrupt-driven).

**Peripherals used:** `ICACHE`, `COM1 (USART)`

---

### `model_IA` — First AI model

**Purpose:** First integration of an AI model with the X-CUBE-AI middleware.  
Runs `rain_model.tflite` (binary rain/no-rain classifier) on the STM32 using the Neural-ART accelerator.

**Main loop behaviour:**
- Calls `MX_X_CUBE_AI_Init()` at startup to initialise the inference engine.
- Runs `MX_X_CUBE_AI_Process()` every **3 seconds** in the main loop.
- The model is fed constant/placeholder sensor values in this version (no real sensors wired yet).

**Peripherals used:** `ICACHE`, `COM1 (USART)`, X-CUBE-AI runtime

**AI Model:** `rain_model.tflite` — Binary classifier (rain / no rain)

---

### `implementation_gros_model` — Full project

**Purpose:** The complete MetAI firmware. Reads real sensor data, runs multi-class AI inference, and transmits the result via LoRaWAN every 30 seconds.

#### Startup sequence
1. HAL + peripheral initialisation (`ICACHE`, `I2C1`, `LPUART1`, `X-CUBE-AI`).
2. I2C bus scan — detects all IKS01A3 sensors and reads their `WHO_AM_I` registers.
3. LoRaWAN join sequence over UART to the LoRa-E5 module:
   ```
   AT  →  AT+MODE=LWOTAA  →  AT+JOIN
   ```

#### Main loop (every 30 s)
```
Wake LoRa-E5  →  Read HTS221 (humidity + temperature)
              →  Read LPS22HH (pressure + temperature)
              →  Run AI inference (MX_X_CUBE_AI_Process)
              →  Format LoRa payload
              →  Send via AT+MSG
              →  Put LoRa-E5 to sleep (AT+LOWPOWER)
              →  MCU enters WFI sleep for 30 000 ms
```

#### LoRa uplink payload format
Uplinks are sent as **text** via `AT+MSG`:
```
AT+MSG="P=<pressure>,H=<humidity>,T=<temperature>,C=<class>"
```
Example:
```
AT+MSG="P=1013.25,H=65.40,T=18.20,C=2"
```

| Field | Unit | Source |
|---|---|---|
| `P` | hPa | LPS22HH barometric pressure sensor |
| `H` | % | HTS221 relative humidity sensor |
| `T` | °C | LPS22HH temperature |
| `C` | 0–N | Predicted weather class index |

#### AI Model: `meteo_multiclasse.tflite`
Multi-class weather classifier. Inputs: temperature (°C), humidity (%), pressure (hPa).  
Output: index of the predicted weather class (clear, cloudy, rain, fog, snow, …).

#### Clock & power configuration
| Parameter | Value |
|---|---|
| Oscillator | MSI at range 4 (~4 MHz) |
| PLL | Disabled |
| Voltage scaling | Scale 4 (lowest power) |
| SMPS regulator | Enabled (more efficient than LDO) |
| LoRa-E5 UART | LPUART1 at 9 600 baud |
| Debug UART | COM1 (USART) at 115 200 baud |
| I2C | I2C1 (standard mode, 7-bit addressing) |

---

<a id="peripherals--pin-configuration"></a>
## Peripherals & Pin Configuration

| Peripheral | Function | Notes |
|---|---|---|
| `I2C1` | IKS01A3 sensor bus | HTS221 @ 0x5F, LPS22HH @ 0x5C/0x5D |
| `LPUART1` | LoRa-E5 AT command interface | 9 600 baud, TX/RX |
| `COM1 (USART)` | Debug output via ST-LINK VCP | 115 200 baud |
| `ICACHE` | Instruction cache | 1-way (direct-mapped) |
| `GPIO` | User LED, USER button | LED_GREEN, BUTTON_USER (EXTI) |
| `X-CUBE-AI` | Neural-ART accelerator driver | STEdgeAI runtime |

---

<a id="firmware-architecture"></a>
## Firmware Architecture

```
main.c
│
├── Initialisation phase
│   ├── HAL_Init() / SystemClock_Config() / SystemPower_Config()
│   ├── MX_GPIO_Init()        — LED + button
│   ├── MX_ICACHE_Init()      — instruction cache
│   ├── MX_I2C1_Init()        — sensor bus
│   ├── MX_LPUART1_UART_Init()— LoRa-E5 link
│   └── MX_X_CUBE_AI_Init()   — AI inference engine
│
├── LoRaWAN join (AT+JOIN via LoRa-E5)
│
└── Main loop (every 30 s)
    ├── Wake LoRa-E5
    ├── hts221_read_data_polling()   — humidity + temperature
    ├── lps22hh_read_data_polling()  — pressure + temperature
    ├── MX_X_CUBE_AI_Process()       — AI inference
    ├── MX_X_CUBE_AI_GetLastPredictedClass()
    ├── AT+MSG (LoRa uplink)
    ├── AT+LOWPOWER (LoRa sleep)
    └── SleepForMs(30000)            — MCU WFI sleep
```

The AI inference engine (`X-CUBE-AI`) is integrated as a **middleware library** (`NetworkRuntime1020_CM33_GCC.a`).  
The network weights and architecture are embedded in the firmware flash at compile time via the generated files in `X-CUBE-AI/App/` (`network.c`, `network_data.c`, `network_data_params.c`).

---

*Université Savoie Mont-Blanc — Licence Électronique et Systèmes Embarqués et Télécommunications (ESET) — 2025/2026*  
*Maram Mezlini & Benjamin Avocat-Maulaz*
