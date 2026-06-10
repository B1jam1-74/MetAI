# 📓 Jupyter Notebooks — AI Model Training

This directory contains the two Google Colab notebooks used to train, evaluate, and export the AI models embedded in the MetAI firmware.

Both notebooks are designed to run on **Google Colab** (GPU runtime recommended) and store all artefacts in **Google Drive**.

---

## Summary

- [Notebooks Overview](#notebooks-overview)
- [How to Run](#how-to-run)
- [Model_test.ipynb — Binary rain classifier](#model_testipynb--binary-rain-classifier)
- [Model_final.ipynb — Multi-class weather classifier](#model_finalipynb--multi-class-weather-classifier)
- [Data Pipeline — Meteostat](#data-pipeline--meteostat)
- [Feature Engineering](#feature-engineering)
- [TFLite Export for STM32](#tflite-export-for-stm32)
- [Dependencies](#dependencies)

---

<a id="notebooks-overview"></a>
## Notebooks Overview

| Notebook            | Model                                    | Task                                       | Output                     |
| ------------------- | ---------------------------------------- | ------------------------------------------ | -------------------------- |
| `Model_test.ipynb`  | Binary classifier (Dense 64→32→1)        | Rain / No rain                             | `rain_model.tflite`        |
| `Model_final.ipynb` | Multi-class classifier (Residual blocks) | 9 weather classes                          | `meteo_multiclasse.tflite` |
| `MetAI_v2.ipynb`    | More accurate model using previous data  | 7 weather classes + 3 and 6 hours ago data | `metai_v2.tflite`          |

---

<a id="how-to-run"></a>
## How to Run

Both notebooks are self-contained and designed to run on **Google Colab**.

1. Open [Google Colab](https://colab.research.google.com/) and upload the `.ipynb` file (or open it directly from Google Drive).
2. Select a **GPU runtime** (`Runtime → Change runtime type → GPU`) for faster training.
3. Run the first cell to mount your Google Drive:
   ```python
   from google.colab import drive
   drive.mount('/content/gdrive')
   ```
4. All model artefacts (`.keras`, `.tflite`, training curves) are saved to:
   ```
   /content/gdrive/MyDrive/TF_Project/
   ```
5. Install dependencies (handled by the notebook):
   ```python
   %pip install "pandas>=2.0,<3.0" meteostat
   ```

> **TensorFlow version** — The notebooks use the TensorFlow version pre-installed in Colab. No manual TF install is required.  
> GPU availability is verified automatically at startup (`tf.config.experimental.list_physical_devices('GPU')`).

---

<a id="model_testipynb--binary-rain-classifier"></a>
## `Model_test.ipynb` — Binary Rain Classifier

### Goal
First proof-of-concept: can a small dense neural network predict whether it will rain from only three sensor inputs?

### Data
Historical hourly data from [Meteostat](https://meteostat.net/) — weather station near **Le Bourget du Lac, France** (lat: 45.64, lon: 5.87) from **2021 to 2025**.

| Feature | Unit | Source |
|---|---|---|
| Barometric pressure | hPa | Meteostat `PRES` |
| Air temperature | °C | Meteostat `TEMP` |
| Relative humidity | % | Meteostat `RHUM` |

**Label:** `1` = rain (`prcp > 0.5 mm`), `0` = no rain.

### Preprocessing
- Drop rows containing NaN values.
- Normalize features to `[0, 1]` using min-max scaling:
  ```
  X_norm = (X - X_min) / (X_max - X_min)
  ```
- 80/20 stratified train/test split.

### Model Architecture

```
Input (3,)
  └─ Dense 64  ReLU
  └─ Dense 32  ReLU
  └─ Dense 1   Sigmoid   ← rain probability ∈ [0, 1]
```

| Parameter | Value |
|---|---|
| Loss | `binary_crossentropy` |
| Optimizer | Adam |
| Epochs | 50 |
| Batch size | 32 |
| Metric | Accuracy |

### Outputs
- `rain_model_AccuXXXX.keras` — saved Keras model  
- `rain_model.tflite` — exported TFLite model for STM32 deployment

### Inference helper
```python
def predict_rain(pressure, temperature, humidity):
    # Returns "🌧 Rain" or "☀️ No rain"
```

---

<a id="model_finalipynb--multi-class-weather-classifier"></a>
## `Model_final.ipynb` — Multi-class Weather Classifier

### Goal
Production model: classify the current weather into one of **9 coherent weather conditions** from sensor readings, using a deeper residual network with temporal history features.

### Weather Classes (9)

| Index | Class (French) | Description |
|---|---|---|
| 0 | Clair / ensoleillé | Clear / sunny |
| 1 | Peu / partiellement nuageux | Few / partly cloudy |
| 2 | Nuageux / couvert | Cloudy / overcast |
| 3 | Brouillard / brume | Fog / mist |
| 4 | Précipitations liquides | Rain or showers |
| 5 | Précipitations neigeuses | Snow (light or heavy) |
| 6 | Pluie et neige mêlées | Sleet / mixed rain-snow |
| 7 | Orage | Thunderstorm (any intensity) |
| 8 | Vent fort | Strong wind |

> **Why 9 classes instead of 13?** From pressure, temperature, and humidity alone, some class pairs are physically indistinguishable (e.g., *Rain* vs *Showers*, or *Snow* vs *Light snow*). Merging them removes label noise and improves model accuracy.

### Data
Same Meteostat source as `Model_test.ipynb`. Labels are derived from the Meteostat **COCO** condition code combined with precipitation (`prcp`), snow depth (`snow`), and wind speed (`wspd`) fields.

### Feature Engineering
The final model uses **10 engineered features** (not just the raw 3):

| Feature | Description |
|---|---|
| `pres` | Current pressure (hPa) |
| `temp` | Current temperature (°C) |
| `rhum` | Current relative humidity (%) |
| `dew_point` | Dew point approximation: `temp - (100 - rhum) / 5` |
| `abs_humidity` | Absolute humidity (g/m³) |
| `pres_t-1`, `temp_t-1`, `rhum_t-1` | Previous reading (t-1) |
| `Δpres`, `Δtemp`, `Δrhum` | Delta from t-1 to current reading |
| `Δpres_3h` | 3-step pressure trend (`pres` − `pres_t-3`) |

> The `HISTORY_STEPS = 3` setting keeps 3 previous readings in memory. This is feasible on STM32 as only 3 floats (p, t, h) per past step need to be stored.

### Preprocessing
- Min-max normalization → `[0, 1]` range using min/max computed on the training set.
- Class-weight balancing with `sklearn.utils.class_weight.compute_class_weight` to handle imbalanced classes (sunny days >> foggy days).
- 80/20 stratified train/test split.

### Model Architecture

The final model uses a **residual block** architecture:

```
Input (10,)
  └─ Dense 128  ReLU  + BatchNorm + Dropout(0.1)
  └─ Residual block [128 → 128]  Dropout(0.07)
  └─ Residual block [128 → 96]   Dropout(0.05)
  └─ Residual block [96  → 64]   Dropout(0.04)
  └─ Dense 32   ReLU  + BatchNorm + Dropout(0.03)
  └─ Dense 9    Softmax   ← class probabilities
```

| Parameter | Value |
|---|---|
| Loss | Focal loss (`γ=2.0`, class-weighted `α`) |
| Optimizer | Adam (lr=8e-4) |
| Max epochs | 150 |
| Batch size | 64 |
| Metrics | Top-1 accuracy, Top-3 accuracy |

### Training Callbacks
- **ModelCheckpoint** — saves best model on `val_accuracy`
- **EarlyStopping** — stops after 25 epochs with no improvement, restores best weights
- **ReduceLROnPlateau** — halves learning rate after 8 stale epochs (min lr: 5e-6)

### Outputs
- `best_meteo_v2.keras` — best checkpoint during training  
- `meteo_v2_accXXXX.keras` — final model  
- `meteo_v2.tflite` — exported TFLite model for STM32 deployment  
- `training_curves_v2.png` — top-1 and top-3 accuracy curves  

### Inference helper
```python
def predict_meteo_fr(pressure, temperature, humidity, history=None):
    """
    history: list of (pressure, temperature, humidity) for t-1, t-2, t-3
             If None, repeats current reading (cold start).
    """
    # Returns (class_index, probabilities_array)
```

Usage examples:
```python
predict_meteo_fr(1015, 22, 35)   # Clear day → "Clair / ensoleillé"
predict_meteo_fr(1006,  9, 90,   # Rainy with history
    history=[(1008, 10, 86), (1009, 11, 83), (1010, 11, 80)])
```

---

<a id="data-pipeline--meteostat"></a>
## Data Pipeline — Meteostat

Both notebooks use the same data pipeline:

```python
import meteostat as ms

POINT = ms.Point(45.641632, 5.869613, 113)  # Le Bourget du Lac
START = date(2021, 1, 1)
END   = date(2025, 12, 31)

stations = ms.stations.nearby(POINT, limit=4)
ts = ms.hourly(stations, START, END)
df = ms.interpolate(ts, POINT).fetch()
```

The data is fetched once, interpolated from nearby weather stations, and stored as NumPy arrays for training.

> `ms.config.block_large_requests = False` is set to allow requests spanning more than 3 years.

---

<a id="feature-engineering"></a>
## Feature Engineering

```
Model_test   →  3 raw features  (pres, temp, rhum)
Model_final  →  10 features     (raw + dew point + absolute humidity + t-1 deltas + 3h pressure trend)
```

The `build_sensor_features()` function in `Model_final.ipynb` handles the full feature construction pipeline from raw arrays.  
The resulting scaler constants (`SCALER_MEAN`, `SCALER_STD`) are printed as a ready-to-copy C header for use in the STM32 firmware pre-processing code:

```c
/* ── scaler_params.h ─────────── */
#define N_FEATURES 10
static const float SCALER_MEAN[10] = { ... };
static const float SCALER_STD[10]  = { ... };
```

---

<a id="tflite-export-for-stm32"></a>
## TFLite Export for STM32

Both notebooks export a standard float32 TFLite model:

```python
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()
with open("model.tflite", "wb") as f:
    f.write(tflite_model)
```

The exported `.tflite` file is then:
1. Placed in the `AI Models/` folder of this repository.
2. Converted by **STM32Cube.AI / STEdgeAI** into C source files (`network.c`, `network_data.c`) embedded directly in the STM32 firmware.

A quick validation cell at the end of each notebook confirms the TFLite model's input/output shapes before export:

```python
interpreter = tf.lite.Interpreter(model_content=tflite_model)
interpreter.allocate_tensors()
print("Input shape :", input_details[0]['shape'])   # [1, 3] or [1, 10]
print("Input dtype :", input_details[0]['dtype'])   # float32
print("Output shape:", output_details[0]['shape'])  # [1, 1] or [1, 9]
```

> Optional INT8 quantisation is commented out in `Model_final.ipynb`. Uncomment `converter.optimizations = [tf.lite.Optimize.DEFAULT]` for a smaller binary footprint on STM32 Flash (at a potential minor accuracy cost).

---

<a id="dependencies"></a>
## Dependencies

All dependencies are installed inside the notebooks via `%pip install`. No local setup is required.

| Package | Version | Purpose |
|---|---|---|
| `tensorflow` | Colab default (≥2.15) | Model training & TFLite export |
| `meteostat` | latest | Historical weather data |
| `pandas` | ≥2.0, <3.0 | Data manipulation |
| `numpy` | Colab default | Numerical computing |
| `matplotlib` | Colab default | Training curves & data visualisation |
| `scikit-learn` | Colab default | Train/test split, class weights |

---

*Université Savoie Mont-Blanc — Licence Électronique et Systèmes Embarqués et Télécommunications (ESET) — 2025/2026*  
*Maram Mezlini & Benjamin Avocat-Maulaz*
