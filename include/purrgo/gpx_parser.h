#ifndef PURRGO_GPX_PARSER_H
#define PURRGO_GPX_PARSER_H

#include "purrgo/navigation.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Контекст потокового парсера
typedef struct {
    purrgo_waypoint_t *waypoints; // Указатель на массив для сохранения точек
    size_t max_waypoints;         // Вместимость массива
    size_t current_count;         // Количество успешно загруженных точек

    // Состояние конечного автомата
    bool in_wpt;
    bool in_name;
    bool in_ele;
    bool inside_tag_brackets;     // Флаг нахождения внутри скобок < ... >

    purrgo_waypoint_t temp_wp;    // Временный буфер для собираемой точки
    
    char tag_buffer[128];         // Буфер для накопления текущего тега
    uint8_t tag_len;

    char text_buffer[32];         // Буфер для накопления текста (имя, высота)
    uint8_t text_len;
} purrgo_gpx_parser_t;

// Инициализация парсера
void purrgo_gpx_parser_init(purrgo_gpx_parser_t *parser, purrgo_waypoint_t *waypoints, size_t max_waypoints);

// Подача "сырых" байтов в конечный автомат (размер chunk может быть любым)
void purrgo_gpx_parser_feed(purrgo_gpx_parser_t *parser, const char *chunk, size_t len);

#endif // PURRGO_GPX_PARSER_H