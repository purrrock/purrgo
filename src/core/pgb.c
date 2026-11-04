#include "purrgo/pgb.h"

#include <string.h>

/* Порция чтения при проверке CRC: один сектор SD. */
#define PGB_CHUNK 512u

/* Поля file_crc32 и meta_crc32 занимают байты 32..39 заголовка. */
#define PGB_CRC_FIELDS_BEGIN 32u
#define PGB_CRC_FIELDS_END   40u

/* ---- Описание статусов ------------------------------------------------------ */

const char *pgb_status_text(pgb_status_t status)
{
    switch (status) {
    case PGB_OK:              return "OK";
    case PGB_ERR_IO:          return "read error";
    case PGB_ERR_MAGIC:       return "not a PGB file";
    case PGB_ERR_VERSION:     return "unsupported version";
    case PGB_ERR_UNSUPPORTED: return "unsupported features";
    case PGB_ERR_HEADER:      return "bad header";
    case PGB_ERR_GEOMETRY:    return "wrong screen size";
    case PGB_ERR_SIZE:        return "wrong file size";
    case PGB_ERR_META_CRC:    return "damaged header/index";
    case PGB_ERR_FILE_CRC:    return "damaged file";
    case PGB_ERR_INDEX:       return "bad page index";
    case PGB_ERR_PAGE_NUMBER: return "no such page";
    case PGB_ERR_BUFFER:      return "buffer too small";
    case PGB_ERR_PAGE_DATA:   return "bad page data";
    case PGB_ERR_CANCELLED:   return "cancelled";
    case PGB_ERR_DECOMPRESS:  return "bad compressed page";
    case PGB_ERR_NOT_CACHED:  return "page not in memory";
    default:                  return "unknown error";
    }
}

/* ---- Little-endian ---------------------------------------------------------- */

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ---- CRC-32/ISO-HDLC (полином 0xEDB88320), таблица из 16 элементов ----------- */

static const uint32_t k_crc_nibble[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

static uint32_t crc_byte(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    crc = (crc >> 4) ^ k_crc_nibble[crc & 0x0Fu];
    crc = (crc >> 4) ^ k_crc_nibble[crc & 0x0Fu];
    return crc;
}

uint32_t pgb_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc = crc_byte(crc, data[i]);
    }
    return crc ^ 0xFFFFFFFFu;
}

void pgb_crc_init(pgb_crc_t *crc)
{
    crc->state = 0xFFFFFFFFu;
    crc->pos = 0u;
}

void pgb_crc_feed(pgb_crc_t *crc, const uint8_t *data, size_t len)
{
    uint32_t state = crc->state;
    uint32_t pos = crc->pos;
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = (pos >= PGB_CRC_FIELDS_BEGIN && pos < PGB_CRC_FIELDS_END) ? 0u : data[i];
        state = crc_byte(state, byte);
        pos++;
    }
    crc->state = state;
    crc->pos = pos;
}

uint32_t pgb_crc_value(const pgb_crc_t *crc)
{
    return crc->state ^ 0xFFFFFFFFu;
}

/* ---- Заголовок -------------------------------------------------------------- */

pgb_status_t pgb_header_decode(const uint8_t *buf, size_t len, pgb_header_t *header)
{
    if (buf == NULL || header == NULL || len < PGB_HEADER_SIZE) {
        return PGB_ERR_HEADER;
    }
    if (memcmp(buf, "PGB1", 4) != 0) {
        return PGB_ERR_MAGIC;
    }
    if (rd16(buf + 4) != PGB_VERSION_MAJOR || rd16(buf + 6) != PGB_VERSION_MINOR) {
        return PGB_ERR_VERSION;
    }
    if (rd16(buf + 8) != PGB_HEADER_SIZE) {
        return PGB_ERR_HEADER;
    }
    /* flags, compression (0 - нет, 1 - LZ4 Block), reserved0 */
    if (rd16(buf + 10) != 0u || buf[42] > PGB_COMPRESSION_LZ4 || buf[43] != 0u) {
        return PGB_ERR_UNSUPPORTED;
    }
    for (size_t i = 48u; i < PGB_HEADER_SIZE; i++) {
        if (buf[i] != 0u) {
            return PGB_ERR_UNSUPPORTED;
        }
    }

    pgb_header_t h;
    h.page_count    = rd32(buf + 12);
    h.meta_offset   = rd32(buf + 16);
    h.index_offset  = rd32(buf + 20);
    h.data_offset   = rd32(buf + 24);
    h.file_size     = rd32(buf + 28);
    h.file_crc32    = rd32(buf + 32);
    h.meta_crc32    = rd32(buf + 36);
    h.cols          = buf[40];
    h.rows          = buf[41];
    h.compression   = buf[42];
    h.title_length  = rd16(buf + 44);
    h.author_length = rd16(buf + 46);

    if (h.page_count == 0u || h.cols == 0u || h.rows == 0u) {
        return PGB_ERR_HEADER;
    }
    if (h.title_length > PGB_MAX_TITLE || h.author_length > PGB_MAX_AUTHOR) {
        return PGB_ERR_HEADER;
    }
    if (h.meta_offset != PGB_HEADER_SIZE) {
        return PGB_ERR_HEADER;
    }
    if (h.index_offset != h.meta_offset + (uint32_t)h.title_length + (uint32_t)h.author_length) {
        return PGB_ERR_HEADER;
    }
    uint64_t data_offset = (uint64_t)h.index_offset + (uint64_t)h.page_count * PGB_INDEX_ENTRY_SIZE;
    if (data_offset != (uint64_t)h.data_offset) {
        return PGB_ERR_HEADER;
    }
    /* Каждая страница занимает не меньше одного байта. */
    if ((uint64_t)h.file_size < data_offset + (uint64_t)h.page_count) {
        return PGB_ERR_HEADER;
    }

    *header = h;
    return PGB_OK;
}

/*
 * Строгий декодер LZ4 Block (https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md).
 * Страница приходит с SD-карты, то есть из недоверенных данных: каждая длина и
 * каждое смещение проверяются до обращения к памяти, переполнений size_t нет
 * (длины растут порциями по 255 и ограничиваются размером выходного буфера).
 */
bool pgb_lz4_decode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len)
{
    size_t ip = 0u;
    size_t op = 0u;

    if (src == NULL || dst == NULL) {
        return false;
    }
    for (;;) {
        if (ip >= src_len) {
            return false;                       /* нет токена: блок оборван */
        }
        const uint8_t token = src[ip++];

        size_t literals = (size_t)(token >> 4);
        if (literals == 15u) {
            uint8_t extra;
            do {
                if (ip >= src_len) {
                    return false;
                }
                extra = src[ip++];
                literals += extra;
                if (literals > dst_len) {
                    return false;
                }
            } while (extra == 255u);
        }
        if (literals > dst_len - op || literals > src_len - ip) {
            return false;
        }
        if (literals > 0u) {
            memcpy(dst + op, src + ip, literals);
            op += literals;
            ip += literals;
        }
        if (ip == src_len) {
            break;                              /* последняя последовательность: только литералы */
        }

        if (src_len - ip < 2u) {
            return false;
        }
        const size_t offset = (size_t)src[ip] | ((size_t)src[ip + 1u] << 8);
        ip += 2u;
        if (offset == 0u || offset > op) {
            return false;
        }

        size_t match = (size_t)(token & 0x0Fu);
        if (match == 15u) {
            uint8_t extra;
            do {
                if (ip >= src_len) {
                    return false;
                }
                extra = src[ip++];
                match += extra;
                if (match > dst_len) {
                    return false;
                }
            } while (extra == 255u);
        }
        match += 4u;                            /* MINMATCH */
        if (match > dst_len - op) {
            return false;
        }
        /* Копирование вперёд побайтно: при offset < match области перекрываются намеренно. */
        for (size_t i = 0u; i < match; i++) {
            dst[op + i] = dst[op + i - offset];
        }
        op += match;
    }
    return op == dst_len;
}

void pgb_index_entry_decode(const uint8_t *buf, pgb_index_entry_t *entry)
{
    entry->offset      = rd32(buf);
    entry->stored_size = rd16(buf + 4);
    entry->raw_size    = rd16(buf + 6);
}

pgb_status_t pgb_index_entry_check(const pgb_header_t *header, uint32_t number,
                                   const pgb_index_entry_t *entry,
                                   const pgb_index_entry_t *next)
{
    if (entry->stored_size == 0u || entry->raw_size == 0u) {
        return PGB_ERR_INDEX;
    }
    /* Без сжатия размеры равны; с LZ4 страница либо сжата (stored < raw), либо хранится как есть. */
    if (header->compression == PGB_COMPRESSION_NONE ? (entry->stored_size != entry->raw_size)
                                                    : (entry->stored_size > entry->raw_size)) {
        return PGB_ERR_INDEX;
    }
    if (entry->raw_size > PGB_MAX_PAGE_BYTES(header->cols, header->rows)) {
        return PGB_ERR_INDEX;
    }
    if (entry->offset < header->data_offset || (number == 0u && entry->offset != header->data_offset)) {
        return PGB_ERR_INDEX;
    }
    uint64_t end = (uint64_t)entry->offset + entry->stored_size;
    uint64_t expected_end = (next != NULL) ? (uint64_t)next->offset : (uint64_t)header->file_size;
    if (end != expected_end || end > header->file_size) {
        return PGB_ERR_INDEX;
    }
    return PGB_OK;
}

pgb_status_t pgb_page_check_text(const pgb_header_t *header, const uint8_t *text, size_t len)
{
    if (len == 0u || text[len - 1u] == '\n') {
        return PGB_ERR_PAGE_DATA;
    }
    uint32_t lines = 1u;
    uint32_t line_len = 0u;
    for (size_t i = 0; i < len; i++) {
        uint8_t c = text[i];
        if (c == '\n') {
            lines++;
            line_len = 0u;
            if (lines > header->rows) {
                return PGB_ERR_PAGE_DATA;
            }
            continue;
        }
        if (c < 0x20u || c == 0x7Fu) {
            return PGB_ERR_PAGE_DATA;
        }
        line_len++;
        if (line_len > header->cols) {
            return PGB_ERR_PAGE_DATA;
        }
    }
    return PGB_OK;
}

/* ---- Файл ------------------------------------------------------------------- */

static bool read_exact(purrgo_file_t *file, uint32_t offset, uint8_t *buf, uint32_t size)
{
    return purrgo_fs_seek(file, offset) && purrgo_fs_read(file, buf, size) == size;
}

/* Названия приходят с SD: управляющие коды заменяются пробелом, строка завершается нулём. */
static void copy_text(char *dst, const uint8_t *src, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t c = src[i];
        dst[i] = (c < 0x20u || c == 0x7Fu) ? ' ' : (char)c;
    }
    dst[len] = '\0';
}

/*
 * Размер файла на диске должен быть равен file_size: последний байт читается,
 * следующего нет. (Сравнение через f_size недоступно в fs_hal; seek за конец
 * файла на STM32 ограничивается размером файла.)
 */
static pgb_status_t check_file_size(purrgo_file_t *file, uint32_t file_size)
{
    uint8_t byte;
    if (!read_exact(file, file_size - 1u, &byte, 1u)) {
        return PGB_ERR_SIZE;
    }
    if (!purrgo_fs_seek(file, file_size)) {
        return PGB_ERR_IO;
    }
    if (purrgo_fs_read(file, &byte, 1u) != 0u) {
        return PGB_ERR_SIZE;
    }
    return PGB_OK;
}

pgb_status_t pgb_read_info(const char *path, pgb_info_t *info)
{
    if (path == NULL || info == NULL) {
        return PGB_ERR_IO;
    }
    memset(info, 0, sizeof(*info));

    purrgo_file_t *file = purrgo_fs_open(path, FS_READ);
    if (file == NULL) {
        return PGB_ERR_IO;
    }
    uint8_t buf[PGB_INFO_READ_SIZE];
    uint32_t got = 0u;
    if (purrgo_fs_seek(file, 0u)) {
        got = purrgo_fs_read(file, buf, sizeof(buf));
    }
    (void)purrgo_fs_close(file);

    pgb_header_t header;
    pgb_status_t status = pgb_header_decode(buf, got, &header);
    if (status != PGB_OK) {
        return (got < PGB_HEADER_SIZE) ? PGB_ERR_IO : status;
    }
    if (got < (uint32_t)PGB_HEADER_SIZE + header.title_length + header.author_length) {
        return PGB_ERR_IO;
    }

    copy_text(info->title, buf + PGB_HEADER_SIZE, header.title_length);
    copy_text(info->author, buf + PGB_HEADER_SIZE + header.title_length, header.author_length);
    info->page_count = header.page_count;
    info->file_crc32 = header.file_crc32;
    info->cols = header.cols;
    info->rows = header.rows;
    info->compression = header.compression;
    return PGB_OK;
}

static pgb_status_t open_checked(pgb_book_t *book, uint8_t expected_cols, uint8_t expected_rows)
{
    purrgo_file_t *file = book->file;
    uint8_t chunk[PGB_CHUNK];

    if (!read_exact(file, 0u, chunk, PGB_HEADER_SIZE)) {
        return PGB_ERR_IO;
    }
    pgb_status_t status = pgb_header_decode(chunk, PGB_HEADER_SIZE, &book->header);
    if (status != PGB_OK) {
        return status;
    }
    const pgb_header_t *h = &book->header;
    if (h->cols != expected_cols || h->rows != expected_rows) {
        return PGB_ERR_GEOMETRY;
    }
    /* Сжатая страница распаковывается через book->scratch. */
    if (h->compression != PGB_COMPRESSION_NONE && PGB_MAX_PAGE_BYTES(h->cols, h->rows) > PGB_SCRATCH_SIZE) {
        return PGB_ERR_GEOMETRY;
    }
    status = check_file_size(file, h->file_size);
    if (status != PGB_OK) {
        return status;
    }

    /* meta_crc32: заголовок + метаданные + индекс, читаем подряд. */
    pgb_crc_t crc;
    pgb_crc_init(&crc);
    if (!purrgo_fs_seek(file, 0u)) {
        return PGB_ERR_IO;
    }
    for (uint32_t done = 0u; done < h->data_offset;) {
        uint32_t left = h->data_offset - done;
        uint32_t n = (left < PGB_CHUNK) ? left : PGB_CHUNK;
        if (purrgo_fs_read(file, chunk, n) != n) {
            return PGB_ERR_IO;
        }
        pgb_crc_feed(&crc, chunk, n);
        done += n;
    }
    if (pgb_crc_value(&crc) != h->meta_crc32) {
        return PGB_ERR_META_CRC;
    }

    /* Название и автор. */
    uint8_t meta[PGB_MAX_TITLE + PGB_MAX_AUTHOR] = { 0u };
    uint32_t meta_len = (uint32_t)h->title_length + h->author_length;
    if (meta_len > 0u && !read_exact(file, h->meta_offset, meta, meta_len)) {
        return PGB_ERR_IO;
    }
    copy_text(book->title, meta, h->title_length);
    copy_text(book->author, meta + h->title_length, h->author_length);
    return PGB_OK;
}

pgb_status_t pgb_open(const char *path, uint8_t expected_cols, uint8_t expected_rows,
                      pgb_book_t *book)
{
    if (path == NULL || book == NULL) {
        return PGB_ERR_IO;
    }
    memset(book, 0, sizeof(*book));

    book->file = purrgo_fs_open(path, FS_READ);
    if (book->file == NULL) {
        return PGB_ERR_IO;
    }
    pgb_status_t status = open_checked(book, expected_cols, expected_rows);
    if (status != PGB_OK) {
        pgb_close(book);
        memset(book, 0, sizeof(*book));
    }
    return status;
}

pgb_status_t pgb_page_read(pgb_book_t *book, uint32_t number,
                           uint8_t *buf, size_t buf_size, size_t *out_len)
{
    if (book == NULL || book->file == NULL || buf == NULL || out_len == NULL) {
        return PGB_ERR_IO;
    }
    *out_len = 0u;
    const pgb_header_t *h = &book->header;
    if (number >= h->page_count) {
        return PGB_ERR_PAGE_NUMBER;
    }

    /* Запись страницы и следующая запись: вместе они проверяют стыковку страниц. */
    bool has_next = (number + 1u) < h->page_count;
    uint8_t raw[2u * PGB_INDEX_ENTRY_SIZE];
    uint32_t raw_len = has_next ? (2u * PGB_INDEX_ENTRY_SIZE) : PGB_INDEX_ENTRY_SIZE;
    uint32_t entry_offset = h->index_offset + number * PGB_INDEX_ENTRY_SIZE;
    if (!read_exact(book->file, entry_offset, raw, raw_len)) {
        return PGB_ERR_IO;
    }
    pgb_index_entry_t entry = { 0u, 0u, 0u };
    pgb_index_entry_t next = { 0u, 0u, 0u };
    pgb_index_entry_decode(raw, &entry);
    if (has_next) {
        pgb_index_entry_decode(raw + PGB_INDEX_ENTRY_SIZE, &next);
    }
    pgb_status_t status = pgb_index_entry_check(h, number, &entry, has_next ? &next : NULL);
    if (status != PGB_OK) {
        return status;
    }
    if (buf_size < entry.raw_size) {
        return PGB_ERR_BUFFER;
    }
    if (entry.stored_size == entry.raw_size) {
        /* Страница хранится без сжатия. */
        if (!read_exact(book->file, entry.offset, buf, entry.stored_size)) {
            return PGB_ERR_IO;
        }
    } else {
        /* stored_size < raw_size гарантирует проверка записи индекса; scratch не меньше страницы. */
        if (entry.stored_size > sizeof(book->scratch)) {
            return PGB_ERR_INDEX;
        }
        if (!read_exact(book->file, entry.offset, book->scratch, entry.stored_size)) {
            return PGB_ERR_IO;
        }
        if (!pgb_lz4_decode(book->scratch, entry.stored_size, buf, entry.raw_size)) {
            return PGB_ERR_DECOMPRESS;
        }
    }
    status = pgb_page_check_text(h, buf, entry.raw_size);
    if (status != PGB_OK) {
        return status;
    }
    *out_len = entry.raw_size;
    return PGB_OK;
}

/* ---- Образ книги в памяти ---------------------------------------------------- */

pgb_status_t pgb_meta_check_mem(const pgb_header_t *header, const uint8_t *prefix)
{
    if (header == NULL || prefix == NULL) {
        return PGB_ERR_IO;
    }
    pgb_crc_t crc;
    pgb_crc_init(&crc);
    pgb_crc_feed(&crc, prefix, header->data_offset);
    return (pgb_crc_value(&crc) == header->meta_crc32) ? PGB_OK : PGB_ERR_META_CRC;
}

pgb_status_t pgb_page_locate(const pgb_header_t *header, const uint8_t *prefix,
                             uint32_t number, pgb_index_entry_t *entry)
{
    if (header == NULL || prefix == NULL || entry == NULL) {
        return PGB_ERR_IO;
    }
    if (number >= header->page_count) {
        return PGB_ERR_PAGE_NUMBER;
    }
    const bool has_next = (number + 1u) < header->page_count;
    /* Индекс целиком лежит в prefix: index_offset + page_count * 8 == data_offset (проверено заголовком). */
    const uint8_t *p = prefix + header->index_offset + (size_t)number * PGB_INDEX_ENTRY_SIZE;
    pgb_index_entry_t next = { 0u, 0u, 0u };
    pgb_index_entry_decode(p, entry);
    if (has_next) {
        pgb_index_entry_decode(p + PGB_INDEX_ENTRY_SIZE, &next);
    }
    return pgb_index_entry_check(header, number, entry, has_next ? &next : NULL);
}

pgb_status_t pgb_page_read_mem(const pgb_header_t *header, const uint8_t *prefix,
                               const uint8_t *data, uint32_t data_base, uint32_t data_len,
                               uint32_t number, uint8_t *buf, size_t buf_size, size_t *out_len)
{
    if (header == NULL || prefix == NULL || buf == NULL || out_len == NULL ||
        (data == NULL && data_len != 0u)) {
        return PGB_ERR_IO;
    }
    *out_len = 0u;

    pgb_index_entry_t entry;
    pgb_status_t status = pgb_page_locate(header, prefix, number, &entry);
    if (status != PGB_OK) {
        return status;
    }
    if (buf_size < entry.raw_size) {
        return PGB_ERR_BUFFER;
    }
    /* Страница должна целиком лежать в окне [data_base, data_base + data_len). */
    const uint64_t win_end = (uint64_t)data_base + data_len;
    if (entry.offset < data_base || (uint64_t)entry.offset + entry.stored_size > win_end) {
        return PGB_ERR_NOT_CACHED;
    }
    const uint8_t *stored = data + (entry.offset - data_base);

    if (entry.stored_size == entry.raw_size) {
        memcpy(buf, stored, entry.stored_size);
    } else if (!pgb_lz4_decode(stored, entry.stored_size, buf, entry.raw_size)) {
        return PGB_ERR_DECOMPRESS;
    }
    status = pgb_page_check_text(header, buf, entry.raw_size);
    if (status != PGB_OK) {
        return status;
    }
    *out_len = entry.raw_size;
    return PGB_OK;
}

pgb_status_t pgb_verify_file(pgb_book_t *book, pgb_progress_fn progress, void *user)
{
    if (book == NULL || book->file == NULL) {
        return PGB_ERR_IO;
    }
    const uint32_t total = book->header.file_size;
    uint8_t chunk[PGB_CHUNK];
    pgb_crc_t crc;
    pgb_crc_init(&crc);
    if (!purrgo_fs_seek(book->file, 0u)) {
        return PGB_ERR_IO;
    }
    for (uint32_t done = 0u; done < total;) {
        uint32_t left = total - done;
        uint32_t n = (left < PGB_CHUNK) ? left : PGB_CHUNK;
        if (purrgo_fs_read(book->file, chunk, n) != n) {
            return PGB_ERR_IO;
        }
        pgb_crc_feed(&crc, chunk, n);
        done += n;
        if (progress != NULL && !progress(done, total, user)) {
            return PGB_ERR_CANCELLED;
        }
    }
    return (pgb_crc_value(&crc) == book->header.file_crc32) ? PGB_OK : PGB_ERR_FILE_CRC;
}

void pgb_close(pgb_book_t *book)
{
    if (book != NULL && book->file != NULL) {
        (void)purrgo_fs_close(book->file);
        book->file = NULL;
    }
}
