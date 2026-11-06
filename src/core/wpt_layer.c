#include "purrgo/wpt_layer.h"
#include "purrgo/config.h"
#include "purrgo/fs_hal.h"
#include "purrgo/gfx_icon.h"
#include "purrgo/gpx_parser.h"
#include "purrgo/logger.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/purrgo_poi_icons.h"
#include "map_projection.h"
#include <string.h>

_Static_assert(PURRGO_WPT_LAYER_ICON < PURRGO_POI_ICON_COUNT, "wpt layer icon out of range");

/* Размер блока чтения GPX с SD. */
#define WPT_LAYER_READ_CHUNK        512u

/* Больше этого размера файл не читается (защита от многомегабайтных GPX). */
#define WPT_LAYER_MAX_FILE_BYTES    (4u * 1024u * 1024u)

/* Подсказка HAL: читать файл блоками по 8 секторов (последовательное чтение). */
#define WPT_LAYER_READ_AHEAD_SECTORS 8u

static purrgo_waypoint_t s_pts[PURRGO_WPT_LAYER_MAX];
static size_t            s_count;
static bool              s_truncated;

/* Что загружено (для purrgo_wpt_layer_sync). */
static char              s_loaded_route[PURRGO_ROUTE_NAME_MAX];
static bool              s_loaded;       /* true - набор актуален, повторять не нужно */


/* ------------------------------------------------------------------ */
/*  Загрузка                                                            */
/* ------------------------------------------------------------------ */

/* Файл *.gpx (регистр расширения любой), имя помещается в PURRGO_ROUTE_NAME_MAX. */
static bool is_gpx_file(const purrgo_fs_dirent_t *entry)
{
    if (entry->is_directory || entry->name[0] == '.') {
        return false;
    }

    const size_t len = strlen(entry->name);

    if (len < 5u || len >= PURRGO_ROUTE_NAME_MAX) {
        return false;
    }

    const char *ext = entry->name + len - 4u;

    return ext[0] == '.' &&
           (ext[1] == 'g' || ext[1] == 'G') &&
           (ext[2] == 'p' || ext[2] == 'P') &&
           (ext[3] == 'x' || ext[3] == 'X');
}

/* Только имя файла: без путей и скрытых файлов, помещается в настройку. */
static bool route_name_is_valid(const char *name)
{
    if (name == NULL || name[0] == '\0' || name[0] == '.') {
        return false;
    }

    size_t len = 0;
    for (const char *p = name; *p != '\0'; p++, len++) {
        if (*p == '/' || *p == '\\' || *p == ':') {
            return false;
        }
    }
    return len < PURRGO_ROUTE_NAME_MAX;
}

/*
 * Дописывает <wpt> файла path в s_pts. true - файл открылся (даже если точек
 * в нём нет). Чтение прекращается, как только встретилась точка, которая уже
 * не помещается.
 */
static bool append_from_file(const char *path)
{
    purrgo_file_t *file = purrgo_fs_open(path, FS_READ);

    if (file == NULL) {
        return false;
    }
    purrgo_fs_set_read_ahead(file, (uint8_t)WPT_LAYER_READ_AHEAD_SECTORS);

    purrgo_gpx_parser_t parser;
    purrgo_gpx_parser_init(&parser, &s_pts[s_count], PURRGO_WPT_LAYER_MAX - s_count);

    char buf[WPT_LAYER_READ_CHUNK];
    uint32_t total = 0;

    for (;;) {
        const uint32_t n = purrgo_fs_read(file, (uint8_t *)buf, (uint32_t)sizeof buf);

        if (n == 0) {
            break;
        }
        purrgo_gpx_parser_feed(&parser, buf, n);

        if (parser.total_seen > parser.current_count) {
            break;      /* лишняя точка: дальше читать незачем */
        }
        total += n;
        if (total >= WPT_LAYER_MAX_FILE_BYTES) {
            PURRGO_LOGW("wpt layer: file too large, rest ignored\n\r");
            break;
        }
    }
    (void)purrgo_fs_close(file);

    s_count += parser.current_count;
    if (parser.total_seen > parser.current_count) {
        s_truncated = true;
    }
    return true;
}

/* true - файл маршрута открылся (маршрута нет или файл недоступен - false). */
static bool append_route(const char *route_file)
{
    if (!route_name_is_valid(route_file)) {
        return false;
    }

    char path[PURRGO_FS_MAX_PATH + PURRGO_ROUTE_NAME_MAX + 2];
    const int pn = purrgo_snprintf(path, sizeof path, "%s/%s",
                                   purrgo_fs_get_routes_path(), route_file);
    if (pn <= 0 || (size_t)pn >= sizeof path) {
        return false;
    }
    return append_from_file(path);
}

/* true - каталог WAYPTS прочитан (пустой тоже). */
static bool append_waypts_dir(void)
{
    const char *dir_path = purrgo_fs_get_waypts_path();
    purrgo_dir_t *dir = purrgo_fs_opendir(dir_path);

    if (dir == NULL) {
        return false;
    }

    purrgo_fs_dirent_t entry;

    while (purrgo_fs_readdir(dir, &entry)) {
        if (!is_gpx_file(&entry)) {
            continue;
        }
        if (s_count >= PURRGO_WPT_LAYER_MAX) {
            s_truncated = true;     /* есть ещё файл, а места нет */
            break;
        }

        char path[PURRGO_FS_MAX_PATH + PURRGO_ROUTE_NAME_MAX + 2];
        const int pn = purrgo_snprintf(path, sizeof path, "%s/%s", dir_path, entry.name);

        if (pn <= 0 || (size_t)pn >= sizeof path) {
            continue;
        }
        if (!append_from_file(path)) {
            PURRGO_LOGW("wpt layer: cannot open %s\n\r", path);
        }
    }
    purrgo_fs_closedir(dir);
    return true;
}

void purrgo_wpt_layer_clear(void)
{
    s_count = 0;
    s_truncated = false;
    s_loaded = false;
    s_loaded_route[0] = '\0';
}

bool purrgo_wpt_layer_sync(const char *route_file)
{
    const char *route = (route_file != NULL) ? route_file : "";

    if (s_loaded && strcmp(route, s_loaded_route) == 0) {
        return s_count > 0;
    }

    s_count = 0;
    s_truncated = false;

    /* Сначала маршрут: его точки остаются, если всё не помещается. */
    const bool route_ok = append_route(route);
    const bool dir_ok = append_waypts_dir();

    /*
     * Повтор нужен только если SD, судя по всему, не готова: не открылось
     * ничего - ни файл маршрута, ни каталог WAYPTS. Если хоть что-то
     * открылось, остальное (нет файла маршрута, нет каталога) - постоянное
     * состояние, и перечитывать SD при каждой перерисовке карты не нужно.
     */
    s_loaded = route_ok || dir_ok;
    purrgo_snprintf(s_loaded_route, sizeof s_loaded_route, "%.*s",
                    (int)(sizeof s_loaded_route - 1u), route);

    if (s_truncated) {
        PURRGO_LOGW("wpt layer: more than %u points, rest dropped\n\r",
                    (unsigned)PURRGO_WPT_LAYER_MAX);
    }
    PURRGO_LOG("wpt layer: %lu points (route=%d dir=%d)\n\r",
               (unsigned long)s_count, route_ok ? 1 : 0, dir_ok ? 1 : 0);

    return s_count > 0;
}

size_t purrgo_wpt_layer_count(void)
{
    return s_count;
}

const purrgo_waypoint_t *purrgo_wpt_layer_get(size_t index)
{
    return (index < s_count) ? &s_pts[index] : NULL;
}

bool purrgo_wpt_layer_truncated(void)
{
    return s_truncated;
}


/* ------------------------------------------------------------------ */
/*  Отрисовка                                                           */
/* ------------------------------------------------------------------ */

void purrgo_wpt_layer_render(gfx_context_t *gfx, const purrgo_bbox_t *camera,
                             const purrgo_viewport_t *vp)
{
    if (gfx == NULL || camera == NULL || vp == NULL || s_count == 0) {
        return;
    }

    /* Иконка 11x11 с центром в точке: запас до края окна - её полуразмер. */
    const int m = (int)(PURRGO_POI_ICON_WIDTH / 2u) + 1;

    for (size_t i = 0; i < s_count; i++) {
        int16_t sx, sy;

        project_to_screen(s_pts[i].lon_1e7, s_pts[i].lat_1e7, camera, vp, &sx, &sy);

        if (sx < vp->offset_x - m || sx > vp->offset_x + (int)vp->width + m ||
            sy < vp->offset_y - m || sy > vp->offset_y + (int)vp->height + m) {
            continue;
        }
        gfx_draw_icon_11x11(gfx, sx, sy, purrgo_poi_icons[PURRGO_WPT_LAYER_ICON]);
    }
}
