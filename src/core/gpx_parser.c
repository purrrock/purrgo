#include "purrgo/gpx_parser.h"
#include <stdio.h>
#include <string.h>

// Парсинг строки с плавающей точкой в формат 1e7 без использования float
static int32_t parse_coord_1e7(const char* str) {
    int32_t result = 0;
    int32_t sign = 1;
    
    while (*str == ' ') str++; // Пропуск пробелов
    
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') { str++; }
    
    // Парсинг целой части
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    result *= 10000000; // Сдвиг порядка
    
    // Парсинг дробной части (до 7 знаков)
    if (*str == '.') {
        str++;
        int32_t frac = 0;
        int32_t multiplier = 1000000;
        while (*str >= '0' && *str <= '9' && multiplier > 0) {
            frac += (*str - '0') * multiplier;
            multiplier /= 10;
            str++;
        }
        result += frac;
    }
    return result * sign;
}

// Простой парсер целого числа (для высоты)
static int16_t parse_int16(const char* str) {
    int32_t result = 0;
    int32_t sign = 1;
    while (*str == ' ' || *str == '\n' || *str == '\r') str++;
    if (*str == '-') { sign = -1; str++; }
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return (int16_t)(result * sign);
}

void purrgo_gpx_parser_init(purrgo_gpx_parser_t *parser, purrgo_waypoint_t *waypoints, size_t max_waypoints) {
    if (!parser) return;
    memset(parser, 0, sizeof(purrgo_gpx_parser_t));
    parser->waypoints = waypoints;
    parser->max_waypoints = max_waypoints;
}

// Обработка собранного тега
static void process_tag(purrgo_gpx_parser_t *p) {
    // Начало путевой точки: <wpt lat="53.7" lon="28.4">
    if (strncmp(p->tag_buffer, "wpt ", 4) == 0 || strcmp(p->tag_buffer, "wpt") == 0) {
        p->in_wpt = true;
        memset(&p->temp_wp, 0, sizeof(purrgo_waypoint_t));
        
        char *lat_ptr = strstr(p->tag_buffer, "lat=\"");
        char *lon_ptr = strstr(p->tag_buffer, "lon=\"");
        
        if (lat_ptr) p->temp_wp.lat_1e7 = parse_coord_1e7(lat_ptr + 5);
        if (lon_ptr) p->temp_wp.lon_1e7 = parse_coord_1e7(lon_ptr + 5);
    } 
    // Конец путевой точки: сохраняем в массив
    else if (strcmp(p->tag_buffer, "/wpt") == 0) {
        if (p->in_wpt && p->current_count < p->max_waypoints && p->waypoints != NULL) {
            p->waypoints[p->current_count++] = p->temp_wp;
        }
        p->in_wpt = false;
    }
    // Тег имени: <name>
    else if (strcmp(p->tag_buffer, "name") == 0) {
        p->in_name = true;
        p->text_len = 0;
        p->text_buffer[0] = '\0';
    } 
    // Конец имени: </name>
    else if (strcmp(p->tag_buffer, "/name") == 0) {
        p->in_name = false;
        if (p->in_wpt) {
            // Копируем имя, защищаясь от переполнения
            snprintf(p->temp_wp.name, sizeof(p->temp_wp.name), "%.*s", (int)(sizeof(p->temp_wp.name) - 1), p->text_buffer);
        }
    }
    // Тег высоты: <ele>
    else if (strcmp(p->tag_buffer, "ele") == 0) {
        p->in_ele = true;
        p->text_len = 0;
        p->text_buffer[0] = '\0';
    } 
    // Конец высоты: </ele>
    else if (strcmp(p->tag_buffer, "/ele") == 0) {
        p->in_ele = false;
        if (p->in_wpt) {
            p->temp_wp.ele_m = parse_int16(p->text_buffer);
        }
    }
}

void purrgo_gpx_parser_feed(purrgo_gpx_parser_t *p, const char *chunk, size_t len) {
    if (!p) return;

    for (size_t i = 0; i < len; i++) {
        char c = chunk[i];

        if (c == '<') {
            p->inside_tag_brackets = true;
            p->tag_len = 0;
            p->tag_buffer[0] = '\0';
        } else if (c == '>') {
            p->inside_tag_brackets = false;
            p->tag_buffer[p->tag_len] = '\0'; // Закрываем строку тега
            process_tag(p);
        } else if (p->inside_tag_brackets) {
            // Накапливаем содержимое тега, защищаясь от переполнения
            if (p->tag_len < sizeof(p->tag_buffer) - 1) {
                p->tag_buffer[p->tag_len++] = c;
            }
        } else {
            // Накапливаем текстовое содержимое между тегами (для <name> и <ele>)
            if ((p->in_name || p->in_ele) && c != '\n' && c != '\r') {
                if (p->text_len < sizeof(p->text_buffer) - 1) {
                    p->text_buffer[p->text_len++] = c;
                    p->text_buffer[p->text_len] = '\0';
                }
            }
        }
    }
}