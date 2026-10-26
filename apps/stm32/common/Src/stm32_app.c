#include "purrgo/ui/ui_map_dirty.h"
#include "stm32_app.h"
#include "main.h"

#include <stdbool.h>
#include <stdint.h>
#include <purrgo/config.h>
#include <purrgo/display_hal.h>
#include <purrgo/app_fsm.h>
#include <purrgo/map.h>
#include <purrgo/map_controller.h>
#include <purrgo/app_ui.h>
#include <purrgo/gfx_renderer.h>
#include <purrgo/gfx_text.h>
#include <purrgo/system_time.h>
#include <purrgo/sun.h>
#include "purrgo/gnss_io.h"
#include "purrgo/logger.h"
#include "buttons.h"
#include "display_stm32.h"
// #include "debug_buttons.h"

#include "sd_card.h"
#include "gnss_monitor.h"

/*
 * Интервал обновления логики GNSS/FSM.
 *
 * Это не частота чтения UART: входные байты обрабатываются
 * при каждом проходе главного цикла.
 */
#define GNSS_UPDATE_PERIOD_MS 1000U

/* Состояние SD на прошлом проходе цикла: для реакции на вставку карты. */
static purrgo_sd_state_t s_prev_sd_state = PURRGO_SD_NO_CARD;

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
   * SD-карта (F411: SDIO 4 бита, U585: SPI2) + FatFs.
   * -------------------------------------------------------------------------
   * Монтирование, создание каталогов 0:/PURRGO/... и диагностика карты
   * выполняются в sd_card.c. Если карты нет, прошивка продолжает работу;
   * вставка карты отслеживается в purrgo_stm32_process().
   */
  if (!purrgo_sd_init())
  {
      PURRGO_LOG("SD card is not available\r\n");
  }
  s_prev_sd_state = purrgo_sd_state();

  // purrgo_debug_buttons_init();
  // PURRGO_LOG("UART2 Buttons OK\r\n");

  /*
   * -------------------------------------------------------------------------
   * GNSS.
   * -------------------------------------------------------------------------
   * AT6558R подключён к USART1.
   */
  purrgo_gnss_init();
  PURRGO_LOG("GNSS USART1 INIT FINISHED\r\n");
  purrgo_gnss_monitor_init();

  /*
   * -------------------------------------------------------------------------
   * Display driver.
   * -------------------------------------------------------------------------
   */
  display_init();
  PURRGO_LOG("Display OK\r\n");

  /*
   * -------------------------------------------------------------------------
   * Buttons.
   * -------------------------------------------------------------------------
   * Кнопки E-Ink HAT: K1 PA0, K2 PA1, K3 PC14, K4 PC15.
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

  /*
   * Подключаем быстрый framebuffer clear.
   * display_clear() непосредственно заполняет весь 2-bpp framebuffer
   * через memset(), поэтому GFX больше не будет очищать экран
   * пиксель за пикселем.
   */
  gfx_set_clear_callback(&global_gfx_ctx, stm32_clear_cb);

  /*
   * Быстрая заливка горизонтальных отрезков (заливка полигонов карты,
   * очистка прямоугольников): целые байты 2-bpp буфера - одним memset().
   */
  gfx_set_fill_span_callback(&global_gfx_ctx, stm32_fill_span_cb);

  PURRGO_LOG("GFX OK\r\n");

  PURRGO_LOG("Splash Screen\r\n");
  /* Splash: PurrGO! */
  gfx_set_color(&global_gfx_ctx, COLOR_WHITE, COLOR_BLACK);
  gfx_clear(&global_gfx_ctx);
  gfx_draw_string(&global_gfx_ctx, DISPLAY_WIDTH / 2 - 23, DISPLAY_HEIGHT / 2, "PurrGO!");
  display_refresh();
  HAL_Delay(300);

  last_button_poll_ms = purrgo_system_time_ms();
}

void purrgo_stm32_process(void)
{
  uint32_t current_time_ms = purrgo_system_time_ms();

  /* Отчёт о связи с GNSS (отладочный лог). */
  purrgo_gnss_monitor_process();

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
   * FSM получает события через единый API purrgo_app_handle_button().
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
  // purrgo_debug_buttons_process();

  /*
   * -----------------------------------------------------------------------
   * 4. Перерисовка UI.
   * -----------------------------------------------------------------------
   * Решение о перерисовке и сброс dirty-флагов принадлежат ядру:
   * purrgo_app_ui_needs_render() учитывает флаги и паузу после неудачного
   * рендера карты, purrgo_app_ui_render() сам забирает/сбрасывает флаги.
   * Платформа флаги не трогает.
   */
  if (purrgo_app_ui_needs_render(current_time_ms))
  {
    purrgo_app_ui_render(
        &global_gfx_ctx,
        purrgo_app_get_gnss_solution(),
        purrgo_app_get_sun_info()
    );
  }
  /*
   * -----------------------------------------------------------------------
   * 5. SD-карта: отслеживание вставки/извлечения.
   * -----------------------------------------------------------------------
   * Вызов дешёвый (сам ограничивает частоту). Блокирует цикл только при
   * повторном монтировании карты.
   */
  purrgo_sd_poll(current_time_ms);

  /*
   * После (пере)монтирования карты открытые файлы карты недействительны, а
   * ядро помнит слои, которые не открылись. Закрываем всё и перерисовываем:
   * файлы откроются заново на следующем кадре.
   */
  {
    purrgo_sd_state_t sd_state = purrgo_sd_state();
    if (sd_state == PURRGO_SD_READY && s_prev_sd_state != PURRGO_SD_READY)
    {
      purrgo_map_shutdown();
      purrgo_ui_map_mark_dirty();
    }
    s_prev_sd_state = sd_state;
  }

  /*
   * Ждём событий
  */
  __WFI();
}
