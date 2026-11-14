#include "purrgo/gnss_io.h"
#include "purrgo/gnss_ring.h"
#include "purrgo/gnss_receiver.h"
#include "usart.h"
#include "lpdma.h"
#include "main.h"
#include "purrgo/hardware_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "purrgo/logger.h"
#include "gnss_monitor.h"
#include "gnss_link.h"

/*
 * GNSS на STM32U585: LPUART1_RX -> LPDMA1 Channel 0, linked-list в круговом
 * режиме (один узел, ссылающийся на себя):
 *   - очередь узлов собирается здесь, а не берётся из linked_list.c CubeMX:
 *     MX_LPDMA1_Init() инициализирует канал в режиме LINKEDLIST_NORMAL, а
 *     замкнутая очередь допустима только в LINKEDLIST_CIRCULAR
 *     (HAL_DMAEx_List_LinkQ), поэтому канал инициализируется заново;
 *   - HAL_UART_Receive_DMA() сам записывает в узел размер блока, адрес RDR и
 *     адрес буфера, поэтому узел собирается с нулевыми адресами;
 *   - LPUART1 обслуживается LPDMA1 (запрос LPDMA1_REQUEST_LPUART1_RX); режим
 *     Stop 2 пока не используется;
 *   - LPDMA1 (домен SmartRun) является мастером только шины SRD и видит из всех
 *     SRAM лишь SRAM4 (RM0456, 2.1.13) - и в Run, и в Stop. Поэтому буфер приёма
 *     и узел linked-list размещены в секции .sram4 (STM32U585xx_PURRGO.ld, SRAM4);
 *   - остаток блока читается из CBR1.BNDT (__HAL_DMA_GET_COUNTER); его
 *     семантика для кольцевого буфера соответствует
 *     purrgo_gnss_ring_head_from_ndtr() из gnss_ring.h;
 *   - скорость UART определяет и меняет gnss_link (stm32/port/gnss):
 *     после включения LPUART1 работает на заводской скорости модуля
 *     (PURRGO_GNSS_BAUD_DEFAULT, CubeMX: 38400), затем модулю отправляется команда
 *     смены скорости и LPUART1 переключается на PURRGO_GNSS_BAUD_TARGET.
 *     Настройка NMEA отправляется только после установления связи.
 */

/*
 * Размер кольцевого DMA-буфера GNSS.
 *
 * DMA непрерывно записывает входные байты в этот массив.
 * После достижения конца массива DMA начинает запись с начала.
 *
 * G10A F30 стартует на 38400 бод, рабочая скорость - PURRGO_GNSS_BAUD_TARGET
 * (115200: до 11520 байт/с в пределе); после настройки (GGA, RMC, GSA, 1 Гц)
 * реальный поток - несколько сотен байт/с, от скорости линии он не зависит.
 */
#define GNSS_RX_BUFFER_SIZE 2048U

#if (GNSS_RX_BUFFER_SIZE & (GNSS_RX_BUFFER_SIZE - 1U)) != 0U
#error "GNSS_RX_BUFFER_SIZE must be a power of two"
#endif

/*
 * Буфер приёма GNSS.
 *
 * DMA является владельцем записи в этот массив.
 * Потребитель читает данные через purrgo_gnss_read_byte().
 */
static uint8_t gnss_rx_buffer[GNSS_RX_BUFFER_SIZE] __attribute__((section(".sram4"), aligned(4)));

/*
 * Потребитель кольцевого буфера (индекс tail и расчёт положения DMA по счётчику).
 * Логика вынесена в purrgo/gnss_ring.h и проверяется host-тестами
 * (tests/host/test_gnss.c).
 *
 * Условие корректности: потребитель обязан вызывать
 * purrgo_gnss_read_byte() чаще, чем DMA заполняет весь буфер
 * (после настройки приёмника - несколько секунд; при полной загрузке линии
 * 38400 бод - ~0.53 с, 115200 бод - ~0.18 с на 2048 байт; фактический поток
 * при 1 Гц от скорости линии не зависит, она меняет только длительность пачки).
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

/* Код ошибки UART, с которым вызван HAL_UART_ErrorCallback (для лога). */
static volatile uint32_t gnss_rx_error_code = 0U;

#if (PURRGO_GNSS_CONFIG_RETRY_MS > 0)
/* Повторная отправка настройки NMEA через PURRGO_GNSS_CONFIG_RETRY_MS после установления связи. */
static uint32_t gnss_t_cfg = 0U;
static bool gnss_cfg_retry_pending = false;
#endif

/* Определение скорости UART и её смена (см. gnss_link.h). */
static gnss_link_t gnss_link;

/*
 * Последнее достоверное положение producer (индекс следующего байта,
 * который запишет DMA). HAL при завершении abort обнуляет CBR1 (чтобы
 * следующий старт загрузил узел из памяти), поэтому после ошибки счётчик
 * использовать нельзя и положение берётся из этой переменной: она
 * обновляется при каждом чтении счётчика в штатной работе. Байты, пришедшие
 * между последним чтением и ошибкой, теряются; разбор NMEA после этого
 * пересинхронизируется сам.
 */
static volatile uint16_t gnss_last_head = 0U;

/*
 * GNSS включён (EN в High, LPUART1 и DMA работают). После сброса МК EN High
 * (MX_GPIO_Init), поэтому по умолчанию true. Пока false, purrgo_gnss_read_byte()
 * ничего не отдаёт.
 */
static volatile bool gnss_powered = true;

/*
 * Узел и очередь linked-list: узел обязан быть выровнен по 4 байтам и лежать
 * в SRAM4 (доступна LPDMA1). Очередь (DMA_QListTypeDef) читает только CPU.
 */
static DMA_NodeTypeDef gnss_rx_node __attribute__((section(".sram4"), aligned(32)));
static DMA_QListTypeDef gnss_rx_queue;

/**
 * @brief Собрать круговую очередь из одного линейного узла и привязать её
 *        к каналу LPDMA1 и к LPUART1 (приёмник).
 *
 * @return true при успехе.
 */
static bool gnss_dma_setup(void)
{
    DMA_NodeConfTypeDef node_cfg;
    HAL_StatusTypeDef ret = HAL_OK;

    /* SRAM4 должна быть тактирована: из неё читает и в неё пишет LPDMA1. */
    __HAL_RCC_SRAM4_CLK_ENABLE();

    memset(&node_cfg, 0, sizeof node_cfg);
    memset(&gnss_rx_node, 0, sizeof gnss_rx_node);
    memset(&gnss_rx_queue, 0, sizeof gnss_rx_queue);

    node_cfg.NodeType = DMA_LPDMA_LINEAR_NODE;
    node_cfg.Init.Request = LPDMA1_REQUEST_LPUART1_RX;
    node_cfg.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
    node_cfg.Init.Direction = DMA_PERIPH_TO_MEMORY;
    node_cfg.Init.SrcInc = DMA_SINC_FIXED;
    node_cfg.Init.DestInc = DMA_DINC_INCREMENTED;
    node_cfg.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
    node_cfg.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
    node_cfg.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    node_cfg.Init.Mode = DMA_NORMAL;
    node_cfg.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
    node_cfg.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
    node_cfg.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;

    ret |= HAL_DMAEx_List_BuildNode(&node_cfg, &gnss_rx_node);
    ret |= HAL_DMAEx_List_InsertNode_Tail(&gnss_rx_queue, &gnss_rx_node);
    ret |= HAL_DMAEx_List_SetCircularMode(&gnss_rx_queue);

    /*
     * MX_LPDMA1_Init() инициализировал канал в режиме LINKEDLIST_NORMAL, а
     * очередь с замыканием на себя допустима только в LINKEDLIST_CIRCULAR
     * (HAL_DMAEx_List_LinkQ). Канал инициализируется заново.
     */
    handle_LPDMA1_Channel0.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
    ret |= HAL_DMAEx_List_Init(&handle_LPDMA1_Channel0);
    ret |= HAL_DMA_ConfigChannelAttributes(&handle_LPDMA1_Channel0, DMA_CHANNEL_NPRIV);
    ret |= HAL_DMAEx_List_LinkQ(&handle_LPDMA1_Channel0, &gnss_rx_queue);

    __HAL_LINKDMA(&hlpuart1, hdmarx, handle_LPDMA1_Channel0);

    return (ret == HAL_OK);
}

/**
 * @brief Отправка байт приёмнику GNSS; возвращается после полной передачи
 *        (HAL_UART_Transmit ждёт флаг TC).
 *
 * @return true при успехе.
 */
static bool gnss_tx_raw(const uint8_t *data, size_t len)
{
    const HAL_StatusTypeDef st =
        HAL_UART_Transmit(&hlpuart1, (uint8_t *)(uintptr_t)data, (uint16_t)len, 1000U);

    if (st != HAL_OK)
    {
        PURRGO_LOGE("GNSS: TX FAILED (HAL status %d, uart err 0x%lX)\r\n",
                   (int)st, (unsigned long)hlpuart1.ErrorCode);
    }
    return (st == HAL_OK);
}

/**
 * @brief Отправка настройки приёмнику GNSS (транспорт для gnss_receiver).
 */
static void gnss_uart_tx(const uint8_t *data, size_t len)
{
    if (gnss_tx_raw(data, len))
    {
        PURRGO_LOG("GNSS: config sent to module (%u bytes)\r\n", (unsigned)len);
    }
}

static void gnss_link_begin(void);

/**
 * @brief Инициализировать приём GNSS через LPUART1.
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
        PURRGO_LOGE("GNSS DMA SETUP ERROR!\r\n");
        return;
    }

    /*
     * 1. Запускаем непрерывный приём через Circular DMA ДО отправки конфигурации,
     * чтобы не пропустить возможные ответы модуля или асинхронные пакеты,
     * и избежать аппаратной ошибки переполнения буфера ORE (Overrun Error).
     */
    if (HAL_UART_Receive_DMA(
            &hlpuart1,
            gnss_rx_buffer,
            GNSS_RX_BUFFER_SIZE) != HAL_OK)
    {
        PURRGO_LOGE("GNSS INIT ERROR!\r\n");
    }

    /*
     * 2. Установление связи: определение скорости UART, смена скорости модуля и
     * только потом настройка NMEA (gnss_link; настройку отправляет link_on_state
     * после приёма корректного NMEA). Любые ответы модуля мгновенно и без задержек
     * перехватывает работающий контроллер DMA.
     * Команды настройки приёмника зависят от его модели и вынесены в ядро
     * (gnss_receiver.c, выбор - PURRGO_HW_GNSS_RECEIVER в hardware_config.h).
     */
    gnss_link_begin();
}

/**
 * @brief Выключить GNSS по EN.
 *
 * Порядок: остановить приём (DMA) -> EN Low -> выключить LPUART1 (HAL сводит
 * PA2/PA3 в Analog без подтяжки) -> PA2 (TX в сторону модуля) в Low. Иначе
 * модуль мог бы подпитываться через защитные диоды линии UART.
 */
void purrgo_gnss_power_off(void)
{
    GPIO_InitTypeDef gpio = {0};

    if (!gnss_powered)
    {
        return;
    }
    gnss_powered = false;
    gnss_link_stop(&gnss_link);
#if (PURRGO_GNSS_CONFIG_RETRY_MS > 0)
    gnss_cfg_retry_pending = false;
#endif
    purrgo_gnss_monitor_stop();   /* отчёты и счёт 1PPS - только при включённом GNSS */

    /* 1. Остановить приём; HAL_UART_AbortReceive() останавливает и DMA. */
    (void)HAL_UART_AbortReceive(&hlpuart1);
    gnss_rx_error_flag = false;

    /* 2. EN в Low: модуль выключен (PB0 - Push-Pull). */
    HAL_GPIO_WritePin(GNSS_EN_GPIO_Port, GNSS_EN_Pin, GPIO_PIN_RESET);

    /* 3. LPUART1 выключен; PA2/PA3 -> Analog, без подтяжек. */
    (void)HAL_UART_DeInit(&hlpuart1);

    /* 4. PA2 (TX в сторону модуля) в Low. PA3 (RX) остаётся Analog без подтяжки. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_2;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);

    PURRGO_LOG("GNSS: power off (EN low)\r\n");
}

/**
 * @brief Включить GNSS и настроить его заново.
 *
 * Порядок: EN в High (Push-Pull) -> LPUART1 заново
 * инициализирован (PA2/PA3 снова AF) -> purrgo_gnss_init() (DMA-приём и
 * настройка приёмника). Модуль после включения загружается не сразу, поэтому
 * первая настройка может потеряться; её повторяет PURRGO_GNSS_CONFIG_RETRY_MS.
 */
void purrgo_gnss_power_on(void)
{
    if (gnss_powered)
    {
        return;
    }

    /* 1. EN в High (Push-Pull): модуль включён. */
    HAL_GPIO_WritePin(GNSS_EN_GPIO_Port, GNSS_EN_Pin, GPIO_PIN_SET);

    /* 2. LPUART1 заново: MspInit возвращает PA2/PA3 в AF8 (PA3 с подтяжкой). */
    MX_LPUART1_UART_Init();

    /* 3. DMA-приём, затем связь с модулем (скорость, набор NMEA). Модуль после EN Low -> High
     *    снова на заводской скорости (настройка слоя RAM потеряна). */
    gnss_powered = true;
    gnss_last_head = 0U;
    purrgo_gnss_init();
    purrgo_gnss_monitor_init();

    PURRGO_LOG("GNSS: power on (EN high)\r\n");
}

/**
 * @brief Принудительно вернуть канал LPDMA в состояние READY.
 *
 * Нужен, если DMA застрял в ABORT/SUSPEND/ERROR (например, вторая ошибка UART
 * пришла до завершения suspend, и HAL_DMA_Abort_IT вернул ошибку). Повторяет то,
 * что HAL делает при штатном завершении abort.
 */
static void gnss_dma_force_ready(void)
{
    DMA_HandleTypeDef *const h = &handle_LPDMA1_Channel0;

    h->Instance->CCR |= DMA_CCR_RESET;
    __HAL_DMA_CLEAR_FLAG(h, (DMA_FLAG_TC | DMA_FLAG_HT | DMA_FLAG_DTE | DMA_FLAG_ULE |
                             DMA_FLAG_USE | DMA_FLAG_SUSP | DMA_FLAG_TO));
    h->Instance->CBR1 = 0U;
    h->ErrorCode = HAL_DMA_ERROR_NONE;
    h->State = HAL_DMA_STATE_READY;
    if (h->LinkedListQueue != NULL)
    {
        h->LinkedListQueue->State = HAL_DMA_QUEUE_STATE_READY;
    }
    __HAL_UNLOCK(h);
    hlpuart1.RxState = HAL_UART_STATE_READY;
}

/**
 * @brief Перезапустить кольцевой приём после ошибки UART.
 *
 * @return true при успехе.
 */
static bool gnss_rx_restart(void)
{
    if (HAL_UART_Receive_DMA(&hlpuart1, gnss_rx_buffer, GNSS_RX_BUFFER_SIZE) == HAL_OK)
    {
        return true;
    }

    /* Канал мог остаться не в READY: сбросить и повторить один раз. */
    /* Здесь коды - от неудавшегося перезапуска (DMA err 0x40 = BUSY), а не исходной ошибки. */
    PURRGO_LOGE("GNSS RX restart: channel busy, forcing READY (uart err 0x%lX, dma err 0x%lX, dma state %d)\r\n",
               (unsigned long)hlpuart1.ErrorCode,
               (unsigned long)handle_LPDMA1_Channel0.ErrorCode,
               (int)handle_LPDMA1_Channel0.State);

    gnss_dma_force_ready();

    return (HAL_UART_Receive_DMA(&hlpuart1, gnss_rx_buffer, GNSS_RX_BUFFER_SIZE) == HAL_OK);
}

/*
 * Колбэки gnss_link. Вызываются из purrgo_gnss_read_byte() (основной цикл), то
 * есть в том же контексте, что и перезапуск приёма после ошибки UART.
 */

/* Паузa после команды смены скорости: модуль применяет её не мгновенно. */
#define GNSS_LINK_SETTLE_MS 100U

/**
 * @brief Перенастроить LPUART1 на скорость baud и перезапустить приём.
 *
 * Порядок: остановить приём (и DMA) -> HAL_UART_Init с новой скоростью ->
 * сбросить потребителя и флаг ошибки (приём остановлен, прерывание его не
 * поставит) -> запустить круговой DMA заново с начала буфера.
 */
static bool link_set_baud(uint32_t baud, void *ctx)
{
    (void)ctx;

    (void)HAL_UART_AbortReceive(&hlpuart1);

    hlpuart1.Init.BaudRate = baud;
    if (HAL_UART_Init(&hlpuart1) != HAL_OK)
    {
        PURRGO_LOGE("GNSS: LPUART1 reinit at %lu baud FAILED\r\n", (unsigned long)baud);
        return false;
    }

    gnss_rx_error_flag = false;
    purrgo_gnss_ring_reset(&gnss_ring);
    gnss_last_head = 0U;

    if (!gnss_rx_restart())
    {
        PURRGO_LOGE("GNSS: RX restart at %lu baud FAILED\r\n", (unsigned long)baud);
        return false;
    }
    return true;
}

/**
 * @brief Отправить модулю команду смены скорости (после полной передачи на старой скорости).
 */
static bool link_send_baud_cmd(uint32_t baud, void *ctx)
{
    uint8_t frame[24];
    (void)ctx;

    const size_t len = purrgo_gnss_receiver_build_baud(frame, sizeof frame, baud);

    if (len == 0U)
    {
        PURRGO_LOGW("GNSS: receiver model cannot change baud rate\r\n");
        return false;
    }
    return gnss_tx_raw(frame, len);
}

static void link_on_state(gnss_link_state_t state, uint32_t baud, void *ctx)
{
    (void)ctx;

    switch (state)
    {
    case GNSS_LINK_PROBE:
        purrgo_gnss_monitor_link(baud, false);
        PURRGO_LOG("GNSS: listening for NMEA at %lu baud\r\n", (unsigned long)baud);
        break;

    case GNSS_LINK_SWITCH:
        PURRGO_LOG("GNSS: NMEA at %lu baud, module -> %lu baud\r\n",
                   (unsigned long)baud, (unsigned long)PURRGO_GNSS_BAUD_TARGET);
        break;

    case GNSS_LINK_VERIFY:
        PURRGO_LOG("GNSS: UART at %lu baud, waiting for NMEA\r\n", (unsigned long)baud);
        break;

    case GNSS_LINK_READY:
    case GNSS_LINK_DEGRADED:
        if (state == GNSS_LINK_READY)
        {
            PURRGO_LOG("GNSS: link OK at %lu baud\r\n", (unsigned long)baud);
        }
        else
        {
            PURRGO_LOGW("GNSS: module did not switch to %lu baud, staying at %lu baud\r\n",
                       (unsigned long)PURRGO_GNSS_BAUD_TARGET, (unsigned long)baud);
        }
        /* Диагностика с нуля на рабочей скорости, затем настройка набора NMEA. */
        purrgo_gnss_monitor_link(baud, true);
        purrgo_gnss_receiver_configure(gnss_uart_tx);
#if (PURRGO_GNSS_CONFIG_RETRY_MS > 0)
        gnss_t_cfg = HAL_GetTick();
        gnss_cfg_retry_pending = true;
#endif
        break;

    case GNSS_LINK_FAILED:
        PURRGO_LOGE("GNSS: no NMEA at %lu and %lu baud, link FAILED (UART left at %lu baud)\r\n",
                   (unsigned long)PURRGO_GNSS_BAUD_DEFAULT, (unsigned long)PURRGO_GNSS_BAUD_TARGET,
                   (unsigned long)PURRGO_GNSS_BAUD_DEFAULT);
        /* Монитор объяснит причину (нет данных / не NMEA / неверное подключение). */
        purrgo_gnss_monitor_link(gnss_link_baud(&gnss_link), true);
        break;

    case GNSS_LINK_IDLE:
    default:
        break;
    }
}

/**
 * @brief Запустить установление связи с модулем (после старта DMA-приёма).
 */
static void gnss_link_begin(void)
{
#if (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_NONE)
    /* Приёмник без команд настройки: скорость не меняется, ждать нечего. */
    gnss_link_stop(&gnss_link);
    purrgo_gnss_receiver_configure(gnss_uart_tx);
#else
    const gnss_link_cfg_t cfg = {
        PURRGO_GNSS_BAUD_DEFAULT, PURRGO_GNSS_BAUD_TARGET,
        PURRGO_GNSS_LINK_PROBE_MS, GNSS_LINK_SETTLE_MS, (uint8_t)PURRGO_GNSS_LINK_ATTEMPTS
    };
    const gnss_link_ops_t ops = { link_set_baud, link_send_baud_cmd, link_on_state, NULL };

#if (PURRGO_GNSS_CONFIG_RETRY_MS > 0)
    gnss_cfg_retry_pending = false;
#endif
    gnss_link_init(&gnss_link, &cfg, &ops);
    gnss_link_start(&gnss_link, HAL_GetTick());
#endif
}

/**
 * @brief Отправить байты приёмнику GNSS (команды UBX из ядра, см. purrgo/gnss_io.h).
 *
 * Не отправляет, пока GNSS выключен по EN, не запущен приём (DMA) или идёт поиск
 * скорости UART (gnss_link): в этих состояниях команда пропала бы или сбила поиск.
 */
bool purrgo_gnss_write(const uint8_t *data, uint16_t length)
{
    if ((data == NULL) || (length == 0U) || !gnss_powered || (hlpuart1.hdmarx == NULL) ||
        gnss_link_busy(&gnss_link))
    {
        return false;
    }
    return gnss_tx_raw(data, length);
}

/**
 * @brief Забрать байт из кольцевого буфера и показать его монитору связи.
 */
static bool gnss_pop(uint16_t head, uint8_t *byte)
{
    if (purrgo_gnss_ring_pop(&gnss_ring, head, byte))
    {
        purrgo_gnss_monitor_feed(*byte);
        gnss_link_feed(&gnss_link, *byte);
        return true;
    }
    return false;
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

    /* GNSS выключен по EN: LPUART1 и DMA остановлены. */
    if (!gnss_powered)
    {
        return false;
    }

    /* Если DMA ещё не был запущен, читать нечего. */
    if (hlpuart1.hdmarx == NULL)
    {
        return false;
    }

    /* Определение скорости и её смена: таймауты, переходы (после READY отправляется настройка). */
    gnss_link_poll(&gnss_link, HAL_GetTick());

#if (PURRGO_GNSS_CONFIG_RETRY_MS > 0)
    /* Повторяем настройку один раз после установления связи: команда могла потеряться. */
    if (gnss_cfg_retry_pending &&
        ((uint32_t)(HAL_GetTick() - gnss_t_cfg) >= (uint32_t)PURRGO_GNSS_CONFIG_RETRY_MS))
    {
        gnss_cfg_retry_pending = false;
        purrgo_gnss_receiver_configure(gnss_uart_tx);
    }
#endif

    /*
     * Положение producer по счётчику CBR1 (в штатной работе). После ошибки UART
     * DMA остановлен и HAL обнулил CBR1, поэтому используется gnss_last_head.
     */
    if (gnss_rx_error_flag)
    {
        /* DMA остановлен, CBR1 обнулён HAL: берём последнее известное положение. */
        head = gnss_last_head;
    }
    else
    {
        head = purrgo_gnss_ring_head_from_ndtr(&gnss_ring,
                                               __HAL_DMA_GET_COUNTER(hlpuart1.hdmarx));

        /*
         * Прерывание ошибки могло сработать между проверкой флага и чтением счётчика:
         * HAL_UART_AbortReceive() уже обнулил CBR1, и head выше посчитан по нулю
         * (трактуется как 0), а не по реальному положению DMA. Проверяем флаг ещё раз
         * ПОСЛЕ чтения счётчика (без запрета прерываний): если флаг теперь установлен,
         * счётчику верить нельзя. Если ISR сработает уже после этой проверки, head
         * получен до остановки DMA и остаётся достоверным.
         */
        if (gnss_rx_error_flag)
        {
            head = gnss_last_head;
        }
        else
        {
            gnss_last_head = head;
        }
    }

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
        if (gnss_pop(head, byte))
        {
            return true;
        }

        gnss_rx_error_flag = false;
        purrgo_gnss_ring_reset(&gnss_ring);

        /* При поиске скорости ошибки кадра ожидаемы (приём на неверной скорости): без лога. */
        if (!gnss_link_busy(&gnss_link))
        {
            PURRGO_LOGE("GNSS RX error 0x%lX (PE=1 NE=2 FE=4 ORE=8 DMA=0x10), dma err 0x%lX"
                       " (DTE=1 ULE=2 USE=4), restarting RX\r\n",
                       (unsigned long)gnss_rx_error_code,
                       (unsigned long)handle_LPDMA1_Channel0.ErrorCode);
        }

        gnss_last_head = 0U;

        if (!gnss_rx_restart())
        {
            PURRGO_LOGE("GNSS RX RESTART ERROR! (uart err 0x%lX, dma err 0x%lX, dma state %d)\r\n",
                       (unsigned long)hlpuart1.ErrorCode,
                       (unsigned long)handle_LPDMA1_Channel0.ErrorCode,
                       (int)handle_LPDMA1_Channel0.State);
        }
        return false;
    }

    /* false - новых данных нет. */
    return gnss_pop(head, byte);
}

/**
 * @brief Callback половины DMA-передачи LPUART1 RX.
 */
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != LPUART1)
    {
        return;
    }
}

/**
 * @brief Callback полного DMA-буфера LPUART1 RX.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != LPUART1)
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

    if (huart->Instance != LPUART1)
    {
        return;
    }

    /*
     * Останавливаем текущий ошибочный приём.
     * HAL_UART_AbortReceive() также останавливает связанный DMA RX;
     * его счётчик NDTR остаётся на достигнутом значении, по нему основной цикл
     * определит, сколько байт ещё нужно вычитать.
     */
    /* Код ошибки - для монитора связи (до того, как Abort его сбросит). */
    gnss_rx_error_code = huart->ErrorCode;
    purrgo_gnss_monitor_uart_error(huart->ErrorCode);

    (void)HAL_UART_AbortReceive(huart);

    /*
     * Сигнализируем основному циклу: вычитать остаток, затем перезапустить DMA.
     * Флаг ставится только после остановки DMA.
     */
    gnss_rx_error_flag = true;
}

/**
 * @brief Фронт 1PPS (PA8, EXTI8, Rising).
 *
 * Вызывается из прерывания EXTI8. Приоритет EXTI8 в NVIC должен быть ниже
 * (числом больше), чем у LPDMA1 и LPUART1, чтобы импульс не вытеснял приём
 * GNSS (настройка в CubeMX). При выключенном GNSS (EN Low) фронты игнорируются:
 * линия PPS у отключённого модуля не должна давать срабатываний, а если они
 * есть, это видно по отсутствию счёта в мониторе.
 */
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
    if ((GPIO_Pin == GNSS_PPS_Pin) && gnss_powered)
    {
        purrgo_gnss_monitor_pps_edge(HAL_GetTick());
    }
}
