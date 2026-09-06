#include "purrgo/gnss_io.h"
#include "purrgo/gnss_mock.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Прочитать один байт из STM32 GNSS-потока.
 *
 * Сейчас функция возвращает байты из тестового NMEA MOCK
 * через общий потоковый интерфейс.
 *
 * @param[out] byte
 *     Адрес переменной, куда будет записан очередной байт.
 *
 * @return true
 *     Если байт получен.
 *
 * @return false
 *     Если входной поток временно не содержит данных.
 */
bool purrgo_gnss_read_byte(uint8_t *byte)
{
    if (byte == NULL)
    {
        return false;
    }

    /*
     * Читаем байт из потока Mock.
     * Реальный транспорт (UART/DMA) будет использовать похожую логику,
     * но читать из кольцевого буфера.
     */
    return purrgo_gnss_mock_read_byte(byte);
}
