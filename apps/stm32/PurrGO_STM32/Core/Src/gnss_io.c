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
 * Монотонно возрастающие счетчики для корректного обнаружения
 * переполнения буфера без проблемы "head == tail" на полном обороте.
 */
static uint32_t total_written = 0U;
static uint32_t total_read = 0U;
static uint16_t prev_dma_head = 0U;

/**
 * @brief Инициализировать приём GNSS через USART1.
 *
 * Приём выполняется через Circular DMA. Функция должна быть вызвана
 * после MX_USART1_UART_Init() и инициализации DMA.
 */
void purrgo_gnss_init(void)
{
    total_written = 0U;
    total_read = 0U;
    prev_dma_head = 0U;

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
    uint16_t curr_dma_head = GNSS_RX_BUFFER_SIZE - ndtr;

    /* Handle boundary case */
    if (curr_dma_head >= GNSS_RX_BUFFER_SIZE)
    {
        curr_dma_head = 0U;
    }

    /* Определяем, сколько байт было записано с прошлого вызова. */
    uint16_t added_bytes;
    if (curr_dma_head >= prev_dma_head)
    {
        added_bytes = curr_dma_head - prev_dma_head;
    }
    else
    {
        added_bytes = GNSS_RX_BUFFER_SIZE - prev_dma_head + curr_dma_head;
    }

    total_written += added_bytes;
    prev_dma_head = curr_dma_head;

    /*
     * Механизм обнаружения переполнения:
     * Если разница между total_written и total_read превышает размер буфера,
     * значит старые данные были перезаписаны DMA.
     * Сдвигаем total_read, чтобы отбросить затертые данные.
     */
    if ((total_written - total_read) > GNSS_RX_BUFFER_SIZE)
    {
        total_read = total_written - GNSS_RX_BUFFER_SIZE;
    }

    if (total_written == total_read)
    {
        return false;
    }

    *byte = gnss_rx_buffer[total_read % GNSS_RX_BUFFER_SIZE];
    total_read++;

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
     * и запускаем заново.
     */
    HAL_UART_AbortReceive(huart);

    /* Сбрасываем счетчики при рестарте DMA, чтобы не было неконсистентности */
    total_written = 0U;
    total_read = 0U;
    prev_dma_head = 0U;

    (void)HAL_UART_Receive_DMA(&huart1, gnss_rx_buffer, GNSS_RX_BUFFER_SIZE);
}
