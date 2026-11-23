#ifndef PURRGO_POI_SEARCH_H
#define PURRGO_POI_SEARCH_H

/*
 * Простой поиск объектов на карте: категория -> ближайшие объекты.
 * Проект: docs/design/poi-search.md. Нужны только pois.idx и pois.db карты V4, формат не меняется.
 *
 * Поиск идёт порциями: purrgo_poi_search_start() только запоминает запрос, каждый вызов
 * purrgo_poi_search_step() выполняет один проход по индексу для очередного радиуса (1, 2, 4, ...
 * 64 км), между проходами приложение обрабатывает кнопки и рисует экран. Без float, без
 * выделения памяти: список результатов - массив фиксированного размера (PURRGO_POI_SEARCH_MAX_RESULTS).
 *
 * Названия читаются из pois.db только когда экран их запросил (purrgo_poi_search_name()), и
 * только у выведенных строк.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PURRGO_POI_SEARCH_MAX_RESULTS 32u

/* Радиусы проходов: 1 км, затем вдвое больше до PURRGO_POI_SEARCH_MAX_RADIUS_KM включительно. */
#define PURRGO_POI_SEARCH_MIN_RADIUS_KM 1u
#define PURRGO_POI_SEARCH_MAX_RADIUS_KM 64u

/* Название результата: 22 символа подписи карты и завершающий ноль. */
#define PURRGO_POI_SEARCH_NAME_SIZE 23u

/* Откуда считаются расстояния. */
typedef enum {
    PURRGO_POI_ORIGIN_GNSS = 0,   /* текущий фикс GNSS */
    PURRGO_POI_ORIGIN_LAST,       /* последняя известная позиция GNSS (фикса сейчас нет) */
    PURRGO_POI_ORIGIN_MAP         /* центр вида карты */
} purrgo_poi_origin_t;

typedef enum {
    PURRGO_POI_SEARCH_IDLE = 0,   /* запроса нет (или отменён) */
    PURRGO_POI_SEARCH_RUNNING,    /* идут проходы, purrgo_poi_search_step() продолжает */
    PURRGO_POI_SEARCH_DONE,       /* список готов (может быть пустым) */
    PURRGO_POI_SEARCH_ERROR       /* нет pois.idx, нарушение формата или ошибка чтения SD */
} purrgo_poi_search_state_t;

/* ---------------------------------------------------------------------------
 * Категории
 * ------------------------------------------------------------------------- */

/* Число категорий. */
size_t purrgo_poi_category_count(void);

/* Название "RU / EN" (UTF-8, в прошивке выводится через purrgo_utf8_to_pgo_inplace()). */
const char *purrgo_poi_category_title(size_t category);

/* Битовая маска кодов Type: бит (код - 128) для кодов 128..191 (map-format-v4.md, раздел 8.1.1). */
uint64_t purrgo_poi_category_mask(size_t category);

/* ---------------------------------------------------------------------------
 * Поиск
 * ------------------------------------------------------------------------- */

/*
 * Начать поиск категории category в карте map_dir от точки (lat_1e7, lon_1e7). origin - только
 * подпись для экрана. Отменяет прежний поиск и очищает результаты. false (состояние ERROR) -
 * категории нет или карта не открывается; в этом случае purrgo_poi_search_step() ничего не делает.
 */
bool purrgo_poi_search_start(const char *map_dir, size_t category, purrgo_poi_origin_t origin,
                             int32_t lat_1e7, int32_t lon_1e7);

/*
 * Один проход поиска (чтение индекса с SD). Возвращает состояние после прохода: RUNNING -
 * вызвать ещё раз, DONE или ERROR - конец. Вне RUNNING ничего не делает.
 */
purrgo_poi_search_state_t purrgo_poi_search_step(void);

/* Забыть запрос и результаты (BACK на экране результатов). */
void purrgo_poi_search_cancel(void);

purrgo_poi_search_state_t purrgo_poi_search_state(void);
bool purrgo_poi_search_running(void);

/* ---------------------------------------------------------------------------
 * Результаты (по возрастанию расстояния)
 * ------------------------------------------------------------------------- */

size_t purrgo_poi_search_count(void);
purrgo_poi_origin_t purrgo_poi_search_origin(void);

/* Радиус последнего выполненного прохода, км (0 - проходов ещё не было). */
uint32_t purrgo_poi_search_radius_km(void);

/* Категория текущего запроса. */
size_t purrgo_poi_search_category(void);

/* Объект index (< purrgo_poi_search_count()): координаты 1e-7 градуса и код Type (иконка). */
bool purrgo_poi_search_position(size_t index, int32_t *lat_1e7, int32_t *lon_1e7);
uint8_t purrgo_poi_search_type(size_t index);

/* Расстояние от начала отсчёта, м, и азимут на объект, градусы (0 - север, по часовой). */
uint32_t purrgo_poi_search_distance_m(size_t index);
uint16_t purrgo_poi_search_azimuth_deg(size_t index);

/*
 * Название объекта (PurrGO-256, "_" -> пробел, не больше 22 символов; длиннее - ".."). Читается
 * из pois.db при первом обращении и запоминается. Пустая строка - имени нет (файла нет, ошибка
 * чтения). Указатель действителен до следующего purrgo_poi_search_start()/cancel().
 */
const char *purrgo_poi_search_name(size_t index);

#endif /* PURRGO_POI_SEARCH_H */
