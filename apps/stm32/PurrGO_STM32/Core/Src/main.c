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
#include "fatfs.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include "fatfs.h"
#include <stdio.h>
#include <string.h>
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <purrgo/display_hal.h>
#include <purrgo/app_fsm.h>
#include <purrgo/app_ui.h>
#include <purrgo/gfx_renderer.h>
#include "purrgo_logger.h"
#include <purrgo/gnss_io.h>
#include "purrgo/gnss.h"
#include "purrgo/gnss_adapter.h"
#include "purrgo/gnss_types.h"
#include "purrgo/gnss_mock.h"
#include "buttons.h"
#include "display_stm32.h"


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
purrgo_gnss_solution_t gnss_solution = {0};
/*
 * Графический контекст приложения.
 * Он связывает общий графический код PurrGO
 * с framebuffer STM32.
 */
static gfx_context_t global_gfx_ctx;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*
 * Callback от общего графического ядра PurrGO
 * к STM32 framebuffer.
 *
 * GFX работает только с абстрактным framebuffer и не знает,
 * как именно STM32 хранит пиксели.
 */
static void stm32_draw_pixel_cb(
    void *fb,
    int16_t x,
    int16_t y,
    gfx_color_t color
)
{
    /*
     * Текущий STM32 display driver сам владеет framebuffer.
     * Параметр fb пока не используется.
     */
    (void)fb;
    display_set_pixel(x, y, color);
}

/*
 * Callback чтения пикселя из STM32 framebuffer.
 */
static gfx_color_t stm32_read_pixel_cb(
    void *fb,
    int16_t x,
    int16_t y
)
{
    (void)fb;

    return display_get_pixel(x, y);
}
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
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_FATFS_Init();
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
     * Оно заполняется Core-кодом через purrgo_gnss_process_nmea().
     */

    /*
     * Инкрементальный NMEA parser.
     * Он получает данные побайтно и собирает из них законченные
     * NMEA-предложения.
     */
    purrgo_gnss_parser_t gnss_parser;

    purrgo_gnss_parser_init(&gnss_parser);

    purrgo_logger_write("GNSS MOCK parser test\r\n");
    purrgo_gnss_mock_init();
    purrgo_logger_write("GNSS OK\r\n");

    display_init();
    purrgo_logger_write("Display OK\r\n");

    purrgo_stm32_buttons_init();
    purrgo_logger_write("Buttons OK\r\n");

/*
 * Инициализация конечного автомата приложения.
 * После этого PurrGO находится в начальном состоянии
 * APP_STATE_MAP.
 */
purrgo_app_init();
purrgo_logger_write("App FSM OK\r\n");
/*
 * Инициализация графического контекста.
 * Общий UI-код PurrGO будет рисовать через этот контекст,
 * а callbacks выше будут записывать пиксели в STM32 framebuffer.
 * framebuffer передаём как непрозрачный указатель.
 * Сам STM32 display driver предоставляет доступ к нему
 * через display_get_framebuffer().
 */
if (!gfx_init(
        &global_gfx_ctx,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT,
        (void *)display_get_framebuffer(),
        stm32_draw_pixel_cb,
        stm32_read_pixel_cb))
{
    purrgo_logger_write("GFX INIT ERROR\r\n");
    /*
     * Без графического контекста приложение продолжать
     * работу не должно.
     */
    Error_Handler();
}
purrgo_logger_write("GFX OK\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /*
     * ---------------------------------------------------------
     * 1. Получение и обработка GNSS.
     * ---------------------------------------------------------
     * Пока используется GNSS MOCK.
     * В будущем источник байтов здесь будет заменён
     * на реальный UART/DMA GNSS.
     */
    {
        uint8_t gnss_byte;
        uint16_t bytes_processed = 0U;
        while (
            purrgo_gnss_read_byte(&gnss_byte) &&
            bytes_processed < 256U
        )
        {
            /*
             * Передаём байт потоковому NMEA parser.
             * true означает, что получено полное NMEA предложение.
             */
            if (purrgo_gnss_parser_feed(&gnss_parser, gnss_byte))
            {
                /*
                 * Передаём готовое NMEA предложение
                 * в общий GNSS adapter.
                 */
                purrgo_gnss_process_nmea(
                    gnss_parser.line,
                    &gnss_solution
                );
                /*
                 * Parser готов к следующему предложению.
                 */
                purrgo_gnss_parser_init(&gnss_parser);
            }
            bytes_processed++;
        }
    }
    /*
     * ---------------------------------------------------------
     * 2. Периодическое обновление GNSS MOCK и application FSM.
     * ---------------------------------------------------------
     * Один раз в секунду передаём актуальное GNSS решение
     * в application layer.
     */
    {
        static uint32_t last_app_update = 0U;
        uint32_t now = HAL_GetTick();
        if ((now - last_app_update) >= 1000U)
        {
            last_app_update = now;
            /*
             * Временно двигаем GNSS MOCK.
             * Для реального GNSS этот вызов здесь больше не понадобится.
             */
            purrgo_gnss_mock_update();
            /*
             * Передаём текущее GNSS решение конечному автомату
             * приложения. Не работает, пока не решим проблему с purrgo_system_time
             */
            // purrgo_app_update(&gnss_solution);
            purrgo_logger_write("APP: update\r\n");
        }
    }
    /*
     * ---------------------------------------------------------
     * 3. Обработка кнопок.
     * ---------------------------------------------------------
     */
    {
        const purrgo_btn_t all_buttons[] = {
            PURRGO_BTN_UP,
            PURRGO_BTN_DOWN,
            PURRGO_BTN_LEFT,
            PURRGO_BTN_RIGHT,
            PURRGO_BTN_PLUS,
            PURRGO_BTN_MINUS,
            PURRGO_BTN_MENU,
            PURRGO_BTN_OK
        };
        for (
            size_t i = 0;
            i < sizeof(all_buttons) / sizeof(all_buttons[0]);
            i++
        )
        {
            if (purrgo_stm32_button_is_pressed(all_buttons[i]))
            {
                purrgo_app_handle_button(all_buttons[i]);
            }
        }
    }
    /*
     * ---------------------------------------------------------
     * 4. Отрисовка UI.
     * ---------------------------------------------------------
     * UI должен перерисовываться только когда есть изменения.
     * Это особенно важно для E-Ink, поскольку обновление экрана
     * является медленной операцией.
     */
    if (
        purrgo_app_ui_is_dirty() ||
        purrgo_app_map_is_dirty()
    )
    {
        purrgo_logger_write("APP: UI render\r\n");
        purrgo_app_ui_render(
            &global_gfx_ctx,
            &gnss_solution,
            NULL
        );

        purrgo_app_ui_clear_dirty();
        /*
         * Пока физический E-Ink дисплей не подключён,
         * этот вызов работает как stub и только сообщает
         * о refresh через UART.
         */
        display_refresh();
    }
    /*
     * Небольшая задержка освобождает CPU между итерациями
     * главного цикла.
     */
    HAL_Delay(10);

         /*
         * Светодиод и диагностическое сообщение, чтобы было видно, что main loop продолжает работать.
         */
		static uint32_t last_tick = 0;
		if (HAL_GetTick() - last_tick >= 5000) {
		last_tick = HAL_GetTick();
		HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
		 purrgo_logger_write("PurrGO STM32 alive\r\n");
		 purrgo_gnss_mock_update();
		}
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
