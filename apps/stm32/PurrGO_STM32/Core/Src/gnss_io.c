#include "purrgo/gnss_io.h"
#include "purrgo/gnss_ring.h"
#include "purrgo/gnss_receiver.h"
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

#if (GNSS_RX_BUFFER_SIZE & (GNSS_RX_BUFFER_SIZE - 1U)) != 0U
#error "GNSS_RX_BUFFER_SIZE must be a power of two"
#endif

/*
 * Буфер приёма GNSS.
 *
 * DMA является владельцем записи в этот массив.
 * Потребитель читает данные через purrgo_gnss_read_byte().
 */
static uint8_t gnss_rx_buffer[GNSS_RX_BUFFER_SIZE];

/*
 * Потребитель кольцевого буфера (индекс tail и расчёт положения DMA по NDTR).
 * Логика вынесена в purrgo/gnss_ring.h и проверяется host-тестами
 * (tests/host/test_gnss.c).
 *
 * Условие корректности: потребитель обязан вызывать
 * purrgo_gnss_read_byte() чаще, чем DMA заполняет весь буфер
 * (при 9600 бод и 1024 байтах это ~1.07 с).
 */
static purrgo_gnss_ring_t gnss_ring;

/*
 * Флаг ошибки UART.
 *
 * Устанавливается в обработчике прерывания HAL_UART_ErrorCallback.
 * Обрабатывается синхронно в purrgo_gnss_read_byte() для
 * безопасного перезапуска без гонки данных.
 */
static volatile bool gnss_rx_error_flag = false;

/**
 * @brief Отправка байт приёмнику GNSS (транспорт для gnss_receiver).
 */
static void gnss_uart_tx(const uint8_t *data, size_t len)
{
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)(uintptr_t)data, (uint16_t)len, 1000U);
}

/**
 * @brief Инициализировать приём GNSS через USART1.
 */
void purrgo_gnss_init(void)
{
    /*
     * Сначала сбрасываем состояние software producer/consumer.
     */
    (void)purrgo_gnss_ring_init(&gnss_ring, gnss_rx_buffer, GNSS_RX_BUFFER_SIZE);
    gnss_rx_error_flag = false;

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
    /*
     * Команды настройки приёмника зависят от его модели и вынесены в ядро
     * (gnss_receiver.c, выбор - PURRGO_HW_GNSS_RECEIVER в hardware_config.h).
     */
    purrgo_gnss_receiver_configure(gnss_uart_tx);
}

/**
 * @brief Прочитать один байт из входного GNSS-потока.
 */
bool purrgo_gnss_read_byte(uint8_t *byte)
{
    uint16_t head;

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
        purrgo_gnss_ring_reset(&gnss_ring);

        if (HAL_UART_Receive_DMA(
                &huart1,
                gnss_rx_buffer,
                GNSS_RX_BUFFER_SIZE) != HAL_OK)
        {
            PURRGO_LOG("GNSS RX RESTART ERROR!\r\n");
        }
        return false;
    }

    head = purrgo_gnss_ring_head_from_ndtr(&gnss_ring,
                                           __HAL_DMA_GET_COUNTER(huart1.hdmarx));

    /* false - новых данных нет. */
    return purrgo_gnss_ring_pop(&gnss_ring, head, byte);
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
     * Положение producer определяется по NDTR, поэтому счётчик оборотов
     * больше не нужен.
     */
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