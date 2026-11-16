#ifndef PURRGO_TRACK_NAME_H
#define PURRGO_TRACK_NAME_H

/*
 * Имена файлов записанных треков (каталог TRACKS).
 *
 * Логгер называет файл секундами UTC от 01.01.2000 в HEX, 8 символов:
 * "2F8A91C3.gpx" (см. track_logger.c). Для списка на экране имя переводится
 * в дату и время; имена прежнего формата и чужие файлы показываются как есть.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Раскодировать имя вида XXXXXXXX.gpx (8 hex-цифр, расширение любого регистра)
 * в секунды от 01.01.2000 UTC. false - имя другого формата или значение
 * меньше порога прежнего формата (там секунды от 01.01.2026 по местному
 * времени, их в дату не перевести).
 */
bool purrgo_track_name_decode(const char *name, uint32_t *epoch_out);

/*
 * Подпись для списка: "2026-11-16 08:12" в местном времени (tz_offset_min -
 * смещение от UTC в минутах). Если имя не раскодировать, в out копируется
 * само имя (обрезается по size).
 */
void purrgo_track_name_label(const char *name, int16_t tz_offset_min,
                             char *out, size_t size);

/*
 * true - файл name из каталога TRACKS сейчас открыт логгером на запись.
 * Такой файл нельзя читать (FatFs блокирует повторное открытие), да и
 * активный трек уже рисуется слоем TRACK из RAM.
 */
bool purrgo_track_name_is_recording(const char *name);

/* Сравнение имён без учёта регистра ASCII: <0, 0, >0 (порядок = порядок по времени для hex-имён). */
int purrgo_track_name_cmp(const char *a, const char *b);

#endif /* PURRGO_TRACK_NAME_H */
