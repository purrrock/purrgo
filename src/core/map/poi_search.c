#include "purrgo/poi_search.h"

#include "purrgo/geo.h"
#include "purrgo/logger.h"
#include "purrgo/map.h"
#include "map_idx.h"
#include "map_internal.h"

#include <string.h>

/*
 * Простой поиск объектов (docs/design/poi-search.md): обход LOD 0 pois.idx (в нём все POI) по
 * квадрату вокруг начала отсчёта, фильтр по маске кодов Type, топ-N по квадрату расстояния.
 * Радиус растёт 1 -> 2 -> 4 -> ... -> 64 км, пока N-й результат не окажется внутри круга радиуса
 * прохода (тогда список точен: всё, что ближе радиуса, лежит внутри квадрата).
 */

#define POI_CODE_FIRST 128u
#define POI_CODE_LAST  191u

/* ---------------------------------------------------------------------------
 * Категории (docs/design/poi-search.md, таблица "Категории")
 * ------------------------------------------------------------------------- */

#define BIT(code) (1ULL << ((code) - POI_CODE_FIRST))

typedef struct {
    const char *title;   /* "RU / EN"; на экране 176 px - не больше 27 символов 5x7 */
    uint64_t mask;
} poi_category_t;

static const poi_category_t k_categories[] = {
    { "Жильё / Lodging",
      BIT(134) | BIT(135) | BIT(159) | BIT(165) | BIT(166) | BIT(167) },
    { "Еда и напитки / Food",
      BIT(133) | BIT(149) },
    { "Магазины / Shops",
      BIT(131) | BIT(136) | BIT(137) | BIT(138) | BIT(139) | BIT(140) | BIT(160) | BIT(161) },
    { "Здоровье / Health",
      BIT(132) },
    { "Вода / Water",
      BIT(143) },
    { "Туалеты, душ / Toilets",
      BIT(142) | BIT(150) },
    { "АЗС / Fuel",
      BIT(145) },
    { "Транспорт / Transport",
      BIT(146) | BIT(147) },
    { "Банки и банкоматы / Money",
      BIT(151) },
    { "Экстренные / Emergency",
      BIT(130) | BIT(156) },
    { "Интересное / Sights",
      BIT(141) | BIT(153) | BIT(154) | BIT(157) | BIT(162) | BIT(163) | BIT(164) },
    { "Перевалы / Passes",
      BIT(155) },
    { "Населённые пункты / Towns",
      BIT(128) | BIT(129) },
};

#define CATEGORY_COUNT (sizeof(k_categories) / sizeof(k_categories[0]))

size_t purrgo_poi_category_count(void)
{
    return CATEGORY_COUNT;
}

const char *purrgo_poi_category_title(size_t category)
{
    return (category < CATEGORY_COUNT) ? k_categories[category].title : "";
}

uint64_t purrgo_poi_category_mask(size_t category)
{
    return (category < CATEGORY_COUNT) ? k_categories[category].mask : 0u;
}

/* ---------------------------------------------------------------------------
 * Состояние поиска
 * ------------------------------------------------------------------------- */

typedef struct {
    int32_t  lat_1e7;
    int32_t  lon_1e7;
    uint64_t d2;                 /* квадрат расстояния до начала отсчёта, (1e-7 градуса)^2 */
    uint32_t db_index;           /* v2: номер записи в pois.db */
    uint8_t  type;
    uint8_t  name_state;         /* 0 - не читалось, 1 - прочитано, 2 - имени нет */
    char     name[PURRGO_POI_SEARCH_NAME_SIZE];
} poi_hit_t;

typedef struct {
    purrgo_poi_search_state_t state;
    purrgo_poi_origin_t origin;
    size_t   category;
    uint64_t mask;
    int32_t  lat_1e7;            /* начало отсчёта */
    int32_t  lon_1e7;
    int32_t  cos_10k;            /* cos широты начала отсчёта * 10000 (не меньше 100) */
    uint32_t next_radius_km;     /* радиус следующего прохода */
    uint32_t radius_km;          /* радиус последнего выполненного прохода */
    size_t   count;
    char     map_dir[PURRGO_FS_MAX_PATH + 32];
    poi_hit_t hit[PURRGO_POI_SEARCH_MAX_RESULTS];
} poi_search_t;

static poi_search_t s;

/* Контекст одного прохода (leaf-обработчик map_idx_visit_section()). */
typedef struct {
    purrgo_bbox_t cam;
} poi_pass_t;

/* Квадрат расстояния от начала отсчёта до точки; разность долгот берётся через антимеридиан. */
static uint64_t poi_d2(int32_t lat_1e7, int32_t lon_1e7)
{
    const int64_t dy = (int64_t)lat_1e7 - (int64_t)s.lat_1e7;
    int64_t dlon = (int64_t)lon_1e7 - (int64_t)s.lon_1e7;

    if (dlon > 1800000000LL) {
        dlon -= 3600000000LL;
    } else if (dlon < -1800000000LL) {
        dlon += 3600000000LL;
    }

    const int64_t dx = (dlon * (int64_t)s.cos_10k) / 10000;

    return (uint64_t)(dx * dx) + (uint64_t)(dy * dy);
}

/* Вставка в массив топ-N по возрастанию d2; при равенстве новая запись идёт после прежних. */
static void poi_insert(const poi_hit_t *h)
{
    size_t pos = s.count;

    if (s.count == PURRGO_POI_SEARCH_MAX_RESULTS) {
        if (h->d2 >= s.hit[s.count - 1u].d2) {
            return;
        }
        pos = s.count - 1u;   /* вытесняем худшую */
    } else {
        s.count++;
    }

    while (pos > 0u && h->d2 < s.hit[pos - 1u].d2) {
        s.hit[pos] = s.hit[pos - 1u];
        pos--;
    }
    s.hit[pos] = *h;
}

static void poi_leaf(void *ctx, const uint8_t *node)
{
    const poi_pass_t *pass = (const poi_pass_t *)ctx;
    const uint8_t type = node[16];

    if (type < POI_CODE_FIRST || type > POI_CODE_LAST ||
        (s.mask & BIT(type)) == 0u) {
        return;
    }

    /* POI - точка: bbox вырожденный, координата в (xmin, ymin). */
    const int32_t lon = unpack_i32_le(&node[0]);
    const int32_t lat = unpack_i32_le(&node[4]);

    if (lat < pass->cam.min_y || lat > pass->cam.max_y) {
        return;
    }
    if (pass->cam.min_x <= pass->cam.max_x) {
        if (lon < pass->cam.min_x || lon > pass->cam.max_x) {
            return;
        }
    } else if (lon < pass->cam.min_x && lon > pass->cam.max_x) {
        return;   /* камера через антимеридиан */
    }

    poi_hit_t h;
    h.lat_1e7 = lat;
    h.lon_1e7 = lon;
    h.d2 = poi_d2(lat, lon);
    h.db_index = unpack_u32_le(&node[21]);
    h.type = type;
    h.name_state = 0u;
    h.name[0] = '\0';
    poi_insert(&h);
}

/* ---------------------------------------------------------------------------
 * Управление
 * ------------------------------------------------------------------------- */

bool purrgo_poi_search_start(const char *map_dir, size_t category, purrgo_poi_origin_t origin,
                             int32_t lat_1e7, int32_t lon_1e7)
{
    memset(&s, 0, sizeof s);
    s.state = PURRGO_POI_SEARCH_ERROR;

    if (map_dir == NULL || map_dir[0] == '\0' || category >= CATEGORY_COUNT ||
        strlen(map_dir) >= sizeof s.map_dir) {
        return false;
    }

    s.origin = origin;
    s.category = category;
    s.mask = k_categories[category].mask;
    s.lat_1e7 = lat_1e7;
    s.lon_1e7 = lon_1e7;
    s.cos_10k = purrgo_geo_cos_10k(lat_1e7);
    if (s.cos_10k < 100) {
        s.cos_10k = 100;   /* у полюса долгота не сжимается до нуля */
    }
    s.next_radius_km = PURRGO_POI_SEARCH_MIN_RADIUS_KM;
    strcpy(s.map_dir, map_dir);

    /* Кэш секторов карты в арене: без него повторное чтение верхних узлов обходится дорого. */
    map_ram_enter();

    s.state = PURRGO_POI_SEARCH_RUNNING;
    return true;
}

/* true - N-й результат внутри круга радиуса radius_km: список не изменится при расширении. */
static bool poi_list_exact(uint32_t radius_km)
{
    if (s.count < PURRGO_POI_SEARCH_MAX_RESULTS) {
        return false;
    }
    const uint64_t r = (uint64_t)radius_km * 1000u * PURRGO_1E7_PER_METER;

    return s.hit[s.count - 1u].d2 <= r * r;
}

purrgo_poi_search_state_t purrgo_poi_search_step(void)
{
    if (s.state != PURRGO_POI_SEARCH_RUNNING) {
        return s.state;
    }

    purrgo_file_t *idx = NULL;
    purrgo_file_t *db = NULL;
    map_section_t sec;

    if (!purrgo_map_poi_files(s.map_dir, &idx, &db) || !map_idx_open_lod(idx, 0u, &sec)) {
        PURRGO_LOGW("poi search: no usable pois.idx in %s\n\r", s.map_dir);
        s.state = PURRGO_POI_SEARCH_ERROR;
        return s.state;
    }

    const uint32_t radius_km = s.next_radius_km;

    /* Квадрат со стороной 2R вокруг начала отсчёта (пропорции 1:1, долгота - по косинусу широты). */
    static const purrgo_viewport_t square = { 1u, 1u, 0, 0 };
    poi_pass_t pass;

    purrgo_geo_bbox_from_center(s.lat_1e7, s.lon_1e7, 2u * radius_km * 1000u, &square, &pass.cam);

    /* Каждый проход считает список заново: объекты малого радиуса попадут в него снова. */
    s.count = 0u;
    s.radius_km = radius_km;

    if (!map_idx_visit_section(idx, &sec, &pass.cam, poi_leaf, &pass)) {
        PURRGO_LOGW("poi search: pois.idx read error\n\r");
        s.state = PURRGO_POI_SEARCH_ERROR;
        return s.state;
    }

    if (poi_list_exact(radius_km) || radius_km >= PURRGO_POI_SEARCH_MAX_RADIUS_KM) {
        s.state = PURRGO_POI_SEARCH_DONE;
    } else {
        s.next_radius_km = radius_km * 2u;
    }
    return s.state;
}

void purrgo_poi_search_cancel(void)
{
    memset(&s, 0, sizeof s);
    s.state = PURRGO_POI_SEARCH_IDLE;
}

purrgo_poi_search_state_t purrgo_poi_search_state(void)
{
    return s.state;
}

bool purrgo_poi_search_running(void)
{
    return s.state == PURRGO_POI_SEARCH_RUNNING;
}

/* ---------------------------------------------------------------------------
 * Результаты
 * ------------------------------------------------------------------------- */

size_t purrgo_poi_search_count(void)
{
    return (s.state == PURRGO_POI_SEARCH_DONE) ? s.count : 0u;
}

purrgo_poi_origin_t purrgo_poi_search_origin(void)
{
    return s.origin;
}

uint32_t purrgo_poi_search_radius_km(void)
{
    return s.radius_km;
}

size_t purrgo_poi_search_category(void)
{
    return s.category;
}

bool purrgo_poi_search_position(size_t index, int32_t *lat_1e7, int32_t *lon_1e7)
{
    if (index >= purrgo_poi_search_count() || lat_1e7 == NULL || lon_1e7 == NULL) {
        return false;
    }
    *lat_1e7 = s.hit[index].lat_1e7;
    *lon_1e7 = s.hit[index].lon_1e7;
    return true;
}

uint8_t purrgo_poi_search_type(size_t index)
{
    return (index < purrgo_poi_search_count()) ? s.hit[index].type : 0u;
}

uint32_t purrgo_poi_search_distance_m(size_t index)
{
    if (index >= purrgo_poi_search_count()) {
        return 0u;
    }
    return purrgo_geo_distance_m(s.lat_1e7, s.lon_1e7, s.hit[index].lat_1e7, s.hit[index].lon_1e7);
}

uint16_t purrgo_poi_search_azimuth_deg(size_t index)
{
    if (index >= purrgo_poi_search_count()) {
        return 0u;
    }
    return purrgo_geo_azimuth_deg(s.lat_1e7, s.lon_1e7, s.hit[index].lat_1e7, s.hit[index].lon_1e7);
}

const char *purrgo_poi_search_name(size_t index)
{
    if (index >= purrgo_poi_search_count()) {
        return "";
    }

    poi_hit_t *h = &s.hit[index];

    if (h->name_state == 0u) {
        purrgo_file_t *idx = NULL;
        purrgo_file_t *db = NULL;

        h->name_state = 2u;
        h->name[0] = '\0';
        if (purrgo_map_poi_files(s.map_dir, &idx, &db) && db != NULL &&
            map_label_read(db, h->db_index, true, h->name)) {
            h->name_state = 1u;
        } else {
            h->name[0] = '\0';
        }
    }
    return h->name;
}
