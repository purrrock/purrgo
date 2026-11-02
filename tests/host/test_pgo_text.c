/*
 * Host-тесты перекодировки имён с SD (UTF-8 -> PurrGO-256), src/core/pgo_text.c:
 *   - ASCII без изменений;
 *   - русская, белорусская, украинская, сербская кириллица;
 *   - латиница с диакритикой сводится к общим глифам;
 *   - типографские кавычки и апострофы, номер;
 *   - неизвестные символы, некорректный/обрезанный UTF-8, 4-байтные (эмодзи);
 *   - границы буфера, преобразование на месте.
 * Совпадение таблицы с purrgo_models.py проверяет tests/check_pgo_text_table.py.
 */
#include <string.h>
#include "purrgo/pgo_text.h"
#include "test_common.h"

static int eq(const char *utf8, const char *expect)
{
    char out[64];
    size_t n = purrgo_utf8_to_pgo(utf8, out, sizeof(out));
    return n == strlen(expect) && strcmp(out, expect) == 0;
}

int main(void)
{
    /* ASCII */
    CHECK(eq("", ""));
    CHECK(eq("ROUTES/track_01.gpx", "ROUTES/track_01.gpx"));

    /* русский: А=80 Е=85 Ж=86 Я=9F; а=A0 е=A5 ж=A6 я=BF; Ё=F0 ё=F1 */
    CHECK(eq("\xD0\x90\xD0\x95\xD0\x96\xD0\xAF", "\x80\x85\x86\x9F"));
    CHECK(eq("\xD0\xB0\xD0\xB5\xD0\xB6\xD1\x8F", "\xA0\xA5\xA6\xBF"));
    CHECK(eq("\xD0\x81\xD1\x91", "\xF0\xF1"));
    /* "Минск" */
    CHECK(eq("\xD0\x9C\xD0\xB8\xD0\xBD\xD1\x81\xD0\xBA", "\x8C\xA8\xAD\xB1\xAA"));

    /* белорусский/украинский: І=C2 і=D2 Ў=C4 ў=D4 Ґ=C0 ґ=D0 Ї=C3 ї=D3 Є=C1 є=D1 */
    CHECK(eq("\xD0\x86\xD1\x96", "\xC2\xD2"));
    CHECK(eq("\xD0\x8E\xD1\x9E", "\xC4\xD4"));
    CHECK(eq("\xD2\x90\xD2\x91", "\xC0\xD0"));
    CHECK(eq("\xD0\x87\xD1\x97", "\xC3\xD3"));
    CHECK(eq("\xD0\x84\xD1\x94", "\xC1\xD1"));

    /* латиница с диакритикой: Ä/À/Å -> E0, Ž/Ź/Ż -> EE, ß -> E3 */
    CHECK(eq("\xC3\x84\xC3\x80\xC3\xA5", "\xE0\xE0\xE0"));
    CHECK(eq("\xC5\xBD\xC5\xB9\xC5\xBC", "\xEE\xEE\xEE"));
    CHECK(eq("\xC3\x9F", "\xE3"));

    /* типографика: «» “” „ -> '"', ’ ‘ ´ ` -> '\'', № -> 'N' */
    CHECK(eq("\xC2\xAB" "x" "\xC2\xBB", "\"x\""));
    CHECK(eq("\xE2\x80\x9C" "x" "\xE2\x80\x9D", "\"x\""));
    CHECK(eq("\xE2\x80\x99", "'"));
    CHECK(eq("\xE2\x84\x96" "5", "N5"));

    /* неизвестное: греческая альфа, эмодзи (4 байта), CJK -> по одному '?' */
    CHECK(eq("\xCE\xB1", "?"));
    CHECK(eq("\xF0\x9F\x90\xB1", "?"));
    CHECK(eq("a\xE4\xB8\xAD" "b", "a?b"));

    /* некорректный UTF-8: одиночный continuation, обрезанная последовательность,
       overlong NUL (C0 80), суррогат (ED A0 80), CP866-байты вместо UTF-8 */
    CHECK(eq("\x80", "?"));
    CHECK(eq("ab\xD0", "ab?"));
    CHECK(eq("\xD0" "z", "?z"));
    CHECK(eq("\xC0\x80", "??"));
    CHECK(eq("\xED\xA0\x80", "???"));
    CHECK(eq("\xAB\xA8", "??"));

    /* границы буфера: всегда завершается нулём, обрезается по размеру */
    {
        char out[4];
        size_t n = purrgo_utf8_to_pgo("\xD0\x96\xD0\x96\xD0\x96\xD0\x96\xD0\x96", out, sizeof(out));
        CHECK(n == 3);
        CHECK(memcmp(out, "\x86\x86\x86", 3) == 0 && out[3] == '\0');
        char one[1] = { 'x' };
        CHECK(purrgo_utf8_to_pgo("abc", one, 1) == 0 && one[0] == '\0');
        CHECK(purrgo_utf8_to_pgo("abc", NULL, 10) == 0);
        CHECK(purrgo_utf8_to_pgo(NULL, out, sizeof(out)) == 0 && out[0] == '\0');
    }

    /* на месте */
    {
        char s[] = "[\xD0\x9C\xD0\xB8\xD0\xBD\xD1\x81\xD0\xBA]\xE2\x84\x96";
        purrgo_utf8_to_pgo_inplace(s);
        CHECK(strcmp(s, "[\x8C\xA8\xAD\xB1\xAA]N") == 0);
        purrgo_utf8_to_pgo_inplace(NULL);
        char bad[] = "\xD0";
        purrgo_utf8_to_pgo_inplace(bad);
        CHECK(strcmp(bad, "?") == 0);
    }

    /* точечный поиск */
    CHECK(purrgo_unicode_to_pgo('A') == 'A');
    CHECK(purrgo_unicode_to_pgo(0x0416) == 0x86);
    CHECK(purrgo_unicode_to_pgo(0x10FFFF) == '?');
    CHECK(purrgo_unicode_to_pgo(0x0080) == '?');

    return test_finish("test_pgo_text");
}
