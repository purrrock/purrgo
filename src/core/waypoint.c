#include "purrgo/waypoint.h"
#include "purrgo/config.h"
#include "purrgo/fs_hal.h"
#include "purrgo/gfx_icon.h"
#include "purrgo/gpx_parser.h"
#include "purrgo/logger.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/purrgo_poi_icons.h"
#include "map_projection.h"
#include <string.h>

_Static_assert(PURRGO_WPT_TARGET_ICON < PURRGO_POI_ICON_COUNT, "wpt target icon out of range");

/* Размер блока чтения GPX с SD. */
#define WPT_READ_CHUNK      512u

/* Больше этого размера файл не читается (защита от многомегабайтных GPX). */
#define WPT_MAX_FILE_BYTES  (4u * 1024u * 1024u)

/* Цель, собранная из app_config (возвращается purrgo_wpt_target()). */
static purrgo_waypoint_t s_target;

static purrgo_wpt_nav_t s_nav;

static purrgo_waypoint_t s_list[PURRGO_WPT_LIST_MAX];
static size_t s_list_count;
static bool s_list_truncated;


/* ------------------------------------------------------------------ */
/*  Цель                                                                */
/* ------------------------------------------------------------------ */

bool purrgo_wpt_is_set(void)
{
    return app_config.wpt_valid;
}

const purrgo_waypoint_t *purrgo_wpt_target(void)
{
    if (!app_config.wpt_valid) {
        return NULL;
    }

    s_target.lat_1e7 = app_config.wpt_lat_1e7;
    s_target.lon_1e7 = app_config.wpt_lon_1e7;
    s_target.ele_m = 0;
    purrgo_snprintf(s_target.name, sizeof s_target.name, "%s", app_config.wpt_name);
    return &s_target;
}

/* Копирует имя без управляющих символов (строка конфига однострочная), с обрезкой по границе UTF-8. */
static void copy_name(char *dst, size_t size, const char *src)
{
    size_t n = 0;

    if (src != NULL) {
        for (; *src != '\0' && n + 1 < size; src++) {
            const unsigned char c = (unsigned char)*src;
            dst[n++] = (c < 0x20u) ? ' ' : (char)c;
        }
    }
    dst[n] = '\0';

    /* Обрезанная последовательность UTF-8 в конце: отбросить. */
    size_t i = n;
    while (i > 0 && n - i < 3 && ((unsigned char)dst[i - 1] & 0xC0u) == 0x80u) {
        i--;
    }
    if (i > 0) {
        const unsigned char lead = (unsigned char)dst[i - 1];
        size_t need = 1;
        if ((lead & 0xE0u) == 0xC0u) {
            need = 2;
        } else if ((lead & 0xF0u) == 0xE0u) {
            need = 3;
        } else if ((lead & 0xF8u) == 0xF0u) {
            need = 4;
        }
        if (n - (i - 1) < need) {
            dst[i - 1] = '\0';
        }
    }

    /* Пробелы по краям не нужны. */
    size_t len = strlen(dst);
    while (len > 0 && dst[len - 1] == ' ') {
        dst[--len] = '\0';
    }
}

bool purrgo_wpt_set(int32_t lat_1e7, int32_t lon_1e7, const char *name)
{
    if (lat_1e7 < -900000000 || lat_1e7 > 900000000 ||
        lon_1e7 < -1800000000 || lon_1e7 > 1800000000) {
        return false;
    }

    app_config.wpt_valid = true;
    app_config.wpt_lat_1e7 = lat_1e7;
    app_config.wpt_lon_1e7 = lon_1e7;
    copy_name(app_config.wpt_name, sizeof app_config.wpt_name, name);
    if (app_config.wpt_name[0] == '\0') {
        purrgo_snprintf(app_config.wpt_name, sizeof app_config.wpt_name, "WPT");
    }

    memset(&s_nav, 0, sizeof s_nav);
    return true;
}

void purrgo_wpt_clear(void)
{
    app_config.wpt_valid = false;
    app_config.wpt_lat_1e7 = 0;
    app_config.wpt_lon_1e7 = 0;
    app_config.wpt_name[0] = '\0';
    memset(&s_nav, 0, sizeof s_nav);
}


/* ------------------------------------------------------------------ */
/*  Запись точки в WAYPTS                                               */
/* ------------------------------------------------------------------ */

/* Файлы точек: MP + 6 цифр + .GPX (8.3, без длинных имён FAT). */
#define WPT_SAVE_PREFIX     "MP"
#define WPT_SAVE_DIGITS     6u
#define WPT_SAVE_MAX_INDEX  999999u

/* Номер из имени вида MPnnnnnn.GPX (регистр любой). false - имя другое. */
static bool saved_file_index(const char *name, uint32_t *index_out)
{
    if (name == NULL || strlen(name) != 2u + WPT_SAVE_DIGITS + 4u) {
        return false;
    }
    if ((name[0] != 'M' && name[0] != 'm') || (name[1] != 'P' && name[1] != 'p')) {
        return false;
    }

    uint32_t v = 0;
    for (size_t i = 2; i < 2u + WPT_SAVE_DIGITS; i++) {
        if (name[i] < '0' || name[i] > '9') {
            return false;
        }
        v = v * 10u + (uint32_t)(name[i] - '0');
    }

    const char *ext = name + 2u + WPT_SAVE_DIGITS;
    if (ext[0] != '.' ||
        (ext[1] != 'G' && ext[1] != 'g') ||
        (ext[2] != 'P' && ext[2] != 'p') ||
        (ext[3] != 'X' && ext[3] != 'x')) {
        return false;
    }

    *index_out = v;
    return true;
}

/* Самый большой номер среди MPnnnnnn.GPX; 0 - файлов нет. false - каталог не открылся. */
static bool saved_max_index(const char *dir_path, uint32_t *max_out)
{
    purrgo_dir_t *dir = purrgo_fs_opendir(dir_path);

    if (dir == NULL) {
        return false;
    }

    uint32_t max_index = 0;
    purrgo_fs_dirent_t entry;

    while (purrgo_fs_readdir(dir, &entry)) {
        uint32_t idx;

        if (!entry.is_directory && saved_file_index(entry.name, &idx) && idx > max_index) {
            max_index = idx;
        }
    }
    purrgo_fs_closedir(dir);

    *max_out = max_index;
    return true;
}

bool purrgo_wpt_save_to_waypts(int32_t lat_1e7, int32_t lon_1e7,
                               char *name_out, size_t name_size)
{
    if (lat_1e7 < -900000000 || lat_1e7 > 900000000 ||
        lon_1e7 < -1800000000 || lon_1e7 > 1800000000) {
        return false;
    }

    const char *dir_path = purrgo_fs_get_waypts_path();
    uint32_t max_index;

    if (!saved_max_index(dir_path, &max_index) || max_index >= WPT_SAVE_MAX_INDEX) {
        PURRGO_LOGE("wpt: WAYPTS unavailable or full\n\r");
        return false;
    }
    const uint32_t index = max_index + 1u;

    char path[PURRGO_FS_MAX_PATH + 16];
    const int pn = purrgo_snprintf(path, sizeof path, "%s/%s%06u.GPX", dir_path,
                                   WPT_SAVE_PREFIX, (unsigned)index);
    if (pn <= 0 || (size_t)pn >= sizeof path) {
        return false;
    }

    char name[PURRGO_WPT_NAME_MAX];
    (void)purrgo_snprintf(name, sizeof name, "%s%u", WPT_SAVE_PREFIX, (unsigned)index);

    const int32_t lat_abs = lat_1e7 < 0 ? -lat_1e7 : lat_1e7;
    const int32_t lon_abs = lon_1e7 < 0 ? -lon_1e7 : lon_1e7;

    char text[320];
    const int tn = purrgo_snprintf(text, sizeof text,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<gpx version=\"1.1\" creator=\"PurrGo\">\n"
        "<wpt lat=\"%s%d.%07d\" lon=\"%s%d.%07d\">\n"
        "  <name>%s</name>\n"
        "</wpt>\n"
        "</gpx>\n",
        lat_1e7 < 0 ? "-" : "", (int)(lat_abs / 10000000), (int)(lat_abs % 10000000),
        lon_1e7 < 0 ? "-" : "", (int)(lon_abs / 10000000), (int)(lon_abs % 10000000),
        name);
    if (tn <= 0 || (size_t)tn >= sizeof text) {
        return false;
    }

    /* Новое имя заведомо свободно (номер больше всех имеющихся): существующий файл не затирается. */
    purrgo_file_t *file = purrgo_fs_open(path, FS_WRITE_CREATE);

    if (file == NULL) {
        PURRGO_LOGE("wpt: cannot create %s\n\r", path);
        return false;
    }

    const bool written = purrgo_fs_write(file, (const uint8_t *)text, (uint32_t)tn) == (uint32_t)tn;
    const bool synced = written && purrgo_fs_sync(file);
    const bool closed = purrgo_fs_close(file);

    if (!(written && synced && closed)) {
        PURRGO_LOGE("wpt: write failed %s\n\r", path);
        return false;
    }

    if (name_out != NULL && name_size > 0) {
        (void)purrgo_snprintf(name_out, name_size, "%s", name);
    }
    return true;
}


/* ------------------------------------------------------------------ */
/*  Расстояние и направление                                            */
/* ------------------------------------------------------------------ */

void purrgo_wpt_update(const purrgo_gnss_solution_t *fix)
{
    const purrgo_waypoint_t *target = purrgo_wpt_target();

    memset(&s_nav, 0, sizeof s_nav);

    if (target == NULL || fix == NULL || !fix->valid) {
        return;
    }

    purrgo_nav_update(fix, target, PURRGO_WPT_ARRIVAL_RADIUS_M, &s_nav.nav);
    s_nav.valid = true;
}

const purrgo_wpt_nav_t *purrgo_wpt_nav(void)
{
    return &s_nav;
}

bool purrgo_wpt_arrow_angle(const purrgo_gnss_solution_t *fix, uint16_t bearing_deg,
                            uint16_t *angle_deg_out)
{
    bool course_up = false;
    int32_t angle = (int32_t)(bearing_deg % 360u);

    if (fix != NULL && fix->course_valid &&
        fix->speed_knots >= PURRGO_WPT_COURSE_MIN_SPEED_KNOTS_100) {
        int32_t course = (fix->course_deg_100 / 100) % 360;

        if (course < 0) {
            course += 360;
        }
        angle -= course;
        course_up = true;
    }

    angle %= 360;
    if (angle < 0) {
        angle += 360;
    }

    if (angle_deg_out != NULL) {
        *angle_deg_out = (uint16_t)angle;
    }
    return course_up;
}

/* sin(0..90 градусов) * 10000 с шагом 5 градусов. */
static const int16_t k_sin_10k[19] = {
    0, 872, 1736, 2588, 3420, 4226, 5000, 5736, 6428, 7071,
    7660, 8192, 8660, 9063, 9397, 9659, 9848, 9962, 10000
};

static int32_t sin_10k_0_90(int32_t deg)
{
    const int32_t i = deg / 5;
    const int32_t f = deg % 5;

    if (i >= 18) {
        return 10000;
    }
    return k_sin_10k[i] + ((k_sin_10k[i + 1] - k_sin_10k[i]) * f) / 5;
}

static int32_t sin_10k(int32_t deg)
{
    deg %= 360;
    if (deg < 0) {
        deg += 360;
    }

    if (deg <= 90) {
        return sin_10k_0_90(deg);
    }
    if (deg <= 180) {
        return sin_10k_0_90(180 - deg);
    }
    if (deg <= 270) {
        return -sin_10k_0_90(deg - 180);
    }
    return -sin_10k_0_90(360 - deg);
}

static int32_t cos_10k(int32_t deg)
{
    return sin_10k(deg + 90);
}

/* v * k / 10000 с округлением к ближайшему. */
static int16_t scale_10k(int32_t v, int32_t k)
{
    const int32_t p = v * k;

    return (int16_t)((p >= 0) ? (p + 5000) / 10000 : -((-p + 5000) / 10000));
}

void purrgo_wpt_arrow_points(int16_t cx, int16_t cy, int16_t r, uint16_t angle_deg,
                             purrgo_wpt_pt_t pts[4])
{
    /* Направление остриё: (sin a, -cos a) в экранных координатах (y вниз); поперёк: (cos a, sin a). */
    const int32_t s = sin_10k(angle_deg);
    const int32_t c = cos_10k(angle_deg);
    const int32_t dx = s, dy = -c;
    const int32_t px = c, py = s;

    const int16_t tip_x = (int16_t)(cx + scale_10k(r, dx));
    const int16_t tip_y = (int16_t)(cy + scale_10k(r, dy));
    const int16_t tail_len = (int16_t)((r * 6) / 10);
    const int16_t base_len = (int16_t)(r / 2);          /* от остриё назад */
    const int16_t wing = (int16_t)((r * 35) / 100);

    const int16_t base_x = (int16_t)(tip_x - scale_10k(base_len, dx));
    const int16_t base_y = (int16_t)(tip_y - scale_10k(base_len, dy));

    pts[0].x = (int16_t)(cx - scale_10k(tail_len, dx));
    pts[0].y = (int16_t)(cy - scale_10k(tail_len, dy));
    pts[1].x = tip_x;
    pts[1].y = tip_y;
    pts[2].x = (int16_t)(base_x - scale_10k(wing, px));
    pts[2].y = (int16_t)(base_y - scale_10k(wing, py));
    pts[3].x = (int16_t)(base_x + scale_10k(wing, px));
    pts[3].y = (int16_t)(base_y + scale_10k(wing, py));
}

void purrgo_wpt_polar(int16_t cx, int16_t cy, int16_t r, uint16_t angle_deg,
                      purrgo_wpt_pt_t *out)
{
    if (out == NULL) {
        return;
    }
    out->x = (int16_t)(cx + scale_10k(r, sin_10k(angle_deg)));
    out->y = (int16_t)(cy - scale_10k(r, cos_10k(angle_deg)));
}

void purrgo_wpt_format_distance(char *buf, size_t size, uint32_t meters)
{
    if (buf == NULL || size == 0) {
        return;
    }

    if (meters < 1000u) {
        purrgo_snprintf(buf, size, "%u m", (unsigned)meters);
    } else if (meters < 10000u) {
        purrgo_snprintf(buf, size, "%u.%02u km", (unsigned)(meters / 1000u),
                        (unsigned)((meters % 1000u) / 10u));
    } else if (meters < 100000u) {
        purrgo_snprintf(buf, size, "%u.%u km", (unsigned)(meters / 1000u),
                        (unsigned)((meters % 1000u) / 100u));
    } else {
        purrgo_snprintf(buf, size, "%u km", (unsigned)(meters / 1000u));
    }
}

const char *purrgo_wpt_cardinal(uint16_t bearing_deg)
{
    static const char *const names[8] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };

    return names[(((unsigned)bearing_deg % 360u) + 22u) / 45u % 8u];
}


/* ------------------------------------------------------------------ */
/*  Список точек из GPX                                                 */
/* ------------------------------------------------------------------ */

/* Только имя файла: без путей и скрытых файлов, помещается в настройку. */
static bool name_is_valid(const char *name)
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

size_t purrgo_wpt_list_load(const char *filename)
{
    s_list_count = 0;
    s_list_truncated = false;

    if (!name_is_valid(filename)) {
        return 0;
    }

    char path[PURRGO_FS_MAX_PATH + PURRGO_ROUTE_NAME_MAX + 2];
    const int pn = purrgo_snprintf(path, sizeof path, "%s/%s",
                                   purrgo_fs_get_routes_path(), filename);
    if (pn <= 0 || (size_t)pn >= sizeof path) {
        return 0;
    }

    purrgo_file_t *file = purrgo_fs_open(path, FS_READ);
    if (file == NULL) {
        PURRGO_LOGE("wpt: cannot open %s\n\r", path);
        return 0;
    }

    purrgo_gpx_parser_t parser;
    purrgo_gpx_parser_init(&parser, s_list, PURRGO_WPT_LIST_MAX);

    char buf[WPT_READ_CHUNK];
    uint32_t total = 0;

    for (;;) {
        const uint32_t n = purrgo_fs_read(file, (uint8_t *)buf, (uint32_t)sizeof buf);

        if (n == 0) {
            break;
        }
        purrgo_gpx_parser_feed(&parser, buf, n);

        total += n;
        if (total >= WPT_MAX_FILE_BYTES) {
            PURRGO_LOGW("wpt: file too large, rest ignored\n\r");
            break;
        }
    }
    (void)purrgo_fs_close(file);

    s_list_count = parser.current_count;
    s_list_truncated = parser.total_seen > parser.current_count;

    for (size_t i = 0; i < s_list_count; i++) {
        if (s_list[i].name[0] == '\0') {
            purrgo_snprintf(s_list[i].name, sizeof s_list[i].name, "WPT %u", (unsigned)(i + 1u));
        }
    }
    return s_list_count;
}

size_t purrgo_wpt_list_count(void)
{
    return s_list_count;
}

const purrgo_waypoint_t *purrgo_wpt_list_get(size_t index)
{
    return (index < s_list_count) ? &s_list[index] : NULL;
}

bool purrgo_wpt_list_truncated(void)
{
    return s_list_truncated;
}


/* ------------------------------------------------------------------ */
/*  Карта                                                               */
/* ------------------------------------------------------------------ */

/*
 * Иконка цели - булавка 11x11, остриё в нижнем центральном пикселе (ряд 10,
 * столбец 5). gfx_draw_icon_11x11() ставит центр иконки в (x, y), поэтому центр
 * сдвигается вверх на 5 px: остриё приходится ровно на координату цели.
 */
#define WPT_ICON_HALF       ((int)(PURRGO_POI_ICON_WIDTH / 2u))
#define WPT_ICON_TIP_DY     ((int)(PURRGO_POI_ICON_HEIGHT / 2u))

void purrgo_wpt_render(gfx_context_t *gfx, const purrgo_bbox_t *camera,
                       const purrgo_viewport_t *vp)
{
    if (gfx == NULL || camera == NULL || vp == NULL || !app_config.wpt_valid) {
        return;
    }

    int16_t sx, sy;
    project_to_screen(app_config.wpt_lon_1e7, app_config.wpt_lat_1e7, camera, vp, &sx, &sy);

    /* Иконка занимает x: sx-5..sx+5, y: sy-10..sy. Целиком вне окна карты - не рисуется. */
    if (sx + WPT_ICON_HALF < vp->offset_x ||
        sx - WPT_ICON_HALF > vp->offset_x + (int)vp->width - 1 ||
        sy < vp->offset_y ||
        sy - 2 * WPT_ICON_TIP_DY > vp->offset_y + (int)vp->height - 1) {
        return;
    }

    gfx_draw_icon_11x11(gfx, sx, (int16_t)(sy - WPT_ICON_TIP_DY),
                        purrgo_poi_icons[PURRGO_WPT_TARGET_ICON]);
}
