
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
#include "network.h"
#include "network_data.h"

/* Network runtime objects --------------------------------------------------*/
ai_handle network = AI_HANDLE_NULL;
AI_ALIGNED(4) static ai_u8 data_activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];
ai_handle data_activations0[] = { data_activations };
AI_ALIGNED(4) static ai_i8 in_data[AI_NETWORK_IN_1_SIZE_BYTES];
AI_ALIGNED(4) static ai_i8 out_data[AI_NETWORK_OUT_1_SIZE_BYTES];

ai_i8* data_ins[AI_NETWORK_IN_NUM] = { in_data };
ai_i8* data_outs[AI_NETWORK_OUT_NUM] = { out_data };

static ai_buffer* ai_input = NULL;
static ai_buffer* ai_output = NULL;
static uint8_t ai_is_initialized = 0;

static void ai_log_error(void)
{
  ai_error err = ai_network_get_error(network);
  printf("AI error type=%d code=%d\r\n", err.type, err.code);
}

/* Entry points --------------------------------------------------------------*/

void MX_X_CUBE_AI_Init(void)
{
  ai_error err;

  err = ai_network_create_and_init(&network, data_activations0, NULL);
  if (err.type != AI_ERROR_NONE)
  {
    printf("AI init failed\r\n");
    ai_log_error();
    return;
  }

  ai_input = ai_network_inputs_get(network, NULL);
  ai_output = ai_network_outputs_get(network, NULL);
  if ((ai_input == NULL) || (ai_output == NULL))
  {
    printf("AI IO buffer fetch failed\r\n");
    return;
  }

  ai_input[0].data = AI_HANDLE_PTR(data_ins[0]);
  ai_output[0].data = AI_HANDLE_PTR(data_outs[0]);
  ai_is_initialized = 1;
}

void MX_X_CUBE_AI_Process(void)
{
    (void)MX_X_CUBE_AI_Run();
}

int MX_X_CUBE_AI_Run(void)
{
  ai_i32 n_batch;

  if (!ai_is_initialized)
  {
    return 0;
  }

  n_batch = ai_network_run(network, ai_input, ai_output);
  if (n_batch != 1)
  {
    ai_log_error();
    return 0;
  }

  return 1;
}
#ifdef __cplusplus
}
#endif
