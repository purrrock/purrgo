#include "purrgo/gnss_io.h"
#include "usart.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Кольцевой буфер входного потока USART1.
 *
 * 1024 байта достаточно для временного накопления NMEA-потока,
 * пока основной цикл занят другими задачами.
 */
#define GNSS_RX_BUFFER_SIZE 1024U

static uint8_t gnss_rx_buffer[GNSS_RX_BUFFER_SIZE];

/*
 * head изменяется только обработчиком UART.
 * tail изменяется только из основного контекста.
 */
static volatile uint16_t gnss_rx_head = 0U;
static volatile uint16_t gnss_rx_tail = 0U;

/*
 * Буфер одного байта для HAL_UART_Receive_IT().
 *
 * После получения байта HAL вызывает HAL_UART_RxCpltCallback(),
 * где байт помещается в кольцевой буфер и приём немедленно
 * запускается снова.
 */
static uint8_t gnss_rx_byte;

/**
 * @brief Инициализировать приём GNSS через USART1.
 *
 * Приём выполняется в interrupt mode. Функция должна быть вызвана
 * после MX_USART1_UART_Init().
 */
void purrgo_gnss_init(void)
{
    gnss_rx_head = 0U;
    gnss_rx_tail = 0U;

    /*
     * Запускаем приём одного байта.
     * После получения HAL вызовет HAL_UART_RxCpltCallback().
     */
    if (HAL_UART_Receive_IT(&huart1, &gnss_rx_byte, 1U) != HAL_OK)
    {
        /*
         * Ошибка запуска UART-приёма является аппаратной ошибкой
         * конфигурации USART1.
         */
        Error_Handler();
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
 * @brief Callback завершения приёма одного байта UART.
 *
 * Этот callback вызывается из HAL после получения очередного байта.
 * Здесь нельзя выполнять разбор NMEA или другую длительную работу.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }

    /*
     * Следующая позиция записи.
     */
    uint16_t next_head = gnss_rx_head + 1U;

    if (next_head >= GNSS_RX_BUFFER_SIZE)
    {
        next_head = 0U;
    }

    /*
     * Если next_head совпал с tail, буфер заполнен.
     *
     * В этом случае новый байт отбрасываем.
     * Старые данные сохраняем, чтобы не разрушать ещё не обработанную
     * NMEA-строку.
     */
    if (next_head != gnss_rx_tail)
    {
        gnss_rx_buffer[gnss_rx_head] = gnss_rx_byte;
        gnss_rx_head = next_head;
    }

    /*
     * Немедленно снова включаем приём следующего байта.
     */
    (void)HAL_UART_Receive_IT(&huart1, &gnss_rx_byte, 1U);
}

/**
 * @brief Callback ошибки UART.
 *
 * После ошибки снова запускаем приём.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }

    (void)HAL_UART_Receive_IT(&huart1, &gnss_rx_byte, 1U);
}



/**
 * @brief Прочитать один байт из STM32 GNSS-потока.
 *
 * Эта отладочная функция возвращает байты из тестового NMEA MOCK
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
// bool purrgo_gnss_read_byte(uint8_t *byte)
// {
//    if (byte == NULL)
//    {
//        return false;
//    }
//
//    /*
//     * Читаем байт из потока Mock.
//     * Реальный транспорт (UART/DMA) будет использовать похожую логику,
//     * но читать из кольцевого буфера.
//     */
//    return purrgo_gnss_mock_read_byte(byte);
// }
