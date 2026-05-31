/**
 * @file   metai_v2_config.h
 * @brief  Configuration MetAI v2 pour STM32U545RE-Q
 *         Genere automatiquement par le notebook MetAI_v2.
 *
 * Workflow firmware :
 *   1. Charger metai_v2_int8.tflite dans CubeAI / STEdgeAI
 *   2. Inclure ce header dans Core/Inc/
 *   3. Appeler METAI_normalize() puis METAI_quantize_input()
 *      avant chaque appel ai_run().
 */
#pragma once
#include <stdint.h>
#include <math.h>

/* -- Dimensions ---------------------------------------------------- */
#define METAI_NUM_FEATURES  18
#define METAI_NUM_CLASSES   9

/* -- Normalisation min-max ----------------------------------------- */
/* x_norm[i] = (x_raw[i] - X_MIN[i]) / X_RANGE[i]  resultat dans [0,1] */
static const float METAI_X_MIN[18] = {-9.1000004f, 980.4000244f, 18.0000000f, -9.1000004f, 980.4000244f, 18.0000000f, -9.1000004f, 980.4000244f, 18.0000000f, -11.8999996f, -11.0000000f, -45.0000000f, -12.7000008f, -15.0999756f, -57.0000000f, -18.0000000f, -1.0000000f, -1.0000000f};
static const float METAI_X_RANGE[18] = {44.1999969f, 62.7999268f, 82.0000000f, 44.1999969f, 62.7999268f, 82.0000000f, 44.1999969f, 62.7999268f, 82.0000000f, 20.7999992f, 19.5000000f, 99.0000000f, 26.0000000f, 30.5000000f, 117.0000000f, 40.5999985f, 2.0000000f, 2.0000000f};

/* -- Quantification INT8 ------------------------------------------ */
/* Entree : quant[i] = roundf(norm[i] / INPUT_SCALE + INPUT_ZP)       */
/* Sortie : prob[i]  = (out_i8[i] - OUTPUT_ZP) * OUTPUT_SCALE         */
#define METAI_INPUT_SCALE       0.00392157f
#define METAI_INPUT_ZERO_POINT  -128
#define METAI_OUTPUT_SCALE      0.00390625f
#define METAI_OUTPUT_ZERO_POINT -128

/* -- Noms des classes (UART debug) --------------------------------- */
static const char* const METAI_CLASS_NAMES[9] = {
  "Clair / Ensoleille",
  "Peu nuageux",
  "Partiellement nuageux",
  "Nuageux / Couvert",
  "Pluie",
  "Averses",
  "Neige",
  "Orage",
  "Brouillard / Brume"
};

/* -- Noms des features (UART debug) -------------------------------- */
static const char* const METAI_FEAT_NAMES[18] = {
  "temp_t0",
  "pres_t0",
  "rhum_t0",
  "temp_t3",
  "pres_t3",
  "rhum_t3",
  "temp_t6",
  "pres_t6",
  "rhum_t6",
  "dtemp_3h",
  "dpres_3h",
  "drhum_3h",
  "dtemp_6h",
  "dpres_6h",
  "drhum_6h",
  "dew_point",
  "hour_sin",
  "hour_cos"
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
/* Retourne l indice de la classe la plus probable                     */
static inline uint8_t METAI_decode_output(const int8_t* out_i8) {
  uint8_t best = 0;
  int8_t  bval = out_i8[0];
  for (int i = 1; i < METAI_NUM_CLASSES; i++) {
    if (out_i8[i] > bval) { bval = out_i8[i]; best = (uint8_t)i; }
  }
  return best;
}