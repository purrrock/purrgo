/*
 * Host-тест слоя очереди E-Ink (apps/stm32/common/Src/display_eink.c) поверх
 * настоящей библиотеки epd4g и простой модели панели за портом:
 * считаются сброс (reset), активации обновления (0x20), deep sleep (0x10/0x01)
 * и объём переданных SPI-данных. Время виртуальное.
 */
#include <stdio.h>
#include <string.h>

#include "display_eink.h"
#include "epd4g.h"
#include "epd4g_port.h"
#include "main.h"
#include "spi.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

/* ------------------------------ модель панели ----------------------------- */
static uint32_t now_ms;
static int      dc, rst_pulses, activations, sleeps, last_cmd = -1;
static long     spi_bytes;
static uint32_t busy_until;
static int      busy_stuck;
static int      prev_rst = 1;

SPI_HandleTypeDef hspi1;
int HAL_SPI_DeInit(SPI_HandleTypeDef *h) { (void)h; return 0; }
int HAL_SPI_Init(SPI_HandleTypeDef *h) { (void)h; return 0; }
uint32_t HAL_GetTick(void) { return now_ms; }
void HAL_Delay(uint32_t ms) { now_ms += ms; }

void epd4g_port_gpio_rst(int l) { if (prev_rst && !l) { rst_pulses++; } prev_rst = l; }
void epd4g_port_gpio_dc(int l) { dc = l; }
void epd4g_port_gpio_cs(int l) { (void)l; }
int epd4g_port_gpio_busy(void) { return busy_stuck || (int32_t)(now_ms - busy_until) < 0; }
void epd4g_port_delay_ms(uint32_t ms) { now_ms += ms; }
uint32_t epd4g_port_tick_ms(void) { return now_ms; }
int epd4g_port_spi_write(const uint8_t *d, size_t n)
{
    spi_bytes += (long)n;
    if (!dc) {
        for (size_t i = 0; i < n; i++) {
            last_cmd = d[i];
            if (d[i] == 0x20) { activations++; busy_until = now_ms + 800U; }
            if (d[i] == 0x12) { busy_until = now_ms + 5U; }
        }
    } else if (last_cmd == 0x10 && n == 1 && d[0] == 0x01) {
        sleeps++;
    }
    return 0;
}

/* ------------------------------- helpers ---------------------------------- */
static uint8_t fb[EPD4G_FB_BYTES];

static void settle(uint32_t max_ms)
{
    for (uint32_t i = 0; i < max_ms && display_eink_is_busy(); i++) {
        now_ms++;
        display_eink_poll();
    }
}

static void idle_for(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) { now_ms++; display_eink_poll(); }
}

static void set_px(int x, int y, uint8_t c)
{
    int idx = y * EPD4G_WIDTH + x, sh = (3 - (idx & 3)) * 2;
    fb[idx >> 2] = (uint8_t)((fb[idx >> 2] & ~(3 << sh)) | (c << sh));
}

typedef struct { int rst, act, slp; long bytes; } snap_t;
static snap_t snap(void) { snap_t s = { rst_pulses, activations, sleeps, spi_bytes }; return s; }

int main(void)
{
    memset(fb, 0xFF, sizeof(fb));
    display_eink_init(fb);
    CHECK(hspi1.Init.BaudRatePrescaler == SPI_BAUDRATEPRESCALER_8);

    /* 1. Первое обновление - полное, 1 bit. */
    display_eink_set_mode(DISPLAY_MODE_MONO);
    display_eink_request_full();
    snap_t a = snap();
    settle(20000);
    snap_t b = snap();
    CHECK(!display_eink_is_busy());
    CHECK(b.rst > a.rst);
    CHECK(b.act > a.act);

    /* 2. Частичное 1 bit: без init, одна активация, мало данных. */
    a = snap();
    for (int y = 100; y < 124; y++) for (int x = 10; x < 34; x++) set_px(x, y, 0);
    display_eink_request_region(10, 100, 24, 24);
    settle(20000);
    b = snap();
    CHECK(b.act == a.act + 1);
    CHECK(b.bytes - a.bytes < 1500);
    CHECK(display_eink_partial_count() == 1);

    /* 3. Запрос в 4 gray во время работы 1 bit: смена режима = init + полное. */
    display_eink_set_mode(DISPLAY_MODE_GRAY4);
    display_eink_request_full();
    a = snap();
    settle(20000);
    b = snap();
    CHECK(b.act > a.act);
    CHECK(b.rst > a.rst);               /* смена режима: init */
    CHECK(display_eink_partial_count() == 0);

    /* 4. Частичное 4 gray: только изменившиеся пиксели, заметно меньше полного. */
    a = snap();
    for (int y = 50; y < 60; y++) for (int x = 20; x < 30; x++) set_px(x, y, 0);
    display_eink_request_region(20, 50, 10, 10);
    settle(20000);
    b = snap();
    CHECK(b.act > a.act);
    CHECK(b.rst == a.rst);
    CHECK(display_eink_partial_count() == 1);

    /* 5. Тот же кадр ещё раз: изменений нет - панель не трогаем. */
    a = snap();
    display_eink_request_region(0, 0, EPD4G_WIDTH, EPD4G_HEIGHT);
    settle(20000);
    b = snap();
    CHECK(b.act == a.act);
    CHECK(!display_eink_is_busy());

    /* 6. Простой > 1 с: deep sleep; следующее частичное будит (init), но остаётся частичным. */
    a = snap();
    idle_for(4000);   /* 1 с простоя + power off + deep sleep (в модели ~1 с на каждое) */
    b = snap();
    CHECK(b.slp == a.slp + 1);
    a = snap();
    for (int y = 70; y < 80; y++) for (int x = 40; x < 50; x++) set_px(x, y, 1);
    display_eink_request_region(40, 70, 10, 10);
    settle(20000);
    b = snap();
    CHECK(b.rst > a.rst);                          /* разбудили: init */
    CHECK(display_eink_partial_count() == 2);      /* но обновление частичное */

    /* 7. Лимит частичных подряд (8): девятое - полное. */
    for (int i = 0; i < 6; i++) {
        set_px(100 + i, 200, (uint8_t)(i & 3));
        set_px(100 + i, 201, 0);
        display_eink_request_region(100, 200, 8, 2);
        settle(20000);
    }
    CHECK(display_eink_partial_count() == 8);
    set_px(150, 220, 0);
    display_eink_request_region(150, 220, 1, 1);
    settle(20000);
    CHECK(display_eink_partial_count() == 0);      /* выполнено полное */

    /* 8. data_busy снимается раньше конца волны: можно рендерить, запрос встаёт в очередь. */
    display_eink_request_full();
    int data_released_while_busy = 0, guard = 0;
    while (display_eink_is_busy() && guard++ < 20000) {
        now_ms++;
        display_eink_poll();
        if (!display_eink_data_busy() && display_eink_is_busy()) { data_released_while_busy = 1; break; }
    }
    CHECK(data_released_while_busy);
    set_px(1, 1, 0);                               /* рендер во время волны */
    display_eink_request_region(1, 1, 1, 1);       /* ставится в очередь */
    CHECK(display_eink_is_busy());
    a = snap();
    settle(20000);
    b = snap();
    CHECK(b.act >= a.act + 1);
    CHECK(!display_eink_is_busy());

    /* 9. Ошибка (BUSY завис): таймаут, потом восстановление init + полное. */
    a = snap();
    busy_stuck = 1;
    set_px(5, 5, 0);
    display_eink_request_region(5, 5, 1, 1);
    for (int i = 0; i < 12000; i++) { now_ms++; display_eink_poll(); }
    busy_stuck = 0;
    settle(40000);
    b = snap();
    CHECK(!display_eink_is_busy());
    CHECK(b.rst > a.rst);                          /* панель заново проинициализирована */
    CHECK(display_eink_partial_count() == 0);      /* и обновлена полным кадром */

    /* 10. Выключение: блокирующий sleep дожидается поставленного full и усыпляет. */
    display_eink_request_full();
    a = snap();
    display_eink_sleep();
    b = snap();
    CHECK(!display_eink_is_busy());
    CHECK(b.act > a.act);
    CHECK(b.slp >= a.slp + 1);

    if (g_fail == 0) { printf("test_display_eink: all passed\n"); return 0; }
    printf("test_display_eink: %d failures\n", g_fail);
    return 1;
}
