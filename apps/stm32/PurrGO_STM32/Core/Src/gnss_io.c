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
 *
 * Увеличивается в HAL_UART_RxCpltCallback() после каждого
 * transfer complete.
 *
 * Например:
 *
 *   оборот 0: байты 0..1023
 *   оборот 1: байты 1024..2047
 *   оборот 2: байты 2048..3071
 *
 * Абсолютная позиция DMA вычисляется как:
 *
 *   wrap_count * GNSS_RX_BUFFER_SIZE + position_inside_buffer
 *
 * Переменная volatile, поскольку изменяется из обработчика
 * прерывания DMA/UART и читается из основного контекста.
 */
static volatile uint32_t gnss_rx_wrap_count;

/**
 * @brief Получить согласованное состояние DMA producer.
 *
 * DMA работает независимо от основного кода и одновременно
 * с чтением NDTR может произойти Transfer Complete interrupt.
 *
 * Поэтому сначала читается счётчик оборотов, затем NDTR,
 * затем счётчик оборотов читается повторно.
 *
 * Если за время чтения произошёл полный оборот DMA, значения
 * не согласованы и чтение повторяется.
 *
 * @return Абсолютная позиция следующего байта, который будет
 *         записан DMA.
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
         *
         * Поэтому текущая позиция записи:
         *
         *     BUFFER_SIZE - NDTR
         *
         * При нормальной работе она находится в диапазоне
         * 0..BUFFER_SIZE.
         */
        ndtr = __HAL_DMA_GET_COUNTER(huart1.hdmarx);

        /*
         * Считываем счётчик ещё раз.
         */
        wrap_after = gnss_rx_wrap_count;

        /*
         * Если DMA завершил оборот между двумя чтениями
         * wrap_count, полученная пара wrap/NDTR несогласована.
         *
         * Повторяем чтение.
         */
    } while (wrap_before != wrap_after);

    /*
     * Защита от некорректного значения NDTR.
     *
     * В штатном режиме Circular DMA NDTR не должен превышать
     * размер переданного буфера.
     */
    if (ndtr > GNSS_RX_BUFFER_SIZE)
    {
        ndtr = GNSS_RX_BUFFER_SIZE;
    }

    position = GNSS_RX_BUFFER_SIZE - ndtr;

    /*
     * Возвращаем абсолютную позицию producer.
     *
     * Если position == BUFFER_SIZE, это означает, что DMA
     * находится непосредственно на границе полного оборота.
     * Такая позиция корректно преобразуется в начало следующего
     * логического диапазона.
     */
    return wrap_before * GNSS_RX_BUFFER_SIZE + position;
}

/**
 * @brief Инициализировать приём GNSS через USART1.
 *
 * Приём выполняется через Circular DMA.
 * Функция должна быть вызвана после MX_USART1_UART_Init()
 * и инициализации DMA.
 */
void purrgo_gnss_init(void)
{
    /*
     * Сначала сбрасываем состояние software producer/consumer.
     */
    gnss_rx_tail = 0U;
    gnss_rx_wrap_count = 0U;

    /*
     * AT6558R:
     * установить период выдачи навигационных данных 1000 мс.
     *
     * Формат команды:
     *
     *     $PCAS02,1000*2E\r\n
     */
    static const uint8_t gnss_update_rate_cmd[] =
        "$PCAS02,1000*2E\r\n";

    /*
     * Для выдачи только GGA + GSA + RMC.
     */
    static const uint8_t gnss_nmea_cmd[] =
        "$PCAS03,1,0,1,0,1,0,0,0,0,0,,,0*3A\r\n";

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
     *
     * В режиме Circular DMA после достижения конца буфера
     * DMA автоматически продолжает запись с его начала.
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
 * Функция не блокируется.
 *
 * Основной код является consumer, DMA является producer.
 *
 * Producer и consumer используют абсолютные позиции:
 *
 *     producer = текущая позиция DMA
 *     consumer = gnss_rx_tail
 *
 * Количество доступных байтов:
 *
 *     available = producer - consumer
 *
 * Если available > GNSS_RX_BUFFER_SIZE, DMA уже сделал
 * как минимум один полный оборот и перезаписал непрочитанные
 * данные.
 *
 * В таком случае старые данные считаются потерянными, а consumer
 * перемещается на самое старое ещё доступное место:
 *
 *     consumer = producer - BUFFER_SIZE
 *
 * Таким образом, состояние "пусто" больше не смешивается
 * с состоянием "буфер полностью заполнен".
 *
 * @param[out] byte Адрес переменной для принятого байта.
 *
 * @return true  — байт получен.
 * @return false — новых данных нет или byte == NULL.
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

    /*
     * Если DMA ещё не был запущен, читать нечего.
     */
    if (huart1.hdmarx == NULL)
    {
        return false;
    }

    /*
     * Получаем согласованную абсолютную позицию DMA producer.
     */
    producer = gnss_rx_get_producer_position();

    /*
     * Благодаря unsigned arithmetic разность корректно работает
     * и при естественном переполнении uint32_t.
     *
     * При нормальной работе producer >= tail в логическом смысле.
     */
    available = producer - gnss_rx_tail;

    /*
     * DMA успел записать больше данных, чем помещается
     * в кольцевом буфере.
     *
     * Значит, часть непрочитанных байтов уже была перезаписана.
     *
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
     * Преобразуем абсолютную позицию consumer в индекс
     * внутри физического кольцевого буфера.
     *
     * Размер буфера 1024, но намеренно не используем битовую
     * маску: реализация не зависит от степени двойки.
     */
    buffer_index = gnss_rx_tail % GNSS_RX_BUFFER_SIZE;

    /*
     * Читаем байт.
     *
     * DMA может одновременно записывать в другой элемент
     * буфера. Текущий элемент уже находится перед consumer,
     * поэтому DMA его не должен перезаписывать до следующего
     * полного оборота.
     */
    *byte = gnss_rx_buffer[buffer_index];

    /*
     * Переходим к следующему байту.
     */
    gnss_rx_tail++;

    return true;
}

/**
 * @brief Callback половины DMA-передачи USART1 RX.
 *
 * Circular DMA вызывает этот callback после заполнения первой
 * половины буфера.
 *
 * Счётчик полных оборотов здесь НЕ увеличивается: полный оборот
 * ещё не завершён.
 *
 * Callback намеренно пустой. Его наличие явно показывает, что
 * Half Transfer event является штатным событием DMA и не должен
 * интерпретироваться как полный оборот.
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
 *
 * В режиме Circular DMA этот callback вызывается после каждого
 * полного заполнения буфера.
 *
 * После callback DMA начинает следующий оборот с начала буфера,
 * поэтому увеличиваем абсолютный счётчик оборотов.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }

    /*
     * DMA завершил очередной полный оборот буфера.
     *
     * После этого producer продолжает работу с начала
     * физического массива, но логическая абсолютная позиция
     * продолжает увеличиваться.
     */
    gnss_rx_wrap_count++;
}

/**
 * @brief Callback ошибки UART.
 *
 * После ошибки (например, ORE или FE) останавливаем текущий
 * приём и запускаем Circular DMA заново.
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
     *
     * HAL_UART_AbortReceive() также останавливает связанный
     * DMA RX.
     */
    (void)HAL_UART_AbortReceive(huart);

    /*
     * После ошибки старое содержимое буфера больше не считается
     * валидным потоком.
     *
     * Начинаем новый логический поток с нулевой позиции.
     */
    gnss_rx_tail = 0U;
    gnss_rx_wrap_count = 0U;

    /*
     * Снова запускаем непрерывный Circular DMA.
     */
    if (HAL_UART_Receive_DMA(
            &huart1,
            gnss_rx_buffer,
            GNSS_RX_BUFFER_SIZE) != HAL_OK)
    {
        PURRGO_LOG("GNSS RX RESTART ERROR!\r\n");
    }
}