/*
 ******************************************************************************
 * @file    lps22df_app.h
 * @brief   LPS22DF application helpers
 ******************************************************************************
 */

#ifndef LPS22DF_APP_H
#define LPS22DF_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32u5xx_hal.h"
#include "lps22df_reg.h"

#include "stdio.h"

extern I2C_HandleTypeDef hi2c1;

int32_t LPS22DF_App_Init(I2C_HandleTypeDef *hi2c);
int32_t LPS22DF_App_ReadData(lps22df_data_t *data);
int32_t LPS22DF_App_ReadPressure(float *pressure_hpa);
int32_t LPS22DF_App_ReadTemperature(float *temperature_c);
void Pressure_Init(void);
void Pressure_Read(lps22df_data_t lps22df_data, float *pressure, float *temperature_c);

#ifdef __cplusplus
}
#endif

#endif /* LPS22DF_APP_H */