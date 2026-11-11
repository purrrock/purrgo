#include "purrgo/gnss_io.h"
#include "purrgo/hardware_config.h"
#include <purrgo/pc_gnss_mock.h>

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

#include <purrgo/pc_gnss_mock.h>

/*
 * При использовании MOCK GNSS байтового потока,
 * читаем байты из сгенерированных MOCK NMEA предложений.
 */
bool purrgo_gnss_read_byte(uint8_t *byte)
{
    return pc_gnss_mock_read_byte(byte);
}

#endif /* USE_MOCK_GNSS */

/*
 * Команды приёмнику (сброс одометра, опрос версии). Эмулятор GNSS принимает их;
 * с реальным COM-портом передача на PC не поддерживается (serial_hal только читает).
 */
bool purrgo_gnss_write(const uint8_t *data, uint16_t length)
{
#if defined(USE_MOCK_GNSS) && PURRGO_GNSS_UBX
    return pc_gnss_mock_write(data, length);
#else
    (void)data;
    (void)length;
    return false;
#endif
}

/*
 * Управление питанием GNSS на PC: приёмник эмулируется (serial_hal или mock),
 * поэтому включать и выключать нечего.
 */
void purrgo_gnss_power_on(void)
{
}

void purrgo_gnss_power_off(void)
{
}
