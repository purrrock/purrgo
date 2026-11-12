/*
 * display_eink.c - E-Ink Waveshare 2.7" V2 (176 x 264) на библиотеке epd4g.
 *
 * Слой между display_stm32.c (framebuffer, накопление областей) и epd4g:
 * очередь запросов (одно обновление в работе, остальное сливается), выбор
 * полное/частичное обновление, смена режима панели 4 gray <-> 1 bit,
 * deep sleep панели в простое, восстановление после ошибки.
 *
 * Сама библиотека epd4g: считает частичные обновления подряд и заменяет очередное
 * частичное полным (EPD4G_PARTIALS_BEFORE_FULL), а также выключает питание панели
 * после EPD4G_AUTO_POWEROFF_MS простоя (внутри epd4g_poll()). Здесь этого нет.
 * Подробности: docs/architecture.md, раздел "E-Ink driver (epd4g)".
 */
#include "display_eink.h"

#include <string.h>

#include "main.h"
#include "spi.h"
#include "epd4g.h"
#include "epd4g_bw.h"
#include "purrgo/logger.h"

/* ---------------------------- настройки ---------------------------------- */

/*
 * Делитель SPI1 для панели. Ядро SPI1 у U585 - 80 МГц, делитель 2 даёт
 * ~40 МГц: панель при такой скорости искажает данные. /8 = 10 МГц.
 */
#ifndef PURRGO_EINK_SPI_PRESCALER
#define PURRGO_EINK_SPI_PRESCALER SPI_BAUDRATEPRESCALER_8
#endif

/*
 * Частичных обновлений подряд до полного (сброс «шлейфов»): после стольких частичных
 * библиотека epd4g сама выполняет следующий запрос как полное обновление того же кадра.
 * Самотест на железе (сцены S7, S8: 40 частичных подряд, худший случай) следов не показал,
 * лимит 10 взят с запасом. По умолчанию значения библиотеки (EPD4G_PARTIALS_BEFORE_FULL,
 * EPD4G_BW_PARTIALS_BEFORE_FULL); переопределяются -D и применяются в display_eink_init().
 * Режим автоматического полного обновления в 4 gray - EPD4G_AUTO_FULL_MODE (по умолчанию
 * EPD4G_FULL_FAST, задаётся при сборке библиотеки), в 1 bit - всегда EPD4G_BW_NORMAL.
 */
#ifndef PURRGO_EINK_GRAY_PARTIALS_BEFORE_FULL
#define PURRGO_EINK_GRAY_PARTIALS_BEFORE_FULL EPD4G_PARTIALS_BEFORE_FULL
#endif
#ifndef PURRGO_EINK_MONO_PARTIALS_BEFORE_FULL
#define PURRGO_EINK_MONO_PARTIALS_BEFORE_FULL EPD4G_BW_PARTIALS_BEFORE_FULL
#endif

/* Полное обновление: 4 gray (FAST ~3 с, DEEP ~5 с) и 1 bit (NORMAL ~2 с, FAST ~1 с). */
#ifndef PURRGO_EINK_GRAY_FULL_MODE
#define PURRGO_EINK_GRAY_FULL_MODE EPD4G_FULL_FAST
#endif
#ifndef PURRGO_EINK_MONO_FULL_MODE
#define PURRGO_EINK_MONO_FULL_MODE EPD4G_BW_NORMAL
#endif

/*
 * Сколько панель простаивает до deep sleep, мс. Питание аналоговой части библиотека
 * отключает раньше сама (EPD4G_AUTO_POWEROFF_MS = 500 мс), так что к deep sleep
 * остаётся только команда 0x10.
 */
#ifndef PURRGO_EINK_SLEEP_AFTER_MS
#define PURRGO_EINK_SLEEP_AFTER_MS 1000U
#endif

/* Пауза перед повтором после ошибки и предел блокирующего ожидания, мс. */
#define EINK_RETRY_DELAY_MS 2000U
#define EINK_WAIT_LIMIT_MS  30000U

/* ------------------------------ состояние -------------------------------- */

typedef enum { OP_NONE, OP_INIT, OP_FULL, OP_PARTIAL, OP_SLEEP } eink_op_t;
typedef enum { HW_OFF, HW_GRAY, HW_MONO } eink_hw_t;

static const uint8_t *s_fb;
static uint8_t s_prev[EPD4G_FB_BYTES];   /* что сейчас на панели (только 4 gray) */

static display_mode_t s_target_mode = DISPLAY_MODE_GRAY4;

/* Запрос. */
static bool    s_req_full;
static bool    s_req_region;
static int16_t s_rx0, s_ry0, s_rx1, s_ry1;

/* Панель. */
static eink_hw_t s_hw = HW_OFF;                /* в каком режиме инициализирована */
static bool      s_mono_asleep;                /* 1 bit: в deep sleep, partial разбудит сам */
static bool      s_img_valid;                  /* на панели известная картинка режима s_img_mode */
static display_mode_t s_img_mode;

/* Выполняемая операция. */
static eink_op_t     s_op = OP_NONE;
static display_mode_t s_op_mode;
static uint32_t      s_idle_since;
static uint32_t      s_retry_at;
static bool          s_have_retry;

/* ------------------------------- helpers --------------------------------- */

static bool time_reached(uint32_t now, uint32_t t)
{
    return (int32_t)(now - t) >= 0;
}

static void invalidate_panel(void)
{
    s_hw = HW_OFF;
    s_mono_asleep = false;
    s_img_valid = false;
}

static bool request_pending(void)
{
    return s_req_full || s_req_region;
}

static void op_done(int rc, uint32_t now)
{
    const eink_op_t op = s_op;
    s_op = OP_NONE;
    s_idle_since = now;

    if (rc != EPD4G_OK) {
        PURRGO_LOGE("E-Ink: op %d failed, rc=%d, panel will be re-initialised\n\r", (int)op, rc);
        epd4g_abort();
        invalidate_panel();
        if (op == OP_FULL || op == OP_PARTIAL) {
            /* Кадр на панель не попал (флаги запроса сняты при старте): повторить полным. */
            s_req_full = true;
        }
        s_retry_at = now + EINK_RETRY_DELAY_MS;
        s_have_retry = true;
        return;
    }

    switch (op) {
        case OP_INIT:
            s_hw = (s_op_mode == DISPLAY_MODE_GRAY4) ? HW_GRAY : HW_MONO;
            s_mono_asleep = false;
            break;
        case OP_SLEEP:
            if (s_hw == HW_GRAY) {
                s_hw = HW_OFF;           /* после sleep 4 gray нужен init, картинка остаётся */
            } else if (s_hw == HW_MONO) {
                s_mono_asleep = true;    /* partial разбудит сам (аппаратный reset) */
            }
            break;
        default:
            break;
    }
}

/* Запуск операции; rc != OK - сразу ошибка, !busy после OK - завершилась мгновенно. */
static void op_started(eink_op_t op, display_mode_t mode, int rc, uint32_t now)
{
    s_op = op;
    s_op_mode = mode;
    if (rc != EPD4G_OK || !epd4g_is_busy()) {
        op_done(rc, now);
    }
}

static bool awake_for(display_mode_t mode)
{
    return (mode == DISPLAY_MODE_GRAY4) ? (s_hw == HW_GRAY)
                                        : (s_hw == HW_MONO && !s_mono_asleep);
}

/* Взять из очереди следующий шаг и запустить его. Библиотека свободна. */
static void start_next(uint32_t now)
{
    if (!request_pending()) {
        return;
    }
    if (s_have_retry && !time_reached(now, s_retry_at)) {
        return;
    }
    s_have_retry = false;

    const display_mode_t mode = s_target_mode;
    /* Лимит частичных подряд здесь не проверяется: его контролирует epd4g. */
    const bool need_full = s_req_full || !s_img_valid || s_img_mode != mode;

    if (need_full) {
        if (!awake_for(mode)) {
            op_started(OP_INIT, mode,
                       (mode == DISPLAY_MODE_GRAY4) ? epd4g_start_init()
                                                    : epd4g_start_bw_init(PURRGO_EINK_MONO_FULL_MODE),
                       now);
            return;
        }
        const int rc = (mode == DISPLAY_MODE_GRAY4)
                     ? epd4g_start_full(s_fb, PURRGO_EINK_GRAY_FULL_MODE)
                     : epd4g_start_bw_full(s_fb, EPD4G_SRC_2BPP, PURRGO_EINK_MONO_FULL_MODE);
        if (rc == EPD4G_OK) {
            /* Кадр уже зафиксирован: данные берутся из s_fb, пока data_busy. */
            if (mode == DISPLAY_MODE_GRAY4) {
                memcpy(s_prev, s_fb, sizeof(s_prev));
            }
            s_req_full = false;
            s_req_region = false;
            s_img_valid = true;
            s_img_mode = mode;
        }
        op_started(OP_FULL, mode, rc, now);
        return;
    }

    /* Частичное обновление: картинка этого режима на панели известна. */
    if (mode == DISPLAY_MODE_GRAY4) {
        if (!awake_for(mode)) {
            op_started(OP_INIT, mode, epd4g_start_init(), now);
            return;
        }
        uint32_t changed = 0;
        const int rc = epd4g_start_partial_diff(s_prev, s_fb, NULL, &changed);
        if (rc == EPD4G_OK) {
            s_req_region = false;
            if (changed != 0U) {
                /* Если лимит исчерпан, библиотека выполнит полное обновление этого же кадра:
                 * на панели в любом случае будет s_fb. */
                memcpy(s_prev, s_fb, sizeof(s_prev));
            }
            /* При changed == 0 библиотека ставит пустое задание (epd4g_start_noop()): оно
             * завершается в следующем epd4g_poll(). Его нужно довести до конца, иначе
             * библиотека останется занятой и следующий start вернёт EPD4G_ERR_BUSY. */
        }
        op_started(OP_PARTIAL, mode, rc, now);
    } else {
        const epd4g_rect_t r = { s_rx0, s_ry0, s_rx1, s_ry1 };
        const int rc = epd4g_start_bw_partial(s_fb, EPD4G_SRC_2BPP, &r);
        if (rc == EPD4G_OK) {
            s_req_region = false;
            s_mono_asleep = false;      /* частичное (или полное вместо него) будит панель само */
        }
        op_started(OP_PARTIAL, mode, rc, now);
    }
}

static void poll_core(bool allow_sleep)
{
    const uint32_t now = HAL_GetTick();

    /*
     * epd4g_poll() вызывается всегда, не только при своей операции: именно он запускает
     * автоматическое выключение питания панели после простоя. Это обычное задание
     * (~0,3 с, epd4g_is_busy() == 1, молчаливое: epd4g_result() не меняется), и пока оно
     * идёт, любой epd4g_start_*() вернул бы EPD4G_ERR_BUSY (в op_started() это ошибка).
     */
    epd4g_poll();
    if (epd4g_is_busy()) {
        return;
    }
    if (s_op != OP_NONE) {
        op_done(epd4g_result(), now);
    }

    start_next(now);
    if (s_op != OP_NONE) {
        return;
    }

    /* Простой: deep sleep панели (питание аналоговой части уже снял epd4g). */
    if (allow_sleep && !request_pending() && (s_hw != HW_OFF) && !s_mono_asleep &&
        time_reached(now, s_idle_since + PURRGO_EINK_SLEEP_AFTER_MS)) {
        op_started(OP_SLEEP, s_img_mode, epd4g_start_sleep(), now);
    }
}

/* -------------------------------- API ------------------------------------ */

void display_eink_init(const uint8_t *framebuffer)
{
    s_fb = framebuffer;

    /* Панель не принимает SPI на скорости, выбранной CubeMX для SPI1. */
    hspi1.Init.BaudRatePrescaler = PURRGO_EINK_SPI_PRESCALER;
    (void)HAL_SPI_DeInit(&hspi1);
    (void)HAL_SPI_Init(&hspi1);

    epd4g_set_partials_before_full(PURRGO_EINK_GRAY_PARTIALS_BEFORE_FULL,
                                   PURRGO_EINK_MONO_PARTIALS_BEFORE_FULL);

    memset(s_prev, 0, sizeof(s_prev));
    s_req_full = false;
    s_req_region = false;
    s_op = OP_NONE;
    s_have_retry = false;
    s_idle_since = HAL_GetTick();
    invalidate_panel();
}

void display_eink_set_mode(display_mode_t mode)
{
    if (mode != s_target_mode) {
        s_target_mode = mode;
        s_req_full = true;     /* смена режима панели - только полным обновлением */
    }
}

void display_eink_request_full(void)
{
    s_req_full = true;
}

void display_eink_request_region(int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    const int16_t x1 = (int16_t)(x + w - 1);
    const int16_t y1 = (int16_t)(y + h - 1);
    if (!s_req_region) {
        s_rx0 = x;  s_ry0 = y;  s_rx1 = x1;  s_ry1 = y1;
        s_req_region = true;
        return;
    }
    if (x < s_rx0)  { s_rx0 = x; }
    if (y < s_ry0)  { s_ry0 = y; }
    if (x1 > s_rx1) { s_rx1 = x1; }
    if (y1 > s_ry1) { s_ry1 = y1; }
}

void display_eink_poll(void)
{
    poll_core(true);
}

bool display_eink_data_busy(void)
{
    return epd4g_data_busy() != 0;
}

bool display_eink_is_busy(void)
{
    return s_op != OP_NONE || request_pending();
}

bool display_eink_can_stop(void)
{
    /*
     * Нет операции, запроса и отложенной повторной попытки; панель уже в deep
     * sleep (или не инициализирована) и библиотека не доделывает автоотключение
     * питания. Пока условие не выполнено, display_eink_poll() нужен по таймеру
     * (SysTick), поэтому Stop запрещён.
     */
    return s_op == OP_NONE && !request_pending() && !s_have_retry &&
           (s_hw == HW_OFF || s_mono_asleep) && !epd4g_is_busy();
}

void display_eink_set_partial_limits(unsigned gray, unsigned mono)
{
    epd4g_set_partials_before_full((gray != 0U) ? gray : PURRGO_EINK_GRAY_PARTIALS_BEFORE_FULL,
                                   (mono != 0U) ? mono : PURRGO_EINK_MONO_PARTIALS_BEFORE_FULL);
}

uint8_t display_eink_partial_count(void)
{
    const unsigned n = epd4g_partials_since_full();
    return (uint8_t)((n > 255U) ? 255U : n);
}

void display_eink_wait(void)
{
    const uint32_t start = HAL_GetTick();

    while (display_eink_is_busy()) {
        if (HAL_GetTick() - start > EINK_WAIT_LIMIT_MS) {
            PURRGO_LOGE("E-Ink: wait timeout, dropping pending update\n\r");
            epd4g_abort();
            s_op = OP_NONE;
            s_req_full = false;
            s_req_region = false;
            invalidate_panel();
            break;
        }
        poll_core(false);
        HAL_Delay(1);
    }
}

void display_eink_sleep(void)
{
    display_eink_wait();
    (void)epd4g_wait();        /* автоматическое выключение питания, если оно ещё идёт: иначе sleep вернёт ERR_BUSY */
    if (s_hw != HW_OFF && !s_mono_asleep) {
        (void)epd4g_sleep();   /* блокирующий, ошибка не критична: питание всё равно снимается */
        if (s_hw == HW_GRAY) {
            s_hw = HW_OFF;
        } else {
            s_mono_asleep = true;
        }
    }
}
