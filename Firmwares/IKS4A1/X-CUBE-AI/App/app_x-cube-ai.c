
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

#include "app_x-cube-ai.h"

/* Entry points --------------------------------------------------------------*/

#include "network.h"
#include "network_data.h"

static ai_handle network = AI_HANDLE_NULL;
static ai_u8 activations[AI_NETWORK_DATA_ACTIVATION_1_SIZE];
static ai_buffer ai_input[AI_NETWORK_IN_NUM];
static ai_buffer ai_output[AI_NETWORK_OUT_NUM];

void MX_X_CUBE_AI_Init(void)
{
    ai_error err;

    /* Create and initialize the network */
    const ai_handle acts[] = { activations };
    err = ai_network_create_and_init(&network, acts, NULL);
    if (err.type != AI_ERROR_NONE) {
        printf("AI network initialization error\r\n");
        return;
    }

    ai_input[0] = ai_network_inputs_get(network, NULL)[0];
    ai_output[0] = ai_network_outputs_get(network, NULL)[0];
}

const char* CLASSES_FR[] = {
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
    "Orage violent",
};

static float prev_pressure = 0.0f;
static float prev_temperature = 0.0f;
static float prev_humidity = 0.0f;
static int is_first_reading = 1;

void MX_X_CUBE_AI_Process(float pressure, float temperature, float humidity,
                          uint8_t *out_class_idx, uint8_t *out_confidence_pct)
{
    if (is_first_reading) {
        prev_pressure = pressure;
        prev_temperature = temperature;
        prev_humidity = humidity;
        is_first_reading = 0;
    }

    float dew_point = temperature - (100.0f - humidity) / 5.0f;

    float x_in[10];
    x_in[0] = pressure;
    x_in[1] = temperature;
    x_in[2] = humidity;
    x_in[3] = dew_point;
    x_in[4] = pressure - prev_pressure;
    x_in[5] = temperature - prev_temperature;
    x_in[6] = humidity - prev_humidity;
    x_in[7] = prev_pressure;
    x_in[8] = prev_temperature;
    x_in[9] = prev_humidity;

    const float X_min[10] = {980.4f, -5.5f, 19.0f, -11.299999f, -9.799988f, -5.000001f, -27.0f, 980.4f, -5.5f, 19.0f};
    const float X_max[10] = {1043.1f, 34.8f, 100.0f, 21.800001f, 5.5f, 7.6000004f, 30.0f, 1043.1f, 34.8f, 100.0f};

    for (int i=0; i<10; i++) {
        float denom = X_max[i] - X_min[i];
        if (denom == 0.0f) denom = 1.0f;
        x_in[i] = (x_in[i] - X_min[i]) / denom;
    }

    float out_data[13] = {0};
    
    ai_input[0].data = AI_HANDLE_PTR(x_in);
    ai_output[0].data = AI_HANDLE_PTR(out_data);

    ai_i32 n_batch = ai_network_run(network, &ai_input[0], &ai_output[0]);
    if (n_batch != 1) {
        printf("Error running network\r\n");
        if (out_class_idx)     *out_class_idx     = 0xFF;
        if (out_confidence_pct) *out_confidence_pct = 0;
        return;
    }

    int class_idx = 0;
    float max_prob = out_data[0];
    for (int i=1; i<13; i++) {
        if (out_data[i] > max_prob) {
            max_prob = out_data[i];
            class_idx = i;
        }
    }

    printf("Prevision meteo: %s (Confiance: %.1f %%)\r\n", CLASSES_FR[class_idx], max_prob * 100.0f);

    if (out_class_idx)      *out_class_idx      = (uint8_t)class_idx;
    if (out_confidence_pct) *out_confidence_pct = (uint8_t)(max_prob * 100.0f);
    
    // Update history
    prev_pressure = pressure;
    prev_temperature = temperature;
    prev_humidity = humidity;
}
#ifdef __cplusplus
}
#endif
