/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_x-cube-ai.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <math.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define WHO_AM_I_REG              0x0FU
#define I2C_TIMEOUT_MS            100U
#define I2C_MAX_ADDRESS_7BIT      0x77U
#define LORA_SLEEP_CMD            "AT+LOWPOWER\r\n"
#define LORA_WAKE_CMD             "AT\r\n"

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;
__IO uint32_t BspButtonState = BUTTON_RELEASED;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef hlpuart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void SystemPower_Config(void);
static void MX_GPIO_Init(void);
static void MX_ICACHE_Init(void);
static void MX_I2C1_Init(void);
static void MX_LPUART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static void SleepForMs(uint32_t delay_ms);
static void LoRa_SendATAndPrintResponse(const char *label, const uint8_t *cmd, uint16_t cmd_len,
                                        uint32_t response_window_ms, uint32_t post_cmd_delay_ms);
static void I2C_PrintDetectedDevices(I2C_HandleTypeDef *hi2c);
static void I2C_ReadWhoAmI(I2C_HandleTypeDef *hi2c, uint16_t address_7bit, const char *label);
static void I2C_CheckSensorsWhoAmI(I2C_HandleTypeDef *hi2c);
void hts221_read_data_polling(float_t *humidity_out, float_t *temperature_out);
void lps22hh_read_data_polling(float_t *pressure_out, float_t *temperature_out);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the System Power */
  SystemPower_Config();

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ICACHE_Init();
  MX_I2C1_Init();
  MX_LPUART1_UART_Init();
  MX_X_CUBE_AI_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Initialize led */
  BSP_LED_Init(LED_GREEN);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN BSP */

  /* -- Sample board code to send message over COM1 port ---- */
  printf("Welcome to STM32 world !\n\r");

  // Affiche les capteurs et leurs adresses via I2C
  I2C_CheckSensorsWhoAmI(&hi2c1);

  // Eteint la led verte de la carte
  BSP_LED_Off(LED_GREEN);

  /* USER CODE END BSP */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  // Connexion à la gateway Lora
  LoRa_SendATAndPrintResponse("AT", (const uint8_t *)"AT\r\n", sizeof("AT\r\n") - 1U, 500U, 1000U);
  LoRa_SendATAndPrintResponse("AT+MODE=LWOTAA", (const uint8_t *)"AT+MODE=LWOTAA\r\n",
                              sizeof("AT+MODE=LWOTAA\r\n") - 1U, 700U, 1000U);
  LoRa_SendATAndPrintResponse("AT+JOIN", (const uint8_t *)"AT+JOIN\r\n",
                              sizeof("AT+JOIN\r\n") - 1U, 2000U, 5000U);

  while (1)
  {

    /* USER CODE END WHILE */

  MX_X_CUBE_AI_Process();
    /* USER CODE BEGIN 3 */

    // Variables pour stocker les données des capteurs + la classe prédite par le modèle AI
    float_t pressure_hpa = 0.0f;
    float_t pressure_temperature_deg_c = 0.0f;
    float_t humidity_perc = 0.0f;
    float_t humidity_temperature_deg_c = 0.0f;
    char lora_cmd[96];
    int lora_cmd_len = 0;
    int32_t predicted_class = -1;

    // Reveil du module Lora pour l'envoi des données
    LoRa_SendATAndPrintResponse("AT(wake)", (const uint8_t *)LORA_WAKE_CMD,
                  sizeof(LORA_WAKE_CMD) - 1U, 500U, 300U);

    // Lecture des données des capteurs 
    lps22hh_read_data_polling(&pressure_hpa, &pressure_temperature_deg_c);
    hts221_read_data_polling(&humidity_perc, &humidity_temperature_deg_c);

    // Récupération de la classe prédite par le modèle AI
    predicted_class = MX_X_CUBE_AI_GetLastPredictedClass();

    // Formatage de la payload à envoyer (ex: AT+MSG="P=1013.25,H=45.00,T=22.50,C=0")
    lora_cmd_len = snprintf(lora_cmd, sizeof(lora_cmd),
                "AT+MSG=\"P=%.2f,H=%.2f,T=%.2f,C=%ld\"\r\n",
                            (double)pressure_hpa,
                            (double)humidity_perc,
                (double)pressure_temperature_deg_c,
                (long)predicted_class);
    if ((lora_cmd_len > 0) && ((size_t)lora_cmd_len < sizeof(lora_cmd)))
    {
      LoRa_SendATAndPrintResponse("AT+MSG(sensor)", (const uint8_t *)lora_cmd,
                                  (uint16_t)lora_cmd_len, 3000U, 1000U);
    }
    else
    { 
      printf("LoRa payload format error\n\r");
    }

    // Mise en sleep du lora
    LoRa_SendATAndPrintResponse("AT+LOWPOWER", (const uint8_t *)LORA_SLEEP_CMD,
                                sizeof(LORA_SLEEP_CMD) - 1U, 500U, 100U);

    // Veille pendant 30 secondes 
    SleepForMs(30000);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_4;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Power Configuration
  * @retval None
  */
static void SystemPower_Config(void)
{

  /*
   * Switch to SMPS regulator instead of LDO
   */
  if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK)
  {
    Error_Handler();
  }
/* USER CODE BEGIN PWR */
/* USER CODE END PWR */
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00000E14;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
static void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */

  /** Enable instruction cache in 1-way (direct mapped cache)
  */
  if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/**
  * @brief LPUART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 9600;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  hlpuart1.FifoMode = UART_FIFOMODE_DISABLE;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

static void SleepForMs(uint32_t delay_ms)
{
  const uint32_t start = HAL_GetTick();

  while ((HAL_GetTick() - start) < delay_ms)
  {
    /* Enter Sleep mode until next interrupt (SysTick or external IRQ). */
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
  }
}

static void LoRa_SendATAndPrintResponse(const char *label, const uint8_t *cmd, uint16_t cmd_len,
                                        uint32_t response_window_ms, uint32_t post_cmd_delay_ms)
{
  uint8_t rx_byte = 0U;
  char response[128];
  uint32_t response_len = 0U;
  uint32_t start = 0U;
  uint32_t last_rx_tick = 0U;

  (void)HAL_UART_Transmit(&hlpuart1, (uint8_t *)cmd, cmd_len, HAL_MAX_DELAY);

  start = HAL_GetTick();
  last_rx_tick = start;

  while ((HAL_GetTick() - start) < response_window_ms)
  {
    if (HAL_UART_Receive(&hlpuart1, &rx_byte, 1U, 20U) == HAL_OK)
    {
      if (response_len < (sizeof(response) - 1U))
      {
        response[response_len++] = (char)rx_byte;
      }
      last_rx_tick = HAL_GetTick();
    }
    else
    {
      /* Stop quickly after end of frame once data has started to arrive. */
      if ((response_len > 0U) && ((HAL_GetTick() - last_rx_tick) > 120U))
      {
        break;
      }
    }
  }

  response[response_len] = '\0';

  if (response_len > 0U)
  {
    printf("LoRa response to %s: %s", label, response);
    if (response[response_len - 1U] != '\n')
    {
      printf("\n\r");
    }
  }
  else
  {
    printf("LoRa response to %s: (no response)\n\r", label);
  }

  HAL_Delay(post_cmd_delay_ms);
}

static void I2C_PrintDetectedDevices(I2C_HandleTypeDef *hi2c)
{
  uint8_t found = 0U;

  printf("I2C scan start...\n\r");
  for (uint16_t addr = 1U; addr <= I2C_MAX_ADDRESS_7BIT; addr++)
  {
    const uint16_t dev_addr = (uint16_t)(addr << 1U);
    if (HAL_I2C_IsDeviceReady(hi2c, dev_addr, 2U, I2C_TIMEOUT_MS) == HAL_OK)
    {
      found = 1U;
      printf("  - device at 7-bit address 0x%02X\n\r", (unsigned int)addr);
    }
  }

  if (found == 0U)
  {
    printf("  - no I2C device detected\n\r");
  }
}

static void I2C_ReadWhoAmI(I2C_HandleTypeDef *hi2c, uint16_t address_7bit, const char *label)
{
  uint8_t who_am_i = 0U;
  const uint16_t dev_addr = (uint16_t)(address_7bit << 1U);

  if (HAL_I2C_IsDeviceReady(hi2c, dev_addr, 2U, I2C_TIMEOUT_MS) != HAL_OK)
  {
    printf("%s @0x%02X: not responding\n\r", label, (unsigned int)address_7bit);
    return;
  }

  if (HAL_I2C_Mem_Read(hi2c, dev_addr, WHO_AM_I_REG, I2C_MEMADD_SIZE_8BIT,
                       &who_am_i, 1U, I2C_TIMEOUT_MS) == HAL_OK)
  {
    printf("%s @0x%02X: WHO_AM_I = 0x%02X\n\r", label,
           (unsigned int)address_7bit, (unsigned int)who_am_i);
  }
  else
  {
    printf("%s @0x%02X: WHO_AM_I read failed\n\r", label, (unsigned int)address_7bit);
  }
}

static void I2C_CheckSensorsWhoAmI(I2C_HandleTypeDef *hi2c)
{
  printf("\n\r=== I2C sensor check ===\n\r");
  I2C_PrintDetectedDevices(hi2c);

  /* Common ST MEMS addresses (WHO_AM_I register is usually 0x0F). */
  I2C_ReadWhoAmI(hi2c, 0x1EU, "LIS2MDL/LIS3MDL");
  I2C_ReadWhoAmI(hi2c, 0x18U, "LIS2DW12/IIS2DLPC");
  I2C_ReadWhoAmI(hi2c, 0x19U, "LIS2DW12/IIS2DLPC");
  I2C_ReadWhoAmI(hi2c, 0x5CU, "LPS22xx");
  I2C_ReadWhoAmI(hi2c, 0x5DU, "LPS22xx");
  I2C_ReadWhoAmI(hi2c, 0x5FU, "HTS221");
  I2C_ReadWhoAmI(hi2c, 0x6AU, "LSM6DSx");
  I2C_ReadWhoAmI(hi2c, 0x6BU, "LSM6DSx");
  printf("=== end of I2C sensor check ===\n\r\n\r");
}

/* USER CODE END 4 */

/**
  * @brief BSP Push Button callback
  * @param Button Specifies the pressed button
  * @retval None
  */
void BSP_PB_Callback(Button_TypeDef Button)
{
  if (Button == BUTTON_USER)
  {
    BspButtonState = BUTTON_PRESSED;
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
