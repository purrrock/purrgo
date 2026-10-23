/*
 * Host-тест буферизованного чтения геометрии MLP (map_mlp.c):
 *   - все точки и границы частей совпадают с исходными при любом смещении
 *     объекта относительно границы сектора (512 Б);
 *   - обращений к ФС немного, и чтение не заходит в сектора за концом объекта
 *     (кроме остатка сектора, в котором объект заканчивается);
 *   - усечённый файл даёт отказ, а не мусорные точки.
 */
#include <stdlib.h>
#include <string.h>
#include "map_mlp.h"
#include "purrgo/fs_hal.h"
#include "fake_fs.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

/* Файл карты - файл фейковой ФС; статистику чтения ведёт fake_fs. */
static purrgo_file_t *open_blob(const uint8_t *data, uint32_t size)
{
    fake_fs_reset();
    fake_fs_put_bin("t.mlp", data, size);
    return purrgo_fs_open("t.mlp", FS_READ);
}

static void put_i32(uint8_t *p, int32_t v)
{
    uint32_t u = (uint32_t)v;
    p[0] = (uint8_t)u; p[1] = (uint8_t)(u >> 8); p[2] = (uint8_t)(u >> 16); p[3] = (uint8_t)(u >> 24);
}

#define MAXF 8192

/* Объект: 16 байт bbox, parts, points, затем parts[], затем точки. Возвращает размер объекта. */
static uint32_t build(uint8_t *file, uint32_t v1, int parts, int points, int32_t *xs, int32_t *ys)
{
    uint32_t base = 32u + v1;
    memset(file, 0xEE, MAXF);
    put_i32(&file[base + 16], parts);
    put_i32(&file[base + 20], points);
    uint32_t o = base + 24;
    for (int i = 0; i < parts; i++) {
        put_i32(&file[o], (int32_t)((uint32_t)i * (uint32_t)points / (uint32_t)parts));
        o += 4;
    }
    for (int i = 0; i < points; i++) {
        xs[i] = i * 1000 - 77777; ys[i] = -i * 31 + 5;
        put_i32(&file[o], xs[i]); put_i32(&file[o + 4], ys[i]);
        o += 8;
    }
    return o - base;
}

static void run_one(uint32_t v1, int parts, int points)
{
    static uint8_t file[MAXF];
    int32_t xs[PURRGO_MAP_MAX_POINTS], ys[PURRGO_MAP_MAX_POINTS];
    uint32_t objsz = build(file, v1, parts, points, xs, ys);
    uint32_t obj_end = 32u + v1 + objsz;
    purrgo_file_t *fs = open_blob(file, 32u + v1 + objsz + 100u);
    map_mlp_iter_t it;

    CHECK(fs != NULL);
    CHECK(map_mlp_iter_init(&it, fs, v1));
    int n = 0, new_parts = 0;
    int32_t x, y;
    bool np;
    while (map_mlp_iter_next(&it, &x, &y, &np)) {
        CHECK(n < points && x == xs[n] && y == ys[n]);
        if (np) { new_parts++; }
        n++;
    }
    CHECK(n == points);
    CHECK(new_parts == (parts > 1 ? parts - 1 : 0));

    /* Дальше конца сектора, в котором заканчивается объект, читать нельзя. */
    uint32_t sector_end = (obj_end + 511u) / 512u * 512u;
    CHECK(fake_fs_max_read_end() <= sector_end);
    /* Не больше чем «один сектор за раз» + начальный остаток. */
    CHECK(fake_fs_read_calls() <= objsz / 512u + 3u);
    (void)purrgo_fs_close(fs);
}

static void test_layouts(void)
{
    uint32_t offs[] = { 0, 1, 7, 8, 100, 440, 480, 488, 495, 511, 512, 1000 };
    int sizes[] = { 1, 2, 5, 63, 64, 65, 200, 511, 512 };
    for (size_t a = 0; a < sizeof offs / sizeof offs[0]; a++) {
        for (size_t b = 0; b < sizeof sizes / sizeof sizes[0]; b++) {
            int pts = sizes[b];
            int parts = pts >= 3 ? 3 : 1;
            run_one(offs[a], parts, pts);
        }
    }
}

static void test_truncated(void)
{
    static uint8_t file[MAXF];
    int32_t xs[PURRGO_MAP_MAX_POINTS], ys[PURRGO_MAP_MAX_POINTS];
    uint32_t objsz = build(file, 300, 2, 100, xs, ys);
    uint32_t full = 32u + 300u + objsz;

    for (uint32_t cut = 32u + 300u; cut < full; cut += 13u) {
        purrgo_file_t *fs = open_blob(file, cut);
        map_mlp_iter_t it;
        int32_t x, y; bool np;
        int n = 0;
        if (map_mlp_iter_init(&it, fs, 300)) {
            while (map_mlp_iter_next(&it, &x, &y, &np)) {
                CHECK(x == xs[n] && y == ys[n]);
                n++;
            }
            CHECK(n < 100);
        }
        CHECK(fake_fs_max_read_end() <= cut);
        (void)purrgo_fs_close(fs);
    }
}

int main(void)
{
    test_layouts();
    test_truncated();
    return test_finish("test_map_mlp_stream");
}
