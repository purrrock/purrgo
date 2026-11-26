#ifndef PURRGO_MAP_SEARCH_H
#define PURRGO_MAP_SEARCH_H

/*
 * Умный поиск объектов карты: слово-категория и/или начало названия, список ближайших объектов
 * обновляется по мере набора. Проект: docs/design/poi-search.md, формат индекса: docs/map-format-v4.md,
 * раздел 21 (search.idx).
 *
 * Ищутся POI, улицы и дороги (roads), реки и ручьи (waterways), водоёмы (water) и землепользование
 * (landuse). Слой объекта определяется его кодом Type (диапазоны кодов - раздел 8.1.1).
 *
 * Работа порциями, как у простого поиска (purrgo/poi_search.h): purrgo_map_search_set_query() только
 * разбирает запрос и ставит расчёт в очередь, каждый purrgo_map_search_step() читает не больше
 * PURRGO_MAP_SEARCH_CHUNK записей индекса, между шагами приложение обрабатывает кнопки. Новый запрос
 * отменяет текущий расчёт. Без float, без выделения памяти: все массивы фиксированного размера.
 *
 * Пока идёт расчёт нового запроса, accessor'ы результатов отдают список прежнего запроса (экран не
 * мигает); он заменяется целиком, когда расчёт закончен.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "purrgo/poi_search.h"   /* purrgo_poi_origin_t: GNSS / LAST / MAP */

/* Результатов в списке. */
#define PURRGO_MAP_SEARCH_MAX_RESULTS 32u

/* Название результата: 22 символа подписи карты и завершающий ноль. */
#define PURRGO_MAP_SEARCH_NAME_SIZE 23u

/* Сколько записей индекса читает один purrgo_map_search_step() (16 записей - один сектор SD). */
#define PURRGO_MAP_SEARCH_CHUNK 256u

/*
 * Наименьшая длина (байт) слова названия, с которой оно участвует в поиске. Префикс из одного байта
 * выбрал бы большую часть секции NAME: пока он единственный, список не строится. Если в запросе
 * есть категория, однобайтное слово названия просто не учитывается (человек ещё печатает).
 */
#define PURRGO_MAP_SEARCH_MIN_PREFIX 2u

/* Слой объекта (по коду Type). */
typedef enum {
    PURRGO_MS_LAYER_POI = 0,
    PURRGO_MS_LAYER_ROADS,
    PURRGO_MS_LAYER_WATERWAYS,
    PURRGO_MS_LAYER_WATER,
    PURRGO_MS_LAYER_LANDUSE,
    PURRGO_MS_LAYER_COUNT
} purrgo_ms_layer_t;

typedef enum {
    PURRGO_MS_IDLE = 0,   /* индекс не открыт или запроса нет */
    PURRGO_MS_RUNNING,    /* идёт расчёт, purrgo_map_search_step() продолжает */
    PURRGO_MS_DONE,       /* список готов (может быть пустым) */
    PURRGO_MS_ERROR       /* ошибка чтения SD или нарушение формата */
} purrgo_ms_state_t;

/* Что понято из запроса (подсказка для строки состояния экрана). */
typedef enum {
    PURRGO_MS_KIND_EMPTY = 0,   /* текста нет */
    PURRGO_MS_KIND_SHORT,       /* есть только слово названия короче PURRGO_MAP_SEARCH_MIN_PREFIX */
    PURRGO_MS_KIND_CATEGORY,    /* только слова-категории */
    PURRGO_MS_KIND_NAME,        /* название (и категория, если есть) */
    PURRGO_MS_KIND_BOTH         /* одно слово, похожее на начало слова-категории: ищутся и категория, и название */
} purrgo_ms_kind_t;

/* ---------------------------------------------------------------------------
 * Нормализация ключа (map-format-v4.md, 21.7). Те же правила, что в компиляторе
 * (tools/map-compiler/purrgo_search.py); совпадение проверяют общие тестовые векторы.
 * ------------------------------------------------------------------------- */

/* Свёртка байта PurrGO-256: регистр, Ё -> Е. */
uint8_t purrgo_search_fold_byte(uint8_t byte);

/* true, если свёрнутый байт - байт слова (цифра или буква); остальные байты - разделители. */
bool purrgo_search_is_word_byte(uint8_t folded);

/*
 * Режет text (len байт PurrGO-256) на слова. words[i] - ключ слова (первые 16 байт после свёртки,
 * дополнены нулями), lens[i] - длина ключа (1..16). Возвращает число слов (не больше max_words);
 * слова сверх max_words отбрасываются.
 */
size_t purrgo_search_split(const uint8_t *text, size_t len, uint8_t (*words)[16], uint8_t *lens,
                           size_t max_words);

/* ---------------------------------------------------------------------------
 * Управление
 * ------------------------------------------------------------------------- */

/*
 * Открыть search.idx каталога map_dir и проверить его (раздел 21.10), включая число записей баз
 * pois.db, roads.db, waterways.db, water.db и landuse.db. false - индекса нет или он не прошёл
 * проверку (поиск по названию недоступен, остаётся простой поиск по категориям). Прежний
 * открытый индекс и результаты сбрасываются. Занимает один слот файловой системы (и ещё один под
 * очередной файл .db при чтении названий); слоты, которые держит карта, не используются.
 */
bool purrgo_map_search_open(const char *map_dir);

/* Закрыть файлы поиска и забыть запрос и результаты. */
void purrgo_map_search_close(void);

bool purrgo_map_search_is_open(void);

/* Начало отсчёта расстояний (источник - подпись на экране). Действует с ближайшего set_query. */
void purrgo_map_search_set_origin(purrgo_poi_origin_t origin, int32_t lat_1e7, int32_t lon_1e7);

/*
 * Новый текст запроса (PurrGO-256, как вводит клавиатура). Отменяет текущий расчёт. Пустой или
 * слишком короткий запрос сразу очищает список (состояние DONE), иначе состояние RUNNING.
 */
void purrgo_map_search_set_query(const char *text);

/* Одна порция расчёта. Возвращает состояние после неё; вне RUNNING ничего не делает. */
purrgo_ms_state_t purrgo_map_search_step(void);

purrgo_ms_state_t purrgo_map_search_state(void);
bool purrgo_map_search_running(void);
purrgo_ms_kind_t purrgo_map_search_kind(void);

/* ---------------------------------------------------------------------------
 * Результаты (по возрастанию расстояния); пока расчёт идёт - список прежнего запроса
 * ------------------------------------------------------------------------- */

size_t purrgo_map_search_count(void);
purrgo_poi_origin_t purrgo_map_search_origin(void);

/* Координаты (1e-7 градуса), код Type и слой объекта index. */
bool purrgo_map_search_position(size_t index, int32_t *lat_1e7, int32_t *lon_1e7);
uint8_t purrgo_map_search_type(size_t index);
purrgo_ms_layer_t purrgo_map_search_layer(size_t index);

/* Расстояние от начала отсчёта, м, и азимут на объект, градусы (0 - север, по часовой). */
uint32_t purrgo_map_search_distance_m(size_t index);
uint16_t purrgo_map_search_azimuth_deg(size_t index);

/*
 * Название объекта (PurrGO-256, "_" -> пробел, не больше 22 символов; длиннее - ".."). Читается
 * из базы слоя при первом обращении и запоминается. Пустая строка - имени нет (ошибка чтения).
 * Указатель действителен до следующего изменения списка (set_query с готовым результатом, close).
 */
const char *purrgo_map_search_name(size_t index);

#endif /* PURRGO_MAP_SEARCH_H */
