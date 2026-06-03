/**
 * @file   metai_v3_config.h
 * @brief  Configuration MetAI v3 pour STM32U545RE-Q
 *         9 features : temp/pres/rhum a t0, t-3h, t-6h
 *         Aligne avec le downlink LoRa (hum_3h, pres_3h, temp_3h, ...)
 *         Genere automatiquement par le notebook MetAI_v3.
 *
 * Workflow firmware :
 *   1. Charger metai_v3_int8.tflite dans CubeAI / STEdgeAI
 *   2. Inclure ce header dans Core/Inc/
 *   3. Appeler METAI_normalize() puis METAI_quantize_input()
 *      avant chaque appel ai_run().
 */
#pragma once
#include <stdint.h>
#include <math.h>

/* -- Dimensions ---------------------------------------------------- */
#define METAI_NUM_FEATURES  9  /* 9 : temp/pres/rhum x (t0, t-3h, t-6h) */
#define METAI_NUM_CLASSES   7

/* -- Ordre des features -------------------------------------------- */
/* raw[0]=temp_t0  raw[1]=pres_t0  raw[2]=rhum_t0                     */
/* raw[3]=temp_t3  raw[4]=pres_t3  raw[5]=rhum_t3  (downlink t-3h)   */
/* raw[6]=temp_t6  raw[7]=pres_t6  raw[8]=rhum_t6  (downlink t-6h)   */

/* -- Normalisation min-max ----------------------------------------- */
static const float METAI_X_MIN[9] = {-9.1000004f, 980.4000244f, 18.0000000f, -9.1000004f, 980.4000244f, 18.0000000f, -9.1000004f, 980.4000244f, 18.0000000f};
static const float METAI_X_RANGE[9] = {44.1999969f, 62.7999268f, 82.0000000f, 44.1999969f, 62.7999268f, 82.0000000f, 44.1999969f, 62.7999268f, 82.0000000f};

/* -- Quantification INT8 ------------------------------------------ */
#define METAI_INPUT_SCALE       0.00392157f
#define METAI_INPUT_ZERO_POINT  -128
#define METAI_OUTPUT_SCALE      0.00390625f
#define METAI_OUTPUT_ZERO_POINT -128

/* -- Noms des classes (UART debug) --------------------------------- */
static const char* const METAI_CLASS_NAMES[7] = {
  "Clair / Ensoleille",
  "Nuageux / Couvert",
  "Pluie",
  "Averses",
  "Neige",
  "Orage",
  "Brouillard / Brume"
};

/* -- Noms des features (UART debug) -------------------------------- */
static const char* const METAI_FEAT_NAMES[9] = {
  "temp_t0",
  "pres_t0",
  "rhum_t0",
  "temp_t3",
  "pres_t3",
  "rhum_t3",
  "temp_t6",
  "pres_t6",
  "rhum_t6"
};

/* -- Normalisation inline ------------------------------------------ */
static inline void METAI_normalize(const float* raw, float* norm) {
  for (int i = 0; i < METAI_NUM_FEATURES; i++) {
    float v = (raw[i] - METAI_X_MIN[i]) / METAI_X_RANGE[i];
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    norm[i] = v;
  }
}

/* -- Quantification entree inline ---------------------------------- */
static inline void METAI_quantize_input(const float* norm, int8_t* quant) {
  for (int i = 0; i < METAI_NUM_FEATURES; i++) {
    float q = norm[i] / METAI_INPUT_SCALE + (float)METAI_INPUT_ZERO_POINT;
    if (q >  127.0f) q =  127.0f;
    if (q < -128.0f) q = -128.0f;
    quant[i] = (int8_t)(q + 0.5f);
  }
}

/* -- Decodage sortie inline ---------------------------------------- */
static inline uint8_t METAI_decode_output(const int8_t* out_i8) {
  uint8_t best = 0;
  int8_t  bval = out_i8[0];
  for (int i = 1; i < METAI_NUM_CLASSES; i++) {
    if (out_i8[i] > bval) { bval = out_i8[i]; best = (uint8_t)i; }
  }
  return best;
}