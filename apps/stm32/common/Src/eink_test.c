#include "eink_test.h"

#include "main.h"
#include "display_eink.h"
#include "display_stm32.h"
#include "purrgo/display_hal.h"
#include "purrgo/gfx_renderer.h"
#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_line.h"
#include "purrgo/gfx_circle.h"
#include "purrgo/logger.h"
#include "epd4g.h"
#include "epd4g_bw.h"

#include <stddef.h>

/* Геометрия панели (epd4g): 176 x 264, 2 бита на пиксель = 11616 байт. */
#define EINK_TEST_WIDTH   EPD4G_WIDTH
#define EINK_TEST_HEIGHT   EPD4G_HEIGHT
#define EINK_TEST_FB  EPD4G_FB_BYTES

/*
 * Тест 1-bit режима библиотеки напрямую (без слоя display_eink):
 * чередующиеся горизонтальные полосы, 1 бит на пиксель (1 = белый).
 * Проверяет только передачу данных и DC/CS/SPI, SPI-скорость выставляет
 * display_eink_init().
 */
void EinkTest_Run(void)
{
    static uint8_t image[EPD4G_BW_FB_BYTES];

    display_eink_init(NULL);

    for (uint32_t y = 0; y < EINK_TEST_HEIGHT; y++)
    {
        const uint8_t value = ((y / 32U) & 1U) ? 0x00U : 0xFFU;

        for (uint32_t x = 0; x < EPD4G_BW_STRIDE; x++)
        {
            image[y * EPD4G_BW_STRIDE + x] = value;
        }
    }

    if (epd4g_bw_init(EPD4G_BW_NORMAL) == EPD4G_OK)
    {
        (void)epd4g_bw_full(image, EPD4G_SRC_1BPP, EPD4G_BW_NORMAL);
    }
    (void)epd4g_sleep();
}


/*
 * Логические цвета PurrGO: 0 = чёрный, 1 = тёмно-серый,
 * 2 = светло-серый, 3 = белый.
 */
static void eink_set_px2(uint8_t *fb, int width, int x, int y, uint8_t c)
{
    int idx   = y * width + x;
    int shift = (3 - (idx & 3)) << 1;

    fb[idx >> 2] = (uint8_t)((fb[idx >> 2] & ~(0x03 << shift)) | ((c & 0x03) << shift));
}

void EinkTest_FillPattern4Gray(uint8_t *fb2bpp, int width, int height)
{
    const int band_h = 24;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            uint8_t c;

            if (y < band_h * 4)
            {
                /* 4 сплошные полосы: проверка всех уровней серого. */
                c = (uint8_t)(y / band_h);
            }
            else if (y < band_h * 5)
            {
                /* Вертикальные полосы 1 px: максимальная плотность переходов. */
                c = (x & 1) ? 3U : 0U;
            }
            else if (y < band_h * 6)
            {
                /* Шахматка 2x2. */
                c = (((x >> 1) + (y >> 1)) & 1) ? 3U : 0U;
            }
            else if (y < band_h * 7)
            {
                /* Горизонтальный градиент из 4 уровней. */
                c = (uint8_t)((x * 4) / width);
            }
            else
            {
                /* Рамка по краю экрана, остальное белое. */
                c = (x < 2 || y < 2 || x >= width - 2 || y >= height - 2) ? 0U : 3U;
            }

            eink_set_px2(fb2bpp, width, x, y, c);
        }
    }
}

void EinkTest_Run4Gray(void)
{
    static uint8_t fb[EINK_TEST_FB];

    EinkTest_FillPattern4Gray(fb, EINK_TEST_WIDTH, EINK_TEST_HEIGHT);

    /* Тот же путь, что у приложения: очередь display_eink -> epd4g. */
    display_eink_init(fb);
    display_eink_set_mode(DISPLAY_MODE_GRAY4);
    display_eink_request_full();
    display_eink_sleep();                /* дожидается обновления и усыпляет панель */
}


/*
 * ===========================================================================
 * Самотест частичного обновления: имитация меню и движения маркёра по карте
 * ===========================================================================
 *
 * Тест идёт через тот же путь, что и приложение: display_init(), gfx с
 * callback'ами display_stm32, display_set_mode(), display_refresh_region() +
 * display_flush(), неблокирующий драйвер E-Ink. Сцены:
 *
 *   S1  1-bit  МЕНЮ  области как в приложении (старая + новая строка), пауза 0,6 с
 *   S2  1-bit  МЕНЮ  те же области, пауза 2,5 с: панель засыпает между шагами
 *   S3  1-bit  МЕНЮ  частичное обновление ВСЕГО экрана, пауза 2,5 с
 *   S4  4 gray МЕНЮ  обновление по разнице кадров, пауза 2,5 с
 *   S5  1-bit  КАРТА маркёр + трек, область вокруг старой и новой позиции, пауза 0,6 с
 *   S6  4 gray КАРТА маркёр + трек, обновление по разнице кадров, пауза 2,5 с
 *   S7  1-bit  ГОСТ  40 частичных подряд: курсор прыгает между двумя строками меню (худший случай
 *                    для остаточного изображения), контрольные точки после 10/20/30/40 шагов
 *   S8  4 gray ГОСТ  40 частичных подряд: маркёр ходит по пути вперёд-назад (трек то растёт, то
 *                    стирается), контрольные точки после 10/20/30/40 шагов
 *
 * S7 и S8 не входят в набор по умолчанию (маска 0xC0U, -DPURRGO_EINK_TEST_SCENES=0xC0U): по ним
 * выбирают, сколько частичных обновлений подряд допустимо до полного
 * (PURRGO_EINK_*_PARTIALS_BEFORE_FULL). В контрольной точке
 * тест 4 с держит кадр и пишет "checkpoint N partials": сфотографировать экран, в конце сцены
 * полное обновление того же кадра - эталон без остаточного изображения.
 *
 * Каждая сцена: полное обновление начального кадра, 10 шагов частичных обновлений,
 * выдержка 3 с, затем полное обновление того же кадра (эталон) и выдержка 3 с.
 * Лимит «частичных подряд до полного» на время теста отключён, чтобы артефакты
 * накапливались (в приложении после PURRGO_EINK_*_PARTIALS_BEFORE_FULL частичных идёт полное).
 * Ход теста пишется в UART1 (строки "EINKTEST ...").
 */

/* Битовая маска сцен S1..S8. По умолчанию S1..S6 (меню и карта), S7 и S8 - по запросу: 0xC0U. */
#ifndef PURRGO_EINK_TEST_SCENES
#define PURRGO_EINK_TEST_SCENES 0x3FU
#endif

#define ET_STEPS        10
#define ET_GHOST_STEPS  40
#define ET_GHOST_EVERY  10
#define ET_CHECK_MS     4000U
#define ET_GAP_SHORT_MS 600U
#define ET_GAP_LONG_MS  2500U
#define ET_HOLD_MS      3000U

#define ET_MENU_ROWS    7
#define ET_ROW_Y0       25
#define ET_ROW_STEP     15

static gfx_context_t s_ctx;

/* Пауза с опросом драйвера: так же засыпает панель, как в главном цикле. */
static void et_delay(uint32_t ms)
{
    /* По HAL_GetTick(), а не счётчиком HAL_Delay(1): HAL_Delay(1) длится 1-2 мс (HAL добавляет тик). */
    const uint32_t t0 = HAL_GetTick();

    while (HAL_GetTick() - t0 < ms)
    {
        display_stm32_poll();
        HAL_Delay(1);
    }
}

static void et_gfx_init(void)
{
    (void)gfx_init(&s_ctx, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                   (void *)display_get_framebuffer(),
                   stm32_draw_pixel_cb, stm32_read_pixel_cb);
    gfx_set_clear_callback(&s_ctx, stm32_clear_cb);
    gfx_set_fill_span_callback(&s_ctx, stm32_fill_span_cb);
}

static const char *const et_menu_text[ET_MENU_ROWS] = {
    "TZ: UTC+03:00", "DIR: /MAPS/BEL", "POI: ALL", "LOG: ON",
    "MAP DETAIL: HIGH", "MAP LAYERS", "POWER OFF"
};

/* Меню как экран CONFIG: заголовок, 7 строк через 15 px, выбранная - белым по чёрному. */
static void et_draw_menu(const char *title, int cursor)
{
    gfx_set_color(&s_ctx, COLOR_BLACK, COLOR_WHITE);
    gfx_clear(&s_ctx);
    gfx_draw_string(&s_ctx, 10, 10, title);

    for (int i = 0; i < ET_MENU_ROWS; i++)
    {
        if (i == cursor) { gfx_set_color(&s_ctx, COLOR_WHITE, COLOR_BLACK); }
        else             { gfx_set_color(&s_ctx, COLOR_BLACK, COLOR_WHITE); }
        gfx_draw_string(&s_ctx, 10, (int16_t)(ET_ROW_Y0 + i * ET_ROW_STEP), et_menu_text[i]);
    }

    gfx_set_color(&s_ctx, COLOR_BLACK, COLOR_WHITE);
    gfx_draw_string(&s_ctx, 10, 135, "UP/DN: Select");
    gfx_draw_string(&s_ctx, 10, 150, "SEL  : Change");
    gfx_draw_string(&s_ctx, 10, 165, "BACK : Save");
}

/* Серая заливка: в 4 gray настоящий серый, в 1 bit - шахматный растр (серый стал бы чёрным). */
static void et_fill_gray(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t level, bool mono)
{
    if (!mono)
    {
        gfx_set_color(&s_ctx, level, COLOR_WHITE);
        gfx_fill_rect(&s_ctx, x, y, w, h);
        return;
    }
    /* светло-серый: 1 пиксель из 4 чёрный, тёмно-серый: 3 из 4 */
    gfx_set_color(&s_ctx, COLOR_WHITE, COLOR_WHITE);
    gfx_fill_rect(&s_ctx, x, y, w, h);
    gfx_set_color(&s_ctx, COLOR_BLACK, COLOR_WHITE);
    for (int16_t yy = y; yy < y + h; yy++)
    {
        for (int16_t xx = x; xx < x + w; xx++)
        {
            const int k = (xx & 1) + 2 * (yy & 1);              /* 0..3 */
            const bool black = (level == COLOR_DARK_GRAY) ? (k != 0) : (k == 0);
            if (black) { gfx_draw_pixel(&s_ctx, xx, yy); }
        }
    }
}

typedef struct { int16_t x, y; } et_pt_t;

/* Путь маркёра: 11 точек (старт + ET_STEPS шагов) по плавной кривой через участок карты. */
static const et_pt_t et_path[ET_STEPS + 1] = {
    { 20, 225 }, { 34, 208 }, { 50, 196 }, { 68, 192 }, { 86, 186 }, { 100, 170 },
    { 108, 150 }, { 116, 130 }, { 128, 112 }, { 142, 98 }, { 152, 80 }
};

/* Карта: фон, леса и вода (серые), дороги, строка состояния, трек до точки idx и маркёр. */
static void et_draw_map(const char *title, int idx, bool mono)
{
    gfx_set_color(&s_ctx, COLOR_BLACK, COLOR_WHITE);
    gfx_clear(&s_ctx);

    et_fill_gray(8, 40, 70, 55, COLOR_LIGHT_GRAY, mono);        /* лес */
    et_fill_gray(100, 60, 66, 70, COLOR_DARK_GRAY, mono);       /* водоём */
    et_fill_gray(20, 120, 60, 40, COLOR_LIGHT_GRAY, mono);      /* лес */

    gfx_set_color(&s_ctx, COLOR_BLACK, COLOR_WHITE);
    gfx_draw_line(&s_ctx, 0, 250, 176, 170);                    /* дороги */
    gfx_draw_line(&s_ctx, 0, 100, 176, 112);
    gfx_draw_line(&s_ctx, 90, 20, 70, 264);
    gfx_draw_dashed_line(&s_ctx, 0, 180, 176, 215);

    gfx_draw_rect(&s_ctx, 0, 0, 176, 14);                       /* верхняя строка: заголовок и масштаб */
    gfx_draw_string(&s_ctx, 3, 3, title);

    for (int i = 0; i < idx; i++)                               /* трек */
    {
        gfx_draw_thick_line(&s_ctx, et_path[i].x, et_path[i].y,
                            et_path[i + 1].x, et_path[i + 1].y, 2);
    }

    gfx_set_color(&s_ctx, COLOR_BLACK, COLOR_WHITE);            /* маркёр: чёрный круг с белой точкой */
    gfx_fill_circle(&s_ctx, et_path[idx].x, et_path[idx].y, 6);
    gfx_set_color(&s_ctx, COLOR_WHITE, COLOR_BLACK);
    gfx_fill_circle(&s_ctx, et_path[idx].x, et_path[idx].y, 2);
}

/* Область, охватывающая старую и новую позиции маркёра (с запасом под круг и линию трека). */
static void et_region_marker(int from, int to)
{
    int16_t x0 = et_path[from].x, x1 = et_path[to].x;
    int16_t y0 = et_path[from].y, y1 = et_path[to].y;
    if (x0 > x1) { const int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { const int16_t t = y0; y0 = y1; y1 = t; }
    display_refresh_region((int16_t)(x0 - 9), (int16_t)(y0 - 9),
                           (int16_t)(x1 - x0 + 19), (int16_t)(y1 - y0 + 19));
}

/* Один шаг частичного обновления: ждём конца обновления, пауза, и в лог время и счётчик. */
static void et_step_done(int scene, int step)
{
    const uint32_t t0 = HAL_GetTick();
    display_flush();
    display_eink_wait();
    PURRGO_LOG("EINKTEST S%d step %d/%d done in %lu ms, partials in a row=%u\r\n",
               scene, step, ET_STEPS, (unsigned long)(HAL_GetTick() - t0),
               (unsigned)display_eink_partial_count());
}

/* Общее начало и конец сцены. */
static void et_scene_begin(int scene, display_mode_t mode)
{
    PURRGO_LOG("EINKTEST S%d start (mode %s)\r\n", scene,
               (mode == DISPLAY_MODE_GRAY4) ? "4gray" : "1bit");
    display_set_mode(mode);
}

static void et_scene_full(void)
{
    PURRGO_LOG("EINKTEST full refresh requested\r\n");
    display_refresh();                 /* полное обновление: на панель уходит кадр из framebuffer */
    display_eink_wait();
}

static void et_scene_end(int scene)
{
    et_delay(ET_HOLD_MS);              /* состояние после частичных обновлений: смотреть и снимать */
    PURRGO_LOG("EINKTEST S%d reference full refresh\r\n", scene);
    et_scene_full();                   /* тот же кадр полным обновлением: эталон */
    et_delay(ET_HOLD_MS);
}

/* Меню: курсор 0 -> 1..6 -> 0 (переход через край) -> 6 -> 3 -> 1. */
static void et_scene_menu(int scene, display_mode_t mode, bool whole_screen, uint32_t gap_ms,
                          const char *title)
{
    static const int cursor_seq[ET_STEPS + 1] = { 0, 1, 2, 3, 4, 5, 6, 0, 6, 3, 1 };

    et_scene_begin(scene, mode);
    et_draw_menu(title, cursor_seq[0]);
    et_scene_full();
    et_delay(gap_ms);

    for (int step = 1; step <= ET_STEPS; step++)
    {
        const int prev = cursor_seq[step - 1];
        const int cur  = cursor_seq[step];

        et_draw_menu(title, cur);
        if (whole_screen)
        {
            display_refresh_region(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        }
        else
        {
            /* как ui_config.c: строка, с которой ушёл курсор, и строка, на которую пришёл */
            display_refresh_region(0, (int16_t)(ET_ROW_Y0 + prev * ET_ROW_STEP), DISPLAY_WIDTH, ET_ROW_STEP);
            display_refresh_region(0, (int16_t)(ET_ROW_Y0 + cur * ET_ROW_STEP), DISPLAY_WIDTH, ET_ROW_STEP);
        }
        et_step_done(scene, step);
        et_delay(gap_ms);
    }
    et_scene_end(scene);
}

/* Карта: маркёр движется по пути, за ним тянется трек. */
static void et_scene_map(int scene, display_mode_t mode, uint32_t gap_ms, const char *title)
{
    const bool mono = (mode == DISPLAY_MODE_MONO);

    et_scene_begin(scene, mode);
    et_draw_map(title, 0, mono);
    et_scene_full();
    et_delay(gap_ms);

    for (int step = 1; step <= ET_STEPS; step++)
    {
        et_draw_map(title, step, mono);
        et_region_marker(step - 1, step);
        et_step_done(scene, step);
        et_delay(gap_ms);
    }
    et_scene_end(scene);
}

/* Контрольная точка сцены остаточного изображения: держим кадр, снимаем экран на фото. */
static void et_ghost_checkpoint(int scene, int step)
{
    if ((step % ET_GHOST_EVERY) == 0)
    {
        PURRGO_LOG("EINKTEST S%d checkpoint %d partials\r\n", scene, step);
        et_delay(ET_CHECK_MS);
    }
}

/* S7: курсор меню прыгает между строками 2 и 3: одни и те же пиксели меняются на каждом шаге. */
static void et_scene_ghost_menu(int scene, display_mode_t mode, const char *title)
{
    et_scene_begin(scene, mode);
    et_draw_menu(title, 2);
    et_scene_full();
    et_delay(ET_GAP_SHORT_MS);

    for (int step = 1; step <= ET_GHOST_STEPS; step++)
    {
        const int cur  = 2 + (step & 1);
        const int prev = 2 + ((step - 1) & 1);

        et_draw_menu(title, cur);
        display_refresh_region(0, (int16_t)(ET_ROW_Y0 + prev * ET_ROW_STEP), DISPLAY_WIDTH, ET_ROW_STEP);
        display_refresh_region(0, (int16_t)(ET_ROW_Y0 + cur * ET_ROW_STEP), DISPLAY_WIDTH, ET_ROW_STEP);
        et_step_done(scene, step);
        et_delay(ET_GAP_SHORT_MS);
        et_ghost_checkpoint(scene, step);
    }
    et_scene_end(scene);
}

/* S8: маркёр идёт по пути вперёд-назад (индекс 0..10..0..), трек растёт и стирается. */
static void et_scene_ghost_map(int scene, display_mode_t mode, const char *title)
{
    const bool mono = (mode == DISPLAY_MODE_MONO);
    int idx = 0;

    et_scene_begin(scene, mode);
    et_draw_map(title, idx, mono);
    et_scene_full();
    et_delay(ET_GAP_SHORT_MS);

    for (int step = 1; step <= ET_GHOST_STEPS; step++)
    {
        const int prev = idx;
        const int k = step % (2 * ET_STEPS);

        idx = (k <= ET_STEPS) ? k : (2 * ET_STEPS - k);
        et_draw_map(title, idx, mono);
        et_region_marker(prev, idx);
        et_step_done(scene, step);
        et_delay(ET_GAP_SHORT_MS);
        et_ghost_checkpoint(scene, step);
    }
    et_scene_end(scene);
}

void EinkTest_RunPartial(void)
{
    display_init();
    et_gfx_init();
    display_eink_set_partial_limits(250U, 250U);

    const unsigned mask = PURRGO_EINK_TEST_SCENES;

    if (mask & 0x01U) { et_scene_menu(1, DISPLAY_MODE_MONO,  false, ET_GAP_SHORT_MS, "S1 1BIT MENU AWAKE"); }
    if (mask & 0x02U) { et_scene_menu(2, DISPLAY_MODE_MONO,  false, ET_GAP_LONG_MS,  "S2 1BIT MENU SLEEP"); }
    if (mask & 0x04U) { et_scene_menu(3, DISPLAY_MODE_MONO,  true,  ET_GAP_LONG_MS,  "S3 1BIT MENU WHOLE"); }
    if (mask & 0x08U) { et_scene_menu(4, DISPLAY_MODE_GRAY4, false, ET_GAP_LONG_MS,  "S4 4GRAY MENU"); }
    if (mask & 0x10U) { et_scene_map (5, DISPLAY_MODE_MONO,  ET_GAP_SHORT_MS, "S5 1BIT MAP"); }
    if (mask & 0x20U) { et_scene_map (6, DISPLAY_MODE_GRAY4, ET_GAP_LONG_MS,  "S6 4GRAY MAP"); }
    if (mask & 0x40U) { et_scene_ghost_menu(7, DISPLAY_MODE_MONO,  "S7 1BIT GHOST"); }
    if (mask & 0x80U) { et_scene_ghost_map (8, DISPLAY_MODE_GRAY4, "S8 4GRAY GHOST"); }

    PURRGO_LOG("EINKTEST done\r\n");
    display_eink_set_partial_limits(0U, 0U);
    display_sleep();
}
