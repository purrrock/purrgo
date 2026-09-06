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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "purrgo_logger.h"
#include <purrgo/gnss_io.h>
#include "purrgo/gnss.h"
#include "purrgo/gnss_adapter.h"
#include "purrgo/gnss_types.h"
#include "purrgo/gnss_mock.h"
#include "../../../../../src/platform/stm32/buttons.h"


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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
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
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

/*
 * USART2 уже полностью инициализирован.
 * Теперь можно использовать UART для диагностического вывода.
 */
purrgo_logger_init();
purrgo_logger_write("PurrGO STM32 boot\r\n");
purrgo_logger_write("UART2 logger OK\r\n");

    /*
     * Состояние разобранного GNSS-решения.
     *
     * Оно заполняется Core-кодом через purrgo_gnss_process_nmea().
     */
    purrgo_gnss_solution_t gnss_solution = {0};

    /*
     * Инкрементальный NMEA parser.
     *
     * Он получает данные побайтно и собирает из них законченные
     * NMEA-предложения.
     */
    purrgo_gnss_parser_t gnss_parser;

    purrgo_gnss_parser_init(&gnss_parser);

    purrgo_logger_write("GNSS MOCK parser test\r\n");
    purrgo_gnss_mock_init();

    purrgo_stm32_buttons_init();

/* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
       /*
         * Обрабатываем доступные байты GNSS.
         *
         * В текущей реализации это байты из STM32 GNSS MOCK.
         * В будущем здесь будут байты реального UART/DMA.
         */
        uint8_t gnss_byte;

        /*
         * Ограничиваем количество обрабатываемых байтов за одну
         * итерацию main loop.
         *
         * Это не позволяет GNSS-потоку полностью занять CPU.
         */
        uint16_t bytes_processed = 0U;

        while (
            purrgo_gnss_read_byte(&gnss_byte) &&
            bytes_processed < 256U
        )
        {
            /*
             * Передаём очередной байт в потоковый NMEA parser.
             *
             * true означает, что получено полное предложение,
             * заканчивающееся символом '\n'.
             */
            if (purrgo_gnss_parser_feed(&gnss_parser, gnss_byte))
            {
                /*
                 * В parser.line находится готовая NMEA-строка
                 * без завершающего '\r'/'\n'.
                 *
                 * Передаём её в существующий Core GNSS adapter.
                 */
                purrgo_gnss_process_nmea(
                    gnss_parser.line,
                    &gnss_solution
                );

                /*
                 * Показываем результат обработки через UART.
                 *
                 * Координаты хранятся в формате градусов * 10^7.
                 * Поэтому выводим отдельно целую и дробную части,
                 * не используя float.
                 */
                purrgo_logger_write(
                    "GNSS: valid=%d lat=%ld lon=%ld "
                    "speed=%ld alt=%ld sats=%d time=%02d:%02d:%02d\r\n",
                    gnss_solution.valid ? 1 : 0,
                    (long)gnss_solution.lat_1e7,
                    (long)gnss_solution.lon_1e7,
                    (long)gnss_solution.speed_knots,
                    (long)gnss_solution.alt_m,
                    gnss_solution.satellites_tracked,
                    gnss_solution.hours,
                    gnss_solution.minutes,
                    gnss_solution.seconds
                );

                /*
                 * Сбрасываем parser для следующего NMEA-предложения.
                 */
                purrgo_gnss_parser_init(&gnss_parser);
            }

            bytes_processed++;
        }

        /*
         * Опрашиваем состояние кнопок.
         */
        purrgo_btn_t all_buttons[] = {
            PURRGO_BTN_UP,
            PURRGO_BTN_DOWN,
            PURRGO_BTN_LEFT,
            PURRGO_BTN_RIGHT,
            PURRGO_BTN_PLUS,
            PURRGO_BTN_MINUS,
            PURRGO_BTN_MENU,
            PURRGO_BTN_OK
        };
        for (size_t i = 0; i < sizeof(all_buttons) / sizeof(all_buttons[0]); i++)
        {
            if (purrgo_stm32_button_is_pressed(all_buttons[i]))
            {
                /* Currently a stub, this branch will not be hit */
                purrgo_app_handle_button(all_buttons[i]);
            }
        }

        /*
         * Светодиод и диагностическое сообщение остаются,
         * чтобы было видно, что main loop продолжает работать.
         */
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

        purrgo_logger_write("PurrGO STM32 alive\r\n");
        purrgo_gnss_mock_update();

        HAL_Delay(1000);
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
