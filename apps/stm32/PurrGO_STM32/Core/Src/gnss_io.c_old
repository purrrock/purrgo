#include "purrgo/gnss_io.h"
#include "usart.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "purrgo/logger.h"

/*
 * Кольцевой буфер входного потока USART1 через DMA.
 *
 * DMA постоянно записывает данные в этот буфер по кругу.
 * Размер буфера должен быть степенью двойки только для удобства,
 * но данная реализация этого не требует.
 */
#define GNSS_RX_BUFFER_SIZE 1024U

static uint8_t gnss_rx_buffer[GNSS_RX_BUFFER_SIZE];

/*
 * Позиция чтения потребителя.
 *
 * DMA head отдельно не хранится: он вычисляется непосредственно
 * из текущего значения NDTR DMA.
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
     * Формат команды: $PCAS02,1000*2E\r\n
     */
    static const uint8_t gnss_update_rate_cmd[] = "$PCAS02,1000*2E\r\n";

    /*
     * Для выдачи только GGA + GSA + RMC.
     */
    static const uint8_t gnss_nmea_cmd[] =
        "$PCAS03,1,0,1,0,1,0,0,0,0,0,,,0,0,,,,0*3A\r\n";

    HAL_UART_Transmit(
        &huart1,
        (uint8_t *)gnss_update_rate_cmd,
        sizeof(gnss_update_rate_cmd) - 1U,
        1000U);

    HAL_UART_Transmit(
        &huart1,
        (uint8_t *)gnss_nmea_cmd,
        sizeof(gnss_nmea_cmd) - 1U,
        1000U);

    /*
     * Запускаем непрерывный приём через Circular DMA.
     */
    if (HAL_UART_Receive_DMA(
            &huart1,
            gnss_rx_buffer,
            GNSS_RX_BUFFER_SIZE) != HAL_OK)
    {
        PURRGO_LOG("GNSS INIT ERROR!\r\n");
    }
}

/**
 * @brief Прочитать один байт из входного GNSS-потока.
 *
 * Функция не блокируется. Если новых данных нет, возвращает false.
 *
 * DMA является единственным владельцем позиции head.
 * Позиция head вычисляется непосредственно из NDTR:
 *
 *     head = BUFFER_SIZE - NDTR
 *
 * tail — позиция следующего байта, который должен прочитать
 * потребитель.
 *
 * Если DMA успел полностью обойти кольцевой буфер до того,
 * как потребитель прочитал данные, старые данные считаются
 * потерянными. В этом случае tail перемещается на head,
 * после чего чтение продолжается с текущего положения DMA.
 *
 * @param[out] byte Адрес переменной для принятого байта.
 *
 * @return true  — байт получен.
 * @return false — новых данных нет или byte == NULL.
 */
bool purrgo_gnss_read_byte(uint8_t *byte)
{
    if (byte == NULL)
    {
        return false;
    }

    /*
     * NDTR содержит количество байт, которые DMA ещё должен
     * передать до конца текущего оборота буфера.
     *
     * Поэтому:
     *
     *     head = BUFFER_SIZE - NDTR
     *
     * head всегда находится в диапазоне [0, BUFFER_SIZE - 1].
     */
    uint16_t ndtr = __HAL_DMA_GET_COUNTER(huart1.hdmarx);
    uint16_t gnss_rx_head = GNSS_RX_BUFFER_SIZE - ndtr;

    /*
     * Защита от некорректного значения NDTR.
     *
     * При нормально работающем Circular DMA это условие
     * выполняться не должно.
     */
    if (gnss_rx_head >= GNSS_RX_BUFFER_SIZE)
    {
        gnss_rx_head = 0U;
    }

    /*
     * Вычисляем количество байт между tail и head.
     *
     * В кольцевом буфере:
     *
     *     head >= tail:
     *         available = head - tail
     *
     *     head < tail:
     *         available = BUFFER_SIZE - tail + head
     *
     * При обычной работе available находится в диапазоне
     * 0 .. BUFFER_SIZE - 1.
     *
     * Если DMA сделал полный оборот, старые данные были
     * перезаписаны. В этом случае расстояние само по себе
     * уже не позволяет восстановить количество потерянных
     * байтов, поэтому начинаем с текущего head.
     */
    uint16_t available;

    if (gnss_rx_head >= gnss_rx_tail)
    {
        available = gnss_rx_head - gnss_rx_tail;
    }
    else
    {
        available = GNSS_RX_BUFFER_SIZE
                  - gnss_rx_tail
                  + gnss_rx_head;
    }

    /*
     * head == tail означает отсутствие непрочитанных данных.
     *
     * Важное ограничение такой схемы: состояние "пусто" и
     * состояние "полный буфер" имеют одинаковые head и tail.
     * Поэтому переполнение определяется по факту того,
     * что потребитель не успевал читать данные между двумя
     * наблюдениями head.
     *
     * В данной реализации head читается непосредственно из DMA,
     * а tail является единственным состоянием потребителя.
     */
    if (gnss_rx_head == gnss_rx_tail)
    {
        return false;
    }

    /*
     * Прочитать следующий байт.
     */
    *byte = gnss_rx_buffer[gnss_rx_tail];

    /*
     * Передвинуть tail.
     */
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
 * После ошибки (например, ORE или FE) снова запускаем
 * приём через DMA.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }

    /*
     * Останавливаем текущий ошибочный приём.
     */
    HAL_UART_AbortReceive(huart);

    /*
     * После перезапуска DMA начинаем чтение с начала буфера.
     *
     * DMA также начинает новый оборот с начала переданного
     * ему буфера.
     */
    gnss_rx_tail = 0U;

    (void)HAL_UART_Receive_DMA(
        &huart1,
        gnss_rx_buffer,
        GNSS_RX_BUFFER_SIZE);
}