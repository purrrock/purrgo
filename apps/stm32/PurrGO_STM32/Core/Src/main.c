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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdbool.h>
#include <stdint.h>
#include <purrgo/config.h>
#include <purrgo/display_hal.h>
#include <purrgo/app_fsm.h>
#include <purrgo/app_ui.h>
#include <purrgo/gfx_renderer.h>
#include <purrgo/gfx_text.h>
#include <purrgo/system_time.h>
#include <purrgo/sun.h>
#include "purrgo/gnss_io.h"
#include "purrgo/gnss_mock.h"
#include "purrgo_logger.h"
#include "buttons.h"
#include "display_stm32.h"
#include "display_st7789.h"


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/*
 * Интервал обновления логики GNSS/FSM.
 *
 * Это не частота чтения UART: входные байты обрабатываются
 * при каждом проходе главного цикла.
 */
#define GNSS_UPDATE_PERIOD_MS 1000U

/*
 * Период опроса кнопок.
 */
#define BUTTON_POLL_PERIOD_MS 10U

/*
 * Период пересчёта параметров восхода/заката.
 */
#define SUN_UPDATE_PERIOD_MS 60000U

/*
 * Максимальное количество GNSS-байтов, обрабатываемых за один
 * проход главного цикла.
 *
 * Ограничение не позволяет бесконечному потоку входных данных
 * полностью заблокировать остальную логику приложения.
 */
#define GNSS_MAX_BYTES_PER_LOOP 256U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/*
 * Доступ к framebuffer осуществляется через callbacks,
 * определённые ниже.
 */
static gfx_context_t global_gfx_ctx;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

static void process_buttons(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief Опрос аппаратных кнопок.
 *
 * На текущем этапе драйвер buttons.c является заглушкой:
 * purrgo_stm32_button_is_pressed() всегда возвращает false.
 *
 * Поэтому этот код уже подключён к FSM, но физические кнопки
 * пока не будут вызывать переходы состояний.
 */
static void process_buttons(void)
{
    /*
     * Обрабатываем все кнопки, определённые в app_fsm.h.
     *
     * Драйвер кнопок возвращает true только для реально нажатой
     * кнопки. Сейчас реализация-заглушка всегда возвращает false.
     */
    static const purrgo_btn_t buttons[] =
    {
        PURRGO_BTN_UP,
        PURRGO_BTN_DOWN,
        PURRGO_BTN_LEFT,
        PURRGO_BTN_RIGHT,
        PURRGO_BTN_PLUS,
        PURRGO_BTN_MINUS,
        PURRGO_BTN_MENU,
        PURRGO_BTN_OK
    };

    const size_t button_count =
        sizeof(buttons) / sizeof(buttons[0]);

    for (size_t i = 0U; i < button_count; ++i)
    {
        if (purrgo_stm32_button_is_pressed(buttons[i]))
        {
            purrgo_app_handle_button(buttons[i]);
        }
    }
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
   * -------------------------------------------------------------------------
   * Диагностический UART.
   * -------------------------------------------------------------------------
   *
   * purrgo_logger использует платформенную реализацию logger,
   * которая уже привязана к USART2.
   */
  purrgo_logger_init();

  purrgo_logger_write("PurrGO STM32 boot\r\n");
  purrgo_logger_write("UART2 logger OK\r\n");


  /*
   * -------------------------------------------------------------------------
   * GNSS MOCK Initialization.
   * -------------------------------------------------------------------------
   */
  purrgo_gnss_mock_init();

  /*
   * -------------------------------------------------------------------------
   * Display framebuffer.
   * -------------------------------------------------------------------------
   *
   * Сейчас display_stm32 является заглушкой аппаратного доступа:
   * display_init() создаёт/очищает framebuffer, а display_refresh()
   * только выдаёт диагностическое сообщение.
   */
  display_init();

  purrgo_logger_write("Display OK\r\n");

  /*
   * -------------------------------------------------------------------------
   * Buttons.
   * -------------------------------------------------------------------------
   *
   * Драйвер пока является заглушкой.
   */
  purrgo_stm32_buttons_init();

  purrgo_logger_write("Buttons OK\r\n");

  /*
   * -------------------------------------------------------------------------
   * Application FSM.
   * -------------------------------------------------------------------------
   */
  purrgo_app_init();

  purrgo_logger_write("App FSM OK\r\n");

  /*
   * -------------------------------------------------------------------------
   * Graphics context.
   * -------------------------------------------------------------------------
   *
   * GFX получает:
   *   - физический размер дисплея;
   *   - framebuffer;
   *   - callback записи пикселя;
   *   - callback чтения пикселя.
   *
   * Сам GFX остаётся платформенно-независимым.
   */
  if (!gfx_init(
          &global_gfx_ctx,
          DISPLAY_WIDTH,
          DISPLAY_HEIGHT,
          (void *)display_get_framebuffer(),
          stm32_draw_pixel_cb,
          stm32_read_pixel_cb))
  {
      /*
       * gfx_init() возвращает false только при некорректных
       * аргументах/нулевых указателях согласно его API.
       */
      purrgo_logger_write("GFX INIT ERROR\r\n");

      Error_Handler();
  }

  purrgo_logger_write("GFX OK\r\n");

  purrgo_logger_write("ST7789 Splash Screen\r\n");
  ST7789_Init();

  /* Splash: Purr... */
  gfx_set_color(&global_gfx_ctx, COLOR_WHITE, COLOR_BLACK);
  gfx_clear(&global_gfx_ctx);
  gfx_draw_string(&global_gfx_ctx, DISPLAY_WIDTH / 2 - 20, DISPLAY_HEIGHT / 2, "Purr...");
  display_refresh();
  HAL_Delay(300);

  /* Splash: GO! */
  gfx_clear(&global_gfx_ctx);
  gfx_draw_string(&global_gfx_ctx, DISPLAY_WIDTH / 2 - 10, DISPLAY_HEIGHT / 2, "GO!");
  display_refresh();
  HAL_Delay(300);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  
  /*
   * Время последнего опроса кнопок.
   */
  uint32_t last_button_poll_ms = purrgo_system_time_ms();
  
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	
	uint32_t current_time_ms = purrgo_system_time_ms();

    static uint32_t last_mock_update_ms = 0;
    if (current_time_ms - last_mock_update_ms >= GNSS_UPDATE_PERIOD_MS) {
        last_mock_update_ms = current_time_ms;
        purrgo_gnss_mock_update();
    }

    /*
     * -----------------------------------------------------------------------
     * 1. Обработка входных GNSS-данных.
     * -----------------------------------------------------------------------
     */
    uint8_t rx_byte;
    uint16_t bytes_processed = 0U;
    while (
        bytes_processed < GNSS_MAX_BYTES_PER_LOOP &&
        purrgo_gnss_read_byte(&rx_byte)
    )
    {
        purrgo_app_feed_gnss_byte(rx_byte);
        bytes_processed++;
    }

    /*
     * -----------------------------------------------------------------------
     * 2. Обновление FSM/GNSS.
     * -----------------------------------------------------------------------
     *
     * Передаём текущее время в FSM, где происходит
     * расчёт Солнца и обновление состояния.
     */
    purrgo_app_tick(current_time_ms);

    /*
     * -----------------------------------------------------------------------
     * 3. Опрос кнопок.
     * -----------------------------------------------------------------------
     *
     * Реальный драйвер кнопок пока не подключён, но FSM уже получает
     * события через единый API purrgo_app_handle_button().
     */
    if (
        (uint32_t)(current_time_ms - last_button_poll_ms)
        >= BUTTON_POLL_PERIOD_MS
    )
    {
      last_button_poll_ms = current_time_ms;

      process_buttons();
    }

    /*
     * -----------------------------------------------------------------------
     * 4. Перерисовка UI.
     * -----------------------------------------------------------------------
     *
     * UI перерисовывается только при наличии dirty-флага.
     *
     * Проверяем как общий UI-флаг, так и флаг карты.
     * Это соответствует циклу PC-эмулятора.
     */
    if (
        purrgo_app_ui_is_dirty() ||
        purrgo_app_map_is_dirty()
    )
    {
      purrgo_app_ui_render(
          &global_gfx_ctx,
          purrgo_app_get_gnss_solution(),
          purrgo_app_get_sun_info()
      );

      /*
       * После отрисовки считаем UI обновлённым.
       */
      purrgo_app_ui_clear_dirty();
    }

    /*
     * Небольшая задержка разгружает CPU.
     *
     * Она не определяет периоды GNSS/FSM/button processing:
     * эти периоды контролируются через system_time_ms().
     */
    HAL_Delay(1);
	
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

	purrgo_logger_write("Wrong parameters value: file %s on line %d\r\n", file, line)
 
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
