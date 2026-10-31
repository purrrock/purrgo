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
 * Отправляет приёмнику стартовую конфигурацию выбранной модели
 * (PURRGO_HW_GNSS_RECEIVER). Вызывать после запуска приёма, чтобы не пропустить
 * ответы приёмника. Конфигурация идемпотентна: можно отправлять повторно.
 */
void purrgo_gnss_receiver_configure(purrgo_gnss_tx_fn tx);

/* Реализации конкретных моделей (всегда доступны, в том числе для host-тестов). */
void purrgo_gnss_receiver_configure_casic(purrgo_gnss_tx_fn tx);
void purrgo_gnss_receiver_configure_ubx(purrgo_gnss_tx_fn tx);

/*
 * UBX (u-blox M10, интерфейс SPG 5.x).
 *
 * Кадр: B5 62 <class> <id> <len LE16> <payload> <CK_A> <CK_B>, контрольная сумма
 * Fletcher-8 по байтам от class до конца payload.
 * Возвращает длину кадра; 0, если кадр не помещается в out или len слишком велик.
 */
size_t purrgo_ubx_build_frame(uint8_t *out, size_t out_size, uint8_t msg_class,
                              uint8_t msg_id, const uint8_t *payload, uint16_t len);

/*
 * Элемент конфигурации (key-value) UBX-CFG-VALSET. Размер значения определяется
 * ключом (биты 30..28: 1 - 1 бит в 1 байте, 2 - 1 байт, 3 - 2 байта, 4 - 4 байта);
 * значение передаётся в little-endian.
 */
typedef struct {
    uint32_t key;
    uint32_t value;
} purrgo_ubx_cfg_item_t;

/*
 * UBX-CFG-VALSET версии 0 (до 64 пар). layers - маска слоёв (0x01 RAM, 0x02 BBR,
 * 0x04 Flash). Возвращает длину кадра; 0 при ошибке (нет места, 0 или более
 * 64 элементов, ключ с 8-байтным значением, layers == 0).
 */
size_t purrgo_ubx_build_valset(uint8_t *out, size_t out_size, uint8_t layers,
                               const purrgo_ubx_cfg_item_t *items, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* PURRGO_GNSS_RECEIVER_H */
