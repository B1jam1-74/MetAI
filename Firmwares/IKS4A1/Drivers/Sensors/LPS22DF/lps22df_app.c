/*
 ******************************************************************************
 * @file    lps22df_app.c
 * @brief   LPS22DF application helpers
 ******************************************************************************
 */

#include "lps22df_app.h"

static I2C_HandleTypeDef *lps22df_hi2c = NULL;
static stmdev_ctx_t lps22df_ctx = {0};

static int32_t LPS22DF_I2C_WriteReg(void *handle, uint8_t reg, const uint8_t *pData,
                                    uint16_t length)
{
  return (HAL_I2C_Mem_Write((I2C_HandleTypeDef *)handle, LPS22DF_I2C_ADD_H,
                            reg, I2C_MEMADD_SIZE_8BIT, (uint8_t *)pData,
                            length, HAL_MAX_DELAY) == HAL_OK) ? 0 : -1;
}

static int32_t LPS22DF_I2C_ReadReg(void *handle, uint8_t reg, uint8_t *pData,
                                   uint16_t length)
{
  return (HAL_I2C_Mem_Read((I2C_HandleTypeDef *)handle, LPS22DF_I2C_ADD_H,
                           reg, I2C_MEMADD_SIZE_8BIT, pData,
                           length, HAL_MAX_DELAY) == HAL_OK) ? 0 : -1;
}

static void LPS22DF_I2C_Delay(uint32_t Delay)
{
  HAL_Delay(Delay);
}

int32_t LPS22DF_App_Init(I2C_HandleTypeDef *hi2c)
{
  lps22df_id_t device_id = {0};
  lps22df_md_t mode = {0};

  if (hi2c == NULL)
  {
    return -1;
  }

  lps22df_hi2c = hi2c;
  lps22df_ctx.handle = lps22df_hi2c;
  lps22df_ctx.read_reg = LPS22DF_I2C_ReadReg;
  lps22df_ctx.write_reg = LPS22DF_I2C_WriteReg;
  lps22df_ctx.mdelay = LPS22DF_I2C_Delay;
  lps22df_ctx.priv_data = NULL;

  if (lps22df_id_get(&lps22df_ctx, &device_id) != 0)
  {
    return -1;
  }

  if (device_id.whoami != LPS22DF_ID)
  {
    return -1;
  }

  if (lps22df_init_set(&lps22df_ctx, LPS22DF_DRV_RDY) != 0)
  {
    return -1;
  }

  mode.odr = LPS22DF_25Hz;
  mode.avg = LPS22DF_4_AVG;
  mode.lpf = LPS22DF_LPF_DISABLE;

  if (lps22df_mode_set(&lps22df_ctx, &mode) != 0)
  {
    return -1;
  }

  return 0;
}

int32_t LPS22DF_App_ReadData(lps22df_data_t *data)
{
  if ((data == NULL) || (lps22df_hi2c == NULL))
  {
    return -1;
  }

  return lps22df_data_get(&lps22df_ctx, data);
}

int32_t LPS22DF_App_ReadPressure(float *pressure_hpa)
{
  lps22df_data_t data = {0};

  if (pressure_hpa == NULL)
  {
    return -1;
  }

  if (LPS22DF_App_ReadData(&data) != 0)
  {
    return -1;
  }

  *pressure_hpa = data.pressure.hpa;

  return 0;
}

int32_t LPS22DF_App_ReadTemperature(float *temperature_c)
{
  lps22df_data_t data = {0};

  if (temperature_c == NULL)
  {
    return -1;
  }

  if (LPS22DF_App_ReadData(&data) != 0)
  {
    return -1;
  }

  *temperature_c = data.heat.deg_c;

  return 0;
}

void Pressure_Init(void)
{
  if (LPS22DF_App_Init(&hi2c1) != 0)
  {
    printf("Failed to initialize LPS22DF sensor\r\n");
  }
  else
  {
    printf("LPS22DF sensor initialized successfully\r\n");
  }
}

void Pressure_Read(lps22df_data_t lps22df_data, float *pressure, float *temperature_c)
{
  if (LPS22DF_App_ReadData(&lps22df_data) == 0)
    {
      *pressure = lps22df_data.pressure.hpa;
      *temperature_c = lps22df_data.heat.deg_c;
      //printf("Pressure: %.2f hPa, Temperature: %.2f C\r\n",*pressure, *temperature_c);
    }
    else
    {
      printf("Failed to read data from LPS22DF sensor\r\n");
    }
}