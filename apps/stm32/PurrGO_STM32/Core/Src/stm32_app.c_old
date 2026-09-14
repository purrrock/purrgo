#include "stm32_app.h"
#include "main.h"

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
#include "purrgo/logger.h"
#include "buttons.h"
#include "display_stm32.h"
#include "display_st7789.h"
#include "debug_buttons.h"

#include "fatfs.h"
#include "fatfs_sd.h"

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

static FATFS purrgo_fs;

/*
 * Доступ к framebuffer осуществляется через callbacks,
 * определённые ниже.
 */
static gfx_context_t global_gfx_ctx;

/*
 * Время последнего опроса кнопок.
 */
static uint32_t last_button_poll_ms;

/**
 * @brief Обновление драйвера кнопок и передача событий в FSM.
 *
 * Драйвер самостоятельно определяет SHORT/LONG.
 * Здесь физические кнопки уже не перебираются:
 * приложение получает готовые события.
 */
static void process_buttons(void)
{
    purrgo_btn_t event;

    /*
     * Обновить debounce и определить новые SHORT/LONG события.
     */
    purrgo_stm32_buttons_update();

    /*
     * Передать FSM все события, накопившиеся за этот цикл.
     */
    while (purrgo_stm32_buttons_get_event(&event))
    {
        purrgo_app_handle_button(event);
    }
}

void purrgo_stm32_init(void)
{
  /*
   * -------------------------------------------------------------------------
   * Диагностический UART.
   * -------------------------------------------------------------------------
   * purrgo_logger использует платформенную реализацию logger,
   * которая уже привязана к USART2.
   */
  purrgo_logger_init();

  PURRGO_LOG("PurrGO STM32 boot\r\n");
  PURRGO_LOG("UART2 logger OK\r\n");

  /*
   * -------------------------------------------------------------------------
   * FatFs: монтирование файловой системы SD-карты.
   * -------------------------------------------------------------------------
   */
  FRESULT fs_result = f_mount(&purrgo_fs, "0:", 1);
  if (fs_result != FR_OK) {
      PURRGO_LOG("FatFs mount ERROR: %d\r\n", fs_result);
  } else {
      PURRGO_LOG("FatFs mount OK\r\n");
  }

  purrgo_debug_buttons_init();
  PURRGO_LOG("UART2 Buttons OK\r\n");

  /*
   * -------------------------------------------------------------------------
   * GNSS.
   * -------------------------------------------------------------------------
   * AT6558R подключён к USART1.
   */
  purrgo_gnss_init();
  PURRGO_LOG("GNSS USART1 INIT FINISHED\r\n");

  /*
   * -------------------------------------------------------------------------
   * Display framebuffer.
   * -------------------------------------------------------------------------
   */
  display_init();
  PURRGO_LOG("ST7789 display init...\r\n");
  ST7789_Init();
  PURRGO_LOG("Display OK\r\n");

  /*
   * -------------------------------------------------------------------------
   * Buttons.
   * -------------------------------------------------------------------------
   */
   purrgo_stm32_buttons_init();
   PURRGO_LOG("Buttons OK\r\n");

  /*
   * -------------------------------------------------------------------------
   * Application FSM.
   * -------------------------------------------------------------------------
   */
  purrgo_app_init();
  PURRGO_LOG("App FSM OK\r\n");

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
      PURRGO_LOG("GFX INIT ERROR\r\n");
      Error_Handler();
  }
  PURRGO_LOG("GFX OK\r\n");

  PURRGO_LOG("Splash Screen\r\n");
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
  //HAL_Delay(300);

  last_button_poll_ms = purrgo_system_time_ms();
}

void purrgo_stm32_process(void)
{
  uint32_t current_time_ms = purrgo_system_time_ms();

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
   * FSM получает
   * события через единый API purrgo_app_handle_button().
   */
  if (
      (uint32_t)(current_time_ms - last_button_poll_ms)
      >= BUTTON_POLL_PERIOD_MS
  )
  {
    last_button_poll_ms = current_time_ms;
    //обработка кнопок
    process_buttons();
  }
  // прием эмуляции кнопок через UART
  purrgo_debug_buttons_process();

  /*
   * -----------------------------------------------------------------------
   * 4. Перерисовка UI.
   * -----------------------------------------------------------------------
   * UI перерисовывается только при наличии dirty-флага.
   * Проверяем как общий UI-флаг, так и флаг карты.
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
   * Ждём событий
  */
  __WFI();
}
