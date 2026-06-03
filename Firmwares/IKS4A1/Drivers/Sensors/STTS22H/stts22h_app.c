/*
 ******************************************************************************
 * @file    stts22h_app.c
 * @brief   STTS22H application helpers
 ******************************************************************************
 */

#include "stts22h_app.h"

static I2C_HandleTypeDef *stts22h_hi2c = NULL;

static int32_t STTS22H_I2C_Init(void)
{
  return STTS22H_OK;
}

static int32_t STTS22H_I2C_DeInit(void)
{
  return STTS22H_OK;
}

static int32_t STTS22H_I2C_ReadReg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  return (HAL_I2C_Mem_Read(stts22h_hi2c, DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, HAL_MAX_DELAY) == HAL_OK)
           ? STTS22H_OK
           : STTS22H_ERROR;
}

static int32_t STTS22H_I2C_WriteReg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  return (HAL_I2C_Mem_Write(stts22h_hi2c, DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, HAL_MAX_DELAY) == HAL_OK)
           ? STTS22H_OK
           : STTS22H_ERROR;
}

static int32_t STTS22H_I2C_GetTick(void)
{
  return (int32_t)HAL_GetTick();
}

static void STTS22H_I2C_Delay(uint32_t Delay)
{
  HAL_Delay(Delay);
}

int32_t STTS22H_App_Init(STTS22H_Object_t *pObj, I2C_HandleTypeDef *hi2c)
{
  STTS22H_IO_t stts22h_io = {0};

  if ((pObj == NULL) || (hi2c == NULL))
  {
    return STTS22H_ERROR;
  }

  stts22h_hi2c = hi2c;

  stts22h_io.Init = STTS22H_I2C_Init;
  stts22h_io.DeInit = STTS22H_I2C_DeInit;
  stts22h_io.BusType = STTS22H_I2C_BUS;
  stts22h_io.Address = STTS22H_I2C_ADD_H & 0xFEU;
  stts22h_io.WriteReg = STTS22H_I2C_WriteReg;
  stts22h_io.ReadReg = STTS22H_I2C_ReadReg;
  stts22h_io.GetTick = STTS22H_I2C_GetTick;
  stts22h_io.Delay = STTS22H_I2C_Delay;

  if ((STTS22H_RegisterBusIO(pObj, &stts22h_io) != STTS22H_OK) ||
      (STTS22H_Init(pObj) != STTS22H_OK) ||
      (STTS22H_TEMP_Enable(pObj) != STTS22H_OK) ||
      (STTS22H_TEMP_SetOutputDataRate(pObj, 25.0f) != STTS22H_OK))
  {
    return STTS22H_ERROR;
  }

  HAL_Delay(100);

  return STTS22H_OK;
}

int32_t STTS22H_App_ReadTemperature(STTS22H_Object_t *pObj, float *temperature)
{
  if ((pObj == NULL) || (temperature == NULL))
  {
    return STTS22H_ERROR;
  }

  return STTS22H_TEMP_GetTemperature(pObj, temperature);
}


void Temperature_Init(STTS22H_Object_t* stts22h_obj) {

    if (STTS22H_App_Init(stts22h_obj, &hi2c1) != STTS22H_OK)
    {
        printf("Failed to initialize STTS22H sensor\r\n");
    }
    else
    {
        printf("STTS22H sensor initialized successfully\r\n");
    }
}


void Temperature_Read(STTS22H_Object_t* stts22h_obj, float* temperature) {
    if (STTS22H_App_ReadTemperature(stts22h_obj, temperature) == STTS22H_OK)
    {
        //printf("Temperature: %.2f °C\r\n", *temperature);
    }
    else
    {
        printf("Failed to read temperature from STTS22H sensor\r\n");
    }
}