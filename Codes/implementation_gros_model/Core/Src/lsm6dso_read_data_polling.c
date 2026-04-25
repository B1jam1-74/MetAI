/*
 ******************************************************************************
 * @file    read_data_polling.c
 * @author  Sensors Software Solution Team
 * @brief   This file show how to get data from sensor.
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
 * - STEVAL_MKI109V3 + STEVAL-MKI196V1
 * - NUCLEO_F401RE + X_NUCLEO_IKS01A3
 * - DISCOVERY_SPC584B + STEVAL-MKI196V1
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
#include "lsm6dso_reg.h"
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
#define    BOOT_TIME      10

/* Private variables ---------------------------------------------------------*/
static int16_t data_raw_acceleration[3];
static int16_t data_raw_angular_rate[3];
static int16_t data_raw_temperature;
static float_t acceleration_mg[3];
static float_t angular_rate_mdps[3];
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
void lsm6dso_read_data_polling(void)
{
  stmdev_ctx_t dev_ctx;
  dev_ctx.write_reg = platform_write;
  dev_ctx.read_reg = platform_read;
  dev_ctx.mdelay = platform_delay;
  dev_ctx.handle = &SENSOR_BUS;
  platform_init();
  platform_delay(BOOT_TIME);

  lsm6dso_device_id_get(&dev_ctx, &whoamI);
  //printf("LSM6DSO whoamI=0x%02X\r\n", whoamI);  // should print 0x6C

  if (whoamI != LSM6DSO_ID) {
    printf("LSM6DSO not found\r\n");
    return;
  }

  /* Restore default configuration */
  lsm6dso_reset_set(&dev_ctx, PROPERTY_ENABLE);
  do {
    lsm6dso_reset_get(&dev_ctx, &rst);
  } while (rst);

  lsm6dso_i3c_disable_set(&dev_ctx, LSM6DSO_I3C_DISABLE);
  lsm6dso_block_data_update_set(&dev_ctx, PROPERTY_ENABLE);
  lsm6dso_xl_data_rate_set(&dev_ctx, LSM6DSO_XL_ODR_12Hz5);
  lsm6dso_gy_data_rate_set(&dev_ctx, LSM6DSO_GY_ODR_12Hz5);
  lsm6dso_xl_full_scale_set(&dev_ctx, LSM6DSO_2g);
  lsm6dso_gy_full_scale_set(&dev_ctx, LSM6DSO_2000dps);
  lsm6dso_xl_hp_path_on_out_set(&dev_ctx, LSM6DSO_LP_ODR_DIV_100);
  lsm6dso_xl_filter_lp2_set(&dev_ctx, PROPERTY_ENABLE);

  /* Wait until accelerometer, gyroscope and temperature data are ready */
  uint8_t reg;
  do { lsm6dso_xl_flag_data_ready_get(&dev_ctx, &reg); } while (!reg);

  memset(data_raw_acceleration, 0x00, 3 * sizeof(int16_t));
  lsm6dso_acceleration_raw_get(&dev_ctx, data_raw_acceleration);
  acceleration_mg[0] = lsm6dso_from_fs2_to_mg(data_raw_acceleration[0]);
  acceleration_mg[1] = lsm6dso_from_fs2_to_mg(data_raw_acceleration[1]);
  acceleration_mg[2] = lsm6dso_from_fs2_to_mg(data_raw_acceleration[2]);
  printf("Acceleration [mg]:%4.2f\t%4.2f\t%4.2f\r\n",
         acceleration_mg[0], acceleration_mg[1], acceleration_mg[2]);

  do { lsm6dso_gy_flag_data_ready_get(&dev_ctx, &reg); } while (!reg);

  memset(data_raw_angular_rate, 0x00, 3 * sizeof(int16_t));
  lsm6dso_angular_rate_raw_get(&dev_ctx, data_raw_angular_rate);
  angular_rate_mdps[0] = lsm6dso_from_fs2000_to_mdps(data_raw_angular_rate[0]);
  angular_rate_mdps[1] = lsm6dso_from_fs2000_to_mdps(data_raw_angular_rate[1]);
  angular_rate_mdps[2] = lsm6dso_from_fs2000_to_mdps(data_raw_angular_rate[2]);
  printf("Angular rate [mdps]:%4.2f\t%4.2f\t%4.2f\r\n",
         angular_rate_mdps[0], angular_rate_mdps[1], angular_rate_mdps[2]);

  do { lsm6dso_temp_flag_data_ready_get(&dev_ctx, &reg); } while (!reg);

  memset(&data_raw_temperature, 0x00, sizeof(int16_t));
  lsm6dso_temperature_raw_get(&dev_ctx, &data_raw_temperature);
  temperature_degC = lsm6dso_from_lsb_to_celsius(data_raw_temperature);
  printf("Temperature [degC]:%6.2f\r\n", temperature_degC);
}
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp,
                              uint16_t len)
{
    HAL_I2C_Mem_Write(handle, LSM6DSO_I2C_ADD_H, reg,  // 0x6B
                      I2C_MEMADD_SIZE_8BIT, (uint8_t *)bufp, len, 1000);
    return 0;
}

static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len)
{
    HAL_I2C_Mem_Read(handle, LSM6DSO_I2C_ADD_H, reg,   // 0x6B
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
#if defined(STEVAL_MKI109V3)
  TIM3->CCR1 = PWM_3V3;
  TIM3->CCR2 = PWM_3V3;
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_Delay(1000);
#endif
}
