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
    static bool mock_initialized = false;
    if (!mock_initialized)
    {
        purrgo_gnss_mock_init();
        mock_initialized = true;
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

/*
 * This platform mock driver implementation does not currently provide
 * an explicit update trigger loop since main.c handles byte polling,
 * but purrgo_gnss_mock_update is exposed in gnss_mock.h and needs to
 * be called periodically if we want to simulate movement.
 * For STM32 mock, we can expose a dedicated function or hook it into SysTick.
 */
void stm32_gnss_mock_update(void) {
    purrgo_gnss_mock_update();
}
