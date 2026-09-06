#include "purrgo/gnss_io.h"

#ifndef USE_MOCK_GNSS

#include "serial_hal.h"

/*
 * PC-реализация общего GNSS byte-stream интерфейса.
 *
 * В режиме реального GNSS передаём запрос существующему
 * serial_hal. Таким образом, Core не знает ни о COM-порте,
 * ни о serial_hal.
 */
bool purrgo_gnss_read_byte(uint8_t *byte)
{
    /*
     * serial_hal_read_byte() уже является существующим
     * интерфейсом транспорта GNSS на PC.
     *
     * Сохраняем его семантику: ненулевой результат означает,
     * что байт был получен.
     */
    return serial_hal_read_byte(byte) != 0;
}

#else

#include "purrgo/gnss_mock.h"

/*
 * При использовании MOCK GNSS байтового потока,
 * читаем байты из сгенерированных MOCK NMEA предложений.
 */
bool purrgo_gnss_read_byte(uint8_t *byte)
{
    return purrgo_gnss_mock_read_byte(byte);
}

#endif /* USE_MOCK_GNSS */
