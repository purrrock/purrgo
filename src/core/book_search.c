#include "purrgo/book_search.h"

uint8_t purrgo_book_search_fold(uint8_t c)
{
    if (c >= 'A' && c <= 'Z') {
        return (uint8_t)(c + 0x20u);
    }
    if (c >= 0x80u && c <= 0x9Fu) {
        return (uint8_t)(c + 0x20u);   /* А-Я -> а-я */
    }
    if (c >= 0xC0u && c <= 0xCFu) {
        return (uint8_t)(c + 0x10u);   /* Ґ Є І Ї Ў Ђ Ј Љ Њ Џ Ѓ Ѕ Ғ Қ Ң Ұ -> строчные */
    }
    if (c == 0xF0u || c == 0xF1u) {
        return 0xA5u;                  /* Ё, ё -> е */
    }
    return c;
}

size_t purrgo_book_search_prepare(const char *typed, uint8_t out[PURRGO_BOOK_SEARCH_MAX + 1u])
{
    size_t n = 0u;
    bool pending_space = false;

    out[0] = '\0';
    if (typed == NULL) {
        return 0u;
    }
    for (const uint8_t *p = (const uint8_t *)typed; *p != '\0' && n < PURRGO_BOOK_SEARCH_MAX; p++) {
        if (*p == ' ' || *p == '\n' || *p == '\t') {
            pending_space = (n > 0u);   /* пробелы в начале отбрасываются, в конце - тоже (см. ниже) */
            continue;
        }
        if (pending_space) {
            if (n + 1u >= PURRGO_BOOK_SEARCH_MAX) {
                break;
            }
            out[n++] = ' ';
            pending_space = false;
        }
        out[n++] = purrgo_book_search_fold(*p);
    }
    out[n] = '\0';
    return n;
}

static bool is_space(uint8_t c)
{
    return c == ' ' || c == '\n';
}

/* Совпадение запроса с текстом, начиная с t[i] (первый символ уже совпал). */
static bool match_here(const uint8_t *t, size_t n, size_t i,
                       const uint8_t *p, size_t m, size_t *end)
{
    size_t j = 0u;

    while (j < m) {
        if (i >= n) {
            return false;
        }
        const uint8_t c = t[i];
        const uint8_t pc = p[j];

        if (pc == ' ') {
            if (!is_space(c)) {
                return false;
            }
            while (i < n && is_space(t[i])) {
                i++;
            }
            j++;
            continue;
        }
        if (j > 0u) {
            /* Конец строки после дефиса: перевод строки в запросе не нужен. */
            if (c == '\n' && t[i - 1u] == '-') {
                i++;
                continue;
            }
            /* Перенос слова: дефис и LF в тексте, а в запросе слово слитно. */
            if (c == '-' && pc != '-' && i + 1u < n && t[i + 1u] == '\n') {
                i += 2u;
                continue;
            }
        }
        if (purrgo_book_search_fold(c) != pc) {
            return false;
        }
        i++;
        j++;
    }
    if (end != NULL) {
        *end = i;
    }
    return true;
}

static bool args_ok(const uint8_t *text, const uint8_t *pat, size_t pat_len)
{
    return text != NULL && pat != NULL && pat_len > 0u && pat_len <= PURRGO_BOOK_SEARCH_MAX &&
           pat[0] != ' ';
}

bool purrgo_book_search_find(const uint8_t *text, size_t text_len, size_t lo, size_t hi,
                             const uint8_t *pat, size_t pat_len,
                             size_t *match_start, size_t *match_end)
{
    if (!args_ok(text, pat, pat_len)) {
        return false;
    }
    if (hi > text_len) {
        hi = text_len;
    }
    for (size_t i = lo; i < hi; i++) {
        size_t end;

        if (purrgo_book_search_fold(text[i]) != pat[0]) {
            continue;
        }
        if (match_here(text, text_len, i, pat, pat_len, &end)) {
            if (match_start != NULL) {
                *match_start = i;
            }
            if (match_end != NULL) {
                *match_end = end;
            }
            return true;
        }
    }
    return false;
}

bool purrgo_book_search_find_last(const uint8_t *text, size_t text_len, size_t lo, size_t hi,
                                  const uint8_t *pat, size_t pat_len,
                                  size_t *match_start, size_t *match_end)
{
    if (!args_ok(text, pat, pat_len)) {
        return false;
    }
    if (hi > text_len) {
        hi = text_len;
    }
    for (size_t i = hi; i > lo; i--) {
        size_t end;
        const size_t pos = i - 1u;

        if (purrgo_book_search_fold(text[pos]) != pat[0]) {
            continue;
        }
        if (match_here(text, text_len, pos, pat, pat_len, &end)) {
            if (match_start != NULL) {
                *match_start = pos;
            }
            if (match_end != NULL) {
                *match_end = end;
            }
            return true;
        }
    }
    return false;
}
