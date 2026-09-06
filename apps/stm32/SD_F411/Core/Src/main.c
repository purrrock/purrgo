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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

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

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  /* UART test */
  char msg[64];

  snprintf(msg, sizeof(msg), "UART OK\r\n");

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)msg,
                    strlen(msg),
                    HAL_MAX_DELAY);

  /* SD card SPI initialization */

  uint8_t dummy = 0xFF;


  /* CS = HIGH */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);


  /* 80 clock cycles with CS = HIGH */
  for (int i = 0; i < 10; i++)
  {
      HAL_SPI_Transmit(&hspi1,
                       &dummy,
                       1,
                       HAL_MAX_DELAY);
  }


  /* CMD0 */

  uint8_t cmd0[] = {
      0x40, 0x00, 0x00, 0x00, 0x00, 0x95
  };

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

  HAL_SPI_Transmit(&hspi1,
                   cmd0,
                   sizeof(cmd0),
                   HAL_MAX_DELAY);


  /* Read CMD0 response */

  uint8_t r1_cmd0 = 0xFF;

  for (int i = 0; i < 10; i++)
  {
      HAL_SPI_TransmitReceive(&hspi1,
                              &dummy,
                              &r1_cmd0,
                              1,
                              HAL_MAX_DELAY);

      if (r1_cmd0 != 0xFF)
      {
          break;
      }
  }


  /* Print CMD0 response */

  snprintf(msg,
           sizeof(msg),
           "CMD0 R1 = 0x%02X\r\n",
           r1_cmd0);

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)msg,
                    strlen(msg),
                    HAL_MAX_DELAY);


  /* Finish CMD0 transaction */

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

  HAL_SPI_Transmit(&hspi1,
                   &dummy,
                   1,
                   HAL_MAX_DELAY);


  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);


  /* Extra clock cycles with CS = HIGH */

  for (int i = 0; i < 2; i++)
  {
      HAL_SPI_Transmit(&hspi1,
                       &dummy,
                       1,
                       HAL_MAX_DELAY);
  }


  /* CMD8 */

  uint8_t cmd8[] = {
      0x48, 0x00, 0x00, 0x01, 0xAA, 0x87
  };

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

  HAL_SPI_Transmit(&hspi1,
                   cmd8,
                   sizeof(cmd8),
                   HAL_MAX_DELAY);


  /* Read CMD8 R1 response */

  uint8_t r1_cmd8 = 0xFF;

  for (int i = 0; i < 10; i++)
  {
      HAL_SPI_TransmitReceive(&hspi1,
                              &dummy,
                              &r1_cmd8,
                              1,
                              HAL_MAX_DELAY);

      if (r1_cmd8 != 0xFF)
      {
          break;
      }
  }


  /* Print CMD8 response */

  snprintf(msg,
           sizeof(msg),
           "CMD8 R1 = 0x%02X\r\n",
           r1_cmd8);

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)msg,
                    strlen(msg),
                    HAL_MAX_DELAY);


  uint8_t response[4];

  for (int i = 0; i < 4; i++)
  {
      HAL_SPI_TransmitReceive(&hspi1,
                              &dummy,
                              &response[i],
                              1,
                              HAL_MAX_DELAY);
  }

  snprintf(msg,
           sizeof(msg),
           "CMD8 data = %02X %02X %02X %02X\r\n",
           response[0],
           response[1],
           response[2],
           response[3]);

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)msg,
                    strlen(msg),
                    HAL_MAX_DELAY);

  /* Finish CMD8 transaction */

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

  for (int i = 0; i < 2; i++)
  {
      HAL_SPI_Transmit(&hspi1, &dummy, 1, HAL_MAX_DELAY);
  }

  // --- CMD55
  uint8_t cmd55[] = {0x77, 0x00, 0x00, 0x00, 0x00, 0x01};
  uint8_t acmd41[] = {0x69, 0x40, 0x00, 0x00, 0x00, 0x77};

  uint8_t r1_cmd55;
  uint8_t r1_acmd41;

  for (int attempt = 0; attempt < 100; attempt++)
  {
      /* CMD55 */

      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

      HAL_SPI_Transmit(&hspi1,
                       cmd55,
                       sizeof(cmd55),
                       HAL_MAX_DELAY);

      r1_cmd55 = 0xFF;

      for (int i = 0; i < 10; i++)
      {
          HAL_SPI_TransmitReceive(&hspi1,
                                  &dummy,
                                  &r1_cmd55,
                                  1,
                                  HAL_MAX_DELAY);

          if (r1_cmd55 != 0xFF)
              break;
      }

      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

      HAL_SPI_Transmit(&hspi1,
                       &dummy,
                       1,
                       HAL_MAX_DELAY);


      /* ACMD41 */

      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

      HAL_SPI_Transmit(&hspi1,
                       acmd41,
                       sizeof(acmd41),
                       HAL_MAX_DELAY);

      r1_acmd41 = 0xFF;

      for (int i = 0; i < 10; i++)
      {
          HAL_SPI_TransmitReceive(&hspi1,
                                  &dummy,
                                  &r1_acmd41,
                                  1,
                                  HAL_MAX_DELAY);

          if (r1_acmd41 != 0xFF)
              break;
      }

      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

      HAL_SPI_Transmit(&hspi1,
                       &dummy,
                       1,
                       HAL_MAX_DELAY);


      snprintf(msg, sizeof(msg),
               "ACMD41 attempt %d: R1 = 0x%02X\r\n",
               attempt + 1,
               r1_acmd41);

      HAL_UART_Transmit(&huart2,
                        (uint8_t *)msg,
                        strlen(msg),
                        HAL_MAX_DELAY);

      if (r1_acmd41 == 0x00)
          break;

      HAL_Delay(10);
  }

  // CMD 58

  uint8_t cmd58[] = {0x7A, 0x00, 0x00, 0x00, 0x00, 0xFD};

  uint8_t r1_cmd58 = 0xFF;
  uint8_t ocr[4];

  /* Select card */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

  /* Send CMD58 */
  HAL_SPI_Transmit(&hspi1,
                   cmd58,
                   sizeof(cmd58),
                   HAL_MAX_DELAY);

  /* Read R1 */
  for (int i = 0; i < 10; i++)
  {
      HAL_SPI_TransmitReceive(&hspi1,
                              &dummy,
                              &r1_cmd58,
                              1,
                              HAL_MAX_DELAY);

      if (r1_cmd58 != 0xFF)
          break;
  }

  /* Read OCR */
  for (int i = 0; i < 4; i++)
  {
      HAL_SPI_TransmitReceive(&hspi1,
                              &dummy,
                              &ocr[i],
                              1,
                              HAL_MAX_DELAY);
  }

  /* Deselect card */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

  HAL_SPI_Transmit(&hspi1,
                   &dummy,
                   1,
                   HAL_MAX_DELAY);

  /* Print R1 */
  snprintf(msg, sizeof(msg),
           "CMD58 R1 = 0x%02X\r\n",
           r1_cmd58);

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)msg,
                    strlen(msg),
                    HAL_MAX_DELAY);

  /* Print OCR */
  snprintf(msg, sizeof(msg),
           "OCR = %02X %02X %02X %02X\r\n",
           ocr[0],
           ocr[1],
           ocr[2],
           ocr[3]);

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)msg,
                    strlen(msg),
                    HAL_MAX_DELAY);


// CMD 17
  uint8_t cmd17[] = {0x51, 0x00, 0x00, 0x00, 0x00, 0x55};

  uint8_t r1_cmd17 = 0xFF;
  uint8_t data_token = 0xFF;

  uint8_t block[512];
  uint8_t crc[2];

  /* Select card */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

  /* Send CMD17 */
  HAL_SPI_Transmit(&hspi1,
                   cmd17,
                   sizeof(cmd17),
                   HAL_MAX_DELAY);

  /* Read R1 */
  for (int i = 0; i < 10; i++)
  {
      HAL_SPI_TransmitReceive(&hspi1,
                              &dummy,
                              &r1_cmd17,
                              1,
                              HAL_MAX_DELAY);

      if (r1_cmd17 != 0xFF)
          break;
  }

  /* Wait for data token 0xFE */
  for (int i = 0; i < 100000; i++)
  {
      HAL_SPI_TransmitReceive(&hspi1,
                              &dummy,
                              &data_token,
                              1,
                              HAL_MAX_DELAY);

      if (data_token == 0xFE)
          break;
  }

  /* Read 512-byte block */
  HAL_SPI_Receive(&hspi1,
                  block,
                  sizeof(block),
                  HAL_MAX_DELAY);

  /* Read CRC16 */
  HAL_SPI_Receive(&hspi1,
                  crc,
                  sizeof(crc),
                  HAL_MAX_DELAY);

  /* Deselect card */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

  HAL_SPI_Transmit(&hspi1,
                   &dummy,
                   1,
                   HAL_MAX_DELAY);

  /* Print R1 */
  snprintf(msg, sizeof(msg),
           "CMD17 R1 = 0x%02X\r\n",
           r1_cmd17);

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)msg,
                    strlen(msg),
                    HAL_MAX_DELAY);

  /* Print data token */
  snprintf(msg, sizeof(msg),
           "Data token = 0x%02X\r\n",
           data_token);

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)msg,
                    strlen(msg),
                    HAL_MAX_DELAY);

  /* Print first 16 bytes */
  snprintf(msg, sizeof(msg),
           "Data[0..15] = "
           "%02X %02X %02X %02X "
           "%02X %02X %02X %02X "
           "%02X %02X %02X %02X "
           "%02X %02X %02X %02X\r\n",
           block[0],  block[1],
           block[2],  block[3],
           block[4],  block[5],
           block[6],  block[7],
           block[8],  block[9],
           block[10], block[11],
           block[12], block[13],
           block[14], block[15]);

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)msg,
                    strlen(msg),
                    HAL_MAX_DELAY);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
