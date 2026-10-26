#include "purrgo/gnss_io.h"
#include "purrgo/gnss_ring.h"
#include "purrgo/gnss_receiver.h"
#include "usart.h"
#include "gpdma.h"
#include "main.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "purrgo/logger.h"

/*
 * GNSS на STM32U585: USART1_RX -> GPDMA1 Channel 0, linked-list в круговом
 * режиме (один узел, ссылающийся на себя). Отличия от F411 (DMA2 Stream2 в
 * circular-режиме):
 *   - очередь узлов собирается здесь, а не берётся из linked_list.c CubeMX:
 *     CubeMX создаёт узел GPDMA_2D, а канал 0 линейный (HAL_DMAEx_List_LinkQ
 *     вернул бы HAL_ERROR), и канал инициализируется им в режиме NORMAL;
 *   - HAL_UART_Receive_DMA() сам записывает в узел размер блока, адрес RDR и
 *     адрес буфера, поэтому узел собирается с нулевыми адресами;
 *   - остаток блока читается из CBR1.BNDT (__HAL_DMA_GET_COUNTER), его
 *     поведение для кольцевого буфера такое же, как у NDTR на F411.
 */

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
 * Потребитель кольцевого буфера (индекс tail и расчёт положения DMA по счётчику).
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
 * Устанавливается в обработчике прерывания HAL_UART_ErrorCallback ПОСЛЕ
 * остановки приёма (DMA уже остановлен, его NDTR не меняется).
 * Обрабатывается синхронно в purrgo_gnss_read_byte(): сначала потребитель
 * вычитывает всё, что DMA успел записать до ошибки, и только потом DMA
 * перезапускается без гонки данных.
 */
static volatile bool gnss_rx_error_flag = false;

/* Узел и очередь linked-list: узел обязан быть выровнен по 4 байтам. */
static DMA_NodeTypeDef gnss_rx_node __attribute__((aligned(32)));
static DMA_QListTypeDef gnss_rx_queue;

/**
 * @brief Собрать круговую очередь из одного линейного узла и привязать её
 *        к каналу GPDMA1 и к USART1 (приёмник).
 *
 * @return true при успехе.
 */
static bool gnss_dma_setup(void)
{
    DMA_NodeConfTypeDef node_cfg;
    HAL_StatusTypeDef ret = HAL_OK;

    memset(&node_cfg, 0, sizeof node_cfg);
    memset(&gnss_rx_node, 0, sizeof gnss_rx_node);
    memset(&gnss_rx_queue, 0, sizeof gnss_rx_queue);

    node_cfg.NodeType = DMA_GPDMA_LINEAR_NODE;
    node_cfg.Init.Request = GPDMA1_REQUEST_USART1_RX;
    node_cfg.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
    node_cfg.Init.Direction = DMA_PERIPH_TO_MEMORY;
    node_cfg.Init.SrcInc = DMA_SINC_FIXED;
    node_cfg.Init.DestInc = DMA_DINC_INCREMENTED;
    node_cfg.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
    node_cfg.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
    node_cfg.Init.SrcBurstLength = 1U;
    node_cfg.Init.DestBurstLength = 1U;
    node_cfg.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
    node_cfg.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    node_cfg.Init.Mode = DMA_NORMAL;
    node_cfg.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
    node_cfg.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
    node_cfg.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;

    ret |= HAL_DMAEx_List_BuildNode(&node_cfg, &gnss_rx_node);
    ret |= HAL_DMAEx_List_InsertNode_Tail(&gnss_rx_queue, &gnss_rx_node);
    ret |= HAL_DMAEx_List_SetCircularMode(&gnss_rx_queue);

    /*
     * MX_GPDMA1_Init() инициализировал канал в режиме LINKEDLIST_NORMAL, а
     * очередь с замыканием на себя допустима только в LINKEDLIST_CIRCULAR
     * (HAL_DMAEx_List_LinkQ). Канал инициализируется заново.
     */
    handle_GPDMA1_Channel0.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
    ret |= HAL_DMAEx_List_Init(&handle_GPDMA1_Channel0);
    ret |= HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel0, DMA_CHANNEL_NPRIV);
    ret |= HAL_DMAEx_List_LinkQ(&handle_GPDMA1_Channel0, &gnss_rx_queue);

    __HAL_LINKDMA(&huart1, hdmarx, handle_GPDMA1_Channel0);

    return (ret == HAL_OK);
}

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

    if (!gnss_dma_setup())
    {
        PURRGO_LOG("GNSS DMA SETUP ERROR!\r\n");
        return;
    }

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
     * Положение producer по NDTR. При остановленном DMA (ошибка UART) NDTR
     * заморожен, поэтому остаток данных определяется так же, как в штатной работе.
     */
    head = purrgo_gnss_ring_head_from_ndtr(&gnss_ring,
                                           __HAL_DMA_GET_COUNTER(huart1.hdmarx));

    /*
     * Если произошла аппаратная ошибка UART (ORE, FE и т.д.),
     * прерывание останавливает приём и устанавливает флаг.
     *
     * Непрочитанные байты, которые DMA успел записать до ошибки, не выбрасываем:
     * пока они есть, отдаём их потребителю (флаг остаётся установленным).
     * Когда остаток вычитан, сбрасываем логику и перезапускаем DMA синхронно,
     * в контексте основного цикла (consumer): так переменные не обнуляются
     * "под ногами" в процессе расчёта доступных данных.
     */
    if (gnss_rx_error_flag)
    {
        if (purrgo_gnss_ring_pop(&gnss_ring, head, byte))
        {
            return true;
        }

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
     * HAL_UART_AbortReceive() также останавливает связанный DMA RX;
     * его счётчик NDTR остаётся на достигнутом значении, по нему основной цикл
     * определит, сколько байт ещё нужно вычитать.
     */
    (void)HAL_UART_AbortReceive(huart);

    /*
     * Сигнализируем основному циклу: вычитать остаток, затем перезапустить DMA.
     * Флаг ставится только после остановки DMA.
     */
    gnss_rx_error_flag = true;
}