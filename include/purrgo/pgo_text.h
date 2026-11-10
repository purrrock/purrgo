#ifndef PURRGO_PGO_TEXT_H
#define PURRGO_PGO_TEXT_H

#include <stddef.h>
#include <stdint.h>

/*
 * Перекодировка текста во внутреннюю 8-битную кодировку шрифта PurrGO-256
 * (docs/font-encoding.md, include/purrgo/font5x7.h).
 *
 * Имена файлов и каталогов с SD приходят из FatFs в UTF-8
 * (FF_LFN_UNICODE = 2), а шрифт 5x7 индексируется байтом PurrGO-256, поэтому
 * перед выводом на дисплей любую строку, содержащую имя с карты, нужно
 * пропустить через эти функции. Исходное имя (для открытия файла, хранения в
 * PURRGO.CFG) менять нельзя: преобразование необратимо (несколько символов
 * Unicode сводятся к одному глифу).
 *
 * Таблица совпадает с _PGO_CHAR_MAP в tools/map-compiler/purrgo_models.py
 * (так же кодируются названия на карте); соответствие проверяет
 * tests/host/test_pgo_text.c.
 */

/* Код PurrGO-256 для кодовой точки Unicode; нет в таблице - '?' (0x3F). */
uint8_t purrgo_unicode_to_pgo(uint32_t codepoint);

/*
 * UTF-8 -> PurrGO-256. Результат всегда завершён нулём (если dst_size > 0),
 * обрезается по dst_size - 1. Некорректные последовательности UTF-8 дают '?'
 * (по одному '?' на каждый испорченный байт). Возвращает длину результата.
 */
size_t purrgo_utf8_to_pgo(const char *src, char *dst, size_t dst_size);

/*
 * То же на месте. Безопасно: результат никогда не длиннее исходной строки
 * (один символ UTF-8 занимает не менее одного байта, на выходе всегда один).
 */
void purrgo_utf8_to_pgo_inplace(char *str);

#endif /* PURRGO_PGO_TEXT_H */
