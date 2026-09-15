#include "purrgo/gnss_io.h"
#include "usart.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "purrgo/logger.h"

/*
 * Размер кольцевого DMA-буфера GNSS.
 *
 * DMA непрерывно записывает входные байты в этот массив.
 * После достижения конца массива DMA начинает запись с начала.
 */
#define GNSS_RX_BUFFER_SIZE 1024U

/*
 * Буфер приёма GNSS.
 *
 * DMA является владельцем записи в этот массив.
 * Потребитель читает данные через purrgo_gnss_read_byte().
 */
static uint8_t gnss_rx_buffer[GNSS_RX_BUFFER_SIZE];

/*
 * Абсолютная позиция следующего байта, который должен прочитать
 * потребитель.
 *
 * Это не индекс внутри буфера.
 *
 * Значение увеличивается без ограничения и естественным образом
 * переполняется uint32_t. Для разности позиций это безопасно,
 * пока разница между producer и consumer существенно меньше
 * UINT32_MAX.
 */
static uint32_t gnss_rx_tail;

/*
 * Количество полных оборотов DMA по кольцевому буферу.
 */
static volatile uint32_t gnss_rx_wrap_count;

/*
 * Флаг ошибки UART.
 *
 * Устанавливается в обработчике прерывания HAL_UART_ErrorCallback.
 * Обрабатывается синхронно в purrgo_gnss_read_byte() для
 * безопасного перезапуска без гонки данных.
 */
static volatile bool gnss_rx_error_flag = false;

/**
 * @brief Получить согласованное состояние DMA producer.
 */
static uint32_t gnss_rx_get_producer_position(void)
{
    uint32_t wrap_before;
    uint32_t wrap_after;
    uint16_t ndtr;
    uint16_t position;

    do
    {
        /*
         * Считаем количество завершённых полных оборотов DMA.
         */
        wrap_before = gnss_rx_wrap_count;

        /*
         * NDTR содержит количество элементов, которые DMA
         * должен передать до окончания текущего оборота.
         */
        ndtr = __HAL_DMA_GET_COUNTER(huart1.hdmarx);

        /*
         * Считываем счётчик ещё раз.
         */
        wrap_after = gnss_rx_wrap_count;

        /*
         * Если DMA завершил оборот между двумя чтениями
         * wrap_count, полученная пара wrap/NDTR несогласована.
         * Повторяем чтение.
         */
    } while (wrap_before != wrap_after);

    /*
     * Защита от некорректного значения NDTR.
     */
    if (ndtr > GNSS_RX_BUFFER_SIZE)
    {
        ndtr = GNSS_RX_BUFFER_SIZE;
    }

    position = GNSS_RX_BUFFER_SIZE - ndtr;

    return wrap_before * GNSS_RX_BUFFER_SIZE + position;
}

/**
 * @brief Инициализировать приём GNSS через USART1.
 */
void purrgo_gnss_init(void)
{
    /*
     * Сначала сбрасываем состояние software producer/consumer.
     */
    gnss_rx_tail = 0U;
    gnss_rx_wrap_count = 0U;
    gnss_rx_error_flag = false;

    /*
     * AT6558R: установить период выдачи навигационных данных 1000 мс.
     */
    static const uint8_t gnss_update_rate_cmd[] =
        "$PCAS02,1000*2E\r\n";

    /*
     * Для выдачи только GGA + GSA + RMC.
     */
    static const uint8_t gnss_nmea_cmd[] =
        "$PCAS03,1,0,1,0,1,0,0,0,0,0,,,0*3A\r\n";

    /*
     * 1. Запускаем непрерывный приём через Circular DMA ДО отправки конфигурации,
     * чтобы не пропустить возможные ответы модуля или асинхронные пакеты,
     * и избежать аппаратной ошибки переполнения буфера ORE (Overrun Error).
     */
    if (HAL_UART_Receive_DMA(
            &huart1,
            gnss_rx_buffer,
            GNSS_RX_BUFFER_SIZE) != HAL_OK)
    {
        PURRGO_LOG("GNSS INIT ERROR!\r\n");
    }

    /*
     * 2. Теперь безопасно отправляем конфигурацию. Любые ответы от модуля 
     * мгновенно и без задержек будут перехвачены работающим контроллером DMA.
     */
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
}

/**
 * @brief Прочитать один байт из входного GNSS-потока.
 */
bool purrgo_gnss_read_byte(uint8_t *byte)
{
    uint32_t producer;
    uint32_t available;
    uint32_t buffer_index;

    if (byte == NULL)
    {
        return false;
    }

    /* Если DMA ещё не был запущен, читать нечего. */
    if (huart1.hdmarx == NULL)
    {
        return false;
    }

    /*
     * Если произошла аппаратная ошибка UART (ORE, FE и т.д.),
     * прерывание останавливает приём и устанавливает этот флаг.
     *
     * Мы выполняем сброс логики и перезапуск DMA синхронно,
     * в контексте основного цикла (consumer). Это гарантирует,
     * что переменные не будут обнулены "под ногами" в процессе
     * расчёта доступных данных.
     */
    if (gnss_rx_error_flag)
    {
        gnss_rx_error_flag = false;
        gnss_rx_tail = 0U;
        gnss_rx_wrap_count = 0U;

        if (HAL_UART_Receive_DMA(
                &huart1,
                gnss_rx_buffer,
                GNSS_RX_BUFFER_SIZE) != HAL_OK)
        {
            PURRGO_LOG("GNSS RX RESTART ERROR!\r\n");
        }
        return false;
    }

    /*
     * Получаем согласованную абсолютную позицию DMA producer.
     */
    producer = gnss_rx_get_producer_position();

    /*
     * Благодаря unsigned arithmetic разность корректно работает
     * и при естественном переполнении uint32_t.
     */
    available = producer - gnss_rx_tail;

    /*
     * DMA успел записать больше данных, чем помещается в буфере.
     * Сохраняем только последние BUFFER_SIZE байт.
     */
    if (available > GNSS_RX_BUFFER_SIZE)
    {
        gnss_rx_tail = producer - GNSS_RX_BUFFER_SIZE;
        available = GNSS_RX_BUFFER_SIZE;
    }

    /*
     * Новых данных нет.
     */
    if (available == 0U)
    {
        return false;
    }

    /*
     * Преобразуем абсолютную позицию consumer в индекс буфера.
     */
    buffer_index = gnss_rx_tail % GNSS_RX_BUFFER_SIZE;

    /* Читаем байт. */
    *byte = gnss_rx_buffer[buffer_index];

    /* Переходим к следующему байту. */
    gnss_rx_tail++;

    return true;
}

/**
 * @brief Callback половины DMA-передачи USART1 RX.
 */
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }
}

/**
 * @brief Callback полного DMA-буфера USART1 RX.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }

    /*
     * DMA завершил очередной полный оборот буфера.
     */
    gnss_rx_wrap_count++;
}

/**
 * @brief Callback ошибки UART.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == NULL)
    {
        return;
    }

    if (huart->Instance != USART1)
    {
        return;
    }

    /*
     * Останавливаем текущий ошибочный приём.
     * HAL_UART_AbortReceive() также останавливает связанный DMA RX.
     */
    (void)HAL_UART_AbortReceive(huart);

    /*
     * Сигнализируем основному циклу о необходимости сброса.
     * Перезапуск будет выполнен синхронно без гонки данных 
     * при следующем вызове purrgo_gnss_read_byte().
     */
    gnss_rx_error_flag = true;
}