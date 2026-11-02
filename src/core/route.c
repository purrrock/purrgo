#include "purrgo/route.h"
#include "purrgo/config.h"
#include "purrgo/fs_hal.h"
#include "purrgo/gfx_line.h"
#include "purrgo/logger.h"
#include "purrgo/purrgo_format.h"
#include "map_projection.h"
#include <string.h>

/* Толщина чёрной линии маршрута и белой линии по её центру, px. */
#define ROUTE_LINE_THICKNESS_PX 3

/* Размер блока чтения GPX с SD. */
#define ROUTE_READ_CHUNK        512u

/* Подсказка HAL: читать файл блоками по 8 секторов (последовательное чтение). */
#define ROUTE_READ_AHEAD_SECTORS 8u

/* Больше этого размера файл не читается (защита от многомегабайтных GPX). */
#define ROUTE_MAX_FILE_BYTES    (4u * 1024u * 1024u)

/* Буфер текста одного тега: <trkpt lat="..." lon="...">. */
#define ROUTE_TAG_MAX           128u

/* Стартовый шаг прореживания при первом переполнении, 1e-7 градуса (~1.4 м). */
#define ROUTE_STEP_FIRST        128u

/* Предел шага прореживания (~100 км): дальше сжимать нечем. */
#define ROUTE_STEP_LIMIT        (1u << 30)

static purrgo_route_point_t s_pts[PURRGO_ROUTE_MAX_POINTS];
static size_t               s_count;
static uint32_t             s_step;

/* Последняя принятая из файла, но не сохранённая точка (ближе шага к предыдущей). */
static bool                 s_pending;
static purrgo_route_point_t s_pending_pt;

/* Границы сохранённых точек (для быстрого отсечения всего маршрута). */
static bool                 s_bounds_valid;
static int32_t              s_min_lat, s_max_lat, s_min_lon, s_max_lon;

/* true - есть хотя бы один отрезок (две точки в одном сегменте). */
static bool                 s_has_line;

/* Разборщик тегов. */
static char                 s_tag[ROUTE_TAG_MAX];
static uint8_t              s_tag_len;
static bool                 s_in_tag;

/* Что загружено (для purrgo_route_sync). */
static char                 s_loaded_name[PURRGO_ROUTE_NAME_MAX];
static bool                 s_loaded_ok;


/* ------------------------------------------------------------------ */
/*  Числа и атрибуты                                                    */
/* ------------------------------------------------------------------ */

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/*
 * Градусы "53.1234567" -> 1e-7 градуса без float. Лишние знаки после седьмого
 * отбрасываются. false - нет цифр или значение вне +-180 градусов.
 */
static bool parse_coord_1e7(const char *s, int32_t *out)
{
    while (is_space(*s)) {
        s++;
    }

    bool neg = false;
    if (*s == '-') {
        neg = true;
        s++;
    } else if (*s == '+') {
        s++;
    }

    int64_t int_part = 0;
    unsigned digits = 0;
    while (*s >= '0' && *s <= '9') {
        int_part = int_part * 10 + (*s - '0');
        digits++;
        s++;
        if (int_part > 1000) {
            return false;
        }
    }

    int64_t frac = 0;
    int32_t mult = 1000000;
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            digits++;
            if (mult > 0) {
                frac += (int64_t)(*s - '0') * mult;
                mult /= 10;
            }
            s++;
        }
    }

    if (digits == 0) {
        return false;
    }

    int64_t v = int_part * 10000000 + frac;
    if (neg) {
        v = -v;
    }
    if (v < -1800000000LL || v > 1800000000LL) {
        return false;
    }

    *out = (int32_t)v;
    return true;
}

/* Значение атрибута name="..." (или name='...') в тексте тега; NULL - атрибута нет. */
static const char *find_attr(const char *tag, const char *name)
{
    const size_t nl = strlen(name);

    for (const char *p = tag; *p != '\0'; p++) {
        if (p != tag && !is_space(p[-1])) {
            continue;
        }
        if (strncmp(p, name, nl) != 0) {
            continue;
        }

        const char *q = p + nl;
        while (is_space(*q)) {
            q++;
        }
        if (*q != '=') {
            continue;
        }
        q++;
        while (is_space(*q)) {
            q++;
        }
        if (*q != '"' && *q != '\'') {
            continue;
        }
        return q + 1;
    }
    return NULL;
}


/* ------------------------------------------------------------------ */
/*  Хранилище точек                                                     */
/* ------------------------------------------------------------------ */

static bool is_break(const purrgo_route_point_t *p)
{
    return p->lat_1e7 == PURRGO_ROUTE_BREAK_LAT;
}

/* Расстояние Чебышёва в 1e-7 градуса (долгота без поправки на широту: для порога достаточно). */
static int64_t cheb(const purrgo_route_point_t *a, const purrgo_route_point_t *b)
{
    int64_t dy = (int64_t)a->lat_1e7 - (int64_t)b->lat_1e7;
    int64_t dx = (int64_t)a->lon_1e7 - (int64_t)b->lon_1e7;
    if (dy < 0) {
        dy = -dy;
    }
    if (dx < 0) {
        dx = -dx;
    }
    return (dx > dy) ? dx : dy;
}

/*
 * Массив заполнен: увеличиваем шаг прореживания и выкидываем точки, которые
 * ближе шага к последней оставленной. Первая и последняя точки каждого
 * сегмента остаются всегда. Повторяется, пока не освободится 1/8 массива.
 */
static void compact(void)
{
    const size_t target = PURRGO_ROUTE_MAX_POINTS - PURRGO_ROUTE_MAX_POINTS / 8u;

    while (s_count > target && s_step < ROUTE_STEP_LIMIT) {
        s_step = (s_step < ROUTE_STEP_FIRST) ? ROUTE_STEP_FIRST : s_step * 2u;

        size_t w = 0;
        bool have_last = false;
        purrgo_route_point_t last = { 0, 0 };

        for (size_t r = 0; r < s_count; r++) {
            const purrgo_route_point_t p = s_pts[r];

            if (is_break(&p)) {
                s_pts[w++] = p;
                have_last = false;
                continue;
            }

            const bool seg_end = (r + 1u == s_count) || is_break(&s_pts[r + 1u]);
            if (!have_last || seg_end || cheb(&p, &last) >= (int64_t)s_step) {
                s_pts[w++] = p;
                last = p;
                have_last = true;
            }
        }
        s_count = w;
    }
}

static bool ensure_room(void)
{
    if (s_count >= PURRGO_ROUTE_MAX_POINTS) {
        compact();
    }
    return s_count < PURRGO_ROUTE_MAX_POINTS;
}

static void push_point(purrgo_route_point_t p)
{
    if (ensure_room()) {
        s_pts[s_count++] = p;
    }
}

static void flush_pending(void)
{
    if (s_pending) {
        s_pending = false;
        push_point(s_pending_pt);
    }
}

static void add_point(int32_t lat, int32_t lon)
{
    const purrgo_route_point_t p = { lat, lon };

    if (s_count > 0 && !is_break(&s_pts[s_count - 1u])) {
        const int64_t d = cheb(&s_pts[s_count - 1u], &p);
        if (d == 0) {
            s_pending = false;
            return;
        }
        if (d < (int64_t)s_step) {
            s_pending = true;
            s_pending_pt = p;
            return;
        }
    }

    s_pending = false;
    push_point(p);
}

static void end_segment(void)
{
    flush_pending();

    if (s_count > 0 && !is_break(&s_pts[s_count - 1u]) && ensure_room()) {
        s_pts[s_count].lat_1e7 = PURRGO_ROUTE_BREAK_LAT;
        s_pts[s_count].lon_1e7 = 0;
        s_count++;
    }
}


/* ------------------------------------------------------------------ */
/*  Разбор GPX                                                          */
/* ------------------------------------------------------------------ */

static void process_tag(void)
{
    /* Имя тега: до пробела или '/' (кроме ведущего '/' закрывающего тега). */
    char name[12];
    size_t n = 0;
    for (size_t i = 0; s_tag[i] != '\0' && !is_space(s_tag[i]) &&
                       !(s_tag[i] == '/' && i > 0); i++) {
        if (n + 1u < sizeof name) {
            name[n++] = s_tag[i];
        } else {
            n = sizeof name;    /* слишком длинное имя: не наш тег */
            break;
        }
    }
    if (n >= sizeof name) {
        return;
    }
    name[n] = '\0';

    if (strcmp(name, "trkpt") == 0 || strcmp(name, "rtept") == 0) {
        const char *lat_s = find_attr(s_tag, "lat");
        const char *lon_s = find_attr(s_tag, "lon");
        int32_t lat, lon;

        if (lat_s != NULL && lon_s != NULL &&
            parse_coord_1e7(lat_s, &lat) && parse_coord_1e7(lon_s, &lon) &&
            lat >= -900000000 && lat <= 900000000) {
            add_point(lat, lon);
        }
    } else if (strcmp(name, "trkseg") == 0 || strcmp(name, "/trkseg") == 0 ||
               strcmp(name, "trk") == 0 || strcmp(name, "/trk") == 0 ||
               strcmp(name, "rte") == 0 || strcmp(name, "/rte") == 0) {
        end_segment();
    }
}

void purrgo_route_clear(void)
{
    s_count = 0;
    s_step = 0;
    s_pending = false;
    s_bounds_valid = false;
    s_has_line = false;
    s_tag_len = 0;
    s_in_tag = false;
}

void purrgo_route_begin(void)
{
    purrgo_route_clear();
}

void purrgo_route_feed(const char *chunk, size_t len)
{
    if (chunk == NULL) {
        return;
    }

    for (size_t i = 0; i < len; i++) {
        const char c = chunk[i];

        if (c == '<') {
            s_in_tag = true;
            s_tag_len = 0;
            s_tag[0] = '\0';
        } else if (c == '>') {
            if (s_in_tag) {
                s_tag[s_tag_len] = '\0';
                process_tag();
            }
            s_in_tag = false;
        } else if (s_in_tag) {
            if (s_tag_len + 1u < ROUTE_TAG_MAX) {
                s_tag[s_tag_len++] = c;
            }
        }
    }
}

void purrgo_route_end(void)
{
    flush_pending();

    /* Хвостовой разрыв не нужен. */
    if (s_count > 0 && is_break(&s_pts[s_count - 1u])) {
        s_count--;
    }

    s_bounds_valid = false;
    s_has_line = false;

    size_t seg_points = 0;
    for (size_t i = 0; i < s_count; i++) {
        if (is_break(&s_pts[i])) {
            seg_points = 0;
            continue;
        }

        const int32_t lat = s_pts[i].lat_1e7;
        const int32_t lon = s_pts[i].lon_1e7;

        if (!s_bounds_valid) {
            s_min_lat = s_max_lat = lat;
            s_min_lon = s_max_lon = lon;
            s_bounds_valid = true;
        } else {
            if (lat < s_min_lat) { s_min_lat = lat; }
            if (lat > s_max_lat) { s_max_lat = lat; }
            if (lon < s_min_lon) { s_min_lon = lon; }
            if (lon > s_max_lon) { s_max_lon = lon; }
        }

        if (++seg_points >= 2u) {
            s_has_line = true;
        }
    }
}


/* ------------------------------------------------------------------ */
/*  Загрузка из файла                                                   */
/* ------------------------------------------------------------------ */

/* Только имя файла: без путей, без скрытых файлов, помещается в настройку. */
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

bool purrgo_route_load(const char *filename)
{
    purrgo_route_clear();

    if (!name_is_valid(filename)) {
        PURRGO_LOGE("route: bad file name\n\r");
        return false;
    }

    char path[PURRGO_FS_MAX_PATH + PURRGO_ROUTE_NAME_MAX + 2];
    const int pn = purrgo_snprintf(path, sizeof path, "%s/%s",
                                   purrgo_fs_get_routes_path(), filename);
    if (pn <= 0 || (size_t)pn >= sizeof path) {
        return false;
    }

    purrgo_file_t *file = purrgo_fs_open(path, FS_READ);
    if (file == NULL) {
        PURRGO_LOGE("route: cannot open %s\n\r", path);
        return false;
    }
    purrgo_fs_set_read_ahead(file, (uint8_t)ROUTE_READ_AHEAD_SECTORS);

    purrgo_route_begin();

    char buf[ROUTE_READ_CHUNK];
    uint32_t total = 0;
    bool truncated = false;

    for (;;) {
        const uint32_t n = purrgo_fs_read(file, (uint8_t *)buf, (uint32_t)sizeof buf);
        if (n == 0) {
            break;
        }
        purrgo_route_feed(buf, n);
        total += n;
        if (total >= ROUTE_MAX_FILE_BYTES) {
            truncated = true;
            break;
        }
    }
    (void)purrgo_fs_close(file);

    purrgo_route_end();

    if (truncated) {
        PURRGO_LOGW("route: %s is too large, read only the first %lu bytes\n\r",
                    filename, (unsigned long)total);
    }
    PURRGO_LOG("route: %s entries=%lu step=%lu line=%d\n\r", filename,
               (unsigned long)s_count, (unsigned long)s_step, s_has_line ? 1 : 0);

    return s_has_line;
}

bool purrgo_route_sync(const char *filename)
{
    if (filename == NULL || filename[0] == '\0') {
        if (s_loaded_name[0] != '\0' || s_count != 0) {
            purrgo_route_clear();
        }
        s_loaded_name[0] = '\0';
        s_loaded_ok = false;
        return false;
    }

    if (s_loaded_ok && strcmp(filename, s_loaded_name) == 0) {
        return true;
    }

    /* Другое имя либо прошлая попытка не удалась: загружаем заново. */
    s_loaded_ok = purrgo_route_load(filename);
    purrgo_snprintf(s_loaded_name, sizeof s_loaded_name, "%.*s",
                    (int)(sizeof s_loaded_name - 1u), filename);
    return s_loaded_ok;
}


/* ------------------------------------------------------------------ */
/*  Доступ к данным и отрисовка                                         */
/* ------------------------------------------------------------------ */

size_t purrgo_route_entry_count(void)
{
    return s_count;
}

bool purrgo_route_get_entry(size_t i, purrgo_route_point_t *out)
{
    if (out == NULL || i >= s_count) {
        return false;
    }
    *out = s_pts[i];
    return true;
}

uint32_t purrgo_route_decimation_step(void)
{
    return s_step;
}

/*
 * Один проход по маршруту. Сначала чёрная линия всех отрезков, потом белая
 * поверх: иначе чёрная "шапка" следующего отрезка закрашивала бы белую
 * линию в стыках.
 */
static void render_pass(
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp,
    bool white)
{
    bool have_prev = false;
    int32_t prev_x = 0;
    int32_t prev_y = 0;

    for (size_t i = 0; i < s_count; i++) {
        if (is_break(&s_pts[i])) {
            have_prev = false;
            continue;
        }

        int32_t sx, sy;
        project_to_screen_i32(s_pts[i].lon_1e7, s_pts[i].lat_1e7, camera, vp, &sx, &sy);

        if (have_prev) {
            int32_t x0 = prev_x, y0 = prev_y, x1 = sx, y1 = sy;

            /* Далёкий конец отрезка отсекается с сохранением наклона. */
            if (map_clip_segment_to_safe(&x0, &y0, &x1, &y1)) {
                if (white) {
                    gfx_draw_line(gfx, (int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1);
                } else {
                    gfx_draw_thick_line(gfx, (int16_t)x0, (int16_t)y0,
                                        (int16_t)x1, (int16_t)y1,
                                        ROUTE_LINE_THICKNESS_PX);
                }
            }
        }

        prev_x = sx;
        prev_y = sy;
        have_prev = true;
    }
}

void purrgo_route_render(
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp)
{
    if (gfx == NULL || camera == NULL || vp == NULL || !s_has_line || !s_bounds_valid) {
        return;
    }

    /* Маршрут целиком за пределами камеры (антимеридиан не отсекаем). */
    if (camera->min_x <= camera->max_x &&
        (s_max_lon < camera->min_x || s_min_lon > camera->max_x ||
         s_max_lat < camera->min_y || s_min_lat > camera->max_y)) {
        return;
    }

    gfx_set_color(gfx, BLACK, gfx->color_bg);
    render_pass(gfx, camera, vp, false);

    gfx_set_color(gfx, WHITE, gfx->color_bg);
    render_pass(gfx, camera, vp, true);
}
