#include "purrgo/map_search.h"

#include "purrgo/fs_hal.h"
#include "purrgo/geo.h"
#include "purrgo/logger.h"
#include "purrgo/map.h"
#include "purrgo/purrgo_format.h"
#include "map_idx.h"
#include "map_internal.h"

#include <string.h>

/*
 * Умный поиск (docs/design/poi-search.md, формат - docs/map-format-v4.md, раздел 21).
 *
 * Запрос режется на слова и делится на слова-категории (совпали со словарём индекса целиком или как
 * префикс от 3 байт) и слова названия. Дальше один или два плана расчёта:
 *   CAT  - секция CAT (записи отсортированы по подтипу и коду Мортона): для каждого подтипа маски
 *          берётся его участок; маленький участок читается целиком, большой - по квадрату вокруг
 *          начала отсчёта, радиус которого растёт 1 -> 4 -> 16 -> 64 км (квадрат раскладывается на
 *          несколько интервалов кода Мортона, в каждом бинарный поиск и чтение подряд);
 *   NAME - секция NAME (записи отсортированы по ключу слова): диапазон слов, начинающихся с первого
 *          префикса запроса, находится двумя бинарными поисками и читается подряд, остальные
 *          префиксы проверяются по полному названию из базы слоя.
 * Лучшие N объектов по расстоянию хранятся в массиве фиксированного размера; объект, найденный
 * дважды (два слова названия или два плана), учитывается один раз.
 */

#define SRCH_HDR_OFF    0x20u
#define SRCH_HDR_SIZE   64u
#define REC_SIZE        32u
#define KEY_SIZE        16u
#define DICT_HDR_SIZE   8u
#define DICT_REC_SIZE   48u
#define SECTOR          512u
#define RECS_PER_SECTOR (SECTOR / REC_SIZE)

#define MAX_WORDS       6u
#define DICT_SCAN_MAX   64u
#define MAX_RANGES      32u

/* Участок подтипа в CAT до этого размера (записей) читается целиком, без квадрата по радиусу. */
#define CAT_LINEAR_LIMIT 1024u

#define DB_HDR_SIZE     129u
#define DB_REC_SIZE     117u
#define DB_NAME_OFF     17u
#define DB_NAME_SIZE    100u

#define MAX_RADIUS_KM   64u

/* Смещения полей записи (раздел 21.4). */
#define E_LON  0u
#define E_LAT  4u
#define E_DBI  8u
#define E_CODE 12u
#define E_SUB  13u
#define E_KEY  16u

static const char *const k_db_name[PURRGO_MS_LAYER_COUNT] = {
    "pois.db", "roads.db", "waterways.db", "water.db", "landuse.db"
};

/* ---------------------------------------------------------------------------
 * Нормализация ключа (21.7)
 * ------------------------------------------------------------------------- */

uint8_t purrgo_search_fold_byte(uint8_t b)
{
    if (b >= 0x41u && b <= 0x5Au) {
        return (uint8_t)(b + 0x20u);
    }
    if (b >= 0x80u && b <= 0x9Fu) {
        return (uint8_t)(b + 0x20u);
    }
    if (b >= 0xC0u && b <= 0xCFu) {
        return (uint8_t)(b + 0x10u);
    }
    if (b == 0xF0u || b == 0xF1u) {
        return 0xA5u;
    }
    return b;
}

bool purrgo_search_is_word_byte(uint8_t f)
{
    return (f >= 0x30u && f <= 0x39u) || (f >= 0x61u && f <= 0x7Au) ||
           (f >= 0xA0u && f <= 0xBFu) || (f >= 0xD0u && f <= 0xDFu) ||
           (f >= 0xE0u && f <= 0xEFu);
}

size_t purrgo_search_split(const uint8_t *text, size_t len, uint8_t (*words)[16], uint8_t *lens,
                           size_t max_words)
{
    size_t n = 0;
    size_t cur = 0;       /* байт в текущем слове (может быть больше 16) */
    bool in_word = false;

    for (size_t i = 0; i <= len; i++) {
        const uint8_t f = (i < len) ? purrgo_search_fold_byte(text[i]) : 0u;

        if (i < len && purrgo_search_is_word_byte(f)) {
            if (!in_word) {
                if (n >= max_words) {
                    return n;
                }
                memset(words[n], 0, KEY_SIZE);
                in_word = true;
                cur = 0;
            }
            if (cur < KEY_SIZE) {
                words[n][cur] = f;
            }
            cur++;
        } else if (in_word) {
            lens[n] = (uint8_t)((cur < KEY_SIZE) ? cur : KEY_SIZE);
            n++;
            in_word = false;
        }
    }
    return n;
}

/* ---------------------------------------------------------------------------
 * Состояние
 * ------------------------------------------------------------------------- */

typedef struct {
    int32_t  lat_1e7;
    int32_t  lon_1e7;
    uint64_t d2;                 /* квадрат расстояния до начала отсчёта, (1e-7 градуса)^2 */
    uint32_t db_idx;
    uint8_t  code;
    uint8_t  layer;
    uint8_t  name_state;         /* 0 - не читалось, 1 - прочитано, 2 - имени нет */
    char     name[PURRGO_MAP_SEARCH_NAME_SIZE];
} hit_t;

typedef enum {
    CP_NEXT_SUB = 0, CP_LINEAR, CP_RADIUS_BEGIN, CP_RNG_LOCATE, CP_RNG_SCAN, CP_RADIUS_END
} cat_phase_t;

typedef struct {
    uint64_t lo;
    uint64_t hi;
} zrange_t;

typedef struct {
    /* индекс */
    purrgo_file_t *file;
    bool     open;
    char     dir[PURRGO_FS_MAX_PATH + 32];
    uint32_t size;
    uint32_t name_off, name_cnt, cat_off, cat_cnt, dict_off, dict_cnt;
    uint32_t blk_pos;            /* смещение закэшированного сектора, UINT32_MAX - нет */
    uint8_t  blk[SECTOR];

    /* база слоя, открытая для чтения названий */
    purrgo_file_t *db;
    int8_t   db_layer;

    /* начало отсчёта */
    purrgo_poi_origin_t origin;
    int32_t  lat_1e7, lon_1e7;
    int32_t  cos_10k;

    /* запрос */
    purrgo_ms_state_t state;
    purrgo_ms_kind_t  kind;
    uint8_t  mask[32];
    bool     has_mask;
    bool     filter_mask;        /* NAME: фильтровать по маске подтипов */
    uint8_t  nwords;             /* слова названия: [0] - диапазон NAME, остальные - проверка */
    uint8_t  name_word[MAX_WORDS][16];
    uint8_t  name_len[MAX_WORDS];
    bool     plan_cat, plan_name, cat_done, name_done;

    /* CAT */
    cat_phase_t cphase;
    uint16_t sub;
    uint32_t prev_hi, c_lo, c_hi, c_cur;
    uint32_t radius_km;
    purrgo_bbox_t cam;
    uint8_t  nrng, irng;
    zrange_t rng[MAX_RANGES];

    /* NAME */
    bool     name_located;
    uint32_t n_cur, n_end;
    bool     prev_valid;         /* диапазон прежнего запроса: новый, продолжающий его, ищется внутри */
    uint8_t  prev_key[16];
    uint8_t  prev_len;
    uint32_t prev_lo, prev_hi_n;

    /* результаты: расчёт идёт в live, показывается shown */
    size_t   live_count;
    hit_t    live[PURRGO_MAP_SEARCH_MAX_RESULTS];
    size_t   shown_count;
    hit_t    shown[PURRGO_MAP_SEARCH_MAX_RESULTS];
} search_t;

static search_t s;

/* ---------------------------------------------------------------------------
 * Чтение индекса
 * ------------------------------------------------------------------------- */

static bool read_at(purrgo_file_t *f, uint32_t off, void *buf, uint32_t n)
{
    return f != NULL && purrgo_fs_seek(f, off) && purrgo_fs_read(f, (uint8_t *)buf, n) == n;
}

/* Запись idx секции с началом base; NULL - ошибка чтения. */
static const uint8_t *get_rec(uint32_t base, uint32_t idx)
{
    const uint32_t pos = base + (idx / RECS_PER_SECTOR) * SECTOR;

    if (s.blk_pos != pos) {
        if (!read_at(s.file, pos, s.blk, SECTOR)) {
            s.blk_pos = UINT32_MAX;
            return NULL;
        }
        s.blk_pos = pos;
    }
    return &s.blk[(idx % RECS_PER_SECTOR) * REC_SIZE];
}

static uint64_t spread32(uint32_t v)
{
    uint64_t x = v;

    x = (x | (x << 16)) & 0x0000FFFF0000FFFFULL;
    x = (x | (x << 8))  & 0x00FF00FF00FF00FFULL;
    x = (x | (x << 4))  & 0x0F0F0F0F0F0F0F0FULL;
    x = (x | (x << 2))  & 0x3333333333333333ULL;
    x = (x | (x << 1))  & 0x5555555555555555ULL;
    return x;
}

static uint64_t morton64(uint32_t x, uint32_t y)
{
    return spread32(x) | (spread32(y) << 1);
}

static uint32_t to_x(int32_t lon)
{
    return (uint32_t)((int64_t)lon + 1800000000LL);
}

static uint32_t to_y(int32_t lat)
{
    return (uint32_t)((int64_t)lat + 900000000LL);
}

static uint64_t rec_morton(const uint8_t *e)
{
    return morton64(to_x(unpack_i32_le(&e[E_LON])), to_y(unpack_i32_le(&e[E_LAT])));
}

typedef enum { BS_KEY_GE, BS_KEY_GT, BS_SUB_GE, BS_MORTON_GE } bs_kind_t;

/*
 * Бинарный поиск в [lo, hi) секции base: первая запись, для которой условие kind истинно
 * (записи отсортированы так, что условие монотонно). false - ошибка чтения.
 */
static bool bsearch_sec(uint32_t base, uint32_t lo, uint32_t hi, bs_kind_t kind,
                        const uint8_t *key, uint32_t value, uint64_t z, uint32_t *out)
{
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2u;
        const uint8_t *e = get_rec(base, mid);
        bool ok;

        if (e == NULL) {
            return false;
        }
        switch (kind) {
            case BS_KEY_GE:     ok = memcmp(&e[E_KEY], key, KEY_SIZE) >= 0; break;
            case BS_KEY_GT:     ok = memcmp(&e[E_KEY], key, KEY_SIZE) > 0;  break;
            case BS_SUB_GE:     ok = (uint32_t)e[E_SUB] >= value;           break;
            default:            ok = rec_morton(e) >= z;                    break;
        }
        if (ok) {
            hi = mid;
        } else {
            lo = mid + 1u;
        }
    }
    *out = lo;
    return true;
}

/* ---------------------------------------------------------------------------
 * Слои и базы названий
 * ------------------------------------------------------------------------- */

static int layer_of_code(uint8_t code)
{
    if (code < 32u)  { return PURRGO_MS_LAYER_ROADS; }
    if (code < 64u)  { return PURRGO_MS_LAYER_WATERWAYS; }
    if (code < 96u)  { return PURRGO_MS_LAYER_WATER; }
    if (code < 128u) { return PURRGO_MS_LAYER_LANDUSE; }
    if (code < 192u) { return PURRGO_MS_LAYER_POI; }
    return -1;
}

static bool make_path(char *out, size_t size, const char *name)
{
    return purrgo_snprintf(out, size, "%s/%s", s.dir, name) < (int)size;
}

static void db_close(void)
{
    if (s.db != NULL) {
        (void)purrgo_fs_close(s.db);
        s.db = NULL;
    }
    s.db_layer = -1;
}

/* База слоя для чтения названий; открытой держится одна, при смене слоя прежняя закрывается. */
static purrgo_file_t *db_for(int layer)
{
    char path[PURRGO_FS_MAX_PATH + 48];

    if (layer < 0 || layer >= (int)PURRGO_MS_LAYER_COUNT) {
        return NULL;
    }
    if (s.db != NULL && s.db_layer == layer) {
        return s.db;
    }
    db_close();
    if (!make_path(path, sizeof path, k_db_name[layer])) {
        return NULL;
    }
    s.db = purrgo_fs_open(path, FS_READ);
    s.db_layer = (s.db != NULL) ? (int8_t)layer : (int8_t)-1;
    return s.db;
}

/* Полное поле названия записи v2 базы слоя. */
static bool read_full_name(int layer, uint32_t v2, uint8_t raw[DB_NAME_SIZE])
{
    purrgo_file_t *f = db_for(layer);

    if (f == NULL || v2 == 0u) {
        return false;
    }
    return read_at(f, 32u + DB_HDR_SIZE + (v2 - 1u) * DB_REC_SIZE + DB_NAME_OFF, raw, DB_NAME_SIZE);
}

/* true, если каждое слово названия запроса (кроме первого) начинает какое-то слово названия объекта. */
static bool verify_extra(int layer, uint32_t v2)
{
    uint8_t raw[DB_NAME_SIZE];
    uint8_t words[24][16];
    uint8_t lens[24];

    if (!read_full_name(layer, v2, raw)) {
        return false;
    }

    size_t len = 0;

    while (len < DB_NAME_SIZE && raw[len] != 0u) {
        len++;
    }

    const size_t n = purrgo_search_split(raw, len, words, lens, 24u);

    for (uint8_t q = 1u; q < s.nwords; q++) {
        bool found = false;

        for (size_t w = 0; w < n && !found; w++) {
            found = lens[w] >= s.name_len[q] && memcmp(words[w], s.name_word[q], s.name_len[q]) == 0;
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

/* ---------------------------------------------------------------------------
 * Открытие и проверка (21.10)
 * ------------------------------------------------------------------------- */

static void reset_query(void)
{
    s.state = s.open ? PURRGO_MS_DONE : PURRGO_MS_IDLE;
    s.kind = PURRGO_MS_KIND_EMPTY;
    s.plan_cat = s.plan_name = s.cat_done = s.name_done = false;
    s.live_count = 0;
}

void purrgo_map_search_close(void)
{
    if (s.file != NULL) {
        (void)purrgo_fs_close(s.file);
    }
    db_close();
    memset(&s, 0, sizeof s);
    s.db_layer = -1;
    s.blk_pos = UINT32_MAX;
    s.state = PURRGO_MS_IDLE;
}

/* Число записей базы (DBF, смещение 0x24) должно совпадать с тем, для чего построен индекс. */
static bool db_records_match(const char *name, uint32_t expect)
{
    char path[PURRGO_FS_MAX_PATH + 48];
    uint8_t b[4];

    if (expect == 0u) {
        return true;   /* слой в индексе не представлен */
    }
    if (!make_path(path, sizeof path, name)) {
        return false;
    }

    purrgo_file_t *f = purrgo_fs_open(path, FS_READ);
    const bool ok = f != NULL && read_at(f, 0x24u, b, 4u) && unpack_u32_le(b) == expect;

    if (f != NULL) {
        (void)purrgo_fs_close(f);
    }
    return ok;
}

bool purrgo_map_search_open(const char *map_dir)
{
    char path[PURRGO_FS_MAX_PATH + 48];
    uint8_t h[SRCH_HDR_OFF + SRCH_HDR_SIZE];

    purrgo_map_search_close();

    if (map_dir == NULL || map_dir[0] == '\0' || strlen(map_dir) >= sizeof s.dir) {
        return false;
    }
    strcpy(s.dir, map_dir);

    if (!make_path(path, sizeof path, "search.idx") || !purrgo_fs_exists(path)) {
        return false;
    }
    s.file = purrgo_fs_open(path, FS_READ);
    if (s.file == NULL) {
        return false;
    }

    bool ok = purrgo_fs_size(s.file, &s.size) && s.size >= sizeof h && read_at(s.file, 0, h, sizeof h);

    /* 1. заголовок PGO */
    ok = ok && h[0] == 'P' && h[1] == 'G' && h[2] == 'O' && h[3] == 4u &&
         (uint64_t)unpack_u32_le(&h[4]) + 32u <= s.size;

    const uint8_t *r = &h[SRCH_HDR_OFF];

    /* 2. SRCH: magic, версия 1, размер записи 32 */
    ok = ok && memcmp(r, "SRCH", 4) == 0 && r[4] == 1u && r[5] == 0u && r[6] == REC_SIZE && r[7] == 0u;

    uint32_t poi_records = 0;
    uint32_t layer_records[4] = { 0, 0, 0, 0 };
    uint32_t dict_size = 0;

    if (ok) {
        poi_records = unpack_u32_le(&r[0x0C]);
        s.name_off  = unpack_u32_le(&r[0x10]);
        s.name_cnt  = unpack_u32_le(&r[0x14]);
        s.cat_off   = unpack_u32_le(&r[0x18]);
        s.cat_cnt   = unpack_u32_le(&r[0x1C]);
        s.dict_off  = unpack_u32_le(&r[0x20]);
        dict_size   = unpack_u32_le(&r[0x24]);
        for (unsigned i = 0; i < 4u; i++) {
            layer_records[i] = unpack_u32_le(&r[0x30 + 4u * i]);
        }

        /* 6. каталог диапазонов не используется */
        ok = unpack_u32_le(&r[0x28]) == 0u && unpack_u32_le(&r[0x2C]) == 0u;
    }

    /* 3, 4. смещения и размеры секций */
    if (ok) {
        const uint64_t dict_end = (uint64_t)s.dict_off + dict_size;
        const uint64_t name_end = (uint64_t)s.name_off + (uint64_t)REC_SIZE * s.name_cnt;
        const uint64_t cat_end  = (uint64_t)s.cat_off + (uint64_t)REC_SIZE * s.cat_cnt;

        ok = s.dict_off >= SRCH_HDR_OFF + SRCH_HDR_SIZE && dict_end <= s.size &&
             s.name_off % SECTOR == 0u && s.cat_off % SECTOR == 0u &&
             dict_end <= s.name_off && name_end <= s.cat_off && cat_end <= s.size;
    }

    /* 7. словарь */
    if (ok) {
        uint8_t d[DICT_HDR_SIZE];

        ok = dict_size >= DICT_HDR_SIZE && read_at(s.file, s.dict_off, d, sizeof d);
        if (ok) {
            s.dict_cnt = (uint32_t)d[0] | ((uint32_t)d[1] << 8);
            ok = ((uint32_t)d[2] | ((uint32_t)d[3] << 8)) == DICT_REC_SIZE &&
                 dict_size == DICT_HDR_SIZE + DICT_REC_SIZE * s.dict_cnt;
        }
    }

    /* 5. индекс построен для этих баз */
    ok = ok && db_records_match(k_db_name[PURRGO_MS_LAYER_POI], poi_records);
    for (unsigned i = 0; ok && i < 4u; i++) {
        ok = db_records_match(k_db_name[i + 1u], layer_records[i]);
    }

    if (!ok) {
        PURRGO_LOGW("search.idx in %s is missing or invalid, name search is off\n\r", map_dir);
        purrgo_map_search_close();
        return false;
    }

    s.open = true;
    s.state = PURRGO_MS_DONE;
    s.blk_pos = UINT32_MAX;

    /* Кэш секторов карты в арене: бинарный поиск перечитывает одни и те же верхние сектора. */
    map_ram_enter();
    return true;
}

bool purrgo_map_search_is_open(void)
{
    return s.open;
}

void purrgo_map_search_set_origin(purrgo_poi_origin_t origin, int32_t lat_1e7, int32_t lon_1e7)
{
    s.origin = origin;
    s.lat_1e7 = lat_1e7;
    s.lon_1e7 = lon_1e7;
    s.cos_10k = purrgo_geo_cos_10k(lat_1e7);
    if (s.cos_10k < 100) {
        s.cos_10k = 100;   /* у полюса долгота не сжимается до нуля */
    }
}

/* ---------------------------------------------------------------------------
 * Разбор запроса (21.9)
 * ------------------------------------------------------------------------- */

typedef struct {
    bool exact;    /* слово совпало с ключевым словом целиком */
    bool prefix;   /* слово (от 3 байт) - начало ключевого слова */
    bool err;
} dict_match_t;

/* Ищет слово в словаре, объединяя в mask маски подошедших ключевых слов. */
static dict_match_t dict_lookup(const uint8_t w[16], uint8_t len, uint8_t mask[32])
{
    dict_match_t m = { false, false, false };
    uint32_t lo = 0;
    uint32_t hi = s.dict_cnt;
    uint8_t e[DICT_REC_SIZE];

    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2u;

        if (!read_at(s.file, s.dict_off + DICT_HDR_SIZE + mid * DICT_REC_SIZE, e, KEY_SIZE)) {
            m.err = true;
            return m;
        }
        if (memcmp(e, w, KEY_SIZE) >= 0) {
            hi = mid;
        } else {
            lo = mid + 1u;
        }
    }

    for (uint32_t i = lo, n = 0; i < s.dict_cnt && n < DICT_SCAN_MAX; i++, n++) {
        if (!read_at(s.file, s.dict_off + DICT_HDR_SIZE + i * DICT_REC_SIZE, e, DICT_REC_SIZE)) {
            m.err = true;
            return m;
        }
        if (memcmp(e, w, len) != 0) {
            break;
        }
        const bool exact = memcmp(e, w, KEY_SIZE) == 0;

        if (exact || len >= 3u) {
            if (exact) {
                m.exact = true;
            } else {
                m.prefix = true;
            }
            for (unsigned k = 0; k < 32u; k++) {
                mask[k] |= e[KEY_SIZE + k];
            }
        }
    }
    return m;
}

/* Разбор текста запроса в поля s (кроме состояния расчёта). false - ошибка чтения словаря. */
static bool parse_query(const char *text)
{
    uint8_t words[MAX_WORDS][16];
    uint8_t lens[MAX_WORDS];
    dict_match_t match[MAX_WORDS];
    const size_t len = (text != NULL) ? strlen(text) : 0u;
    const size_t n = purrgo_search_split((const uint8_t *)text, len, words, lens, MAX_WORDS);

    memset(s.mask, 0, sizeof s.mask);
    s.has_mask = false;
    s.filter_mask = false;
    s.nwords = 0;
    s.plan_cat = s.plan_name = false;
    s.kind = PURRGO_MS_KIND_EMPTY;

    if (n == 0u) {
        return true;
    }

    size_t n_exact = 0;
    size_t n_prefix = 0;
    size_t n_none = 0;
    size_t n_short = 0;

    for (size_t i = 0; i < n; i++) {
        match[i] = dict_lookup(words[i], lens[i], s.mask);
        if (match[i].err) {
            return false;
        }
        if (match[i].exact) {
            n_exact++;
        } else if (match[i].prefix) {
            n_prefix++;
        } else if (lens[i] >= PURRGO_MAP_SEARCH_MIN_PREFIX) {
            n_none++;
        } else {
            n_short++;
        }
    }
    s.has_mask = (n_exact + n_prefix) > 0u;

    if (n_none == 0u) {
        if (n == 1u && n_prefix == 1u) {
            /* "пар": может быть и началом слова-категории, и началом названия ("Парковая") */
            s.kind = PURRGO_MS_KIND_BOTH;
            s.plan_cat = s.plan_name = true;
            memcpy(s.name_word[0], words[0], KEY_SIZE);
            s.name_len[0] = lens[0];
            s.nwords = 1u;
        } else if (s.has_mask) {
            s.kind = PURRGO_MS_KIND_CATEGORY;
            s.plan_cat = true;
        } else {
            s.kind = PURRGO_MS_KIND_SHORT;   /* только однобайтные слова */
        }
        return true;
    }

    for (size_t i = 0; i < n; i++) {
        if (!match[i].exact && !match[i].prefix && lens[i] >= PURRGO_MAP_SEARCH_MIN_PREFIX) {
            memcpy(s.name_word[s.nwords], words[i], KEY_SIZE);
            s.name_len[s.nwords] = lens[i];
            s.nwords++;
        }
    }
    (void)n_short;
    s.kind = PURRGO_MS_KIND_NAME;
    s.plan_name = true;
    s.filter_mask = s.has_mask;
    return true;
}

/* ---------------------------------------------------------------------------
 * Результаты
 * ------------------------------------------------------------------------- */

/* Квадрат расстояния от начала отсчёта до точки; разность долгот берётся через антимеридиан. */
static uint64_t calc_d2(int32_t lat_1e7, int32_t lon_1e7)
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

static void live_insert(const hit_t *h)
{
    size_t pos = s.live_count;

    if (s.live_count == PURRGO_MAP_SEARCH_MAX_RESULTS) {
        pos = s.live_count - 1u;   /* вытесняем худшую */
    } else {
        s.live_count++;
    }
    while (pos > 0u && h->d2 < s.live[pos - 1u].d2) {
        s.live[pos] = s.live[pos - 1u];
        pos--;
    }
    s.live[pos] = *h;
}

/* Запись индекса - кандидат: фильтры, дубликаты, проверка остальных слов названия, вставка. */
static void consider(const uint8_t *e, bool name_plan)
{
    const uint8_t code = e[E_CODE];
    const uint8_t sub = e[E_SUB];
    const int layer = layer_of_code(code);

    if (layer < 0) {
        return;
    }
    if (name_plan && s.filter_mask && (s.mask[sub >> 3] & (uint8_t)(1u << (sub & 7u))) == 0u) {
        return;
    }

    hit_t h;

    h.lon_1e7 = unpack_i32_le(&e[E_LON]);
    h.lat_1e7 = unpack_i32_le(&e[E_LAT]);
    h.d2 = calc_d2(h.lat_1e7, h.lon_1e7);

    if (s.live_count == PURRGO_MAP_SEARCH_MAX_RESULTS &&
        h.d2 >= s.live[s.live_count - 1u].d2) {
        return;
    }

    h.db_idx = unpack_u32_le(&e[E_DBI]);
    h.code = code;
    h.layer = (uint8_t)layer;
    h.name_state = 0u;
    h.name[0] = '\0';

    for (size_t i = 0; i < s.live_count; i++) {
        if (s.live[i].layer == h.layer && s.live[i].db_idx == h.db_idx) {
            return;
        }
    }
    if (name_plan && s.nwords > 1u && !verify_extra(layer, h.db_idx)) {
        return;
    }
    live_insert(&h);
}

/* ---------------------------------------------------------------------------
 * План CAT
 * ------------------------------------------------------------------------- */

static bool list_exact(uint32_t radius_km)
{
    if (s.live_count < PURRGO_MAP_SEARCH_MAX_RESULTS) {
        return false;
    }
    const uint64_t r = (uint64_t)radius_km * 1000u * PURRGO_1E7_PER_METER;

    return s.live[s.live_count - 1u].d2 <= r * r;
}

static void sort_ranges(void)
{
    for (uint8_t i = 1; i < s.nrng; i++) {
        const zrange_t t = s.rng[i];
        uint8_t j = i;

        while (j > 0u && s.rng[j - 1u].lo > t.lo) {
            s.rng[j] = s.rng[j - 1u];
            j--;
        }
        s.rng[j] = t;
    }
}

/*
 * Квадрат со стороной 2R вокруг начала отсчёта -> интервалы кода Мортона. Берётся уровень L, на
 * котором квадрат накрывает не больше 5 x 5 клеток; клетка уровня L - непрерывный интервал кода.
 * false - квадрат пересекает антимеридиан (читать участок целиком).
 */
static bool build_ranges(uint32_t radius_km)
{
    static const purrgo_viewport_t square = { 1u, 1u, 0, 0 };

    purrgo_geo_bbox_from_center(s.lat_1e7, s.lon_1e7, 2u * radius_km * 1000u, &square, &s.cam);
    if (s.cam.min_x > s.cam.max_x) {
        return false;
    }

    const uint32_t x0 = to_x(s.cam.min_x < -1800000000 ? -1800000000 : s.cam.min_x);
    const uint32_t x1 = to_x(s.cam.max_x > 1800000000 ? 1800000000 : s.cam.max_x);
    const uint32_t y0 = to_y(s.cam.min_y < -900000000 ? -900000000 : s.cam.min_y);
    const uint32_t y1 = to_y(s.cam.max_y > 900000000 ? 900000000 : s.cam.max_y);
    unsigned level = 0;

    while (level < 31u && (((x1 >> level) - (x0 >> level)) > 4u || ((y1 >> level) - (y0 >> level)) > 4u)) {
        level++;
    }

    s.nrng = 0;
    for (uint32_t cx = x0 >> level; cx <= (x1 >> level); cx++) {
        for (uint32_t cy = y0 >> level; cy <= (y1 >> level); cy++) {
            const uint64_t lo = morton64(cx, cy) << (2u * level);

            s.rng[s.nrng].lo = lo;
            s.rng[s.nrng].hi = lo + ((1ULL << (2u * level)) - 1u);
            s.nrng++;
        }
    }
    sort_ranges();

    /* Соседние интервалы сливаются. */
    uint8_t out = 0;

    for (uint8_t i = 1; i < s.nrng; i++) {
        if (s.rng[out].hi + 1u == s.rng[i].lo) {
            s.rng[out].hi = s.rng[i].hi;
        } else {
            s.rng[++out] = s.rng[i];
        }
    }
    s.nrng = (uint8_t)(out + 1u);
    return true;
}

static bool in_cam(const uint8_t *e)
{
    const int32_t lon = unpack_i32_le(&e[E_LON]);
    const int32_t lat = unpack_i32_le(&e[E_LAT]);

    return lat >= s.cam.min_y && lat <= s.cam.max_y && lon >= s.cam.min_x && lon <= s.cam.max_x;
}

static bool sub_wanted(uint16_t sub)
{
    return sub <= 255u && (s.mask[sub >> 3] & (uint8_t)(1u << (sub & 7u))) != 0u;
}

/* Порция плана CAT. true - план закончен; при ошибке чтения состояние становится ERROR. */
static bool cat_step(uint32_t *budget)
{
    for (;;) {
        switch (s.cphase) {
        case CP_NEXT_SUB:
            while (s.sub <= 255u && !sub_wanted(s.sub)) {
                s.sub++;
            }
            if (s.sub > 255u) {
                return true;
            }
            if (!bsearch_sec(s.cat_off, s.prev_hi, s.cat_cnt, BS_SUB_GE, NULL, s.sub, 0u, &s.c_lo) ||
                !bsearch_sec(s.cat_off, s.c_lo, s.cat_cnt, BS_SUB_GE, NULL, (uint32_t)s.sub + 1u, 0u, &s.c_hi)) {
                s.state = PURRGO_MS_ERROR;
                return true;
            }
            s.prev_hi = s.c_hi;
            if (s.c_lo == s.c_hi) {
                s.sub++;
                break;
            }
            s.c_cur = s.c_lo;
            s.radius_km = 1u;
            s.cphase = (s.c_hi - s.c_lo <= CAT_LINEAR_LIMIT) ? CP_LINEAR : CP_RADIUS_BEGIN;
            break;

        case CP_LINEAR:
            while (s.c_cur < s.c_hi) {
                if (*budget == 0u) {
                    return false;
                }
                const uint8_t *e = get_rec(s.cat_off, s.c_cur);

                if (e == NULL) {
                    s.state = PURRGO_MS_ERROR;
                    return true;
                }
                consider(e, false);
                s.c_cur++;
                (*budget)--;
            }
            s.sub++;
            s.cphase = CP_NEXT_SUB;
            break;

        case CP_RADIUS_BEGIN:
            if (!build_ranges(s.radius_km)) {
                s.c_cur = s.c_lo;
                s.cphase = CP_LINEAR;
                break;
            }
            s.irng = 0;
            s.cphase = CP_RNG_LOCATE;
            break;

        case CP_RNG_LOCATE:
            if (s.irng >= s.nrng) {
                s.cphase = CP_RADIUS_END;
                break;
            }
            /* интервалы идут по возрастанию кода: всё до c_cur уже меньше начала этого интервала */
            if (s.irng == 0u) {
                s.c_cur = s.c_lo;
            }
            if (!bsearch_sec(s.cat_off, s.c_cur, s.c_hi, BS_MORTON_GE, NULL, 0u, s.rng[s.irng].lo, &s.c_cur)) {
                s.state = PURRGO_MS_ERROR;
                return true;
            }
            s.cphase = CP_RNG_SCAN;
            break;

        case CP_RNG_SCAN:
            while (s.c_cur < s.c_hi) {
                if (*budget == 0u) {
                    return false;
                }
                const uint8_t *e = get_rec(s.cat_off, s.c_cur);

                if (e == NULL) {
                    s.state = PURRGO_MS_ERROR;
                    return true;
                }
                if (rec_morton(e) > s.rng[s.irng].hi) {
                    break;
                }
                if (in_cam(e)) {
                    consider(e, false);
                }
                s.c_cur++;
                (*budget)--;
            }
            s.irng++;
            s.cphase = CP_RNG_LOCATE;
            break;

        default:   /* CP_RADIUS_END */
            if (list_exact(s.radius_km) || s.radius_km >= MAX_RADIUS_KM) {
                s.sub++;
                s.cphase = CP_NEXT_SUB;
            } else {
                s.radius_km *= 4u;
                s.cphase = CP_RADIUS_BEGIN;
            }
            break;
        }
    }
}

/* ---------------------------------------------------------------------------
 * План NAME
 * ------------------------------------------------------------------------- */

static bool name_locate(void)
{
    uint8_t lo_key[KEY_SIZE];
    uint8_t hi_key[KEY_SIZE];
    uint32_t b_lo = 0;
    uint32_t b_hi = s.name_cnt;
    const uint8_t plen = s.name_len[0];

    memset(lo_key, 0x00, sizeof lo_key);
    memset(hi_key, 0xFF, sizeof hi_key);
    memcpy(lo_key, s.name_word[0], plen);
    memcpy(hi_key, s.name_word[0], plen);

    /* Запрос продолжает прежний ("пу" -> "пуш"): ищем внутри прежнего диапазона. */
    if (s.prev_valid && s.prev_len <= plen && memcmp(s.prev_key, s.name_word[0], s.prev_len) == 0) {
        b_lo = s.prev_lo;
        b_hi = s.prev_hi_n;
    }

    if (!bsearch_sec(s.name_off, b_lo, b_hi, BS_KEY_GE, lo_key, 0u, 0u, &s.n_cur) ||
        !bsearch_sec(s.name_off, s.n_cur, b_hi, BS_KEY_GT, hi_key, 0u, 0u, &s.n_end)) {
        return false;
    }

    s.prev_valid = true;
    memcpy(s.prev_key, s.name_word[0], KEY_SIZE);
    s.prev_len = plen;
    s.prev_lo = s.n_cur;
    s.prev_hi_n = s.n_end;
    s.name_located = true;
    return true;
}

static bool name_step(uint32_t *budget)
{
    if (!s.name_located && !name_locate()) {
        s.state = PURRGO_MS_ERROR;
        return true;
    }
    while (s.n_cur < s.n_end) {
        if (*budget == 0u) {
            return false;
        }
        const uint8_t *e = get_rec(s.name_off, s.n_cur);

        if (e == NULL) {
            s.state = PURRGO_MS_ERROR;
            return true;
        }
        consider(e, true);
        s.n_cur++;
        (*budget)--;
    }
    return true;
}

/* ---------------------------------------------------------------------------
 * Управление расчётом
 * ------------------------------------------------------------------------- */

static void publish(void)
{
    memcpy(s.shown, s.live, sizeof s.shown);
    s.shown_count = s.live_count;
    s.state = PURRGO_MS_DONE;
}

void purrgo_map_search_set_query(const char *text)
{
    if (!s.open) {
        return;
    }
    reset_query();

    if (!parse_query(text)) {
        s.state = PURRGO_MS_ERROR;
        return;
    }
    if (!s.plan_cat && !s.plan_name) {
        s.shown_count = 0;   /* пусто или слишком коротко: список очищается сразу */
        s.live_count = 0;
        s.state = PURRGO_MS_DONE;
        return;
    }

    s.cphase = CP_NEXT_SUB;
    s.sub = 1u;
    s.prev_hi = 0;
    s.name_located = false;
    s.state = PURRGO_MS_RUNNING;
}

purrgo_ms_state_t purrgo_map_search_step(void)
{
    if (s.state != PURRGO_MS_RUNNING) {
        return s.state;
    }

    uint32_t budget = PURRGO_MAP_SEARCH_CHUNK;

    if (s.plan_cat && !s.cat_done && cat_step(&budget)) {
        s.cat_done = true;
    }
    if (s.state == PURRGO_MS_RUNNING && (!s.plan_cat || s.cat_done) && s.plan_name && !s.name_done &&
        budget > 0u && name_step(&budget)) {
        s.name_done = true;
    }
    if (s.state == PURRGO_MS_ERROR) {
        PURRGO_LOGW("map search: search.idx read error\n\r");
        return s.state;
    }
    if ((!s.plan_cat || s.cat_done) && (!s.plan_name || s.name_done)) {
        publish();
    }
    return s.state;
}

purrgo_ms_state_t purrgo_map_search_state(void)
{
    return s.state;
}

bool purrgo_map_search_running(void)
{
    return s.state == PURRGO_MS_RUNNING;
}

purrgo_ms_kind_t purrgo_map_search_kind(void)
{
    return s.kind;
}

/* ---------------------------------------------------------------------------
 * Результаты
 * ------------------------------------------------------------------------- */

size_t purrgo_map_search_count(void)
{
    return s.shown_count;
}

purrgo_poi_origin_t purrgo_map_search_origin(void)
{
    return s.origin;
}

bool purrgo_map_search_position(size_t index, int32_t *lat_1e7, int32_t *lon_1e7)
{
    if (index >= s.shown_count || lat_1e7 == NULL || lon_1e7 == NULL) {
        return false;
    }
    *lat_1e7 = s.shown[index].lat_1e7;
    *lon_1e7 = s.shown[index].lon_1e7;
    return true;
}

uint8_t purrgo_map_search_type(size_t index)
{
    return (index < s.shown_count) ? s.shown[index].code : 0u;
}

purrgo_ms_layer_t purrgo_map_search_layer(size_t index)
{
    return (index < s.shown_count) ? (purrgo_ms_layer_t)s.shown[index].layer : PURRGO_MS_LAYER_POI;
}

uint32_t purrgo_map_search_distance_m(size_t index)
{
    if (index >= s.shown_count) {
        return 0u;
    }
    return purrgo_geo_distance_m(s.lat_1e7, s.lon_1e7, s.shown[index].lat_1e7, s.shown[index].lon_1e7);
}

uint16_t purrgo_map_search_azimuth_deg(size_t index)
{
    if (index >= s.shown_count) {
        return 0u;
    }
    return purrgo_geo_azimuth_deg(s.lat_1e7, s.lon_1e7, s.shown[index].lat_1e7, s.shown[index].lon_1e7);
}

const char *purrgo_map_search_name(size_t index)
{
    if (index >= s.shown_count) {
        return "";
    }

    hit_t *h = &s.shown[index];

    if (h->name_state == 0u) {
        purrgo_file_t *f = db_for(h->layer);

        h->name_state = 2u;
        h->name[0] = '\0';
        if (f != NULL && map_label_read(f, h->db_idx, h->layer == (uint8_t)PURRGO_MS_LAYER_POI, h->name)) {
            h->name_state = 1u;
        } else {
            h->name[0] = '\0';
        }
    }
    return h->name;
}
