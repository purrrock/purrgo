/*
 * Host-тест учёта слоёв карты в map.c: какие файлы открываются, закрываются и в каком порядке
 * слои отрисовываются (по порядку чтения индексов).
 *
 * Файлы слоёв здесь - 32 нулевых байта: заголовок PGO невалиден, кадр возвращает false, зато
 * в журнале fake_fs (open / close / read) виден весь ход map_sync_files() и отрисовки. Разбор
 * и пиксели слоёв проверяет test_map.c.
 *
 * Тест описывает поведение, которое не должно меняться при добавлении слоя в таблицу слоёв:
 *   - порядок открытия и закрытия файлов, порядок отрисовки;
 *   - файл слоя, который не открылся, не открывается повторно на каждом кадре (до смены
 *     каталога или выключения слоя в конфиге);
 *   - подписи (.db) открываются только у открытого слоя; waterways включаются переключателем
 *     water, но от наличия water.* не зависят; при poi_mode == NO pois.idx не читается.
 */
#include <string.h>
#include "purrgo/map.h"
#include "purrgo/fs_hal.h"
#include "fake_fs.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

/* ----------------------------------------------------------- графика --- */

#define SCR_W 64
#define SCR_H 64
static uint8_t g_fb[SCR_W * SCR_H];

static void cb_draw(void *fb, int16_t x, int16_t y, gfx_color_t c)
{
    (void)fb; (void)x; (void)y; (void)c;
}
static gfx_color_t cb_read(void *fb, int16_t x, int16_t y)
{
    (void)fb; (void)x; (void)y;
    return 0;
}

static gfx_context_t g_gfx;
static purrgo_map_render_params_t g_params;
static const purrgo_bbox_t CAM = { 0, 0, 1000, 1000 };
static const purrgo_viewport_t VP = { SCR_W, SCR_H, 0, 0 };

/* ------------------------------------------------------------ помощники --- */

#define O(d, f) "open maps/" d "/" f
#define C(d, f) "close maps/" d "/" f
#define R(d, f) "read maps/" d "/" f

static void put(const char *dir, const char *name)
{
    uint8_t zeros[32] = {0};
    char path[160];

    (void)snprintf(path, sizeof path, "maps/%s/%s", dir, name);
    fake_fs_put_bin(path, zeros, sizeof zeros);
}

static void put_all(const char *dir)
{
    static const char *const names[] = {
        "landuse.idx", "landuse.mlp", "landuse.db",
        "water.idx", "water.mlp", "water.db",
        "waterways.idx", "waterways.mlp",
        "roads.idx", "roads.mlp",
        "pois.idx", "pois.db",
    };

    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        put(dir, names[i]);
    }
}

static void all_layers_on(void)
{
    memset(&g_params, 0, sizeof g_params);
    g_params.details = PURRGO_MAP_DETAILS_LOW;
    g_params.poi_mode = PURRGO_POI_MODE_CIRCLES;
    g_params.layer_landuse = true;
    g_params.layer_landuse_labels = true;
    g_params.layer_water = true;
    g_params.layer_water_labels = true;
    g_params.layer_roads = true;
    g_params.layer_poi = true;
    g_params.layer_poi_labels = true;
}

static bool render(const char *dir)
{
    g_params.map_dir = dir;
    fake_fs_log_reset();
    (void)gfx_init(&g_gfx, SCR_W, SCR_H, g_fb, cb_draw, cb_read);
    return purrgo_map_render_viewport(&g_gfx, &VP, &CAM, &g_params);
}

static void check_log(const char *const *exp, int n, int line)
{
    const int got = fake_fs_log_count();
    int ok = (got == n);

    for (int i = 0; ok && i < n; i++) {
        ok = (strcmp(fake_fs_log_at(i), exp[i]) == 0);
    }
    g_checks++;
    if (!ok) {
        g_failures++;
        printf("FAIL %s:%d: журнал ФС не совпал\n  ожидалось (%d):\n", __FILE__, line, n);
        for (int i = 0; i < n; i++) { printf("    %s\n", exp[i]); }
        printf("  получено (%d):\n", got);
        for (int i = 0; i < got; i++) { printf("    %s\n", fake_fs_log_at(i)); }
    }
}

#define EXPECT_LOG(...) do { \
    const char *const e_[] = { __VA_ARGS__ }; \
    check_log(e_, (int)(sizeof e_ / sizeof e_[0]), __LINE__); \
} while (0)
#define EXPECT_NO_EVENTS() check_log(NULL, 0, __LINE__)

/* Каждый слой читает индекс: порядок read - порядок отрисовки. */
#define READ_ALL(d) R(d, "landuse.idx"), R(d, "water.idx"), R(d, "waterways.idx"), \
                    R(d, "roads.idx"), R(d, "pois.idx")

/* Порядок закрытия при смене каталога и выключении карты. */
#define CLOSE_ALL(d) C(d, "landuse.idx"), C(d, "landuse.mlp"), C(d, "landuse.db"), \
                     C(d, "water.idx"), C(d, "water.mlp"), C(d, "water.db"), \
                     C(d, "waterways.idx"), C(d, "waterways.mlp"), \
                     C(d, "roads.idx"), C(d, "roads.mlp"), \
                     C(d, "pois.idx"), C(d, "pois.db")

/* ------------------------------------------------------------- тесты --- */

/*
 * Файлы слоя открываются подряд: сначала основные, затем подписи (.db), и только потом
 * следующий слой, поэтому water.db идёт раньше waterways.*.
 */
static void test_first_frame_and_repeat(void)
{
    purrgo_map_shutdown();
    fake_fs_reset();
    put_all("t");
    all_layers_on();

    /* Первый кадр: слои открываются сверху вниз; кадр с битыми заголовками - false. */
    CHECK(!render("maps/t"));
    EXPECT_LOG(O("t", "landuse.idx"), O("t", "landuse.mlp"), O("t", "landuse.db"),
               O("t", "water.idx"), O("t", "water.mlp"), O("t", "water.db"),
               O("t", "waterways.idx"), O("t", "waterways.mlp"),
               O("t", "roads.idx"), O("t", "roads.mlp"),
               O("t", "pois.idx"), O("t", "pois.db"),
               READ_ALL("t"));
    CHECK(fake_fs_open_handles() == 12);

    /* Повторный кадр: файлы уже открыты, только отрисовка. */
    CHECK(!render("maps/t"));
    EXPECT_LOG(READ_ALL("t"));
}

static void test_layer_toggles(void)
{
    /* Продолжение состояния: все слои открыты. */
    g_params.layer_landuse = false;
    CHECK(!render("maps/t"));
    EXPECT_LOG(C("t", "landuse.idx"), C("t", "landuse.mlp"), C("t", "landuse.db"),
               R("t", "water.idx"), R("t", "waterways.idx"), R("t", "roads.idx"), R("t", "pois.idx"));

    /* Подписи включены, но слой выключен: db не открывается. */
    CHECK(!render("maps/t"));
    EXPECT_LOG(R("t", "water.idx"), R("t", "waterways.idx"), R("t", "roads.idx"), R("t", "pois.idx"));

    g_params.layer_landuse = true;
    CHECK(!render("maps/t"));
    EXPECT_LOG(O("t", "landuse.idx"), O("t", "landuse.mlp"), O("t", "landuse.db"), READ_ALL("t"));

    /* Только подписи выключены / включены: закрывается и открывается один db. */
    g_params.layer_landuse_labels = false;
    CHECK(!render("maps/t"));
    EXPECT_LOG(C("t", "landuse.db"), READ_ALL("t"));
    g_params.layer_landuse_labels = true;
    CHECK(!render("maps/t"));
    EXPECT_LOG(O("t", "landuse.db"), READ_ALL("t"));

    /* Water: выключение закрывает и водоёмы, и водотоки. */
    g_params.layer_water = false;
    CHECK(!render("maps/t"));
    EXPECT_LOG(C("t", "water.idx"), C("t", "water.mlp"), C("t", "water.db"),
               C("t", "waterways.idx"), C("t", "waterways.mlp"),
               R("t", "landuse.idx"), R("t", "roads.idx"), R("t", "pois.idx"));
    g_params.layer_water = true;
    CHECK(!render("maps/t"));
    EXPECT_LOG(O("t", "water.idx"), O("t", "water.mlp"), O("t", "water.db"),
               O("t", "waterways.idx"), O("t", "waterways.mlp"),
               READ_ALL("t"));

    g_params.layer_water_labels = false;
    CHECK(!render("maps/t"));
    EXPECT_LOG(C("t", "water.db"), READ_ALL("t"));
    g_params.layer_water_labels = true;
    CHECK(!render("maps/t"));
    EXPECT_LOG(O("t", "water.db"), READ_ALL("t"));

    /* Roads. */
    g_params.layer_roads = false;
    CHECK(!render("maps/t"));
    EXPECT_LOG(C("t", "roads.idx"), C("t", "roads.mlp"),
               R("t", "landuse.idx"), R("t", "water.idx"), R("t", "waterways.idx"), R("t", "pois.idx"));
    g_params.layer_roads = true;
    CHECK(!render("maps/t"));
    EXPECT_LOG(O("t", "roads.idx"), O("t", "roads.mlp"), READ_ALL("t"));

    /* POI: при poi_mode == NO индекс открыт, но не читается. */
    g_params.poi_mode = PURRGO_POI_MODE_NO;
    CHECK(!render("maps/t"));
    EXPECT_LOG(R("t", "landuse.idx"), R("t", "water.idx"), R("t", "waterways.idx"), R("t", "roads.idx"));
    g_params.poi_mode = PURRGO_POI_MODE_CIRCLES;

    g_params.layer_poi_labels = false;
    CHECK(!render("maps/t"));
    EXPECT_LOG(C("t", "pois.db"), READ_ALL("t"));
    g_params.layer_poi_labels = true;
    CHECK(!render("maps/t"));
    EXPECT_LOG(O("t", "pois.db"), READ_ALL("t"));

    g_params.layer_poi = false;
    CHECK(!render("maps/t"));
    EXPECT_LOG(C("t", "pois.idx"), C("t", "pois.db"),
               R("t", "landuse.idx"), R("t", "water.idx"), R("t", "waterways.idx"), R("t", "roads.idx"));
    CHECK(!render("maps/t"));                      /* подписи включены, слой выключен */
    EXPECT_LOG(R("t", "landuse.idx"), R("t", "water.idx"), R("t", "waterways.idx"), R("t", "roads.idx"));
    g_params.layer_poi = true;
    CHECK(!render("maps/t"));
    EXPECT_LOG(O("t", "pois.idx"), O("t", "pois.db"), READ_ALL("t"));
}

static void test_dir_change_and_shutdown(void)
{
    /* Продолжение состояния: все слои каталога "t" открыты. Каталог "u" пуст. */
    CHECK(render("maps/u"));                        /* слоёв нет: не ошибка */
    EXPECT_LOG(CLOSE_ALL("t"),
               O("u", "landuse.idx"), O("u", "landuse.mlp"),
               O("u", "water.idx"), O("u", "water.mlp"),
               O("u", "waterways.idx"), O("u", "waterways.mlp"),
               O("u", "roads.idx"), O("u", "roads.mlp"),
               O("u", "pois.idx"));
    CHECK(fake_fs_open_handles() == 0);

    /* Отсутствующие файлы повторно не открываются. */
    CHECK(render("maps/u"));
    EXPECT_NO_EVENTS();

    /* Выключение слоя в конфиге снимает «нет файла»: после включения одна новая попытка. */
    g_params.layer_roads = false;
    CHECK(render("maps/u"));
    EXPECT_NO_EVENTS();
    g_params.layer_roads = true;
    CHECK(render("maps/u"));
    EXPECT_LOG(O("u", "roads.idx"), O("u", "roads.mlp"));

    /* purrgo_map_shutdown() закрывает всё и сбрасывает состояние. */
    fake_fs_reset();
    put_all("t");
    CHECK(!render("maps/t"));
    CHECK(fake_fs_open_handles() == 12);
    fake_fs_log_reset();
    purrgo_map_shutdown();
    EXPECT_LOG(CLOSE_ALL("t"));
    CHECK(fake_fs_open_handles() == 0);
}

static void test_too_long_dir(void)
{
    char dir[PURRGO_FS_MAX_PATH + 8];

    memset(dir, 'x', sizeof dir);
    dir[sizeof dir - 1u] = '\0';

    fake_fs_reset();
    put_all("t");
    CHECK(!render("maps/t"));
    CHECK(fake_fs_open_handles() == 12);

    CHECK(!render(dir));                            /* карта не рисуется, файлы закрыты */
    EXPECT_LOG(CLOSE_ALL("t"));
    CHECK(fake_fs_open_handles() == 0);
    CHECK(!render(dir));
    EXPECT_NO_EVENTS();

    CHECK(!render("maps/t"));                       /* нормальный путь: открывается заново */
    CHECK(fake_fs_open_handles() == 12);
    purrgo_map_shutdown();
}

static void test_missing_files(void)
{
    /* Обязательная пара слоя (landuse) только наполовину: оставшийся файл закрывается. */
    purrgo_map_shutdown();
    fake_fs_reset();
    all_layers_on();
    put("h", "landuse.idx");
    CHECK(render("maps/h"));
    EXPECT_LOG(O("h", "landuse.idx"), O("h", "landuse.mlp"), C("h", "landuse.idx"),
               O("h", "water.idx"), O("h", "water.mlp"),
               O("h", "waterways.idx"), O("h", "waterways.mlp"),
               O("h", "roads.idx"), O("h", "roads.mlp"),
               O("h", "pois.idx"));
    CHECK(fake_fs_open_handles() == 0);
    CHECK(render("maps/h"));
    EXPECT_NO_EVENTS();

    /* Необязательная пара (water) только наполовину - то же самое. */
    purrgo_map_shutdown();
    fake_fs_reset();
    put("w", "water.mlp");
    CHECK(render("maps/w"));
    EXPECT_LOG(O("w", "landuse.idx"), O("w", "landuse.mlp"),
               O("w", "water.idx"), O("w", "water.mlp"), C("w", "water.mlp"),
               O("w", "waterways.idx"), O("w", "waterways.mlp"),
               O("w", "roads.idx"), O("w", "roads.mlp"),
               O("w", "pois.idx"));
    CHECK(fake_fs_open_handles() == 0);

    /* Нет подписей: одна попытка, повторов нет; после выключения и включения - снова одна. */
    purrgo_map_shutdown();
    fake_fs_reset();
    put("l", "landuse.idx");
    put("l", "landuse.mlp");
    CHECK(!render("maps/l"));
    EXPECT_LOG(O("l", "landuse.idx"), O("l", "landuse.mlp"), O("l", "landuse.db"),
               O("l", "water.idx"), O("l", "water.mlp"),
               O("l", "waterways.idx"), O("l", "waterways.mlp"),
               O("l", "roads.idx"), O("l", "roads.mlp"),
               O("l", "pois.idx"),
               R("l", "landuse.idx"));
    CHECK(!render("maps/l"));
    EXPECT_LOG(R("l", "landuse.idx"));
    g_params.layer_landuse_labels = false;
    CHECK(!render("maps/l"));
    EXPECT_LOG(R("l", "landuse.idx"));
    g_params.layer_landuse_labels = true;
    CHECK(!render("maps/l"));
    EXPECT_LOG(O("l", "landuse.db"), R("l", "landuse.idx"));

    /* Водотоки включаются переключателем water, но от water.* не зависят; без water.* нет и water.db. */
    purrgo_map_shutdown();
    fake_fs_reset();
    put("ww", "waterways.idx");
    put("ww", "waterways.mlp");
    CHECK(!render("maps/ww"));
    EXPECT_LOG(O("ww", "landuse.idx"), O("ww", "landuse.mlp"),
               O("ww", "water.idx"), O("ww", "water.mlp"),
               O("ww", "waterways.idx"), O("ww", "waterways.mlp"),
               O("ww", "roads.idx"), O("ww", "roads.mlp"),
               O("ww", "pois.idx"),
               R("ww", "waterways.idx"));
    purrgo_map_shutdown();
    CHECK(fake_fs_open_handles() == 0);
}

int main(void)
{
    test_first_frame_and_repeat();
    test_layer_toggles();
    test_dir_change_and_shutdown();
    test_too_long_dir();
    test_missing_files();
    return test_finish("test_map_layers");
}
