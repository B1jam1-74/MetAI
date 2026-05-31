
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __APP_AI_H
#define __APP_AI_H
#ifdef __cplusplus
extern "C" {
#endif
/**
  ******************************************************************************
  * @file    app_x-cube-ai.h
  * @author  X-CUBE-AI C code generator
  * @brief   AI entry function definitions
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

/* IO buffers ----------------------------------------------------------------*/



void MX_X_CUBE_AI_Init(void);
void MX_X_CUBE_AI_Process(float pressure, float temperature, float humidity,
                          uint8_t *out_class_idx, uint8_t *out_confidence_pct);
#ifdef __cplusplus
}
#endif
#endif /*__STMicroelectronics_X-CUBE-AI_10_2_0_H */
