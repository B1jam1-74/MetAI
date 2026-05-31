/*
 ******************************************************************************
 * @file    stts22h_app.h
 * @brief   STTS22H application helpers
 ******************************************************************************
 */

#ifndef STTS22H_APP_H
#define STTS22H_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32u5xx_hal.h"
#include "stts22h.h"

#include "stdio.h"

extern I2C_HandleTypeDef hi2c1;

int32_t STTS22H_App_Init(STTS22H_Object_t *pObj, I2C_HandleTypeDef *hi2c);
int32_t STTS22H_App_ReadTemperature(STTS22H_Object_t *pObj, float *temperature);
void Temperature_Init(STTS22H_Object_t* stts22h_obj);
void Temperature_Read(STTS22H_Object_t* stts22h_obj, float* temperature);

#ifdef __cplusplus
}
#endif

#endif /* STTS22H_APP_H */
