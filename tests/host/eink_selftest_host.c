/*
 * Прогон встроенного самотеста частичного обновления (EinkTest_RunPartial) на хосте:
 * настоящие display_stm32.c + display_eink.c + epd4g + gfx ядра, панель заменена моделью
 * 1-bit RAM контроллера (две плоскости 0x24/0x26, окно, курсор, режим 2 «рисуются только
 * отличающиеся пиксели»). Когда драйвер простаивает, картинка модели в 1-bit режиме
 * сравнивается с тем, что должно быть по framebuffer'у. Расхождение = область обновления
 * не покрывает изменения (ошибка приложения, а не панели).
 *
 * Аргумент: autocopy=1 (по умолчанию) - идеальный контроллер копирует new -> old после
 * обновления; autocopy=0 - не копирует (пессимистичная модель, ожидаются расхождения).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "display_eink.h"
#include "display_stm32.h"
#include "eink_test.h"
#include "epd4g.h"
#include "epd4g_port.h"
#include "main.h"
#include "spi.h"

#define STRIDE 22
#define BWB    (STRIDE * 264)

static uint32_t now_ms;
static int dc, last_cmd = -1, args[8], nargs, wx0 = 0, wx1 = 21, wy0 = 0, wy1 = 263, cx, cy, last_ctrl;
static int cur_mode_gray, glass_known, updates, checked_updates, autocopy = 1;
static long mismatches, checks;
static uint32_t busy_until;
static uint8_t bwn[BWB], bwo[BWB], glass[BWB];

/* Текущий режим сцены (линкер подменяет display_set_mode, см. CMakeLists.txt): модель
 * 1-bit RAM сверяется с framebuffer'ом только в режиме 1 bit. */
void __real_display_set_mode(display_mode_t mode);
void __wrap_display_set_mode(display_mode_t mode);
void __wrap_display_set_mode(display_mode_t mode)
{
    cur_mode_gray = (mode == DISPLAY_MODE_GRAY4);
    __real_display_set_mode(mode);
}

SPI_HandleTypeDef hspi1;
int HAL_SPI_DeInit(SPI_HandleTypeDef *h) { (void)h; return 0; }
int HAL_SPI_Init(SPI_HandleTypeDef *h) { (void)h; return 0; }
uint32_t HAL_GetTick(void) { return now_ms; }

static void check_glass(void)
{
    if (!glass_known || cur_mode_gray || updates == checked_updates || display_eink_is_busy()) { return; }
    checked_updates = updates;
    const uint8_t *fb = display_get_framebuffer();
    long bad = 0; int fx = -1, fy = -1, bx0 = 999, by0 = 999, bx1 = -1, by1 = -1;
    for (int y = 0; y < 264; y++) {
        for (int x = 0; x < 176; x++) {
            const int idx = y * 176 + x;
            const int lvl = (fb[idx >> 2] >> ((3 - (idx & 3)) * 2)) & 3;
            const int want = (lvl >= 3) ? 1 : 0;
            const int got = (glass[y * STRIDE + (x >> 3)] >> (7 - (x & 7))) & 1;
            if (want != got) { if (bad < 6) printf("  px(%d,%d) lvl=%d want=%d got=%d\n", x, y, lvl, want, got); if (!bad) { fx = x; fy = y; } bad++; if (x < bx0) bx0 = x; if (x > bx1) bx1 = x; if (y < by0) by0 = y; if (y > by1) by1 = y; }
        }
    }
    checks++;
    if (bad) {
        mismatches++;
        printf("MISMATCH #%ld at t=%u ms: %ld pixels differ, first at x=%d y=%d, bbox x %d..%d y %d..%d (updates=%d last_ctrl=%02x)\n", mismatches, (unsigned)now_ms, bad, fx, fy, bx0, bx1, by0, by1, updates, last_ctrl);
    }
}

void HAL_Delay(uint32_t ms) { now_ms += ms; check_glass(); }

static void ram_write(uint8_t *ram, uint8_t v)
{
    if (cx >= 0 && cx < STRIDE && cy >= 0 && cy < 264) { ram[cy * STRIDE + cx] = v; }
    if (++cx > wx1) { cx = wx0; if (++cy > wy1) { cy = wy0; } }
}

static void update(void)
{
    /* power off (0x83) и загрузка температуры/LUT (0xB1, 0x91): картинка не меняется */
    if (last_ctrl == 0x83 || last_ctrl == 0xB1 || last_ctrl == 0x91) { return; }
    if (last_ctrl & 0x08) {
        for (int i = 0; i < BWB; i++) {
            const uint8_t d = (uint8_t)(bwn[i] ^ bwo[i]);
            glass[i] = (uint8_t)((glass[i] & ~d) | (bwn[i] & d));
        }
    } else {
        memcpy(glass, bwn, sizeof glass);
        glass_known = 1;
    }
    if (autocopy) { memcpy(bwo, bwn, sizeof bwo); }
    updates++;
}

/* Аппаратный сброс возвращает регистры окна и курсора к значениям по умолчанию (RAM сохраняется). */
static int prev_rst = 1;
void epd4g_port_gpio_rst(int l)
{
    if (prev_rst && !l) { wx0 = 0; wx1 = 21; wy0 = 0; wy1 = 263; cx = 0; cy = 0; }
    prev_rst = l;
}
void epd4g_port_gpio_dc(int l) { dc = l; }
void epd4g_port_gpio_cs(int l) { (void)l; }
int epd4g_port_gpio_busy(void) { return (int32_t)(now_ms - busy_until) < 0; }
void epd4g_port_delay_ms(uint32_t ms) { now_ms += ms; }
uint32_t epd4g_port_tick_ms(void) { return now_ms; }
int epd4g_port_spi_write(const uint8_t *d, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        const int v = d[i];
        if (!dc) {
            last_cmd = v; nargs = 0;
            if (v == 0x20) { update(); busy_until = now_ms + 800U; }
            if (v == 0x12) { busy_until = now_ms + 5U; }
        } else if (last_cmd == 0x24) { ram_write(bwn, (uint8_t)v);
        } else if (last_cmd == 0x26) { ram_write(bwo, (uint8_t)v);
        } else if (last_cmd == 0x22) { last_ctrl = v;
        } else if (last_cmd == 0x44 || last_cmd == 0x45 || last_cmd == 0x4E || last_cmd == 0x4F) {
            args[nargs < 8 ? nargs : 7] = v; nargs++;
            if (last_cmd == 0x44 && nargs == 2) { wx0 = args[0]; wx1 = args[1]; }
            if (last_cmd == 0x45 && nargs == 4) { wy0 = args[0] | (args[1] << 8); wy1 = args[2] | (args[3] << 8); }
            if (last_cmd == 0x4E && nargs == 1) { cx = args[0]; }
            if (last_cmd == 0x4F && nargs == 2) { cy = args[0] | (args[1] << 8); }
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1) { autocopy = atoi(argv[1]); }
    printf("eink selftest on host, controller copies new->old after update: %d\n", autocopy);
    EinkTest_RunPartial();
    printf("1-bit glass checks: %ld, mismatches: %ld\n", checks, mismatches);
    return mismatches ? 1 : 0;
}
