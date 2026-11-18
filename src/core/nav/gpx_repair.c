#include "purrgo/gpx_repair.h"
#include "purrgo/fs_hal.h"
#include "purrgo/logger.h"
#include <string.h>

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void purrgo_gpx_scan_init(purrgo_gpx_scan_t *s)
{
    if (s != NULL) {
        memset(s, 0, sizeof *s);
    }
}

/* Структурные элементы: после их открывающего тега файл уже можно закрывать. */
static bool is_structural(const char *name)
{
    return strcmp(name, "gpx") == 0 || strcmp(name, "trk") == 0 ||
           strcmp(name, "trkseg") == 0 || strcmp(name, "rte") == 0;
}

static void mark_cut(purrgo_gpx_scan_t *s)
{
    s->cut = s->pos;
    s->cut_depth = s->depth;
    memcpy(s->cut_names, s->names, sizeof s->cut_names);
}

static void process_tag(purrgo_gpx_scan_t *s)
{
    s->tag[s->tag_len] = '\0';

    /* <?xml ...?>, <!-- -->, <!DOCTYPE>: на вложенность не влияют. */
    if (s->tag[0] == '?' || s->tag[0] == '!') {
        return;
    }

    if (s->tag[0] == '/') {
        if (strcmp(s->tag + 1, "gpx") == 0) {
            s->closed = true;
        }
        if (s->depth > 0u) {
            s->depth--;
        }
        if (s->depth > 0u && s->depth <= PURRGO_GPX_SCAN_DEPTH) {
            mark_cut(s);
        }
        return;
    }

    const char *name = s->tag;
    const bool selfclose = (s->last_c == '/');

    if (!s->root_seen) {
        s->root_seen = true;
        s->is_gpx = (strcmp(name, "gpx") == 0);
    }

    if (selfclose) {
        /* Открыт и сразу закрыт: вложенность не меняется, но это целый элемент. */
        if (s->depth == 0u && strcmp(name, "gpx") == 0) {
            s->closed = true;
        } else if (s->depth > 0u && s->depth <= PURRGO_GPX_SCAN_DEPTH) {
            mark_cut(s);
        }
        return;
    }

    if (s->depth < 255u) {
        s->depth++;
    }
    if (s->depth <= PURRGO_GPX_SCAN_DEPTH) {
        const unsigned idx = (unsigned)s->depth - 1u;
        if (s->name_long) {
            s->unsafe = true;
        }
        memcpy(s->names[idx], name, strlen(name) + 1u);
        if (is_structural(name)) {
            mark_cut(s);
        }
    }
}

void purrgo_gpx_scan_feed(purrgo_gpx_scan_t *s, const char *chunk, size_t len)
{
    if (s == NULL || chunk == NULL) {
        return;
    }

    for (size_t i = 0; i < len; i++) {
        const char c = chunk[i];
        s->pos++;

        if (c == '<') {
            s->in_tag = true;
            s->tag_len = 0;
            s->name_done = false;
            s->name_long = false;
            s->last_c = '\0';
        } else if (c == '>') {
            if (s->in_tag) {
                s->in_tag = false;
                process_tag(s);
            }
        } else if (s->in_tag) {
            s->last_c = c;
            if (!s->name_done) {
                if (is_space(c) || (c == '/' && s->tag_len > 0u)) {
                    s->name_done = true;
                } else if (s->tag_len + 1u < sizeof s->tag) {
                    s->tag[s->tag_len++] = c;
                } else {
                    s->name_long = true;
                }
            }
        }
    }
}

bool purrgo_gpx_scan_is_broken(const purrgo_gpx_scan_t *s)
{
    return s != NULL && s->is_gpx && !s->closed && !s->unsafe &&
           s->depth > 0u && s->cut_depth > 0u;
}

uint32_t purrgo_gpx_scan_cut_offset(const purrgo_gpx_scan_t *s)
{
    return (s != NULL) ? s->cut : 0u;
}

static bool append(char *out, size_t size, size_t *n, const char *text)
{
    const size_t l = strlen(text);
    if (*n + l >= size) {
        return false;
    }
    memcpy(out + *n, text, l + 1u);
    *n += l;
    return true;
}

size_t purrgo_gpx_scan_fix_text(const purrgo_gpx_scan_t *s, char *out, size_t size)
{
    if (!purrgo_gpx_scan_is_broken(s) || out == NULL || size == 0u) {
        return 0;
    }

    size_t n = 0;
    out[0] = '\0';
    bool ok = append(out, size, &n, "\n");

    /* Все открытые элементы, кроме корневого, - в обратном порядке. */
    for (unsigned i = s->cut_depth; ok && i > 1u; i--) {
        ok = append(out, size, &n, "</") &&
             append(out, size, &n, s->cut_names[i - 1u]) &&
             append(out, size, &n, ">");
    }
    if (ok && s->cut_depth > 1u) {
        ok = append(out, size, &n, "\n");
    }
    ok = ok && append(out, size, &n, "</") &&
         append(out, size, &n, s->cut_names[0]) &&
         append(out, size, &n, ">\n");

    return ok ? n : 0u;
}

bool purrgo_gpx_repair_file(const char *path, const purrgo_gpx_scan_t *s)
{
    char fix[160];
    const size_t fix_len = purrgo_gpx_scan_fix_text(s, fix, sizeof fix);

    if (path == NULL || fix_len == 0u) {
        return false;
    }

    purrgo_file_t *file = purrgo_fs_open(path, FS_WRITE_APPEND);
    if (file == NULL) {
        PURRGO_LOGW("gpx repair: cannot open %s\n\r", path);
        return false;
    }

    uint32_t size = 0;
    bool ok = purrgo_fs_size(file, &size);

    /* Размер должен совпадать с прочитанным: иначе чтение оборвалось по ошибке или файл изменился. */
    if (ok && size != s->pos) {
        PURRGO_LOGW("gpx repair: %s size %lu != scanned %lu, skipped\n\r", path,
                    (unsigned long)size, (unsigned long)s->pos);
        ok = false;
    }

    ok = ok && purrgo_fs_truncate(file, s->cut) &&
         purrgo_fs_write(file, (const uint8_t *)fix, (uint32_t)fix_len) == (uint32_t)fix_len &&
         purrgo_fs_sync(file);

    if (!purrgo_fs_close(file)) {
        ok = false;
    }

    if (ok) {
        PURRGO_LOG("gpx repair: %s fixed, cut %lu of %lu bytes\n\r", path,
                   (unsigned long)(size - s->cut), (unsigned long)size);
    } else {
        PURRGO_LOGW("gpx repair: %s failed\n\r", path);
    }
    return ok;
}
