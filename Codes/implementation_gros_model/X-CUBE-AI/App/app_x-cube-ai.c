
/**
  ******************************************************************************
  * @file    app_x-cube-ai.c
  * @author  X-CUBE-AI C code generator
  * @brief   AI program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

 /*
  * Description
  *   v1.0 - Minimum template to show how to use the Embedded Client API
  *          model. Only one input and one output is supported. All
  *          memory resources are allocated statically (AI_NETWORK_XX, defines
  *          are used).
  *          Re-target of the printf function is out-of-scope.
  *   v2.0 - add multiple IO and/or multiple heap support
  *
  *   For more information, see the embeded documentation:
  *
  *       [1] %X_CUBE_AI_DIR%/Documentation/index.html
  *
  *   X_CUBE_AI_DIR indicates the location where the X-CUBE-AI pack is installed
  *   typical : C:\Users\[user_name]\STM32Cube\Repository\STMicroelectronics\X-CUBE-AI\7.1.0
  */

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

#if defined ( __ICCARM__ )
#elif defined ( __CC_ARM ) || ( __GNUC__ )
#endif

/* System headers */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>

#include "app_x-cube-ai.h"
#include "main.h"
#include "ai_datatypes_defines.h"
#include "network.h"
#include "network_data.h"

/* USER CODE BEGIN includes */

/* USER CODE END includes */

/* IO buffers ----------------------------------------------------------------*/

#if !defined(AI_NETWORK_INPUTS_IN_ACTIVATIONS)
AI_ALIGNED(4) ai_i8 data_in_1[AI_NETWORK_IN_1_SIZE_BYTES];
ai_i8* data_ins[AI_NETWORK_IN_NUM] = {
data_in_1
};
#else
ai_i8* data_ins[AI_NETWORK_IN_NUM] = {
NULL
};
#endif

#if !defined(AI_NETWORK_OUTPUTS_IN_ACTIVATIONS)
AI_ALIGNED(4) ai_i8 data_out_1[AI_NETWORK_OUT_1_SIZE_BYTES];
ai_i8* data_outs[AI_NETWORK_OUT_NUM] = {
data_out_1
};
#else
ai_i8* data_outs[AI_NETWORK_OUT_NUM] = {
NULL
};
#endif

/* Activations buffers -------------------------------------------------------*/

AI_ALIGNED(32)
static uint8_t pool0[AI_NETWORK_DATA_ACTIVATION_1_SIZE];

ai_handle data_activations0[] = {pool0};

/* AI objects ----------------------------------------------------------------*/

static ai_handle network = AI_HANDLE_NULL;

static ai_buffer* ai_input;
static ai_buffer* ai_output;

static void ai_log_err(const ai_error err, const char *fct)
{
  /* USER CODE BEGIN log */
  if (fct)
    printf("TEMPLATE - Error (%s) - type=0x%02x code=0x%02x\r\n", fct,
        err.type, err.code);
  else
    printf("TEMPLATE - Error - type=0x%02x code=0x%02x\r\n", err.type, err.code);

  do {} while (1);
  /* USER CODE END log */
}

static int ai_boostrap(ai_handle *act_addr)
{
  ai_error err;

  /* Create and initialize an instance of the model */
  err = ai_network_create_and_init(&network, act_addr, NULL);
  if (err.type != AI_ERROR_NONE) {
    ai_log_err(err, "ai_network_create_and_init");
    return -1;
  }

  ai_input = ai_network_inputs_get(network, NULL);
  ai_output = ai_network_outputs_get(network, NULL);

#if defined(AI_NETWORK_INPUTS_IN_ACTIVATIONS)
  /*  In the case where "--allocate-inputs" option is used, memory buffer can be
   *  used from the activations buffer. This is not mandatory.
   */
  for (int idx=0; idx < AI_NETWORK_IN_NUM; idx++) {
	data_ins[idx] = ai_input[idx].data;
  }
#else
  for (int idx=0; idx < AI_NETWORK_IN_NUM; idx++) {
	  ai_input[idx].data = data_ins[idx];
  }
#endif

#if defined(AI_NETWORK_OUTPUTS_IN_ACTIVATIONS)
  /*  In the case where "--allocate-outputs" option is used, memory buffer can be
   *  used from the activations buffer. This is no mandatory.
   */
  for (int idx=0; idx < AI_NETWORK_OUT_NUM; idx++) {
	data_outs[idx] = ai_output[idx].data;
  }
#else
  for (int idx=0; idx < AI_NETWORK_OUT_NUM; idx++) {
	ai_output[idx].data = data_outs[idx];
  }
#endif

  return 0;
}

static int ai_run(void)
{
  ai_i32 batch;

  batch = ai_network_run(network, ai_input, ai_output);
  if (batch != 1) {
    ai_log_err(ai_network_get_error(network),
        "ai_network_run");
    return -1;
  }

  return 0;
}

/* USER CODE BEGIN 2 */
static const float kXMin[AI_NETWORK_IN_1_SIZE] = {
  980.9f, -5.4f, 15.0f, -13.1f, -9.599976f,
  -22.2f, -37.0f, 980.9f, -5.4f, 15.0f
};

static const float kXMax[AI_NETWORK_IN_1_SIZE] = {
  1043.5f, 40.8f, 100.0f, 26.0f, 6.5999756f,
  18.7f, 65.0f, 1043.5f, 40.8f, 100.0f
};

static const char* kClassesFr[AI_NETWORK_OUT_1_SIZE] = {
  "Clair / ensoleille",
  "Peu nuageux",
  "Partiellement nuageux",
  "Nuageux / couvert",
  "Pluie",
  "Averses",
  "Neige",
  "Neige legere / averses de neige",
  "Pluie et neige melees",
  "Orage",
  "Brouillard / brume",
  "Vent fort",
  "Orage violent"
};

static uint8_t has_prev_sample = 0u;
static float prev_pressure = 1013.0f;
static float prev_temperature = 20.0f;
static float prev_humidity = 50.0f;
static uint32_t inference_count = 0u;
static int32_t g_last_predicted_class = -1;
static float g_last_predicted_score = 0.0f;

static void print_fixed_2(const char* label, float value, const char* unit)
{
  int32_t scaled = (int32_t)(value * 100.0f);
  int32_t sign = 1;
  int32_t whole;
  int32_t frac;

  if (scaled < 0) {
    sign = -1;
    scaled = -scaled;
  }

  whole = scaled / 100;
  frac = scaled % 100;

  if (sign < 0) {
    printf("%s-%ld.%02ld%s", label, (long)whole, (long)frac, unit);
  } else {
    printf("%s%ld.%02ld%s", label, (long)whole, (long)frac, unit);
  }
}

static void print_fixed_3(const char* label, float value, const char* unit)
{
  int32_t scaled = (int32_t)(value * 1000.0f);
  int32_t sign = 1;
  int32_t whole;
  int32_t frac;

  if (scaled < 0) {
    sign = -1;
    scaled = -scaled;
  }

  whole = scaled / 1000;
  frac = scaled % 1000;

  if (sign < 0) {
    printf("%s-%ld.%03ld%s", label, (long)whole, (long)frac, unit);
  } else {
    printf("%s%ld.%03ld%s", label, (long)whole, (long)frac, unit);
  }
}

static float clamp01(float value)
{
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

void hts221_read_data_polling(float_t *humidity_out, float_t *temperature_out);
void lps22hh_read_data_polling(float_t *pressure_out, float_t *temperature_out);

int acquire_and_process_data(ai_i8* data[])
{
  float features[AI_NETWORK_IN_1_SIZE];
  float pressure;
  float temperature;
  float humidity;
  float_t pressure_hpa = 0.0f;
  float_t pressure_temperature_deg_c = 0.0f;
  float_t humidity_perc = 0.0f;
  float_t humidity_temperature_deg_c = 0.0f;
  float dew_point;
  float pressure_delta;
  float temperature_delta;
  float humidity_delta;
  float* in_data;

  if (data == NULL || data[0] == NULL) {
    return -1;
  }

  lps22hh_read_data_polling(&pressure_hpa, &pressure_temperature_deg_c);
  hts221_read_data_polling(&humidity_perc, &humidity_temperature_deg_c);

  pressure = (float)pressure_hpa;
  temperature = (float)pressure_temperature_deg_c;
  if ((humidity_temperature_deg_c > -50.0f) && (humidity_temperature_deg_c < 100.0f)) {
    temperature = 0.5f * (temperature + (float)humidity_temperature_deg_c);
  }
  humidity = (float)humidity_perc;

  if (!has_prev_sample) {
    prev_pressure = pressure;
    prev_temperature = temperature;
    prev_humidity = humidity;
    has_prev_sample = 1u;
  }

  dew_point = temperature - ((100.0f - humidity) / 5.0f);
  pressure_delta = pressure - prev_pressure;
  temperature_delta = temperature - prev_temperature;
  humidity_delta = humidity - prev_humidity;

  /* Feature order matches build_sensor_features() from Model_final.ipynb */
  features[0] = pressure;
  features[1] = temperature;
  features[2] = humidity;
  features[3] = dew_point;
  features[4] = pressure_delta;
  features[5] = temperature_delta;
  features[6] = humidity_delta;
  features[7] = prev_pressure;
  features[8] = prev_temperature;
  features[9] = prev_humidity;

  in_data = (float*)data[0];
  for (int i = 0; i < AI_NETWORK_IN_1_SIZE; i++) {
    const float denom = kXMax[i] - kXMin[i];
    const float normalized = (denom == 0.0f) ? 0.0f : ((features[i] - kXMin[i]) / denom);
    in_data[i] = clamp01(normalized);
  }

  prev_pressure = pressure;
  prev_temperature = temperature;
  prev_humidity = humidity;

  printf("AI input #%lu\r\n", (unsigned long)(inference_count + 1u));
  print_fixed_2("  pression=", pressure, " hPa");
  printf(", ");
  print_fixed_2("temperature=", temperature, " C");
  printf(", ");
  print_fixed_2("humidite=", humidity, " % \r\n");

  return 0;
}

int post_process(ai_i8* data[])
{
  float* out_data;
  int top_idx = 0;
  float top_score;

  if (data == NULL || data[0] == NULL) {
    return -1;
  }

  out_data = (float*)data[0];
  top_score = out_data[0];

  for (int i = 1; i < AI_NETWORK_OUT_1_SIZE; i++) {
    if (out_data[i] > top_score) {
      top_score = out_data[i];
      top_idx = i;
    }
  }

  inference_count++;
  g_last_predicted_class = top_idx;
  g_last_predicted_score = top_score;
  printf("AI output #%lu\r\n", (unsigned long)inference_count);
  printf("  Classe predite : [%d] %s\r\n", top_idx, kClassesFr[top_idx]);
  print_fixed_2("  Confiance      : ", top_score * 100.0f, " %%\r\n");
  printf("  Probabilites   :\r\n");
  for (int i = 0; i < AI_NETWORK_OUT_1_SIZE; i++) {
    printf("    [%02d] %-35s : ", i, kClassesFr[i]);
    print_fixed_3("", out_data[i] * 100.0f, " %%\r\n");
  }
  printf("\r\n");

  return 0;
}

int32_t MX_X_CUBE_AI_GetLastPredictedClass(void)
{
  return g_last_predicted_class;
}

float MX_X_CUBE_AI_GetLastPredictedScore(void)
{
  return g_last_predicted_score;
}
/* USER CODE END 2 */

/* Entry points --------------------------------------------------------------*/

void MX_X_CUBE_AI_Init(void)
{
    /* USER CODE BEGIN 5 */
  printf("\r\nX-CUBE-AI initialization\r\n");

  ai_boostrap(data_activations0);
    /* USER CODE END 5 */
}

void MX_X_CUBE_AI_Process(void)
{
    /* USER CODE BEGIN 6 */
  static uint32_t last_inference_ms = 0u;
  int res = -1;
  uint32_t now = HAL_GetTick();

  if ((now - last_inference_ms) < 1000u) {
    return;
  }
  last_inference_ms = now;

  if (network) {
    /* 1 - acquire and pre-process input data */
    res = acquire_and_process_data(data_ins);
    /* 2 - process the data - call inference engine */
    if (res == 0)
      res = ai_run();
    /* 3- post-process the predictions */
    if (res == 0)
      res = post_process(data_outs);
  }

  if (res) {
    ai_error err = {AI_ERROR_INVALID_STATE, AI_ERROR_CODE_NETWORK};
    ai_log_err(err, "Process has FAILED");
  }
    /* USER CODE END 6 */
}
#ifdef __cplusplus
}
#endif
