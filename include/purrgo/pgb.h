#ifndef PURRGO_PGB_H
#define PURRGO_PGB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "purrgo/fs_hal.h"

/*
 * Чтение электронных книг PGB 1.1 (tools/book-compiler/PGB-format.md).
 *
 * Модуль не зависит от платформы: файл читается через fs_hal.h. Все поля
 * заголовка и индекса декодируются из байтов (little-endian), в C-структуру
 * файл не отображается. Книга считается недоверенными данными: каждое
 * смещение и размер проверяются до чтения, ни одна функция не выходит за
 * границы переданных буферов.
 *
 * Типичный сценарий:
 *   - список книг:      pgb_read_info() - один сектор на файл, без CRC;
 *   - открытие книги:   pgb_open() - заголовок, геометрия, размер файла и
 *                       meta_crc32 (заголовок + метаданные + индекс);
 *   - перелистывание:   pgb_page_read() - одна страница, проверяется
 *                       индекс вокруг неё и содержимое;
 *   - по запросу:       pgb_verify_file() - полный file_crc32 (медленно).
 */

#define PGB_HEADER_SIZE       64u
#define PGB_INDEX_ENTRY_SIZE  12u
#define PGB_VERSION_MAJOR     1u
#define PGB_VERSION_MINOR     1u
#define PGB_MAX_TITLE         200u
#define PGB_MAX_AUTHOR        100u
#define PGB_COMPRESSION_NONE  0u
#define PGB_COMPRESSION_LZ4   1u   /* LZ4 Block (без кадра) на каждую страницу, см. PGB-format.md, раздел 8 */

/*
 * Геометрия читалки на экране 176x264 (PGB-format.md, раздел 2).
 *   - сверху строка статуса 8 px;
 *   - текст: 25 строк по 29 символов, ячейка символа 6x8 px, шаг строк 10 px
 *     (между строками 2 px пустых), первая строка на PGB_TEXT_TOP_PX;
 *   - картинка: 1 bit на пиксель, вписана в окно 176x256 под строкой статуса.
 */
#define PGB_SCREEN_COLS       29u
#define PGB_SCREEN_ROWS       25u
#define PGB_STATUS_H_PX       8u
#define PGB_LINE_PITCH_PX     10u
#define PGB_TEXT_TOP_PX       12u   /* (264 - 8 - (24 * 10 + 8)) / 2 = 4 пустых px сверху и снизу */
#define PGB_IMG_WINDOW_W      176u
#define PGB_IMG_WINDOW_H      256u

/* Наибольший размер текстовой страницы в байтах: rows строк по cols символов и rows-1 байт LF. */
#define PGB_MAX_PAGE_BYTES(cols, rows) ((uint32_t)(cols) * (uint32_t)(rows) + (uint32_t)(rows) - 1u)
#define PGB_SCREEN_MAX_TEXT_BYTES PGB_MAX_PAGE_BYTES(PGB_SCREEN_COLS, PGB_SCREEN_ROWS) /* 749 */

/* Байт в строке картинки шириной w пикселей (строки выровнены по байту). */
#define PGB_IMG_ROW_BYTES(w) (((uint32_t)(w) + 7u) / 8u)
/* Размер картинки w x h пикселей в байтах (так же, как raw_size страницы-картинки). */
#define PGB_MAX_IMAGE_BYTES(w, h) (PGB_IMG_ROW_BYTES(w) * (uint32_t)(h))
#define PGB_SCREEN_MAX_IMAGE_BYTES PGB_MAX_IMAGE_BYTES(PGB_IMG_WINDOW_W, PGB_IMG_WINDOW_H) /* 5632 */

/* Буфер под любую страницу (текст или картинка) книги для экрана читалки. */
#define PGB_SCREEN_MAX_PAGE_BYTES PGB_SCREEN_MAX_IMAGE_BYTES

/*
 * Буфер для сжатых данных страницы внутри pgb_book_t. Сжатая страница короче
 * распакованной, то есть не больше PGB_SCREEN_MAX_PAGE_BYTES - 1 байт. Книги, у
 * которых страница может быть больше, pgb_open() отклоняет (PGB_ERR_GEOMETRY),
 * если они сжаты.
 */
#ifndef PGB_SCRATCH_SIZE
#define PGB_SCRATCH_SIZE      PGB_SCREEN_MAX_PAGE_BYTES
#endif

/* Столько байт с начала файла достаточно для заголовка, названия и автора. */
#define PGB_INFO_READ_SIZE    512u

typedef enum {
    PGB_OK = 0,
    PGB_ERR_IO,            /* файл не открылся, ошибка или короткое чтение */
    PGB_ERR_MAGIC,         /* не PGB-файл */
    PGB_ERR_VERSION,       /* неизвестная версия формата */
    PGB_ERR_UNSUPPORTED,   /* флаги, сжатие или зарезервированные поля, которых нет в 1.1 */
    PGB_ERR_HEADER,        /* несогласованные размеры и смещения в заголовке */
    PGB_ERR_GEOMETRY,      /* cols/rows или окно картинок книги не совпадают с экраном */
    PGB_ERR_SIZE,          /* фактический размер файла не равен file_size */
    PGB_ERR_META_CRC,      /* повреждены заголовок, метаданные или индекс */
    PGB_ERR_FILE_CRC,      /* повреждён файл (полная проверка) */
    PGB_ERR_INDEX,         /* запись индекса некорректна или не стыкуется с соседями */
    PGB_ERR_PAGE_NUMBER,   /* номер страницы вне книги */
    PGB_ERR_BUFFER,        /* буфер для страницы мал */
    PGB_ERR_PAGE_DATA,     /* недопустимое содержимое страницы (текст или картинка) */
    PGB_ERR_CANCELLED,     /* проверка прервана пользователем */
    PGB_ERR_DECOMPRESS,    /* блок LZ4 повреждён или не даёт ровно raw_size байт */
    PGB_ERR_NOT_CACHED     /* нужные байты файла не входят в переданный образ в памяти */
} pgb_status_t;

/* Короткое ASCII-описание статуса (для журнала и экрана ошибки). Никогда не NULL. */
const char *pgb_status_text(pgb_status_t status);

typedef struct {
    uint32_t page_count;
    uint32_t meta_offset;
    uint32_t index_offset;
    uint32_t data_offset;
    uint32_t file_size;
    uint32_t file_crc32;
    uint32_t meta_crc32;
    uint16_t title_length;
    uint16_t author_length;
    uint16_t img_w;        /* окно картинок: наибольшая ширина и высота, пикселей */
    uint16_t img_h;
    uint8_t  cols;
    uint8_t  rows;
    uint8_t  compression;  /* PGB_COMPRESSION_NONE или PGB_COMPRESSION_LZ4 */
} pgb_header_t;

/*
 * Запись индекса. Страница-картинка: width и height больше нуля, raw_size ==
 * PGB_MAX_IMAGE_BYTES(width, height). Текстовая страница: width == height == 0.
 */
typedef struct {
    uint32_t offset;       /* абсолютное смещение страницы в файле */
    uint16_t stored_size;  /* размер в файле */
    uint16_t raw_size;     /* размер страницы после распаковки */
    uint16_t width;        /* ширина картинки в пикселях, 0 - текстовая страница */
    uint16_t height;       /* высота картинки в пикселях, 0 - текстовая страница */
} pgb_index_entry_t;

/* Страница-картинка (1 bit на пиксель, строки по PGB_IMG_ROW_BYTES, MSB слева, 1 - чёрный). */
#define PGB_ENTRY_IS_IMAGE(e) ((e)->width != 0u)

/* Сведения для списка книг. Строки в PurrGO-256, завершены нулём. */
typedef struct {
    char     title[PGB_MAX_TITLE + 1u];
    char     author[PGB_MAX_AUTHOR + 1u];
    uint32_t page_count;
    uint32_t file_crc32;  /* идентификатор книги для сохранённой позиции чтения */
    uint16_t img_w;
    uint16_t img_h;
    uint8_t  cols;
    uint8_t  rows;
    uint8_t  compression;
} pgb_info_t;

/* Открытая книга. Файл остаётся открытым до pgb_close(). */
typedef struct {
    purrgo_file_t *file;
    pgb_header_t   header;
    char           title[PGB_MAX_TITLE + 1u];
    char           author[PGB_MAX_AUTHOR + 1u];
    uint8_t        scratch[PGB_SCRATCH_SIZE];   /* сжатые данные страницы */
} pgb_book_t;

/* ---- Чистые функции (без файла) ------------------------------------------ */

/* CRC-32/ISO-HDLC (как в zlib): pgb_crc32("123456789") == 0xCBF43926. */
uint32_t pgb_crc32(const uint8_t *data, size_t len);

/*
 * Потоковый CRC файла PGB: байты поля file_crc32 и meta_crc32 (смещения 32..39)
 * считаются нулевыми независимо от их содержимого. Данные подаются подряд с
 * начала файла любыми порциями.
 *   meta_crc32 = CRC по первым data_offset байтам, file_crc32 = CRC по всему файлу.
 */
typedef struct {
    uint32_t state;
    uint32_t pos;
} pgb_crc_t;

void     pgb_crc_init(pgb_crc_t *crc);
void     pgb_crc_feed(pgb_crc_t *crc, const uint8_t *data, size_t len);
uint32_t pgb_crc_value(const pgb_crc_t *crc);

/*
 * Разбор и проверка заголовка (64 байта, len >= 64): magic, версия, флаги,
 * сжатие, зарезервированные поля и согласованность размеров и смещений.
 * CRC и размер файла на диске здесь не проверяются.
 */
pgb_status_t pgb_header_decode(const uint8_t *buf, size_t len, pgb_header_t *header);

/*
 * Строгий декодер сырого LZ4 Block. true только если на выходе ровно dst_len
 * байт, использован весь вход (src_len байт) и блок корректен: смещения от 1 до
 * числа уже полученных байт, длины не выходят за буферы, блок оканчивается
 * последовательностью из одних литералов. Ни чтения за src, ни записи за dst.
 * Буферы src и dst не должны пересекаться.
 */
bool pgb_lz4_decode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len);

/* Разбор записи индекса (PGB_INDEX_ENTRY_SIZE байт). */
void pgb_index_entry_decode(const uint8_t *buf, pgb_index_entry_t *entry);

/*
 * Проверка записи страницы `number`. `next` - запись следующей страницы или NULL
 * для последней. Цепочка проверок по всем страницам эквивалентна требованию
 * «страницы идут подряд без промежутков и перекрытий, конец последней = file_size».
 */
pgb_status_t pgb_index_entry_check(const pgb_header_t *header, uint32_t number,
                                   const pgb_index_entry_t *entry,
                                   const pgb_index_entry_t *next);

/*
 * Проверка текста страницы: нет управляющих кодов кроме LF (и DEL), нет
 * завершающего LF, не больше rows строк, каждая не длиннее cols байт.
 */
pgb_status_t pgb_page_check_text(const pgb_header_t *header, const uint8_t *text, size_t len);

/*
 * Проверка страницы-картинки: len == PGB_MAX_IMAGE_BYTES(width, height), а биты
 * выравнивания в конце каждой строки (правее width) равны нулю.
 */
pgb_status_t pgb_page_check_image(const uint8_t *bits, size_t len, uint16_t width, uint16_t height);

/* ---- Образ книги в памяти (читалка: предзагрузка книги в RAM) -------------- */

/*
 * Читалка держит в RAM начало файла (`prefix`: заголовок + метаданные + индекс,
 * data_offset байт) и непрерывное окно данных страниц: байты файла
 * [data_base, data_base + data_len). Все функции ниже работают только с этими
 * буферами и файла не касаются; содержимое по-прежнему считается недоверенным.
 */

/* meta_crc32 по prefix (data_offset байт); PGB_OK или PGB_ERR_META_CRC. */
pgb_status_t pgb_meta_check_mem(const pgb_header_t *header, const uint8_t *prefix);

/*
 * Запись индекса страницы `number` из prefix, проверенная вместе со следующей
 * (как в pgb_page_read). Нужна, чтобы узнать, какой диапазон файла занимает
 * страница и что на ней: текст или картинка (PGB_ENTRY_IS_IMAGE, width, height).
 */
pgb_status_t pgb_page_locate(const pgb_header_t *header, const uint8_t *prefix,
                             uint32_t number, pgb_index_entry_t *entry);

/*
 * Страница `number` из образа в памяти; сжатая страница распаковывается прямо из
 * окна данных (scratch не нужен). Если байты страницы не лежат целиком в окне,
 * возвращается PGB_ERR_NOT_CACHED (читалка подгружает другое окно).
 */
pgb_status_t pgb_page_read_mem(const pgb_header_t *header, const uint8_t *prefix,
                               const uint8_t *data, uint32_t data_base, uint32_t data_len,
                               uint32_t number, uint8_t *buf, size_t buf_size, size_t *out_len);

/* ---- Работа с файлом ------------------------------------------------------ */

/* Читает только первый сектор: заголовок, название и автор. CRC не проверяется. */
pgb_status_t pgb_read_info(const char *path, pgb_info_t *info);

/*
 * Открывает книгу и проверяет её: заголовок, геометрию (expected_cols x
 * expected_rows - обычно PGB_SCREEN_COLS x PGB_SCREEN_ROWS - и окно картинок
 * PGB_IMG_WINDOW_W x PGB_IMG_WINDOW_H), размер файла и
 * meta_crc32. Читает заголовок, метаданные и индекс последовательно
 * (для книги в 3 МБ около 28 КБ). При ошибке файл закрыт, book->file == NULL.
 */
pgb_status_t pgb_open(const char *path, uint8_t expected_cols, uint8_t expected_rows,
                      pgb_book_t *book);

/*
 * Читает страницу `number` (с нуля) в buf; сжатая страница распаковывается.
 * Размер возвращается в *out_len. Это текст (не заканчивается нулём) или
 * картинка (PGB_ENTRY_IS_IMAGE в записи индекса, см. pgb_page_locate); тип
 * страницы проверяется по записи индекса. Для любой страницы хватает буфера
 * PGB_SCREEN_MAX_PAGE_BYTES.
 */
pgb_status_t pgb_page_read(pgb_book_t *book, uint32_t number,
                           uint8_t *buf, size_t buf_size, size_t *out_len);

/*
 * Полная проверка file_crc32 (читает весь файл). progress (необязательный)
 * вызывается после каждой порции; false прерывает проверку (PGB_ERR_CANCELLED).
 */
typedef bool (*pgb_progress_fn)(uint32_t done, uint32_t total, void *user);
pgb_status_t pgb_verify_file(pgb_book_t *book, pgb_progress_fn progress, void *user);

void pgb_close(pgb_book_t *book);

#endif /* PURRGO_PGB_H */
