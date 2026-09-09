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
#include "purrgo/system_time.h"

bool purrgo_gnss_read_byte(uint8_t *byte)
{
    static bool mock_initialized = false;
    static uint32_t last_mock_update_ms = 0;

    if (!mock_initialized)
    {
        purrgo_gnss_mock_init();
        mock_initialized = true;
        last_mock_update_ms = purrgo_system_time_ms();
    }

    uint32_t current_time_ms = purrgo_system_time_ms();
    if (current_time_ms - last_mock_update_ms >= 1000U)
    {
        purrgo_gnss_mock_update();
        last_mock_update_ms = current_time_ms;
    }

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
