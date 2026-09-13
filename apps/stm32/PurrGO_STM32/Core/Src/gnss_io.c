#include "purrgo/gnss_io.h"
#include "usart.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "purrgo/logger.h"

/*
 * Кольцевой буфер входного потока USART1 (через DMA).
 *
 * 1024 байта достаточно для временного накопления NMEA-потока,
 * пока основной цикл занят другими задачами.
 */
#define GNSS_RX_BUFFER_SIZE 1024U

static uint8_t gnss_rx_buffer[GNSS_RX_BUFFER_SIZE];

/*
 * tail изменяется только из основного контекста (потребитель).
 * head (позиция записи) вычисляется на основе оставшихся байтов передачи DMA.
 */
static uint16_t gnss_rx_tail = 0U;

/**
 * @brief Инициализировать приём GNSS через USART1.
 *
 * Приём выполняется через Circular DMA. Функция должна быть вызвана
 * после MX_USART1_UART_Init() и инициализации DMA.
 */
void purrgo_gnss_init(void)
{
    gnss_rx_tail = 0U;

    /*
     * AT6558R:
     * установить период выдачи навигационных данных 1000 мс,
     * то есть 1 Гц.
     *
     * Формат команды:
     * $PCAS02,1000*2E\r\n
     */
    static const uint8_t gnss_update_rate_cmd[] = "$PCAS02,1000*2E\r\n";
    // Для выдачи только GGA + GSA + RMC
    static const uint8_t gnss_nmea_cmd[] = "$PCAS03,1,0,1,0,1,0,0,0,0,0,,,0,0,,,,0*3A\r\n";

    HAL_UART_Transmit(&huart1, (uint8_t *)gnss_update_rate_cmd, sizeof(gnss_update_rate_cmd) - 1U, 1000U);
    HAL_UART_Transmit(&huart1, (uint8_t *)gnss_nmea_cmd, sizeof(gnss_nmea_cmd) - 1U, 1000U);

    /*
     * Запускаем непрерывный приём через DMA.
     */
    if (HAL_UART_Receive_DMA(&huart1, gnss_rx_buffer, GNSS_RX_BUFFER_SIZE) != HAL_OK)
    {
        PURRGO_LOG("GNSS INIT ERROR!\r\n");
    }
}

/**
 * @brief Прочитать один байт из входного GNSS-потока.
 *
 * Функция не блокируется. Если новых данных нет, возвращает false.
 *
 * @param[out] byte Адрес переменной для принятого байта.
 *
 * @return true  — байт получен.
 * @return false — буфер пуст или byte == NULL.
 */
bool purrgo_gnss_read_byte(uint8_t *byte)
{
    if (byte == NULL)
    {
        return false;
    }

    /*
     * Вычисляем текущую позицию записи DMA.
     * __HAL_DMA_GET_COUNTER возвращает количество байт, которые
     * ОСТАЛОСЬ передать до конца буфера DMA.
     */
    uint16_t ndtr = __HAL_DMA_GET_COUNTER(huart1.hdmarx);
    uint16_t gnss_rx_head = GNSS_RX_BUFFER_SIZE - ndtr;

    /* Handle boundary case if ndtr == 0 */
    if (gnss_rx_head >= GNSS_RX_BUFFER_SIZE)
    {
        gnss_rx_head = 0;
    }

    if (gnss_rx_tail == gnss_rx_head)
    {
        return false;
    }

    *byte = gnss_rx_buffer[gnss_rx_tail];

    gnss_rx_tail++;
    if (gnss_rx_tail >= GNSS_RX_BUFFER_SIZE)
    {
        gnss_rx_tail = 0U;
    }

    return true;
}

/**
 * @brief Callback ошибки UART.
 *
 * После ошибки (например, ORE, FE) снова запускаем приём через DMA.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }

    /*
     * Останавливаем текущий ошибочный приём, сбрасываем состояние
     * и запускаем заново. Очистка буфера и сброс tail зависят от архитектуры,
     * но здесь безопаснее просто перезапустить приём с текущей позиции или с начала.
     */
    HAL_UART_AbortReceive(huart);

    gnss_rx_tail = 0U; // Сброс хвоста при рестарте DMA

    (void)HAL_UART_Receive_DMA(&huart1, gnss_rx_buffer, GNSS_RX_BUFFER_SIZE);
}

