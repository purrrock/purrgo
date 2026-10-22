#ifndef PURRGO_GNSS_RECEIVER_H
#define PURRGO_GNSS_RECEIVER_H

/*
 * Настройка приёмника GNSS: команды, специфичные для конкретной модели, отделены
 * от транспорта (UART/DMA, purrgo/gnss_io.h) и от разбора NMEA (gnss.c).
 *
 * Модель выбирается в hardware_config.h (PURRGO_HW_GNSS_RECEIVER). Модуль не
 * зависит от платформы: платформа передаёт функцию отправки байт приёмнику, поэтому
 * формат команд и контрольные суммы проверяются host-тестами
 * (tests/host/test_gnss_receiver.c).
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Отправка байт приёмнику (блокирующая, например HAL_UART_Transmit). */
typedef void (*purrgo_gnss_tx_fn)(const uint8_t *data, size_t len);

/*
 * Формирует NMEA-кадр "$<body>*HH\r\n" (HH - XOR всех символов body, две
 * шестнадцатеричные цифры верхнего регистра) и '\0' в конце.
 *
 * body - тело команды БЕЗ '$' и '*HH', например "PCAS02,1000".
 * Возвращает длину кадра без '\0'; 0, если кадр с '\0' не помещается в out.
 */
size_t purrgo_nmea_build_frame(char *out, size_t out_size, const char *body);

/*
 * Отправляет приёмнику стартовую конфигурацию выбранной модели.
 * Вызывать после запуска приёма (DMA), чтобы не пропустить ответы приёмника.
 */
void purrgo_gnss_receiver_configure(purrgo_gnss_tx_fn tx);

#ifdef __cplusplus
}
#endif

#endif /* PURRGO_GNSS_RECEIVER_H */
