#include "purrgo/book_reader.h"

#include "purrgo/book_search.h"
#include "purrgo/fs_hal.h"
#include "purrgo/gnss_io.h"
#include "purrgo/logger.h"
#include "purrgo/pgo_text.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/system_time.h"
#include "purrgo/track_logger.h"

#include <string.h>

/* Чтение окна с SD порциями; read-ahead - подсказка HAL (многосекторные запросы). */
#define BOOK_LOAD_CHUNK       (16u * 1024u)
#define BOOK_READ_AHEAD_SECT  8u

/* Индекс не должен занимать больше половины буфера: иначе под данные почти ничего не остаётся. */
#define BOOK_MAX_PREFIX_DIV   2u

/* Окно данных начинается за столько байт до текущей страницы (доля окна). */
#define BOOK_WINDOW_BEHIND_DIV 8u

/* Рабочий буфер поиска: страница, LF и начало следующей страницы (для фраз через границу страниц). */
#define BOOK_FIND_BUF (PGB_SCREEN_MAX_TEXT_BYTES + 1u + PURRGO_BOOK_SEARCH_MAX + \
                       PURRGO_BOOK_SEARCH_TAIL_SLACK)

static _Alignas(4) uint8_t s_default_ram[PURRGO_BOOK_RAM_BYTES];

static struct {
    bool active;

    /* список */
    purrgo_book_item_t items[PURRGO_BOOK_LIST_MAX];
    int  count;
    int  cursor;

    /* RAM-буфер: [0, data_offset) - начало файла, затем окно данных. */
    uint8_t *ram;
    uint32_t ram_size;

    /* открытая книга */
    bool          open;
    pgb_status_t  error;
    pgb_header_t  hdr;
    char          file[PURRGO_BOOK_NAME_MAX];
    purrgo_file_t *fp;          /* только пока окно не покрывает всю книгу */
    uint32_t      win_start;    /* окно данных: байты файла [win_start, win_start + win_len) */
    uint32_t      win_len;      /* лежат в ram + hdr.data_offset */

    /* страница */
    uint32_t      page;
    uint8_t       page_buf[PGB_SCREEN_MAX_PAGE_BYTES];   /* текст или картинка */
    size_t        page_len;
    bool          page_is_image;
    uint16_t      img_w;
    uint16_t      img_h;
    pgb_status_t  page_error;

    /* переход по процентам */
    bool          jump;
    uint8_t       jump_percent;

    /* запись позиции */
    bool          pos_dirty;
    uint32_t      pos_changed_ms;

    /* поиск по книге */
    purrgo_keyboard_t kbd;                      /* клавиатура ввода запроса */
    bool          kbd_layout_valid;             /* в этой книге раскладку уже выбирали сами */
    uint8_t       kbd_layout;                   /* purrgo_kbd_layout_t (EN или RU) */
    char          query[PURRGO_KBD_TEXT_MAX + 1u];          /* запрос как набран */
    uint8_t       pat[PURRGO_BOOK_SEARCH_MAX + 1u];         /* запрос для сравнения */
    size_t        pat_len;
    uint8_t       find;                         /* purrgo_book_find_t */
    int8_t        find_dir;                     /* +1 вперёд, -1 назад */
    uint32_t      find_page0;                   /* страница, с которой начат просмотр */
    uint32_t      find_off0;                    /* вперёд: с какого байта смотреть её, назад: до какого */
    uint32_t      find_step;                    /* шагов просмотра сделано; всего page_count + 1 */
    uint32_t      find_ui_ms;                   /* когда последний раз просили перерисовать прогресс */
    uint8_t       find_ui_percent;
    uint32_t      hit_page;                     /* найденное совпадение: страница, смещение и длина на ней */
    uint32_t      hit_off;
    uint32_t      hit_len;
    uint8_t       find_buf[BOOK_FIND_BUF];
    uint8_t       find_next[PGB_SCREEN_MAX_TEXT_BYTES];
} s = { .ram = s_default_ram, .ram_size = sizeof(s_default_ram) };

/* ---- Вспомогательное ------------------------------------------------------- */

static bool is_book_name(const char *name)
{
    const size_t n = strlen(name);
    if (n < 5u || n >= PURRGO_BOOK_NAME_MAX || name[0] == '.') {
        return false;
    }
    const char *ext = name + n - 4u;
    return ext[0] == '.' &&
           (ext[1] == 'P' || ext[1] == 'p') &&
           (ext[2] == 'G' || ext[2] == 'g') &&
           (ext[3] == 'B' || ext[3] == 'b');
}

static bool make_path(char *out, size_t out_size, const char *file)
{
    const int n = purrgo_snprintf(out, out_size, "%s/%s", purrgo_fs_get_books_path(), file);
    return n > 0 && (size_t)n < out_size;
}

static uint8_t percent_of(uint32_t page, uint32_t count)
{
    if (count <= 1u) {
        return 0u;
    }
    return (uint8_t)(((uint64_t)page * 100u + (count - 1u) / 2u) / (count - 1u));
}

static uint32_t page_of_percent(uint8_t percent, uint32_t count)
{
    if (count <= 1u) {
        return 0u;
    }
    return (uint32_t)(((uint64_t)percent * (count - 1u) + 50u) / 100u);
}

static void copy_trunc(char *dst, size_t dst_cols, const char *src)
{
    size_t n = strlen(src);
    if (n > dst_cols) {
        n = dst_cols;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* ---- Позиция чтения --------------------------------------------------------- */

static void position_changed(void)
{
    s.pos_dirty = true;
    s.pos_changed_ms = purrgo_system_time_ms();
}

bool purrgo_book_reader_position_dirty(void)
{
    return s.pos_dirty;
}

bool purrgo_book_reader_search_running(void)
{
    return s.active && s.open && s.find == PURRGO_BOOK_FIND_RUNNING;
}

bool purrgo_book_reader_flush(void)
{
    if (!s.pos_dirty) {
        return true;
    }
    if (purrgo_config_save()) {
        s.pos_dirty = false;
        return true;
    }
    /* Не записалось (нет карты и т.п.): следующая попытка через тайм-аут. */
    s.pos_changed_ms = purrgo_system_time_ms();
    return false;
}

/* Позиция в app_config (только RAM). */
static void remember_position(void)
{
    if (!s.open) {
        return;
    }
    purrgo_snprintf(app_config.book_file, sizeof(app_config.book_file), "%s", s.file);
    app_config.book_crc32 = s.hdr.file_crc32;
    app_config.book_page = s.page;
    position_changed();
}

/* ---- Книга в RAM ------------------------------------------------------------- */

static void close_book(void)
{
    if (s.fp != NULL) {
        (void)purrgo_fs_close(s.fp);
        s.fp = NULL;
    }
    s.open = false;
    s.win_start = 0u;
    s.win_len = 0u;
    s.page_len = 0u;
    s.page_is_image = false;
    s.page_error = PGB_OK;
    s.jump = false;
    s.find = PURRGO_BOOK_FIND_OFF;
    s.hit_len = 0u;
    s.kbd_layout_valid = false;
}

/*
 * Читает окно данных [start, start + len) файла в ram + data_offset.
 * Файл должен быть открыт (s.fp).
 */
static bool load_window(uint32_t start, uint32_t len)
{
    uint8_t *dst = s.ram + s.hdr.data_offset;

    s.win_len = 0u;   /* пока окно не прочитано, страницы из него не берутся */
    purrgo_fs_set_read_ahead(s.fp, (uint8_t)BOOK_READ_AHEAD_SECT);
    if (!purrgo_fs_seek(s.fp, start)) {
        return false;
    }
    for (uint32_t done = 0u; done < len;) {
        uint32_t n = len - done;
        if (n > BOOK_LOAD_CHUNK) {
            n = BOOK_LOAD_CHUNK;
        }
        if (purrgo_fs_read(s.fp, dst + done, n) != n) {
            return false;
        }
        done += n;
    }
    s.win_start = start;
    s.win_len = len;
    return true;
}

static uint32_t window_cap(void)
{
    return s.ram_size - s.hdr.data_offset;
}

/*
 * Подгружает окно данных, в котором целиком лежит страница `page`: начало окна
 * на `behind` байт раньше страницы (но не раньше начала данных), конец не дальше
 * конца файла. Книга, помещающаяся в буфер, загружается целиком.
 */
static pgb_status_t load_window_around(uint32_t page, uint32_t behind_bytes)
{
    pgb_index_entry_t e;
    pgb_status_t st = pgb_page_locate(&s.hdr, s.ram, page, &e);
    if (st != PGB_OK) {
        return st;
    }
    const uint32_t cap = window_cap();
    const uint32_t total = s.hdr.file_size - s.hdr.data_offset;
    uint32_t start = s.hdr.data_offset;

    if (total > cap) {
        const uint32_t room = e.offset - s.hdr.data_offset;
        start = e.offset - ((behind_bytes < room) ? behind_bytes : room);
        if (start + cap > s.hdr.file_size) {
            start = s.hdr.file_size - cap;
        }
    }
    uint32_t len = s.hdr.file_size - start;
    if (len > cap) {
        len = cap;
    }
    if ((uint64_t)e.offset + e.stored_size > (uint64_t)start + len) {
        return PGB_ERR_BUFFER;   /* буфер меньше одной страницы с запасом */
    }
    return load_window(start, len) ? PGB_OK : PGB_ERR_IO;
}

/* Чтение страниц вперёд: окно начинается на 1/8 окна раньше страницы (чтение назад без SD). */
static pgb_status_t load_window_for_page(uint32_t page)
{
    return load_window_around(page, window_cap() / BOOK_WINDOW_BEHIND_DIV);
}

/*
 * Читает страницу из RAM-образа; если её нет в окне данных, подгружает окно с SD
 * (behind_bytes - сколько байт окна лежит до страницы) и пробует ещё раз.
 */
static pgb_status_t read_page_mem(uint32_t page, uint32_t behind_bytes,
                                  uint8_t *buf, size_t buf_size, size_t *len)
{
    pgb_status_t st = PGB_ERR_NOT_CACHED;

    for (int attempt = 0; attempt < 2; attempt++) {
        st = pgb_page_read_mem(&s.hdr, s.ram, s.ram + s.hdr.data_offset, s.win_start, s.win_len,
                               page, buf, buf_size, len);
        if (st != PGB_ERR_NOT_CACHED || s.fp == NULL || attempt != 0) {
            break;
        }
        st = load_window_around(page, behind_bytes);
        if (st != PGB_OK) {
            break;
        }
    }
    return st;
}

/* Читает страницу в s.page_buf; при выходе за окно подгружает окно с SD. */
static bool fetch_page(uint32_t page)
{
    s.page_len = 0u;
    s.page_is_image = false;

    pgb_status_t st = read_page_mem(page, window_cap() / BOOK_WINDOW_BEHIND_DIV,
                                    s.page_buf, sizeof(s.page_buf), &s.page_len);
    if (st == PGB_OK) {
        /* Тип страницы - по записи индекса (она уже проверена при чтении). */
        pgb_index_entry_t e;
        if (pgb_page_locate(&s.hdr, s.ram, page, &e) == PGB_OK) {
            s.page_is_image = PGB_ENTRY_IS_IMAGE(&e);
            s.img_w = e.width;
            s.img_h = e.height;
            s.page_error = PGB_OK;
            return true;
        }
        st = PGB_ERR_INDEX;
    }
    s.page_len = 0u;
    s.page_error = st;
    PURRGO_LOGW("book page %u: %s\r\n", (unsigned)page, pgb_status_text(st));
    return false;
}

/*
 * Открывает книгу и предзагружает её в RAM. Позиция: сохранённая, если это
 * последняя открытая книга с тем же file_crc32, иначе начало. При ошибке
 * s.open == false и s.error - причина.
 */
static bool open_book(const char *file)
{
    char path[PURRGO_FS_MAX_PATH];

    close_book();
    s.error = PGB_OK;
    memset(&s.hdr, 0, sizeof(s.hdr));
    purrgo_snprintf(s.file, sizeof(s.file), "%s", file);

    if (!make_path(path, sizeof(path), file)) {
        s.error = PGB_ERR_IO;
        return false;
    }

    /* Полная проверка заголовка, размера файла и meta_crc32. */
    static pgb_book_t s_pgb;   /* ~6 КБ (scratch под страницу-картинку): не на стеке */
    pgb_book_t *book = &s_pgb;
    pgb_status_t st = pgb_open(path, PGB_SCREEN_COLS, PGB_SCREEN_ROWS, book);
    if (st != PGB_OK) {
        s.error = st;
        PURRGO_LOGW("book %s: %s\r\n", file, pgb_status_text(st));
        return false;
    }
    s.hdr = book->header;
    s.fp = book->file;       /* дальше файл ведёт читалка */
    book->file = NULL;

    const uint32_t prefix = s.hdr.data_offset;
    if (prefix > s.ram_size / BOOK_MAX_PREFIX_DIV) {
        st = PGB_ERR_BUFFER;   /* индекс не помещается в RAM-буфер */
    } else if (!purrgo_fs_seek(s.fp, 0u) || purrgo_fs_read(s.fp, s.ram, prefix) != prefix) {
        st = PGB_ERR_IO;
    } else {
        st = pgb_meta_check_mem(&s.hdr, s.ram);   /* копия в RAM тоже должна сходиться по CRC */
    }

    uint32_t page = 0u;
    if (st == PGB_OK) {
        if (strcmp(file, app_config.book_file) == 0 &&
            app_config.book_crc32 == s.hdr.file_crc32 &&
            app_config.book_page < s.hdr.page_count) {
            page = app_config.book_page;
        }
        st = load_window_for_page(page);
    }
    if (st != PGB_OK) {
        s.error = st;
        PURRGO_LOGW("book %s: %s\r\n", file, pgb_status_text(st));
        close_book();
        return false;
    }

    /* Окно покрывает всю книгу - файл больше не нужен, слот ФС освобождается. */
    if (s.win_start == s.hdr.data_offset && s.win_len == s.hdr.file_size - s.hdr.data_offset) {
        (void)purrgo_fs_close(s.fp);
        s.fp = NULL;
    }

    s.open = true;
    s.page = page;
    s.jump = false;
    (void)fetch_page(page);
    remember_position();
    PURRGO_LOGI("book %s: %u pages, page %u, window %u..%u of %u%s\r\n", file,
                (unsigned)s.hdr.page_count, (unsigned)page, (unsigned)s.win_start,
                (unsigned)(s.win_start + s.win_len), (unsigned)s.hdr.file_size,
                (s.fp == NULL) ? " (whole book in RAM)" : "");
    return true;
}

/* Переход на страницу (с ограничением по краям). true - страница изменилась. */
static bool goto_page(int64_t target)
{
    if (!s.open) {
        return false;
    }
    if (target < 0) {
        target = 0;
    }
    if (target > (int64_t)s.hdr.page_count - 1) {
        target = (int64_t)s.hdr.page_count - 1;
    }
    if ((uint32_t)target == s.page) {
        return false;
    }
    s.page = (uint32_t)target;
    (void)fetch_page(s.page);
    remember_position();
    return true;
}

/* ---- Поиск по книге ----------------------------------------------------------- */

/*
 * Просмотр идёт шагами k = 0 .. page_count: шаг k - страница find_page0 + k вперёд (или
 * find_page0 - k назад) по кругу. Первый шаг смотрит страницу find_page0 с find_off0 (вперёд)
 * или до find_off0 (назад), последний (k = page_count) - ту же страницу ещё раз, но другой её
 * частью: так, обойдя всю книгу, поиск не пропускает совпадений на странице, с которой начат.
 */

static void find_reset(void)
{
    s.find = PURRGO_BOOK_FIND_OFF;
    s.hit_len = 0u;
}

static uint32_t find_page_at(uint32_t k)
{
    const uint32_t n = s.hdr.page_count;
    const uint32_t d = k % n;

    return (s.find_dir > 0) ? (s.find_page0 + d) % n : (s.find_page0 + n - d) % n;
}

static uint8_t find_percent(void)
{
    const uint64_t total = (uint64_t)s.hdr.page_count + 1u;
    const uint64_t pct = (uint64_t)s.find_step * 100u / total;

    return (uint8_t)((pct > 100u) ? 100u : pct);
}

/*
 * Текст страницы для поиска. false - страница не читается или это картинка: такие
 * страницы в поиске пропускаются. Окно данных при необходимости перечитывается так,
 * чтобы дальнейший просмотр шёл по RAM (при поиске назад окно лежит позади страницы).
 */
static bool find_read_text(uint32_t page, uint8_t *buf, size_t cap, size_t *len)
{
    pgb_index_entry_t e;

    *len = 0u;
    if (pgb_page_locate(&s.hdr, s.ram, page, &e) != PGB_OK || PGB_ENTRY_IS_IMAGE(&e)) {
        return false;
    }
    const uint32_t cap_bytes = window_cap();
    const uint32_t behind = (s.find_dir > 0) ? cap_bytes / BOOK_WINDOW_BEHIND_DIV
                                             : cap_bytes - cap_bytes / BOOK_WINDOW_BEHIND_DIV;

    return read_page_mem(page, behind, buf, cap, len) == PGB_OK && *len > 0u;
}

/* Шаг k: есть ли совпадение на странице шага; если есть - оно записано в s.hit_*. */
static bool find_scan_step(uint32_t k)
{
    const uint32_t n = s.hdr.page_count;
    const uint32_t page = find_page_at(k);
    size_t len = 0u;

    if (!find_read_text(page, s.find_buf, PGB_SCREEN_MAX_TEXT_BYTES, &len)) {
        return false;
    }

    size_t off0 = s.find_off0;
    size_t lo = 0u;
    size_t hi = len;

    if (off0 > len) {
        off0 = len;
    }
    if (s.find_dir > 0) {
        if (k == 0u) {
            lo = off0;
        } else if (k == n) {
            hi = off0;
        }
    } else {
        if (k == 0u) {
            hi = off0;
        } else if (k == n) {
            lo = off0;
        }
    }
    if (lo >= hi) {
        return false;
    }

    /* Фраза может начаться в конце страницы и продолжиться на следующей. */
    size_t total = len;
    size_t next_len = 0u;

    if (page + 1u < n &&
        find_read_text(page + 1u, s.find_next, sizeof(s.find_next), &next_len)) {
        size_t tail = s.pat_len + PURRGO_BOOK_SEARCH_TAIL_SLACK;

        if (tail > next_len) {
            tail = next_len;
        }
        s.find_buf[len] = '\n';
        memcpy(s.find_buf + len + 1u, s.find_next, tail);
        total = len + 1u + tail;
    }

    size_t ms = 0u;
    size_t me = 0u;
    const bool hit = (s.find_dir > 0)
        ? purrgo_book_search_find(s.find_buf, total, lo, hi, s.pat, s.pat_len, &ms, &me)
        : purrgo_book_search_find_last(s.find_buf, total, lo, hi, s.pat, s.pat_len, &ms, &me);

    if (!hit) {
        return false;
    }
    if (me > len) {
        me = len;   /* подсвечивается только то, что на этой странице */
    }
    s.hit_page = page;
    s.hit_off = (uint32_t)ms;
    s.hit_len = (uint32_t)(me - ms);
    return true;
}

/* Поиск завершён: на найденную страницу, либо сообщение "не найдено". */
static void find_finish(bool found)
{
    if (found) {
        s.find = PURRGO_BOOK_FIND_FOUND;
        (void)goto_page((int64_t)s.hit_page);
    } else {
        s.find = PURRGO_BOOK_FIND_NOT_FOUND;
        s.hit_len = 0u;
    }
}

/* До `budget` страниц просмотра. true - поиск завершён (найдено или нет). */
static bool find_run(uint32_t budget)
{
    const uint32_t n = s.hdr.page_count;

    while (s.find_step <= n) {
        if (budget == 0u) {
            return false;
        }
        budget--;
        if (find_scan_step(s.find_step++)) {
            find_finish(true);
            return true;
        }
    }
    find_finish(false);
    return true;
}

/*
 * Начать просмотр с найденной (или текущей) страницы. Подсветка прежнего совпадения
 * гаснет; первая порция страниц просматривается сразу, остальное делает tick.
 */
static void find_begin(int dir, uint32_t page0, uint32_t off0)
{
    s.find = PURRGO_BOOK_FIND_RUNNING;
    s.hit_len = 0u;
    s.find_dir = (int8_t)dir;
    s.find_page0 = page0;
    s.find_off0 = off0;
    s.find_step = 0u;
    s.find_ui_ms = purrgo_system_time_ms();
    s.find_ui_percent = 0u;
    (void)find_run(PURRGO_BOOK_SEARCH_SLICE_PAGES);
}

/* Новый запрос из набранной строки. false - запрос пуст (искать нечего). */
static bool find_start_query(const char *typed)
{
    const size_t len = purrgo_book_search_prepare(typed, s.pat);

    if (len == 0u || !s.open) {
        return false;
    }
    s.pat_len = len;
    purrgo_snprintf(s.query, sizeof(s.query), "%s", typed);
    find_begin(1, s.page, 0u);
    return true;
}

/*
 * Раскладка клавиатуры: выбранная пользователем в этой книге, иначе по тексту
 * страницы на экране (кириллицы больше, чем латиницы, - RU; нет букв - тоже RU).
 */
static purrgo_kbd_layout_t find_input_layout(void)
{
    size_t cyr = 0u;
    size_t lat = 0u;

    if (s.kbd_layout_valid) {
        return (s.kbd_layout == PURRGO_KBD_LAYOUT_EN) ? PURRGO_KBD_LAYOUT_EN : PURRGO_KBD_LAYOUT_RU;
    }
    if (!s.page_is_image) {
        for (size_t i = 0u; i < s.page_len; i++) {
            const uint8_t c = s.page_buf[i];

            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                lat++;
            } else if ((c >= 0x80u && c <= 0xDFu) || c == 0xF0u || c == 0xF1u) {
                cyr++;
            }
        }
    }
    return (lat > cyr) ? PURRGO_KBD_LAYOUT_EN : PURRGO_KBD_LAYOUT_RU;
}

bool purrgo_book_reader_tick(uint32_t now_ms)
{
    bool redraw = false;

    if (s.pos_dirty &&
        (uint32_t)(now_ms - s.pos_changed_ms) >= PURRGO_BOOK_SAVE_TIMEOUT_MS) {
        (void)purrgo_book_reader_flush();
    }

    /* Поиск по книге идёт порциями: между ними обрабатываются кнопки и рисуется экран. */
    if (s.active && s.open && s.find == PURRGO_BOOK_FIND_RUNNING) {
        if (find_run(PURRGO_BOOK_SEARCH_SLICE_PAGES)) {
            redraw = true;   /* найдено или нет */
        } else if ((uint32_t)(now_ms - s.find_ui_ms) >= PURRGO_BOOK_SEARCH_PROGRESS_MS &&
                   find_percent() != s.find_ui_percent) {
            s.find_ui_ms = now_ms;
            s.find_ui_percent = find_percent();
            redraw = true;   /* обновить проценты в строке статуса */
        }
    }
    return redraw;
}

/* ---- Список книг ------------------------------------------------------------- */

static int item_cmp(const purrgo_book_item_t *a, const purrgo_book_item_t *b)
{
    int c = strcmp(a->title, b->title);
    return (c != 0) ? c : strcmp(a->file, b->file);
}

static void fill_item(purrgo_book_item_t *it)
{
    char path[PURRGO_FS_MAX_PATH];
    static pgb_info_t info;   /* ~0,4 КБ: не на стеке */

    memset(it->title, 0, sizeof(it->title));
    memset(it->author, 0, sizeof(it->author));
    it->page_count = 0u;
    it->file_crc32 = 0u;
    it->ok = false;

    if (make_path(path, sizeof(path), it->file) &&
        pgb_read_info(path, &info) == PGB_OK &&
        info.cols == PGB_SCREEN_COLS && info.rows == PGB_SCREEN_ROWS &&
        info.img_w == PGB_IMG_WINDOW_W && info.img_h == PGB_IMG_WINDOW_H) {
        copy_trunc(it->title, PURRGO_BOOK_TITLE_COLS, info.title);
        copy_trunc(it->author, PURRGO_BOOK_AUTHOR_COLS, info.author);
        it->page_count = info.page_count;
        it->file_crc32 = info.file_crc32;
        it->ok = true;
    }
    if (it->title[0] == '\0') {
        /* Названия нет или файл повреждён: имя файла без расширения, UTF-8 -> PurrGO-256. */
        char stem[PURRGO_BOOK_NAME_MAX];
        purrgo_snprintf(stem, sizeof(stem), "%s", it->file);
        stem[strlen(stem) - 4u] = '\0';
        purrgo_utf8_to_pgo_inplace(stem);
        copy_trunc(it->title, PURRGO_BOOK_TITLE_COLS, stem);
    }
}

static void build_list(void)
{
    s.count = 0;
    s.cursor = 0;

    /* Сначала имена (каталог открыт недолго), потом заголовки: по одному сектору на файл. */
    purrgo_dir_t *dir = purrgo_fs_opendir(purrgo_fs_get_books_path());
    if (dir == NULL) {
        return;
    }
    static purrgo_fs_dirent_t ent;
    while (s.count < PURRGO_BOOK_LIST_MAX && purrgo_fs_readdir(dir, &ent)) {
        if (ent.is_directory || !is_book_name(ent.name)) {
            continue;
        }
        purrgo_snprintf(s.items[s.count].file, sizeof(s.items[s.count].file), "%s", ent.name);
        s.count++;
    }
    purrgo_fs_closedir(dir);

    for (int i = 0; i < s.count; i++) {
        fill_item(&s.items[i]);
    }

    /* Вставками: по названию, затем по имени файла. */
    for (int i = 1; i < s.count; i++) {
        static purrgo_book_item_t tmp;
        tmp = s.items[i];
        int j = i - 1;
        while (j >= 0 && item_cmp(&s.items[j], &tmp) > 0) {
            s.items[j + 1] = s.items[j];
            j--;
        }
        s.items[j + 1] = tmp;
    }

    for (int i = 0; i < s.count; i++) {
        if (purrgo_book_list_is_last(i)) {
            s.cursor = i;
            break;
        }
    }
}

int purrgo_book_list_count(void)
{
    return s.count;
}

int purrgo_book_list_cursor(void)
{
    return s.cursor;
}

const purrgo_book_item_t *purrgo_book_list_get(int index)
{
    return (index >= 0 && index < s.count) ? &s.items[index] : NULL;
}

bool purrgo_book_list_is_last(int index)
{
    return index >= 0 && index < s.count &&
           app_config.book_file[0] != '\0' &&
           strcmp(s.items[index].file, app_config.book_file) == 0;
}

/* ---- Вход, выход, кнопки ------------------------------------------------------- */

purrgo_state_t purrgo_book_reader_enter(void)
{
    s.active = true;

    /* Режим энергосбережения: без трека и без GNSS. */
    purrgo_logger_stop();
    purrgo_gnss_power_off();

    close_book();
    s.error = PGB_OK;

    if (app_config.book_file[0] != '\0' && open_book(app_config.book_file)) {
        return APP_STATE_BOOK_READ;
    }
    build_list();
    return APP_STATE_BOOK_LIST;
}

static void leave(void)
{
    (void)purrgo_book_reader_flush();
    close_book();
    s.error = PGB_OK;
    s.active = false;
    purrgo_gnss_power_on();
}

bool purrgo_book_reader_is_active(void)
{
    return s.active;
}

static bool is_long(purrgo_btn_t b)
{
    return b == PURRGO_BTN_KEY1_LONG || b == PURRGO_BTN_KEY2_LONG ||
           b == PURRGO_BTN_KEY3_LONG || b == PURRGO_BTN_KEY4_LONG;
}

/* 1..4 */
static int key_of(purrgo_btn_t b)
{
    switch (b) {
    case PURRGO_BTN_KEY1_SHORT: case PURRGO_BTN_KEY1_LONG: return 1;
    case PURRGO_BTN_KEY2_SHORT: case PURRGO_BTN_KEY2_LONG: return 2;
    case PURRGO_BTN_KEY3_SHORT: case PURRGO_BTN_KEY3_LONG: return 3;
    case PURRGO_BTN_KEY4_SHORT: case PURRGO_BTN_KEY4_LONG: return 4;
    default: return 0;
    }
}

static bool handle_list(purrgo_btn_t b, purrgo_state_t *next)
{
    const int key = key_of(b);
    const int step = is_long(b) ? PURRGO_BOOK_LIST_JUMP : 1;

    switch (key) {
    case 1:
    case 2: {
        if (s.count == 0) {
            return false;
        }
        int c = s.cursor + ((key == 1) ? -step : step);
        if (c < 0) {
            c = 0;
        }
        if (c > s.count - 1) {
            c = s.count - 1;
        }
        if (c == s.cursor) {
            return false;
        }
        s.cursor = c;
        return true;
    }
    case 3:
        if (s.count == 0) {
            return false;
        }
        (void)open_book(s.items[s.cursor].file);   /* при ошибке экран чтения покажет причину */
        *next = APP_STATE_BOOK_READ;
        return true;
    case 4:
        leave();
        *next = APP_STATE_MENU_MAIN;
        return true;
    default:
        return false;
    }
}

/* Открыть экранную клавиатуру для запроса (прежний запрос остаётся в поле для правки). */
static void open_search_input(purrgo_state_t *next)
{
    purrgo_kbd_open(&s.kbd, "SEARCH", s.query, find_input_layout());
    *next = APP_STATE_BOOK_SEARCH_INPUT;
}

/* Кнопка на экране клавиатуры ввода запроса. */
static bool handle_search_input(purrgo_btn_t b, purrgo_state_t *next)
{
    const bool changed = purrgo_kbd_handle_button(&s.kbd, b);
    const purrgo_kbd_result_t res = purrgo_kbd_result(&s.kbd);

    if (res == PURRGO_KBD_RESULT_EDITING) {
        return changed;
    }

    /* Ввод закончен: обратно на страницу; выбранная раскладка запоминается до закрытия книги. */
    s.kbd_layout = (s.kbd.alpha == PURRGO_KBD_LAYOUT_EN) ? PURRGO_KBD_LAYOUT_EN : PURRGO_KBD_LAYOUT_RU;
    s.kbd_layout_valid = true;
    *next = APP_STATE_BOOK_READ;
    if (res == PURRGO_KBD_RESULT_ACCEPTED) {
        (void)find_start_query(purrgo_kbd_text(&s.kbd));   /* пустой запрос - ничего не меняется */
    }
    return true;
}

/* Кнопки, пока поиск идёт или его результат на экране. */
static bool handle_find(int key, bool lng, purrgo_state_t *next)
{
    switch (s.find) {
    case PURRGO_BOOK_FIND_RUNNING:
        if (key == 4) {
            find_reset();   /* остановить поиск: обычное чтение */
            return true;
        }
        return false;

    case PURRGO_BOOK_FIND_FOUND:
        switch (key) {
        case 1:
            find_begin(-1, s.hit_page, s.hit_off);
            return true;
        case 2:
            find_begin(1, s.hit_page, s.hit_off + 1u);
            return true;
        case 3:
            if (lng) {
                break;   /* список книг: общий для всех режимов чтения ниже */
            }
            open_search_input(next);
            return true;
        case 4:
            find_reset();
            return true;
        default:
            return false;
        }
        break;

    default:   /* PURRGO_BOOK_FIND_NOT_FOUND */
        switch (key) {
        case 3:
            if (lng) {
                break;
            }
            open_search_input(next);
            return true;
        case 4:
            find_reset();
            return true;
        default:
            return false;
        }
        break;
    }

    /* K3 длинное: список книг. */
    find_reset();
    build_list();
    *next = APP_STATE_BOOK_LIST;
    return true;
}

static bool handle_read(purrgo_btn_t b, purrgo_state_t *next)
{
    const int key = key_of(b);
    const bool lng = is_long(b);

    /* Книга не открылась: только список или выход. */
    if (!s.open) {
        if (key == 3) {
            build_list();
            *next = APP_STATE_BOOK_LIST;
            return true;
        }
        if (key == 4) {
            leave();
            *next = APP_STATE_MENU_MAIN;
            return true;
        }
        return false;
    }

    if (s.find != PURRGO_BOOK_FIND_OFF) {
        return handle_find(key, lng, next);
    }

    if (s.jump) {
        switch (key) {
        case 1:
        case 2: {
            int p = (int)s.jump_percent + ((key == 1) ? -1 : 1) * (lng ? 10 : 1);
            if (p < 0) {
                p = 0;
            }
            if (p > 100) {
                p = 100;
            }
            if ((uint8_t)p == s.jump_percent) {
                return false;
            }
            s.jump_percent = (uint8_t)p;
            return true;
        }
        case 3:
            s.jump = false;
            (void)goto_page((int64_t)page_of_percent(s.jump_percent, s.hdr.page_count));
            return true;
        case 4:
            s.jump = false;
            return true;
        default:
            return false;
        }
    }

    switch (key) {
    case 1:
        return goto_page((int64_t)s.page - (lng ? PURRGO_BOOK_PAGE_JUMP : 1));
    case 2:
        return goto_page((int64_t)s.page + (lng ? PURRGO_BOOK_PAGE_JUMP : 1));
    case 3:
        if (lng) {
            build_list();
            *next = APP_STATE_BOOK_LIST;
            return true;
        }
        if (s.hdr.page_count <= 1u) {
            return false;
        }
        s.jump = true;
        s.jump_percent = percent_of(s.page, s.hdr.page_count);
        return true;
    case 4:
        if (lng) {
            open_search_input(next);   /* K4 длинное: поиск по книге */
            return true;
        }
        leave();
        *next = APP_STATE_MENU_MAIN;
        return true;
    default:
        return false;
    }
}

bool purrgo_book_reader_handle_button(purrgo_state_t state, purrgo_btn_t button,
                                      purrgo_state_t *next_state)
{
    purrgo_state_t dummy;
    if (next_state == NULL) {
        next_state = &dummy;
    }
    *next_state = state;

    if (!s.active) {
        return false;
    }
    if (state == APP_STATE_BOOK_LIST) {
        return handle_list(button, next_state);
    }
    if (state == APP_STATE_BOOK_READ) {
        return handle_read(button, next_state);
    }
    if (state == APP_STATE_BOOK_SEARCH_INPUT && s.open) {
        return handle_search_input(button, next_state);
    }
    return false;
}

/* ---- Для отрисовки и тестов ------------------------------------------------------ */

void purrgo_book_reader_get_view(purrgo_book_view_t *v)
{
    memset(v, 0, sizeof(*v));
    v->open = s.open;
    v->error = s.error;
    v->file = s.file;
    v->find_query = s.query;
    if (!s.open) {
        return;
    }
    v->page_error = s.page_error;
    v->page = s.page;
    v->page_count = s.hdr.page_count;
    v->percent = percent_of(s.page, s.hdr.page_count);
    v->jump = s.jump;
    v->jump_percent = s.jump_percent;
    v->jump_page = page_of_percent(s.jump_percent, s.hdr.page_count);
    if (s.page_error == PGB_OK && s.page_len > 0u) {
        if (s.page_is_image) {
            v->image = s.page_buf;
            v->image_w = s.img_w;
            v->image_h = s.img_h;
        } else {
            v->text = s.page_buf;
            v->text_len = s.page_len;
        }
    }

    v->find = s.find;
    if (s.find == PURRGO_BOOK_FIND_RUNNING) {
        v->find_percent = find_percent();
    } else if (s.find == PURRGO_BOOK_FIND_FOUND && v->text != NULL && s.hit_page == s.page &&
               s.hit_off < v->text_len) {
        v->hl_start = s.hit_off;
        v->hl_len = (s.hit_len > v->text_len - s.hit_off) ? (uint32_t)v->text_len - s.hit_off
                                                          : s.hit_len;
    }
}

const purrgo_keyboard_t *purrgo_book_reader_get_keyboard(void)
{
    return &s.kbd;
}

void purrgo_book_reader_set_ram(void *buffer, size_t size)
{
    close_book();
    if (buffer == NULL || size == 0u) {
        s.ram = s_default_ram;
        s.ram_size = sizeof(s_default_ram);
    } else {
        s.ram = (uint8_t *)buffer;
        s.ram_size = (size > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)size;
    }
}

void purrgo_book_reader_reset(void)
{
    close_book();
    s.active = false;
    s.count = 0;
    s.cursor = 0;
    s.error = PGB_OK;
    s.pos_dirty = false;
    s.pos_changed_ms = 0u;
    s.file[0] = '\0';
    s.page = 0u;
    s.query[0] = '\0';
    s.pat_len = 0u;
    memset(&s.kbd, 0, sizeof(s.kbd));
    find_reset();
}
