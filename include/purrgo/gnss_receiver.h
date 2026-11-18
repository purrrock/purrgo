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
 * Профиль настройки приёмника:
 *   PURRGO_GNSS_PROFILE_NAV     - навигация (карта, путевой компьютер): данные каждую секунду,
 *                                 при PURRGO_GNSS_PSM = PSMCT;
 *   PURRGO_GNSS_PROFILE_TRACKER - режим "трекер с отключённым дисплеем": решения раз в
 *                                 PURRGO_GNSS_TRACKER_PERIOD_S, PSMCT (PURRGO_GNSS_TRACKER_MODE_CT, по умолчанию)
 *                                 или PSMOO (MODE_OO); при PURRGO_GNSS_TRACKER_MODE_OFF совпадает с
 *                                 PURRGO_GNSS_PROFILE_NAV.
 */
typedef enum {
    PURRGO_GNSS_PROFILE_NAV = 0,
    PURRGO_GNSS_PROFILE_TRACKER,
    /*
     * Навигация после трекера: то же, что PURRGO_GNSS_PROFILE_NAV, плюс удаление из слоя BBR параметров
     * PSMOO (OPERATEMODE, периоды, ONTIME, DONOTENTEROFF), записанных профилем трекера. Слой RAM
     * перекрывается настройкой навигации и так, удаление нужно, чтобы PSMOO не вернулся из BBR, если
     * RAM приёмника когда-нибудь очистится (сброс, состояние "off").
     */
    PURRGO_GNSS_PROFILE_NAV_AFTER_TRACKER
} purrgo_gnss_profile_t;

/*
 * Отправляет приёмнику стартовую конфигурацию выбранной модели
 * (PURRGO_HW_GNSS_RECEIVER) для профиля навигации. Вызывать после запуска приёма, чтобы не
 * пропустить ответы приёмника. Конфигурация идемпотентна: можно отправлять повторно.
 */
void purrgo_gnss_receiver_configure(purrgo_gnss_tx_fn tx);

/*
 * То же для заданного профиля. uart_baud - текущая скорость UART приёмника (0 - не задана): в профиле
 * трекера она записывается в слой BBR, потому что RAM приёмника в состоянии "off" PSMOO очищается и
 * без этого модуль после пробуждения оказался бы на заводской скорости.
 */
void purrgo_gnss_receiver_configure_profile(purrgo_gnss_tx_fn tx, purrgo_gnss_profile_t profile,
                                            uint32_t uart_baud);

/*
 * Реализация для u-blox M10 (всегда доступна, в том числе для host-тестов).
 * Отправляет несколько кадров подряд: базовую настройку (частота, NMEA GGA/RMC/GSA),
 * а при PURRGO_GNSS_UBX = 1 ещё отдельными UBX-CFG-VALSET бинарные сообщения
 * (PVT, ODO, MON-RF, NAV-DOP, SEC-SIG), одометр, модель динамики, выключение NMEA
 * (CFG-UART1OUTPROT-NMEA, PURRGO_GNSS_UBX_NMEA = 0) и опрос UBX-MON-VER.
 */
void purrgo_gnss_receiver_configure_ubx(purrgo_gnss_tx_fn tx);
void purrgo_gnss_receiver_configure_ubx_profile(purrgo_gnss_tx_fn tx, purrgo_gnss_profile_t profile,
                                                uint32_t uart_baud);

/*
 * Собирает команду смены скорости UART приёмника на baud бит/с (u-blox M10:
 * UBX-CFG-VALSET, CFG-UART1-BAUDRATE, слой PURRGO_GNSS_UBX_LAYERS). Отправляет
 * вызывающий (после передачи нужно выдержать паузу и переключить свой UART).
 * Возвращает длину кадра; 0, если выбранная модель не умеет менять скорость
 * (PURRGO_GNSS_RECEIVER_NONE), baud == 0 или кадр не помещается в out
 * (нужно 20 байт).
 */
size_t purrgo_gnss_receiver_build_baud(uint8_t *out, size_t out_size, uint32_t baud);

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
/*
 * UBX-CFG-VALDEL (version 0): удалить значения ключей из слоёв BBR (0x02) и/или Flash (0x04); RAM удалить
 * нельзя. Возвращает длину кадра, 0 - ошибка (нет ключей, больше 64 ключей, нет слоя).
 */
size_t purrgo_ubx_build_valdel(uint8_t *out, size_t out_size, uint8_t layers,
                               const uint32_t *keys, size_t count);

size_t purrgo_ubx_build_valset(uint8_t *out, size_t out_size, uint8_t layers,
                               const purrgo_ubx_cfg_item_t *items, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* PURRGO_GNSS_RECEIVER_H */
