/*
 ******************************************************************************
 * @file    read_data_simple.c
 * @author  Sensors Software Solution Team
 * @brief   This file show the simplest way to get data from sensor.
 *
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/*
 * This example was developed using the following STMicroelectronics
 * evaluation boards:
 *
 * - STEVAL_MKI109V3 + STEVAL-MKI181V1
 * - NUCLEO_F401RE + X_NUCLEO_IKS01A3
 * - DISCOVERY_SPC584B + STEVAL-MKI181V1
 *
 * and STM32CubeMX tool with STM32CubeF4 MCU Package
 *
 * Used interfaces:
 *
 * STEVAL_MKI109V3    - Host side:   USB (Virtual COM)
 *                    - Sensor side: SPI(Default) / I2C(supported)
 *
 * NUCLEO_STM32F401RE - Host side: UART(COM) to USB bridge
 *                    - I2C(Default) / SPI(supported)
 *
 * DISCOVERY_SPC584B  - Host side: UART(COM) to USB bridge
 *                    - Sensor side: I2C(Default) / SPI(supported)
 *
 * If you need to run this example on a different hardware platform a
 * modification of the functions: `platform_write`, `platform_read`,
 * `tx_com` and 'platform_init' is required.
 *
 */

/* STMicroelectronics evaluation boards definition
 *
 * Please uncomment ONLY the evaluation boards in use.
 * If a different hardware is used please comment all
 * following target board and redefine yours.
 */

//#define STEVAL_MKI109V3  /* little endian */
//#define NUCLEO_F401RE    /* little endian */
//#define SPC584B_DIS      /* big endian */

/* ATTENTION: By default the driver is little endian. If you need switch
 *            to big endian please see "Endianness definitions" in the
 *            header file of the driver (_reg.h).
 */

#if defined(STEVAL_MKI109V3)
/* MKI109V3: Define communication interface */
#define SENSOR_BUS hspi2
/* MKI109V3: Vdd and Vddio power supply values */
#define PWM_3V3 915

#elif defined(NUCLEO_F401RE)
/* NUCLEO_F401RE: Define communication interface */
#define SENSOR_BUS hi2c1

#elif defined(SPC584B_DIS)
/* DISCOVERY_SPC584B: Define communication interface */
#define SENSOR_BUS I2CD1

#endif

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "lis2mdl_reg.h"
#include "stm32n6xx_hal.h"

extern I2C_HandleTypeDef hi2c1;
#define SENSOR_BUS hi2c1

#if defined(NUCLEO_F401RE)
#include "stm32f4xx_hal.h"
#include "usart.h"
#include "gpio.h"
#include "i2c.h"

#elif defined(STEVAL_MKI109V3)
#include "stm32f4xx_hal.h"
#include "usbd_cdc_if.h"
#include "gpio.h"
#include "spi.h"
#include "tim.h"

#elif defined(SPC584B_DIS)
#include "components.h"
#endif

/* Private macro -------------------------------------------------------------*/
#define    BOOT_TIME        20 //ms

/* Private variables ---------------------------------------------------------*/
static int16_t data_raw_magnetic[3];
static int16_t data_raw_temperature;
static float_t magnetic_mG[3];
static float_t temperature_degC;
static uint8_t whoamI, rst;

/* Extern variables ----------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
/*
 *   WARNING:
 *   Functions declare in this section are defined at the end of this file
 *   and are strictly related to the hardware platform used.
 *
 */
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp,
                              uint16_t len);
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len);

static void platform_delay(uint32_t ms);
static void platform_init(void);

/* Main Example --------------------------------------------------------------*/
void lis2mdl_read_data_simple(void)
{
  /* Initialize mems driver interface */
  stmdev_ctx_t dev_ctx;
  dev_ctx.write_reg = platform_write;
  dev_ctx.read_reg = platform_read;
  dev_ctx.mdelay = platform_delay;
  dev_ctx.handle = &SENSOR_BUS;
  /* Initialize platform specific hardware */
  platform_init();
  /* Wait sensor boot time */
  platform_delay(BOOT_TIME);
#if defined(STEVAL_MKI109V3)
  /* Default SPI mode is 3 wire, so enable 4 wire mode */
  lis2mdl_spi_mode_set(&dev_ctx, LIS2MDL_SPI_4_WIRE);
#endif
  /* Check device ID */
  lis2mdl_device_id_get(&dev_ctx, &whoamI);

  if (whoamI != LIS2MDL_ID)
    while (1) {
      /* manage here device not found */
    	printf("test");
    }

  /* Restore default configuration */
  lis2mdl_reset_set(&dev_ctx, PROPERTY_ENABLE);

  do {
    lis2mdl_reset_get(&dev_ctx, &rst);
  } while (rst);

  /* Enable Block Data Update */
  lis2mdl_block_data_update_set(&dev_ctx, PROPERTY_ENABLE);
  /* Set Output Data Rate */
  lis2mdl_data_rate_set(&dev_ctx, LIS2MDL_ODR_10Hz);
  /* Set / Reset sensor mode */
  lis2mdl_set_rst_mode_set(&dev_ctx, LIS2MDL_SENS_OFF_CANC_EVERY_ODR);
  /* Enable temperature compensation */
  lis2mdl_offset_temp_comp_set(&dev_ctx, PROPERTY_ENABLE);
  /* Set device in continuous mode */
  lis2mdl_operating_mode_set(&dev_ctx, LIS2MDL_CONTINUOUS_MODE);

  /* Wait until data is ready */
    uint8_t reg;
    do {
      lis2mdl_mag_data_ready_get(&dev_ctx, &reg);
    } while (!reg);

    /* Read magnetic field data */
    memset(data_raw_magnetic, 0x00, 3 * sizeof(int16_t));
    lis2mdl_magnetic_raw_get(&dev_ctx, data_raw_magnetic);
    magnetic_mG[0] = lis2mdl_from_lsb_to_mgauss(data_raw_magnetic[0]);
    magnetic_mG[1] = lis2mdl_from_lsb_to_mgauss(data_raw_magnetic[1]);
    magnetic_mG[2] = lis2mdl_from_lsb_to_mgauss(data_raw_magnetic[2]);
    printf("Magnetic field [mG]:%4.2f\t%4.2f\t%4.2f\r\n",
           magnetic_mG[0], magnetic_mG[1], magnetic_mG[2]);

    /* Read temperature data */
    memset(&data_raw_temperature, 0x00, sizeof(int16_t));
    lis2mdl_temperature_raw_get(&dev_ctx, &data_raw_temperature);
    temperature_degC = lis2mdl_from_lsb_to_celsius(data_raw_temperature);
    printf("Temperature [degC]:%6.2f\r\n", temperature_degC);

}

static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp,
                              uint16_t len)
{
    HAL_I2C_Mem_Write(handle, LIS2MDL_I2C_ADD, reg,  // was LIS2DW12_I2C_ADD_H
                      I2C_MEMADD_SIZE_8BIT, (uint8_t *)bufp, len, 1000);
    return 0;
}

static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len)
{
    HAL_I2C_Mem_Read(handle, LIS2MDL_I2C_ADD, reg,   // was LIS2DW12_I2C_ADD_H
                     I2C_MEMADD_SIZE_8BIT, bufp, len, 1000);
    return 0;
}

static void platform_delay(uint32_t ms)
{
    HAL_Delay(ms);
}

/*
 * @brief  platform specific initialization (platform dependent)
 */
static void platform_init(void)
{
#ifdef STEVAL_MKI109V3
  TIM3->CCR1 = PWM_3V3;
  TIM3->CCR2 = PWM_3V3;
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_Delay(1000);
#endif
}
