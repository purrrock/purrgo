#include "purrgo/gpx_parser.h"
#include "purrgo/purrgo_format.h"
#include <string.h>

/*
 * Потоковый разбор путевых точек <wpt> GPX без float и без динамической памяти.
 * Читаются атрибуты lat/lon (в двойных и одинарных кавычках, в любом порядке),
 * <name> и <ele>. Точка без корректных lat и lon (отсутствуют, не число, вне
 * +-90 / +-180 градусов) отбрасывается. Самозакрывающийся <wpt ... /> без
 * вложенных тегов тоже принимается. Маршрутные и трековые точки не читаются
 * (их разбирает route.c).
 */

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/*
 * Градусы "53.1234567" -> 1e-7 градуса. Лишние знаки после седьмого
 * отбрасываются. false - нет цифр или значение вне +-limit_deg градусов.
 */
static bool parse_coord_1e7(const char *s, int32_t limit_deg, int32_t *out)
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
    if (v < -(int64_t)limit_deg * 10000000 || v > (int64_t)limit_deg * 10000000) {
        return false;
    }

    *out = (int32_t)v;
    return true;
}

/* Целое число (для высоты); дробная часть отбрасывается. */
static int16_t parse_int16(const char *str)
{
    int32_t result = 0;
    int32_t sign = 1;
    while (is_space(*str)) str++;
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') { str++; }
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        if (result > 32767) {
            result = 32767;
        }
        str++;
    }
    return (int16_t)(result * sign);
}

/*
 * Значение атрибута name="..." (или name='...') в тексте тега: указатель на первый
 * символ значения, длина - до закрывающей кавычки. NULL - атрибута нет.
 */
static const char *find_attr(const char *tag, const char *name, size_t *len_out)
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

        const char quote = *q++;
        const char *end = strchr(q, quote);
        if (end == NULL) {
            return NULL;
        }
        *len_out = (size_t)(end - q);
        return q;
    }
    return NULL;
}

/* Разбор числового атрибута; false - нет атрибута, не число или вне диапазона. */
static bool attr_coord(const char *tag, const char *name, int32_t limit_deg, int32_t *out)
{
    size_t len = 0;
    const char *v = find_attr(tag, name, &len);
    char tmp[24];

    if (v == NULL || len == 0 || len >= sizeof tmp) {
        return false;
    }
    memcpy(tmp, v, len);
    tmp[len] = '\0';
    return parse_coord_1e7(tmp, limit_deg, out);
}

/* Обрезает строку так, чтобы она не заканчивалась обрезанной UTF-8 последовательностью. */
static void trim_utf8_tail(char *s)
{
    size_t n = strlen(s);
    size_t i = n;

    /* Назад до начального байта последней последовательности (не более 3 байт продолжения). */
    while (i > 0 && n - i < 3 && ((unsigned char)s[i - 1] & 0xC0u) == 0x80u) {
        i--;
    }
    if (i == 0) {
        return;
    }

    const unsigned char lead = (unsigned char)s[i - 1];
    size_t need = 1;
    if ((lead & 0xE0u) == 0xC0u) {
        need = 2;
    } else if ((lead & 0xF0u) == 0xE0u) {
        need = 3;
    } else if ((lead & 0xF8u) == 0xF0u) {
        need = 4;
    }

    if (n - (i - 1) < need) {
        s[i - 1] = '\0';
    }
}

void purrgo_gpx_parser_init(purrgo_gpx_parser_t *parser, purrgo_waypoint_t *waypoints, size_t max_waypoints)
{
    if (!parser) return;
    memset(parser, 0, sizeof(purrgo_gpx_parser_t));
    parser->waypoints = waypoints;
    parser->max_waypoints = max_waypoints;
}

/* Завершает собираемую точку: сохраняет, если координаты корректны. */
static void finish_wpt(purrgo_gpx_parser_t *p)
{
    if (p->in_wpt && p->temp_ok) {
        p->total_seen++;
        if (p->current_count < p->max_waypoints && p->waypoints != NULL) {
            p->waypoints[p->current_count++] = p->temp_wp;
        }
    }
    p->in_wpt = false;
    p->in_name = false;
    p->in_ele = false;
}

/* Обработка собранного тега. */
static void process_tag(purrgo_gpx_parser_t *p)
{
    char *tag = p->tag_buffer;

    /* Начало путевой точки: <wpt lat="53.7" lon="28.4"> или <wpt lat=".." lon=".." /> */
    if (strncmp(tag, "wpt", 3) == 0 && (tag[3] == '\0' || is_space(tag[3]) || tag[3] == '/')) {
        if (p->in_wpt) {
            finish_wpt(p);      /* предыдущая точка не была закрыта */
        }

        p->in_wpt = true;
        memset(&p->temp_wp, 0, sizeof(purrgo_waypoint_t));
        p->temp_ok = attr_coord(tag, "lat", 90, &p->temp_wp.lat_1e7) &&
                     attr_coord(tag, "lon", 180, &p->temp_wp.lon_1e7);

        const size_t n = strlen(tag);
        if (n > 0 && tag[n - 1] == '/') {
            finish_wpt(p);      /* самозакрывающийся тег */
        }
    }
    /* Конец путевой точки: сохраняем в массив. */
    else if (strcmp(tag, "/wpt") == 0) {
        finish_wpt(p);
    }
    /* Тег имени. */
    else if (strcmp(tag, "name") == 0) {
        p->in_name = true;
        p->text_len = 0;
        p->text_buffer[0] = '\0';
    }
    else if (strcmp(tag, "/name") == 0) {
        p->in_name = false;
        if (p->in_wpt) {
            purrgo_snprintf(p->temp_wp.name,
                            sizeof(p->temp_wp.name),
                            "%.*s",
                            (int)(sizeof(p->temp_wp.name) - 1),
                            p->text_buffer);
            trim_utf8_tail(p->temp_wp.name);
        }
    }
    /* Тег высоты. */
    else if (strcmp(tag, "ele") == 0) {
        p->in_ele = true;
        p->text_len = 0;
        p->text_buffer[0] = '\0';
    }
    else if (strcmp(tag, "/ele") == 0) {
        p->in_ele = false;
        if (p->in_wpt) {
            p->temp_wp.ele_m = parse_int16(p->text_buffer);
        }
    }
}

void purrgo_gpx_parser_feed(purrgo_gpx_parser_t *p, const char *chunk, size_t len)
{
    if (!p) return;

    for (size_t i = 0; i < len; i++) {
        char c = chunk[i];

        if (c == '<') {
            p->inside_tag_brackets = true;
            p->tag_len = 0;
            p->tag_buffer[0] = '\0';
        } else if (c == '>') {
            if (p->inside_tag_brackets) {
                p->inside_tag_brackets = false;
                p->tag_buffer[p->tag_len] = '\0';
                process_tag(p);
            }
        } else if (p->inside_tag_brackets) {
            /* Накапливаем содержимое тега, защищаясь от переполнения. */
            if (p->tag_len < sizeof(p->tag_buffer) - 1) {
                p->tag_buffer[p->tag_len++] = c;
            }
        } else {
            /* Текст между тегами (для <name> и <ele>). */
            if ((p->in_name || p->in_ele) && c != '\n' && c != '\r') {
                if (p->text_len < sizeof(p->text_buffer) - 1) {
                    p->text_buffer[p->text_len++] = c;
                    p->text_buffer[p->text_len] = '\0';
                }
            }
        }
    }
}
