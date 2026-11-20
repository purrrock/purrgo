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
 *
 * Текст <name> и <ele>: раскрываются сущности &amp; &lt; &gt; &quot; &apos; и
 * числовые (&#10; &#x41;), секции <![CDATA[...]]> берутся как текст, комментарии
 * <!-- ... --> пропускаются целиком (в том числе с '<' внутри), переводы строки
 * и серии пробелов сворачиваются в один пробел, пробелы по краям имени
 * отбрасываются. Теги с атрибутами (<name lang="ru">) распознаются.
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

/* Режимы разбора входного потока. */
enum {
    GPX_MODE_TEXT = 0,      /* текст между тегами */
    GPX_MODE_TAG,           /* внутри < ... > */
    GPX_MODE_COMMENT,       /* внутри <!-- ... --> */
    GPX_MODE_CDATA          /* внутри <![CDATA[ ... ]]> */
};

/* Тег называется name: за именем конец строки, пробел или '/'. */
static bool tag_is(const char *tag, const char *name)
{
    const size_t n = strlen(name);

    return strncmp(tag, name, n) == 0 && (tag[n] == '\0' || is_space(tag[n]) || tag[n] == '/');
}

static bool tag_self_closing(const char *tag)
{
    const size_t n = strlen(tag);

    return n > 0 && tag[n - 1] == '/';
}

/* Один символ текста (уже без сущностей); пробельные символы сворачиваются. */
static void text_put(purrgo_gpx_parser_t *p, char c)
{
    if (is_space(c)) {
        if (p->text_len == 0 || p->text_buffer[p->text_len - 1] == ' ') {
            return;
        }
        c = ' ';
    }
    if (p->text_len < sizeof(p->text_buffer) - 1) {
        p->text_buffer[p->text_len++] = c;
        p->text_buffer[p->text_len] = '\0';
    }
}

/* Кодовая точка в UTF-8; помещается целиком или не добавляется совсем. */
static void text_put_codepoint(purrgo_gpx_parser_t *p, uint32_t cp)
{
    char u[4];
    size_t n;

    if (cp < 0x20u) {
        text_put(p, ' ');       /* &#10; и прочие управляющие - как пробел */
        return;
    }
    if (cp < 0x80u) {
        text_put(p, (char)cp);
        return;
    }
    if (cp < 0x800u) {
        u[0] = (char)(0xC0u | (cp >> 6));
        u[1] = (char)(0x80u | (cp & 0x3Fu));
        n = 2;
    } else if (cp < 0x10000u) {
        u[0] = (char)(0xE0u | (cp >> 12));
        u[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        u[2] = (char)(0x80u | (cp & 0x3Fu));
        n = 3;
    } else {
        u[0] = (char)(0xF0u | (cp >> 18));
        u[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        u[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        u[3] = (char)(0x80u | (cp & 0x3Fu));
        n = 4;
    }

    if (p->text_len + n > sizeof(p->text_buffer) - 1) {
        return;
    }
    for (size_t i = 0; i < n; i++) {
        p->text_buffer[p->text_len++] = u[i];
    }
    p->text_buffer[p->text_len] = '\0';
}

/* Сущность без '&' и ';' ("amp", "#65", "#x41") -> кодовая точка; false - неизвестная. */
static bool entity_decode(const char *e, uint32_t *cp)
{
    if (strcmp(e, "amp") == 0)  { *cp = '&';  return true; }
    if (strcmp(e, "lt") == 0)   { *cp = '<';  return true; }
    if (strcmp(e, "gt") == 0)   { *cp = '>';  return true; }
    if (strcmp(e, "quot") == 0) { *cp = '"';  return true; }
    if (strcmp(e, "apos") == 0) { *cp = '\''; return true; }

    if (e[0] != '#' || e[1] == '\0') {
        return false;
    }

    uint32_t v = 0;
    const bool hex = (e[1] == 'x' || e[1] == 'X');
    const char *d = e + (hex ? 2 : 1);

    if (*d == '\0') {
        return false;
    }
    for (; *d != '\0'; d++) {
        uint32_t digit;
        if (*d >= '0' && *d <= '9') {
            digit = (uint32_t)(*d - '0');
        } else if (hex && *d >= 'a' && *d <= 'f') {
            digit = (uint32_t)(*d - 'a') + 10u;
        } else if (hex && *d >= 'A' && *d <= 'F') {
            digit = (uint32_t)(*d - 'A') + 10u;
        } else {
            return false;
        }
        v = v * (hex ? 16u : 10u) + digit;
        if (v > 0x10FFFFu) {
            return false;
        }
    }
    if (v == 0 || (v >= 0xD800u && v <= 0xDFFFu)) {
        return false;
    }
    *cp = v;
    return true;
}

/* Незавершённая или неизвестная сущность выводится как есть. */
static void entity_flush_raw(purrgo_gpx_parser_t *p)
{
    for (uint8_t i = 0; i < p->esc_len; i++) {
        text_put(p, p->esc[i]);
    }
    p->esc_len = 0;
}

/* Символ текста между тегами с раскрытием сущностей. */
static void text_char(purrgo_gpx_parser_t *p, char c)
{
    if (!p->in_name && !p->in_ele) {
        p->esc_len = 0;
        return;
    }

    if (p->esc_len > 0) {
        if (c == ';') {
            uint32_t cp;

            p->esc[p->esc_len] = '\0';
            if (entity_decode(p->esc + 1, &cp)) {
                p->esc_len = 0;
                text_put_codepoint(p, cp);
            } else {
                entity_flush_raw(p);
                text_put(p, ';');
            }
            return;
        }
        if (((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '#') &&
            p->esc_len < sizeof(p->esc) - 1) {
            p->esc[p->esc_len++] = c;
            return;
        }
        entity_flush_raw(p);    /* не сущность: выводим накопленное и разбираем c заново */
    }

    if (c == '&') {
        p->esc[0] = '&';
        p->esc_len = 1;
    } else {
        text_put(p, c);
    }
}

/* Обработка собранного тега. */
static void process_tag(purrgo_gpx_parser_t *p)
{
    char *tag = p->tag_buffer;

    /* Начало путевой точки: <wpt lat="53.7" lon="28.4"> или <wpt lat=".." lon=".." /> */
    if (tag_is(tag, "wpt")) {
        if (p->in_wpt) {
            finish_wpt(p);      /* предыдущая точка не была закрыта */
        }

        p->in_wpt = true;
        memset(&p->temp_wp, 0, sizeof(purrgo_waypoint_t));
        p->temp_ok = attr_coord(tag, "lat", 90, &p->temp_wp.lat_1e7) &&
                     attr_coord(tag, "lon", 180, &p->temp_wp.lon_1e7);

        if (tag_self_closing(tag)) {
            finish_wpt(p);      /* самозакрывающийся тег */
        }
    }
    /* Конец путевой точки: сохраняем в массив. */
    else if (tag_is(tag, "/wpt")) {
        finish_wpt(p);
    }
    /* Тег имени. */
    else if (tag_is(tag, "name")) {
        p->in_name = !tag_self_closing(tag);
        p->text_len = 0;
        p->text_buffer[0] = '\0';
        p->esc_len = 0;
    }
    else if (tag_is(tag, "/name")) {
        p->in_name = false;
        entity_flush_raw(p);
        if (p->in_wpt) {
            while (p->text_len > 0 && p->text_buffer[p->text_len - 1] == ' ') {
                p->text_buffer[--p->text_len] = '\0';
            }
            purrgo_snprintf(p->temp_wp.name,
                            sizeof(p->temp_wp.name),
                            "%.*s",
                            (int)(sizeof(p->temp_wp.name) - 1),
                            p->text_buffer);
            trim_utf8_tail(p->temp_wp.name);
        }
    }
    /* Тег высоты. */
    else if (tag_is(tag, "ele")) {
        p->in_ele = !tag_self_closing(tag);
        p->text_len = 0;
        p->text_buffer[0] = '\0';
        p->esc_len = 0;
    }
    else if (tag_is(tag, "/ele")) {
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
        const char c = chunk[i];

        switch (p->mode) {
        case GPX_MODE_TEXT:
            if (c == '<') {
                entity_flush_raw(p);
                p->mode = GPX_MODE_TAG;
                p->tag_len = 0;
                p->tag_buffer[0] = '\0';
            } else {
                text_char(p, c);
            }
            break;

        case GPX_MODE_TAG:
            if (c == '>') {
                p->mode = GPX_MODE_TEXT;
                p->tag_buffer[p->tag_len] = '\0';
                process_tag(p);
            } else if (c == '<') {
                /* Лишний '<' внутри тега: начинаем тег заново. */
                p->tag_len = 0;
                p->tag_buffer[0] = '\0';
            } else {
                /* Накапливаем содержимое тега, защищаясь от переполнения. */
                if (p->tag_len < sizeof(p->tag_buffer) - 1) {
                    p->tag_buffer[p->tag_len++] = c;
                }
                p->tag_buffer[p->tag_len] = '\0';

                if (p->tag_len == 3 && memcmp(p->tag_buffer, "!--", 3) == 0) {
                    p->mode = GPX_MODE_COMMENT;
                    p->run = 0;
                } else if (p->tag_len == 8 && memcmp(p->tag_buffer, "![CDATA[", 8) == 0) {
                    p->mode = GPX_MODE_CDATA;
                    p->run = 0;
                }
            }
            break;

        case GPX_MODE_COMMENT:
            /* Конец комментария - "-->"; всё внутри, включая '<', пропускается. */
            if (c == '-') {
                if (p->run < 2) {
                    p->run++;
                }
            } else if (c == '>' && p->run >= 2) {
                p->mode = GPX_MODE_TEXT;
                p->run = 0;
            } else {
                p->run = 0;
            }
            break;

        case GPX_MODE_CDATA:
            /* Конец секции - "]]>"; содержимое попадает в текст как есть (без сущностей). */
            if (c == ']') {
                if (p->run < UINT8_MAX) {
                    p->run++;
                }
            } else if (c == '>' && p->run >= 2) {
                if (p->in_name || p->in_ele) {
                    for (uint8_t k = 2; k < p->run; k++) {
                        text_put(p, ']');   /* "]]]>" - лишние ']' принадлежат тексту */
                    }
                }
                p->mode = GPX_MODE_TEXT;
                p->run = 0;
            } else {
                if (p->in_name || p->in_ele) {
                    for (uint8_t k = 0; k < p->run; k++) {
                        text_put(p, ']');
                    }
                    text_put(p, c);
                }
                p->run = 0;
            }
            break;

        default:
            p->mode = GPX_MODE_TEXT;
            break;
        }
    }
}
