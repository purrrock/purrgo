#include "purrgo/ui/ui_dirty.h"
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
#include <purrgo/boot_banner.h>
#include <purrgo/gfx_renderer.h>
#include <purrgo/gfx_text.h>
#include <purrgo/system_time.h>
#include <purrgo/sun.h>
#include "purrgo/gnss_io.h"
#include "purrgo/gnss_duty.h"
#include "purrgo/logger.h"
#include "buttons.h"
#include "display_stm32.h"
// #include "debug_buttons.h"

#include "sd_card.h"
#include "supply_stm32.h"
#include "gnss_monitor.h"
#include "stop_mode.h"
#include "gnss_stop.h"
#include "eink_wake.h"
#include "timebase.h"
#include "sound_stm32.h"

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
 * Stop 2 в простое: читалка, карта, путевой компьютер, трекер (см. stop_mode.h).
 * -DPURRGO_STOP2=0 отключает его целиком.
 */
#ifndef PURRGO_STOP2
#define PURRGO_STOP2 1
#endif

/*
 * Наибольшая длительность одного сна в Stop 2, мс. По этому периоду ядро
 * получает тик: сохранение позиции, опрос батареи, PVD, вставка/извлечение SD,
 * отложенная перерисовка. Кнопки будят сразу.
 */
#ifndef PURRGO_STOP2_MAX_MS
#define PURRGO_STOP2_MAX_MS 5000U
#endif

/*
 * То же, пока GNSS выдаёт решение каждую секунду (карта, путевой компьютер, меню). Приём сам будит МК
 * на каждой эпохе, этот срок - страховка на случай молчания приёмника: он должен быть заметно больше
 * периода эпохи (1 с), чтобы будильник и приём не давали два пробуждения за секунду, и не больше
 * PURRGO_UI_MIN_DATA_REDRAW_MS: отложенная перерисовка по данным не должна ждать дольше.
 */
#ifndef PURRGO_STOP2_NAV_MAX_MS
#define PURRGO_STOP2_NAV_MAX_MS 2000U
#endif

/*
 * Обычный Sleep (WFI между проходами главного цикла). SysTick нет: МК будят
 * прерывания (EXTI кнопок, приём GNSS по LPDMA1) и будильник LPTIM1, который
 * ставится на ближайший срок цикла (purrgo_stm32_sleep()).
 *
 * PURRGO_SLEEP_MAX_MS - самый долгий сон, если ничего срочного нет. Цикл
 * разбирает входной буфер GNSS, ведёт таймеры FSM/SD/батареи и перерисовку, поэтому
 * не должен молчать долго (кольцо GNSS 2048 Б при 115200 бод заполняется за ~180 мс).
 *
 * Пока E-Ink занят, срок сна берётся у дисплея (display_stm32_wait_ms()): ожидание таймера
 * библиотеки, таймаут BUSY, автоотключение питания, deep sleep панели после простоя. Если задание
 * ждёт BUSY, МК будит ещё и спад BUSY (eink_wake.h): опрос раз в миллисекунду не нужен.
 */
#ifndef PURRGO_SLEEP_MAX_MS
#define PURRGO_SLEEP_MAX_MS 20U
#endif

/*
 * Stop 2 вместо Sleep на ожидание дисплея короче этого срока не оправдан: выход из Stop 2 включает
 * подъём PLL и восстановление тактирования (десятки - сотни мкс), а на Sleep мс ток почти не растёт.
 */
#ifndef PURRGO_STOP2_MIN_MS
#define PURRGO_STOP2_MIN_MS 5U
#endif

/* Счётчик LPTIM1 16-битный: HAL_GetTick() должен вызываться чаще, чем он переполняется. */
_Static_assert(PURRGO_SLEEP_MAX_MS < PURRGO_TIMEBASE_MAX_GAP_MS,
               "PURRGO_SLEEP_MAX_MS is too long for the 16-bit LPTIM1 timebase");
_Static_assert(PURRGO_STOP2_MAX_MS < PURRGO_TIMEBASE_MAX_GAP_MS,
               "PURRGO_STOP2_MAX_MS is too long for the 16-bit LPTIM1 timebase");
_Static_assert(PURRGO_STOP2_NAV_MAX_MS < PURRGO_TIMEBASE_MAX_GAP_MS,
               "PURRGO_STOP2_NAV_MAX_MS is too long for the 16-bit LPTIM1 timebase");
_Static_assert(PURRGO_GNSS_STOP_QUIET_MS <= PURRGO_SLEEP_MAX_MS,
               "GNSS quiet window must fit into one Sleep period");

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

/*
 * true - на последнем проходе цикла входной буфер GNSS разобран не до конца
 * (достигнут GNSS_MAX_BYTES_PER_LOOP): спать нельзя, нужен ещё проход.
 */
static bool s_gnss_backlog;

/**
 * @brief Обновление драйвера кнопок и передача событий в FSM.
 *
 * Драйвер самостоятельно определяет SHORT/LONG.
 * Здесь физические кнопки уже не перебираются:
 * приложение получает готовые события.
 *
 * Вызывается раз в BUTTON_POLL_PERIOD_MS, но только пока это нужно драйверу
 * (purrgo_stm32_buttons_poll_needed()): был фронт EXTI либо идёт debounce/
 * удержание/отпускание.
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

/*
 * Обычный Sleep: WFI с будильником LPTIM1. Решение принимается при взведённом
 * PRIMASK: прерывание, пришедшее после проверок, не теряется (ожидающее IRQ
 * обрывает WFI, а выполнится сразу после __enable_irq()).
 */
static void purrgo_stm32_sleep(uint32_t now_ms)
{
  /* Есть что делать сразу: входной буфер GNSS разобран не до конца. */
  if (s_gnss_backlog)
  {
    return;
  }

  /* Что нужно дисплею: 0 - опрос сейчас (идёт отправка кадра по SPI, ожидание кончилось). */
  bool on_busy = false;
  const uint32_t display_ms = display_stm32_wait_ms(&on_busy);
  if (display_ms == 0U)
  {
    return;
  }

  /* Звуковой сигнал: следующий шаг (пауза, конец тона) пора выполнить сейчас. */
  const uint32_t sound_ms = purrgo_sound_stm32_wait_ms(now_ms);
  if (sound_ms == 0U)
  {
    return;
  }

  __disable_irq();

  uint32_t wake_ms = PURRGO_SLEEP_MAX_MS;

  if (purrgo_stm32_buttons_poll_needed())
  {
    /* До следующего опроса кнопок (debounce, LONG, отпускание): не позже BUTTON_POLL_PERIOD_MS. */
    const uint32_t since_ms = now_ms - last_button_poll_ms;
    const uint32_t left_ms = (since_ms >= BUTTON_POLL_PERIOD_MS) ? 1U
                                                                  : (BUTTON_POLL_PERIOD_MS - since_ms);
    if (left_ms < wake_ms)
    {
      wake_ms = left_ms;
    }
  }

  if (display_ms < wake_ms)
  {
    wake_ms = display_ms;
  }

  /* Шаги звукового сигнала (щелчок длится 3 мс): будить точно к концу шага, а не по периоду цикла. */
  if (sound_ms < wake_ms)
  {
    wake_ms = sound_ms;
  }

  /*
   * Спад BUSY будит раньше срока; false - BUSY уже низкий, спать нельзя (фронт упущен). Будильник:
   * false - не поставлен или срок уже прошёл, следующий проход цикла сразу.
   */
  if ((!on_busy || purrgo_eink_busy_wake_arm()) && purrgo_timebase_wake_arm(wake_ms))
  {
    __DSB();
    __WFI();
    purrgo_timebase_wake_disarm();
  }
  if (on_busy)
  {
    purrgo_eink_busy_wake_disarm();
  }

  __enable_irq();
}

/*
 * Ожидание событий в конце прохода главного цикла: Stop 2, если приложение, кнопки и вход GNSS в покое,
 * а дисплею от главного цикла в ближайшие PURRGO_STOP2_MIN_MS ничего не нужно (идёт волна панели,
 * ждём BUSY, или панель уже в deep sleep), иначе обычный Sleep (WFI, пробуждение по прерыванию,
 * спаду BUSY или будильнику LPTIM1).
 *
 * Пока идёт пачка данных от приёмника, МК остаётся в Sleep (будильник PURRGO_SLEEP_MAX_MS):
 * purrgo_gnss_stop_ready() пропускает в Stop 2 только после тишины на входе, иначе МК будили бы
 * на каждый байт пачки и каждый раз поднимали PLL.
 */
static void purrgo_stm32_idle(uint32_t now_ms)
{
#if (PURRGO_STOP2 != 0)
  bool on_busy = false;
  const uint32_t display_ms = display_stm32_wait_ms(&on_busy);
  /* Цикл по EN в трекере: следующее включение, таймаут фикса и т. п. наступают без прерывания (UINT32_MAX - ничего не ждёт). */
  const uint32_t duty_ms = purrgo_gnss_tracker_wait_ms(now_ms);

  if (purrgo_app_deep_sleep_allowed(now_ms) &&
      !purrgo_sound_stm32_busy() &&   /* TIM16 в Stop 2 не работает: сигнал доигрывается в Sleep */
      (display_ms >= PURRGO_STOP2_MIN_MS) &&
      (duty_ms >= PURRGO_STOP2_MIN_MS) &&   /* срок политики вот-вот наступит: главному циклу нужно сработать сразу */
      purrgo_stm32_buttons_idle() &&
      purrgo_gnss_stop_ready(now_ms))
  {
    /* Приёмник выдаёт решение раз в секунду (кроме трекера с PSMOO и выключенного GNSS в читалке). */
    uint32_t max_ms = (purrgo_gnss_is_powered() && !purrgo_app_tracker_mode_active())
                    ? PURRGO_STOP2_NAV_MAX_MS
                    : PURRGO_STOP2_MAX_MS;

    /* Дисплей: срок библиотеки (таймер, BUSY, deep sleep панели после простоя) и спад BUSY. */
    if (display_ms < max_ms)
    {
      max_ms = display_ms;
    }
    if (duty_ms < max_ms)
    {
      max_ms = duty_ms;   /* точность включения по EN: просыпаемся к сроку, а не по периоду PURRGO_STOP2_MAX_MS */
    }

    if (purrgo_stop2_enter(max_ms, on_busy))
    {
      return;
    }
  }
#endif
  purrgo_stm32_sleep(now_ms);
}

/*
 * Дисплей, графический контекст и загрузочный баннер (purrgo/boot_banner.h).
 *
 * Баннер рисуется во framebuffer, уходит на панель полным обновлением (4 gray), затем
 * блокирующе ждём конца обновления (секунды: физически быстрее E-Ink не умеет) и держим
 * картинку ещё PURRGO_BOOT_BANNER_MS. Главный цикл ещё не запущен, поэтому блокировка
 * безопасна; HAL_Delay() работает от таймбейза LPTIM1.
 *
 * В конце панель «забывает» картинку: первый кадр карты пойдёт полным обновлением, а не
 * частичным поверх логотипа.
 */
static void purrgo_stm32_display_and_banner(void)
{
  /*
   * -------------------------------------------------------------------------
   * Display driver.
   * -------------------------------------------------------------------------
   */
  display_init();
  PURRGO_LOG("Display OK\r\n");

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
      PURRGO_LOGE("GFX INIT ERROR\r\n");
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

  /*
   * Линии пишут пиксели прямо в 2-bpp буфер дисплея: stm32_draw_pixel_cb - это
   * gfx_fb2bpp_set_pixel() над тем же буфером (display_stm32.c), раскладка одна.
   */
  gfx_set_direct_fb2bpp(&global_gfx_ctx, true);

  PURRGO_LOG("GFX OK\r\n");

  /*
   * -------------------------------------------------------------------------
   * Загрузочный баннер.
   * -------------------------------------------------------------------------
   */
  display_set_mode(DISPLAY_MODE_GRAY4);
  purrgo_boot_banner_render(&global_gfx_ctx);
  display_refresh();
  display_stm32_wait_idle();       /* пока идёт обновление, баннера на экране ещё нет */
  HAL_Delay(PURRGO_BOOT_BANNER_MS);
  display_stm32_forget_image();
  PURRGO_LOG("Boot banner OK (v%s, %s%s)\r\n", purrgo_boot_version(),
             purrgo_boot_revision(), purrgo_boot_is_debug() ? ", Debug" : "");
}

void purrgo_stm32_init(void)
{
  /*
   * Карта обесточена сразу (и Detect_SD в Hi-Z): баннер и ожидание панели идут секундами,
   * а до purrgo_sd_init() подтяжка Detect_SD подпитывала бы обесточенную карту.
   */
  purrgo_sd_park();

  /*
   * -------------------------------------------------------------------------
   * Диагностический UART.
   * -------------------------------------------------------------------------
   * purrgo_logger использует платформенную реализацию logger,
   * которая уже привязана к USART1.
   */
  purrgo_logger_init();

  PURRGO_LOG("PurrGO STM32 boot\r\n");
  PURRGO_LOG("UART1 logger OK\r\n");

  /*
   * -------------------------------------------------------------------------
   * Контроль напряжения питания (PVD).
   * -------------------------------------------------------------------------
   * До монтирования карты: при запуске от севших батарей карта не монтируется,
   * в работе просадка останавливает запись на SD (purrgo/supply.h).
   */
  purrgo_supply_stm32_init();

  /*
   * -------------------------------------------------------------------------
   * Дисплей, графика и загрузочный баннер.
   * -------------------------------------------------------------------------
   * Первыми из тяжёлых этапов: логотип на экране как можно раньше, а блокирующее
   * ожидание панели (секунды) приходится до инициализации GNSS (иначе кольцо приёма
   * переполнилось бы) и SD.
   */
  purrgo_stm32_display_and_banner();

  /*
   * -------------------------------------------------------------------------
   * SD-карта (SPI2) + FatFs.
   * -------------------------------------------------------------------------
   * Монтирование, создание каталогов 0:/PURRGO/... и диагностика карты
   * выполняются в sd_card.c. Если карты нет, прошивка продолжает работу;
   * вставка карты отслеживается в purrgo_stm32_process().
   */
  if (!purrgo_sd_init())
  {
      PURRGO_LOGW("SD card is not available\r\n");
  }
  s_prev_sd_state = purrgo_sd_state();

  // purrgo_debug_buttons_init();
  // PURRGO_LOG("UART1 Buttons OK\r\n");

  /*
   * -------------------------------------------------------------------------
   * GNSS.
   * -------------------------------------------------------------------------
   * G10A F30 подключён к LPUART1 (PA3 RX, PA2 TX), приём через LPDMA1.
   */
  purrgo_gnss_init();
  PURRGO_LOG("GNSS LPUART1 INIT FINISHED\r\n");
  purrgo_gnss_monitor_init();

  /*
   * -------------------------------------------------------------------------
   * Buttons.
   * -------------------------------------------------------------------------
   * Кнопки E-Ink HAT: K1 PA0, K2 PA1, K3 PC14, K4 PC15.
   */
  purrgo_stm32_buttons_init();
  PURRGO_LOG("Buttons OK\r\n");

  /* Пьезоизлучатель (TIM16, MX_TIM16_Init() уже вызван из main.c). */
  purrgo_sound_stm32_init();

#if (PURRGO_STOP2 != 0)
  /* Stop 2 в простое (таймбейз LPTIM1 уже идёт); при сбое работает обычный Sleep. */
  (void)purrgo_stop2_init();
#endif

  /*
   * -------------------------------------------------------------------------
   * Application FSM.
   * -------------------------------------------------------------------------
   */
  purrgo_app_init();
  PURRGO_LOG("App FSM OK\r\n");

  /*
   * После баннера первый кадр - карта; он ставится в очередь первым
   * purrgo_stm32_process() -> purrgo_app_ui_render() и идёт полным обновлением
   * (см. purrgo_stm32_display_and_banner()).
   */
  last_button_poll_ms = purrgo_system_time_ms();
}

void purrgo_stm32_process(void)
{
  uint32_t current_time_ms = purrgo_system_time_ms();

  /*
   * E-Ink: продвинуть неблокирующий драйвер (запуск обновления из очереди,
   * завершение, засыпание панели в простое).
   */
  display_stm32_poll();

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
  s_gnss_backlog = (bytes_processed >= GNSS_MAX_BYTES_PER_LOOP);

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
   * 3. Кнопки.
   * -----------------------------------------------------------------------
   * Фронт на K1..K4 приходит по EXTI и лишь взводит флаг в драйвере. Пока все
   * кнопки отпущены и фронтов нет, опрашивать нечего; нажатая кнопка
   * (debounce, LONG, отпускание) доопрашивается каждые BUTTON_POLL_PERIOD_MS.
   * FSM получает события через единый API purrgo_app_handle_button().
   */
  if (
      purrgo_stm32_buttons_poll_needed() &&
      (uint32_t)(current_time_ms - last_button_poll_ms)
      >= BUTTON_POLL_PERIOD_MS
  )
  {
    last_button_poll_ms = current_time_ms;
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
   *
   * Пока драйвер E-Ink передаёт данные кадра на панель, рисовать в
   * framebuffer нельзя: рендер откладывается (флаги остаются взведёнными).
   * Саму волну обновления (секунды) ждать не нужно: запрос на новое
   * обновление встаёт в очередь display_eink.c.
   */
  if (display_stm32_ready_for_render() &&
      purrgo_app_ui_needs_render(current_time_ms))
  {
    purrgo_app_ui_render(
        &global_gfx_ctx,
        purrgo_app_get_gnss_solution(),
        purrgo_app_get_sun_info()
    );
    display_stm32_poll();   /* сразу запустить поставленное обновление */
  }
  /*
   * -----------------------------------------------------------------------
   * 5. SD-карта: отслеживание вставки/извлечения.
   * -----------------------------------------------------------------------
   * Вызов дешёвый (сам ограничивает частоту). Блокирует цикл только при
   * повторном монтировании карты.
   */
  purrgo_sd_poll(current_time_ms);

  /* Парковка карты по тайм-ауту простоя (без открытых файлов); просыпается при первом обращении к ФС. */
  purrgo_sd_idle_service();

  /* Файлы карты отпускаются после простоя (иначе открытые слои не дают SD припарковаться). */
  (void)purrgo_map_release_idle(current_time_ms, PURRGO_MAP_IDLE_RELEASE_MS);

  /* Запись лога на SD (если включена в меню): сбрасывает накопленные сообщения пачкой. */
  purrgo_logger_sd_service(current_time_ms);

  /*
   * После (пере)монтирования карты открытые файлы карты недействительны, а
   * ядро помнит слои, которые не открылись. Закрываем всё и перерисовываем:
   * файлы откроются заново на следующем кадре.
   */
  {
    purrgo_sd_state_t sd_state = purrgo_sd_state();
    if (sd_state == PURRGO_SD_READY && s_prev_sd_state != PURRGO_SD_READY)
    {
      /* Пробуждение из парковки: открытых файлов не было, слои закрывать и перерисовывать не нужно. */
      if (!purrgo_sd_take_park_wake())
      {
        purrgo_map_shutdown();
        purrgo_ui_dirty_mark(UI_DIRTY_MAP);
      }
    }
    s_prev_sd_state = sd_state;
  }

  /* Звуковой сигнал: переход к следующему шагу (пауза, конец тона). */
  purrgo_sound_stm32_service(purrgo_system_time_ms());

  /*
   * Ждём событий
  */
  purrgo_stm32_idle(purrgo_system_time_ms());
}
