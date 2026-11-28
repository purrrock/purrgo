#include "purrgo/gnss_io.h"
#include "purrgo/gnss_ring.h"
#include "purrgo/gnss_receiver.h"
#include "purrgo/gnss_ubx.h"
#include "usart.h"
#include "lpdma.h"
#include "main.h"
#include "purrgo/hardware_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "purrgo/logger.h"
#include "purrgo/hw_fault.h"
#include "gnss_monitor.h"
#include "gnss_link.h"
#include "gnss_stop.h"
#include "gnss_raw.h"

/*
 * GNSS на STM32U585: LPUART1_RX -> LPDMA1 Channel 0, linked-list в круговом
 * режиме (один узел, ссылающийся на себя):
 *   - очередь узлов собирается здесь, а не берётся из linked_list.c CubeMX:
 *     MX_LPDMA1_Init() инициализирует канал в режиме LINKEDLIST_NORMAL, а
 *     замкнутая очередь допустима только в LINKEDLIST_CIRCULAR
 *     (HAL_DMAEx_List_LinkQ), поэтому канал инициализируется заново;
 *   - HAL_UART_Receive_DMA() сам записывает в узел размер блока, адрес RDR и
 *     адрес буфера, поэтому узел собирается с нулевыми адресами;
 *   - LPUART1 обслуживается LPDMA1 (запрос LPDMA1_REQUEST_LPUART1_RX); приём
 *     продолжается и в Stop 2 (автономный режим LPUART1/LPDMA1/SRAM4, см. gnss_stop.h):
 *     ядро будит прерывание RXNE LPUART1 (пришёл байт), разрешённое только на время сна;
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

/*
 * Шторм ошибок приёма: модуль мог сбросить настройку (на заводской настройке он снова шлёт NMEA на 38400,
 * а у нас UART на рабочей скорости: сплошные FE/ORE; заметно по мигающему светодиоду PPS) или отвалиться.
 * GNSS_RX_STORM_COUNT ошибок за GNSS_RX_STORM_WINDOW_MS - связь переустанавливается: сначала мягко (заново
 * подбор скорости UART и настройка профиля), при повторном шторме вскоре после этого - выключением по EN.
 */
#define GNSS_RX_STORM_COUNT      5U
#define GNSS_RX_STORM_WINDOW_MS  3000U
#define GNSS_RX_ERR_LOG_MAX      2U      /* в одном окне в лог пишутся только первые ошибки */
#define GNSS_RECOVER_HARD_MS     30000U  /* шторм вскоре после мягкого перезапуска - жёсткий */
#define GNSS_EN_LOW_MS           200U    /* пауза с EN = Low при жёстком перезапуске */
static uint32_t gnss_rx_err_count = 0U;
static uint32_t gnss_rx_err_t0 = 0U;
static uint32_t gnss_recover_t_soft = 0U;
static bool gnss_recover_soft_done = false;

/* Код ошибки UART, с которым вызван HAL_UART_ErrorCallback (для лога). */
static volatile uint32_t gnss_rx_error_code = 0U;

#if (PURRGO_GNSS_CONFIG_RETRY_MS > 0)
/* Повторная отправка настройки NMEA через PURRGO_GNSS_CONFIG_RETRY_MS после установления связи. */
static uint32_t gnss_t_cfg = 0U;
static bool gnss_cfg_retry_pending = false;
#endif

/*
 * Профиль настройки приёмника (purrgo_gnss_set_profile): навигация или трекер (PSMOO). После
 * включения модуля всегда навигация. Выход из трекера идёт в два шага: приёмник в состоянии "off"
 * будится активностью на RX, настройка навигации отправляется после паузы (gnss_profile_apply_pending).
 */
static purrgo_gnss_profile_t gnss_profile = PURRGO_GNSS_PROFILE_NAV;

#if PURRGO_GNSS_TRACKER_EN_ACTIVE && PURRGO_GNSS_TRACKER_EN_BBR
/*
 * Цикл по EN: настройка профиля EN записана в BBR модуля (RAM+BBR, gnss_receiver.c), после EN = High её слать не
 * нужно. Решает первый кадр после включения, без таймеров (модуль может молчать секунды, а МК спать в Stop 2):
 *   - первое включение после перезапуска МК настройку отправляет всегда (gnss_en_cfg_written = false): BBR мог
 *     остаться от другой версии прошивки или быть пустым;
 *   - дальше связь установлена по кадру UBX без единого предложения NMEA - в BBR NMEA выключен, настройка на месте,
 *     слать нечего;
 *   - связь установлена по предложению NMEA - BBR пуст (потеря резервного питания), настройка отправляется сразу;
 *   - после отправки модуль не выключается PURRGO_GNSS_EN_CFG_SETTLE_MS (purrgo_gnss_power_off()).
 */
static bool gnss_en_cfg_written = false;     /* настройка EN отправлялась после старта МК */
static uint32_t gnss_en_pvt_base = 0U;       /* значение pvt_count в момент включения модуля */
static bool gnss_en_settle_pending = false;
static uint32_t gnss_en_t_tx = 0U;           /* когда ушла последняя настройка EN */
#endif

#if PURRGO_GNSS_TRACKER_ACTIVE
static bool gnss_profile_apply_pending = false;
static uint32_t gnss_profile_t_apply = 0U;
/* В слой BBR приёмника записан профиль трекера: настройка навигации должна его очистить. */
static bool gnss_bbr_dirty = false;

static void gnss_wake_receiver(void);

/* Пауза после будящих байтов: приёмник выходит из "off" и запускается не мгновенно. */
#define GNSS_WAKE_SETTLE_MS 300U
/* Пауза после последней команды перед выключением по EN: модуль принимает и применяет её. */
#define GNSS_RESTORE_TX_MS 50U
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
 * "Сырой" режим (gnss_raw.h, аппаратный тест GNSS_BBR): приём через тот же DMA-буфер, но без gnss_link,
 * настройки приёмника, монитора и восстановления связи после шторма ошибок: тест сам решает, что и когда
 * отправлять модулю. В обычной прошивке флаг всегда false.
 */
static volatile bool gnss_raw_mode = false;
static volatile uint32_t gnss_raw_err_total = 0U;

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
        purrgo_hw_fault(PURRGO_HWF_GNSS, "TX failed (HAL status %d, uart err 0x%lX)",
                        (int)st, (unsigned long)hlpuart1.ErrorCode);
    }
    return (st == HAL_OK);
}

/**
 * @brief Отправка настройки приёмнику GNSS (транспорт для gnss_receiver).
 *
 * Расшифровку команды (группа, ключи, значения) пишет в лог gnss_receiver перед отправкой;
 * здесь логируется только сбой передачи (в gnss_tx_raw).
 */
static void gnss_uart_tx(const uint8_t *data, size_t len)
{
    (void)gnss_tx_raw(data, len);
}

/*
 * Отправить приёмнику настройку текущего профиля. Скорость UART передаётся для профиля трекера: он
 * записывает её в слой BBR (RAM приёмника в состоянии "off" PSMOO очищается).
 */
static void gnss_apply_config(void)
{
    purrgo_gnss_profile_t profile = gnss_profile;

#if PURRGO_GNSS_TRACKER_ACTIVE
    if (profile == PURRGO_GNSS_PROFILE_TRACKER)
    {
        gnss_bbr_dirty = true;
    }
    else if ((profile == PURRGO_GNSS_PROFILE_NAV) && gnss_bbr_dirty)
    {
        profile = PURRGO_GNSS_PROFILE_NAV_AFTER_TRACKER;   /* заодно очистить BBR от PSMOO */
    }
    /* Профили цикла по EN (_EN, _CT) настройку пишут только в RAM: BBR не трогается, чистить нечего. */
#endif
#if PURRGO_GNSS_TRACKER_EN_ACTIVE && PURRGO_GNSS_TRACKER_EN_BBR
    if (profile == PURRGO_GNSS_PROFILE_TRACKER_EN)
    {
        gnss_en_cfg_written = true;
        gnss_en_settle_pending = true;
        gnss_en_t_tx = HAL_GetTick();
    }
#endif
    purrgo_gnss_receiver_configure_profile(gnss_uart_tx, profile, gnss_link_baud(&gnss_link));
#if PURRGO_GNSS_TRACKER_EN_ACTIVE && PURRGO_GNSS_TRACKER_EN_BBR
    if (profile == PURRGO_GNSS_PROFILE_TRACKER_EN)
    {
        gnss_en_t_tx = HAL_GetTick();   /* отсчёт от конца передачи (кадры уходят блокирующе) */
    }
#endif
    /* В трекере приёмник молчит между фиксами: монитору не сообщать об этом как о сбое. */
    purrgo_gnss_monitor_expect_gaps(gnss_profile == PURRGO_GNSS_PROFILE_TRACKER);
}

#if (PURRGO_GNSS_CONFIG_RETRY_MS > 0)
/**
 * @brief Приёмник ответил (ACK или NAK) на все группы последней серии настройки.
 *
 * Только для UBX: ответы считает gnss_ubx.c по cfg_begin() из gnss_receiver.c. Для приёмников
 * без UBX (серия не отслеживается, count = 0) подтвердить нечем, повтор остаётся.
 */
static bool gnss_cfg_all_replied(void)
{
    const uint8_t count = purrgo_gnss_ubx_cfg_count();

    return (count > 0U) && (purrgo_gnss_ubx_cfg_replies() >= count);
}
#endif

static void gnss_link_begin(void);

/*
 * Режим Stop для LPUART1: бит UESM, без него LPUART1 не работает в Stop. У UART STM32U5 нет флага WUF и
 * выбора источника пробуждения (HAL_UARTEx_StopModeWakeUpSourceConfig() здесь не нужна): ядро будит обычное
 * прерывание, разрешённое в CR1, а ядро LPUART1 (HSI16) в Stop запрашивается автономным режимом
 * (RCC_SRDAMR, stop_mode.c). Прерывание RXNE разрешается только на время сна (purrgo_gnss_stop_arm()):
 * в Run байты забирает LPDMA1. HAL_UART_Init() сбрасывает UESM, поэтому вызывается после каждой
 * (пере)инициализации LPUART1.
 */
static void gnss_uart_stop_cfg(void)
{
    if (HAL_UARTEx_EnableStopMode(&hlpuart1) != HAL_OK)
    {
        purrgo_hw_fault(PURRGO_HWF_GNSS, "LPUART1 Stop mode config failed");
    }
}

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
        purrgo_hw_fault(PURRGO_HWF_GNSS, "DMA setup error");
        return;
    }

    gnss_uart_stop_cfg();

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
        purrgo_hw_fault(PURRGO_HWF_GNSS, "RX DMA start failed (init error)");
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
 * PA2/PA3 в Analog без подтяжки, то есть в Hi-Z). Линии UART не драйвятся ни в High, ни в Low:
 *   - High: модуль в режиме "hardware backup" (VCC и V_IO = 0), в нём "PIOs must not be driven"
 *     (MAX-M10S datasheet, 3.2), и он подпитывался бы через защитные диоды;
 *   - Low: если V_IO у модуля остаётся, вход RXD подтянут к V_IO внутренним резистором 17 кОм
 *     (Rpu,IO 6-72 кОм, там же), и Low даёт постоянный ток V_IO / 17 кОм (около 190 мкА);
 *   - Hi-Z: тока нет в обоих случаях, а RXD при V_IO удерживается подтяжкой в High (в состояниях
 *     модуля "continuous" и "software standby" RXD - Input pull-up).
 * PURRGO_GNSS_OFF_TX_HIZ = 0 возвращает прежнее поведение (PA2 в Low) для сравнения замером тока.
 */
void purrgo_gnss_power_off(void)
{
    GPIO_InitTypeDef gpio = {0};

    if (!gnss_powered)
    {
        return;
    }

#if PURRGO_GNSS_TRACKER_ACTIVE
    /*
     * EN в Low не гарантирует, что приёмник забудет режим: если питание backup-домена сохраняется, в BBR
     * остался бы PSMOO, и после следующего включения приёмник замолчал бы до первого периода. Поэтому, если
     * мы в трекере (или выход из него ещё не применён), сначала будим приёмник и возвращаем настройку
     * навигации в RAM и BBR. Блокирующе (~0,35 с), только при выключении из трекера.
     */
    if ((gnss_profile == PURRGO_GNSS_PROFILE_TRACKER) || gnss_profile_apply_pending)
    {
        const gnss_link_state_t link = gnss_link_state(&gnss_link);

        gnss_profile = PURRGO_GNSS_PROFILE_NAV;
        gnss_profile_apply_pending = false;
        if ((link == GNSS_LINK_READY) || (link == GNSS_LINK_DEGRADED))
        {
            gnss_wake_receiver();
            HAL_Delay(GNSS_WAKE_SETTLE_MS);
            gnss_apply_config();
            HAL_Delay(GNSS_RESTORE_TX_MS);
        }
    }
#endif

#if PURRGO_GNSS_TRACKER_EN_ACTIVE && PURRGO_GNSS_TRACKER_EN_BBR
    /*
     * Настройка EN ушла в BBR только что (фикс пришёл раньше, чем модуль применил её): EN = Low сразу оборвал бы
     * запись, и следующее включение пришло бы с пустым BBR. Выдержка от конца передачи; ждать приходится только
     * при отправке настройки (первое включение после старта МК, потеря BBR).
     */
    if (gnss_en_settle_pending)
    {
        const uint32_t since_tx = HAL_GetTick() - gnss_en_t_tx;

        gnss_en_settle_pending = false;
        if (since_tx < (uint32_t)PURRGO_GNSS_EN_CFG_SETTLE_MS)
        {
            HAL_Delay((uint32_t)PURRGO_GNSS_EN_CFG_SETTLE_MS - since_tx);
        }
    }
#endif

    gnss_powered = false;
    gnss_link_stop(&gnss_link);
#if (PURRGO_GNSS_CONFIG_RETRY_MS > 0)
    gnss_cfg_retry_pending = false;
#endif
    purrgo_gnss_monitor_stop();   /* отчёты монитора - только при включённом GNSS */

    /* 1. Остановить приём; HAL_UART_AbortReceive() останавливает и DMA. */
    (void)HAL_UART_AbortReceive(&hlpuart1);
    gnss_rx_error_flag = false;

    /* 2. EN в Low: модуль выключен (PB0 - Push-Pull). */
    HAL_GPIO_WritePin(GNSS_EN_GPIO_Port, GNSS_EN_Pin, GPIO_PIN_RESET);

    /* 3. LPUART1 выключен; PA2/PA3 -> Analog, без подтяжек (Hi-Z). */
    (void)HAL_UART_DeInit(&hlpuart1);

#if !PURRGO_GNSS_OFF_TX_HIZ
    /* 4. Прежнее поведение: PA2 (TX в сторону модуля) в Low. PA3 (RX) остаётся Analog без подтяжки. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_2;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
#else
    (void)gpio;   /* PA2 остаётся Analog: ни ток через подтяжку RXD модуля, ни подпитка через диоды */
#endif

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
static void gnss_power_on_profile(purrgo_gnss_profile_t profile)
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
    gnss_profile = profile;   /* профиль нужен до purrgo_gnss_init(): от него зависит скорость связи */
#if PURRGO_GNSS_TRACKER_ACTIVE
    gnss_profile_apply_pending = false;
    gnss_bbr_dirty = false;
#endif
    gnss_powered = true;
    gnss_last_head = 0U;
#if PURRGO_GNSS_TRACKER_EN_ACTIVE && PURRGO_GNSS_TRACKER_EN_BBR
    gnss_en_pvt_base = purrgo_gnss_ubx_status()->pvt_count;   /* до начала приёма: гонки с разбором нет */
    gnss_en_settle_pending = false;
#endif
    purrgo_gnss_init();
    purrgo_gnss_monitor_init();

    PURRGO_LOG("GNSS: power on (EN high)\r\n");
}

void purrgo_gnss_power_on(void)
{
    gnss_power_on_profile(PURRGO_GNSS_PROFILE_NAV);
}

/**
 * @brief Включить GNSS с профилем; уже включён - перезапуск по EN (см. purrgo/gnss_io.h).
 *
 * Выключение без отката настройки трекера: профиль сбрасывается в навигацию заранее (откат не нужен: слой RAM
 * при EN = Low всё равно стирается, а профили цикла по EN в BBR ничего не пишут).
 */
void purrgo_gnss_restart_profile(purrgo_gnss_profile_t profile)
{
    if (gnss_powered)
    {
        gnss_profile = PURRGO_GNSS_PROFILE_NAV;
#if PURRGO_GNSS_TRACKER_ACTIVE
        gnss_profile_apply_pending = false;
#endif
        purrgo_gnss_power_off();
        HAL_Delay(GNSS_EN_LOW_MS);
    }
    gnss_power_on_profile(profile);
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
        purrgo_hw_fault(PURRGO_HWF_GNSS, "LPUART1 reinit at %lu baud failed", (unsigned long)baud);
        return false;
    }
    gnss_uart_stop_cfg();

    gnss_rx_error_flag = false;
    purrgo_gnss_ring_reset(&gnss_ring);
    gnss_last_head = 0U;

    if (!gnss_rx_restart())
    {
        purrgo_hw_fault(PURRGO_HWF_GNSS, "RX restart at %lu baud failed", (unsigned long)baud);
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
    PURRGO_LOGI("GNSS: TX UART1-BAUDRATE=%lu (%u B)\r\n", (unsigned long)baud, (unsigned)len);
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
                   (unsigned long)baud, (unsigned long)gnss_link.cfg.target_baud);
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
                       (unsigned long)gnss_link.cfg.target_baud, (unsigned long)baud);
        }
        /* Диагностика с нуля на рабочей скорости, затем настройка набора NMEA. */
        purrgo_gnss_monitor_link(baud, true);
#if PURRGO_GNSS_TRACKER_EN_ACTIVE && PURRGO_GNSS_TRACKER_EN_BBR
        if ((gnss_profile == PURRGO_GNSS_PROFILE_TRACKER_EN) && gnss_en_cfg_written)
        {
            /* Что пришло, пока искали связь: NMEA - у модуля заводская настройка, только UBX - настройка из BBR. */
            const uint32_t nmea = gnss_link.diag.sentences_ok;
            const uint32_t ubx = gnss_link.diag.ubx_frames_ok;

            if (nmea == 0U)
            {
                PURRGO_LOG("GNSS: after EN only UBX (%u frames), NMEA off: settings kept in BBR, config not sent\r\n",
                           (unsigned)ubx);
                break;
            }
            PURRGO_LOGW("GNSS: after EN NMEA (%u sentences, %u UBX): BBR settings lost, sending config\r\n",
                        (unsigned)nmea, (unsigned)ubx);
        }
#endif
        gnss_apply_config();
#if (PURRGO_GNSS_CONFIG_RETRY_MS > 0)
        gnss_t_cfg = HAL_GetTick();
        gnss_cfg_retry_pending = true;
#endif
        break;

    case GNSS_LINK_FAILED:
        purrgo_hw_fault(PURRGO_HWF_GNSS, "no NMEA at %lu and %lu baud, link failed (UART left at %lu baud)",
                        (unsigned long)PURRGO_GNSS_BAUD_DEFAULT, (unsigned long)gnss_link.cfg.target_baud,
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
    /*
     * Рабочая скорость: PURRGO_GNSS_BAUD_TARGET, но профили цикла по EN (приёмник включён на секунды, после каждого
     * включения слой RAM пуст) остаются на заводской: переключение скорости не окупается и добавляет отказы.
     */
    uint32_t target_baud = PURRGO_GNSS_BAUD_TARGET;
#if PURRGO_GNSS_TRACKER_EN_ACTIVE
    if ((gnss_profile == PURRGO_GNSS_PROFILE_TRACKER_EN) || (gnss_profile == PURRGO_GNSS_PROFILE_TRACKER_CT))
    {
        target_baud = PURRGO_GNSS_BAUD_DEFAULT;
    }
#endif
    const gnss_link_cfg_t cfg = {
        PURRGO_GNSS_BAUD_DEFAULT, target_baud,
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

#if PURRGO_GNSS_TRACKER_ACTIVE
/**
 * @brief Разбудить приёмник из состояния "off" PSMOO.
 *
 * Источник пробуждения - любой фронт на RX модуля (manual MAX-M10S, 3.6.2.5), поэтому достаточно
 * отправить несколько байтов 0xFF (каждый даёт стартовый бит). Сами байты модуль не разбирает.
 */
static void gnss_wake_receiver(void)
{
    static const uint8_t wake[] = { 0xFF, 0xFF, 0xFF, 0xFF };

    PURRGO_LOGI("GNSS: TX wake-up 4 x 0xFF (PSMOO)\r\n");
    (void)gnss_tx_raw(wake, sizeof wake);
}
#endif

#if PURRGO_GNSS_TRACKER_ACTIVE
static const char *gnss_profile_name(purrgo_gnss_profile_t profile)
{
    switch (profile)
    {
    case PURRGO_GNSS_PROFILE_TRACKER:    return "TRACKER";
    case PURRGO_GNSS_PROFILE_TRACKER_EN: return "TRACKER_EN";
    case PURRGO_GNSS_PROFILE_TRACKER_CT: return "TRACKER_CT";
    case PURRGO_GNSS_PROFILE_NAV_AFTER_TRACKER:
    case PURRGO_GNSS_PROFILE_NAV:
    default:                             return "NAV";
    }
}
#endif

/**
 * @brief Переключить профиль настройки приёмника (см. purrgo/gnss_io.h).
 */
void purrgo_gnss_set_profile(purrgo_gnss_profile_t profile)
{
#if PURRGO_GNSS_TRACKER_ACTIVE
    if (profile == gnss_profile)
    {
        return;
    }

    const gnss_link_state_t link = gnss_link_state(&gnss_link);

    gnss_profile = profile;
    gnss_profile_apply_pending = false;

    if (!gnss_powered || ((link != GNSS_LINK_READY) && (link != GNSS_LINK_DEGRADED)))
    {
        /* GNSS выключен или идёт поиск скорости: профиль применится при установлении связи. */
        PURRGO_LOG("GNSS: profile %s (deferred)\r\n", gnss_profile_name(profile));
        return;
    }

    if ((profile == PURRGO_GNSS_PROFILE_TRACKER_EN) || (profile == PURRGO_GNSS_PROFILE_TRACKER_CT))
    {
        /* Цикл по EN: приёмник работает на заводской скорости, настройка в RAM сразу (и повтор по таймеру). */
        PURRGO_LOG("GNSS: profile %s\r\n", gnss_profile_name(profile));
        gnss_apply_config();
#if (PURRGO_GNSS_CONFIG_RETRY_MS > 0)
        gnss_t_cfg = HAL_GetTick();
        gnss_cfg_retry_pending = true;
#endif
    }
    else if (profile == PURRGO_GNSS_PROFILE_TRACKER)
    {
        /* Вход: приёмник работает, настройка сразу (и повтор через PURRGO_GNSS_CONFIG_RETRY_MS). */
        PURRGO_LOG("GNSS: profile TRACKER (%s, fix every %u s)\r\n",
                   PURRGO_GNSS_TRACKER_PSMOO_ACTIVE ? "PSMOO" : "PSMCT",
                   (unsigned)PURRGO_GNSS_TRACKER_PERIOD_S);
        gnss_apply_config();
#if (PURRGO_GNSS_CONFIG_RETRY_MS > 0)
        gnss_t_cfg = HAL_GetTick();
        gnss_cfg_retry_pending = true;
#endif
    }
    else
    {
        /* Выход: приёмник, возможно, в состоянии "off". Будим и ждём паузу, настройка уходит из poll. */
        PURRGO_LOG("GNSS: profile NAV (wake receiver)\r\n");
#if (PURRGO_GNSS_CONFIG_RETRY_MS > 0)
        gnss_cfg_retry_pending = false;
#endif
        purrgo_gnss_monitor_expect_gaps(false);
        gnss_wake_receiver();
        gnss_profile_t_apply = HAL_GetTick();
        gnss_profile_apply_pending = true;
    }
#else
    (void)profile;
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
        if (gnss_raw_mode)
        {
            return true;
        }
        purrgo_gnss_monitor_feed(*byte);
        gnss_link_feed(&gnss_link, *byte);
        return true;
    }
    return false;
}

/**
 * @brief Переустановить связь с приёмником после шторма ошибок приёма.
 *
 * Мягко: сброс состояния приёма и новый подбор скорости UART (gnss_link) с настройкой текущего профиля
 * (навигация или трекер) после установления связи. Если шторм повторился в течение GNSS_RECOVER_HARD_MS
 * после мягкого перезапуска - жёстко: EN в Low на GNSS_EN_LOW_MS и снова в High; профиль сохраняется.
 */
static void gnss_recover_link(uint32_t now)
{
    const bool hard = gnss_recover_soft_done &&
                      ((uint32_t)(now - gnss_recover_t_soft) < GNSS_RECOVER_HARD_MS);

    if (hard)
    {
        const purrgo_gnss_profile_t keep = gnss_profile;

        purrgo_hw_fault(PURRGO_HWF_GNSS, "RX error storm again after link restart, power cycling the module (EN)");
        gnss_profile = PURRGO_GNSS_PROFILE_NAV;   /* без отката настройки трекера перед выключением */
        purrgo_gnss_power_off();
        HAL_Delay(GNSS_EN_LOW_MS);
        purrgo_gnss_power_on();
        gnss_profile = keep;                       /* применится после установления связи */
        gnss_recover_soft_done = false;
        return;
    }

    PURRGO_LOGW("GNSS: RX error storm (%u in %u ms), restarting the link\r\n",
                (unsigned)GNSS_RX_STORM_COUNT, (unsigned)GNSS_RX_STORM_WINDOW_MS);
    gnss_recover_soft_done = true;
    gnss_recover_t_soft = now;

    gnss_rx_error_flag = false;
    purrgo_gnss_ring_reset(&gnss_ring);
    gnss_last_head = 0U;
#if PURRGO_GNSS_TRACKER_ACTIVE
    gnss_profile_apply_pending = false;
#endif
    purrgo_gnss_monitor_expect_gaps(false);
    gnss_link_begin();
}

/* Сколько последних принятых до ошибки байтов показывать в логе (диагностика). */
#define GNSS_ERR_DUMP_BYTES 24U

/**
 * @brief Записать в лог последние байты, принятые до остановки DMA (hex, от старых к новым).
 *
 * По ним видно, чем был поток перед ошибкой: корректные кадры UBX (B5 62 ...) или NMEA ("$GN..." = 24 24 47 4E)
 * либо мусор (приёмник сменил скорость или сбросил настройку).
 */
static void gnss_log_bytes_before(uint16_t head)
{
    static const char hex[] = "0123456789ABCDEF";
    char line[(3U * GNSS_ERR_DUMP_BYTES) + 1U];
    size_t n = 0U;

    for (uint32_t i = GNSS_ERR_DUMP_BYTES; i > 0U; i--)
    {
        const uint8_t b = gnss_rx_buffer[(uint32_t)(head - i) & (GNSS_RX_BUFFER_SIZE - 1U)];

        line[n++] = hex[b >> 4];
        line[n++] = hex[b & 0x0FU];
        line[n++] = ' ';
    }
    line[n] = '\0';
    PURRGO_LOGE("GNSS RX error: %u bytes before it: %s\r\n", (unsigned)GNSS_ERR_DUMP_BYTES, line);
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
    /*
     * Повторяем настройку один раз после установления связи: команда могла потеряться. Если приёмник
     * UBX уже ответил (ACK/NAK) на все группы серии, повтор не нужен: потерь не было, а NAK при
     * повторе придёт снова (причина уже в логе).
     */
    if (gnss_cfg_retry_pending &&
        ((uint32_t)(HAL_GetTick() - gnss_t_cfg) >= (uint32_t)PURRGO_GNSS_CONFIG_RETRY_MS))
    {
        gnss_cfg_retry_pending = false;
        if (gnss_cfg_all_replied())
        {
            PURRGO_LOG("GNSS: config confirmed by module (%u/%u replies), retry skipped\r\n",
                       (unsigned)purrgo_gnss_ubx_cfg_replies(), (unsigned)purrgo_gnss_ubx_cfg_count());
        }
#if PURRGO_GNSS_TRACKER_EN_ACTIVE && PURRGO_GNSS_TRACKER_EN_BBR
        else if ((gnss_profile == PURRGO_GNSS_PROFILE_TRACKER_EN) &&
                 (purrgo_gnss_ubx_status()->pvt_count != gnss_en_pvt_base))
        {
            /* NAV-PVT пошёл: модуль настройку принял (ответы ACK могли ещё не дойти из-за паузы главного цикла). */
            PURRGO_LOG("GNSS: NAV-PVT flowing, config applied (%u/%u replies seen), retry skipped\r\n",
                       (unsigned)purrgo_gnss_ubx_cfg_replies(), (unsigned)purrgo_gnss_ubx_cfg_count());
        }
#endif
        else
        {
            PURRGO_LOG("GNSS: config not confirmed (%u/%u replies), retry\r\n",
                       (unsigned)purrgo_gnss_ubx_cfg_replies(), (unsigned)purrgo_gnss_ubx_cfg_count());
            gnss_apply_config();
        }
    }
#endif

#if PURRGO_GNSS_TRACKER_ACTIVE
    /* Выход из трекера: приёмник разбужен, пауза прошла - отправить настройку навигации. */
    if (gnss_profile_apply_pending &&
        ((uint32_t)(HAL_GetTick() - gnss_profile_t_apply) >= GNSS_WAKE_SETTLE_MS))
    {
        gnss_profile_apply_pending = false;
        gnss_apply_config();
#if (PURRGO_GNSS_CONFIG_RETRY_MS > 0)
        /* Первая отправка могла потеряться (приёмник только проснулся): повтор, как после включения. */
        gnss_t_cfg = HAL_GetTick();
        gnss_cfg_retry_pending = true;
#endif
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

        if (gnss_raw_mode)
        {
            /* Тест: только счёт ошибок и перезапуск приёма, без шторма и без переустановки связи. */
            gnss_raw_err_total++;
            gnss_last_head = 0U;
            (void)gnss_rx_restart();
            return false;
        }

        /* При поиске скорости ошибки кадра ожидаемы (приём на неверной скорости): без лога и счёта. */
        if (!gnss_link_busy(&gnss_link))
        {
            const uint32_t now = HAL_GetTick();

            if ((gnss_rx_err_count == 0U) || ((uint32_t)(now - gnss_rx_err_t0) > GNSS_RX_STORM_WINDOW_MS))
            {
                gnss_rx_err_count = 0U;
                gnss_rx_err_t0 = now;
            }
            gnss_rx_err_count++;

            if (gnss_rx_err_count <= GNSS_RX_ERR_LOG_MAX)
            {
                PURRGO_LOGE("GNSS RX error 0x%lX (PE=1 NE=2 FE=4 ORE=8 DMA=0x10), dma err 0x%lX"
                           " (DTE=1 ULE=2 USE=4; 0x20 = abort of an idle channel, not a transfer error),"
                           " restarting RX\r\n",
                           (unsigned long)gnss_rx_error_code,
                           (unsigned long)handle_LPDMA1_Channel0.ErrorCode);
                gnss_log_bytes_before(head);
            }

            if (gnss_rx_err_count >= GNSS_RX_STORM_COUNT)
            {
                gnss_rx_err_count = 0U;
                gnss_recover_link(now);
                return false;
            }
        }

        gnss_last_head = 0U;

        if (!gnss_rx_restart())
        {
            purrgo_hw_fault(PURRGO_HWF_GNSS, "RX restart error (uart err 0x%lX, dma err 0x%lX, dma state %d)",
                            (unsigned long)hlpuart1.ErrorCode,
                            (unsigned long)handle_LPDMA1_Channel0.ErrorCode,
                            (int)handle_LPDMA1_Channel0.State);
        }
        return false;
    }

    /* false - новых данных нет. */
    return gnss_pop(head, byte);
}

/* ------------------------- сырой режим для теста (gnss_raw.h) ------------------------- */

bool purrgo_gnss_raw_start(uint32_t baud)
{
    gnss_raw_mode = true;
    gnss_link_stop(&gnss_link);
    gnss_powered = true;
    gnss_last_head = 0U;
    gnss_rx_error_flag = false;
    (void)purrgo_gnss_ring_init(&gnss_ring, gnss_rx_buffer, GNSS_RX_BUFFER_SIZE);

    if (!gnss_dma_setup())
    {
        return false;
    }
    return link_set_baud(baud, NULL);
}

bool purrgo_gnss_raw_set_baud(uint32_t baud)
{
    return link_set_baud(baud, NULL);
}

void purrgo_gnss_raw_en(bool on)
{
    if (!on)
    {
        purrgo_gnss_power_off();   /* EN = Low; в сыром режиме откат профиля трекера не выполняется */
        return;
    }
    if (gnss_powered)
    {
        return;
    }

    HAL_GPIO_WritePin(GNSS_EN_GPIO_Port, GNSS_EN_Pin, GPIO_PIN_SET);
    MX_LPUART1_UART_Init();        /* скорость из CubeMX (заводская скорость модуля) */
    (void)purrgo_gnss_raw_start(hlpuart1.Init.BaudRate);
}

uint32_t purrgo_gnss_raw_errors(void)
{
    return gnss_raw_err_total;
}

/* ------------------------- GNSS и Stop 2 (gnss_stop.h) ------------------------- */

/* Положение producer при прошлой проверке и момент, когда оно менялось в последний раз. */
static uint16_t gnss_stop_head;
static uint32_t gnss_stop_change_ms;
static bool     gnss_stop_head_valid;

bool purrgo_gnss_is_powered(void)
{
    return gnss_powered;
}

bool purrgo_gnss_stop_ready(uint32_t now_ms)
{
    if (!gnss_powered)
    {
        gnss_stop_head_valid = false;
        return true;
    }

    /* Приём не запущен или остановлен ошибкой: перезапуск делает основной цикл, спать рано. */
    if ((hlpuart1.hdmarx == NULL) || gnss_rx_error_flag)
    {
        return false;
    }

    /* Поиск скорости и смена скорости идут по таймерам из purrgo_gnss_read_byte(). */
    switch (gnss_link_state(&gnss_link))
    {
    case GNSS_LINK_IDLE:       /* приёмник без команд настройки (PURRGO_GNSS_RECEIVER_NONE) */
    case GNSS_LINK_READY:
    case GNSS_LINK_DEGRADED:
    case GNSS_LINK_FAILED:
        break;
    default:
        return false;
    }

    /* Отложенные команды приёмнику тоже отсчитываются по таймеру. */
#if (PURRGO_GNSS_CONFIG_RETRY_MS > 0)
    if (gnss_cfg_retry_pending)
    {
        return false;
    }
#endif
#if PURRGO_GNSS_TRACKER_ACTIVE
    if (gnss_profile_apply_pending)
    {
        return false;
    }
#endif

    const uint16_t head = purrgo_gnss_ring_head_from_ndtr(&gnss_ring,
                                                          __HAL_DMA_GET_COUNTER(hlpuart1.hdmarx));
    if (!gnss_stop_head_valid || (head != gnss_stop_head))
    {
        /* Данные идут (или это первая проверка): отсчёт тишины заново. */
        gnss_stop_head = head;
        gnss_stop_change_ms = now_ms;
        gnss_stop_head_valid = true;
        return false;
    }

    if (head != gnss_ring.tail)
    {
        return false;   /* в кольце есть то, что основной цикл ещё не разобрал */
    }
    return (uint32_t)(now_ms - gnss_stop_change_ms) >= PURRGO_GNSS_STOP_QUIET_MS;
}

bool purrgo_gnss_stop_arm(void)
{
    if (!gnss_powered)
    {
        return true;   /* LPUART1 остановлен, будить нечему */
    }
    if ((hlpuart1.hdmarx == NULL) || gnss_rx_error_flag)
    {
        return false;
    }

    /* Байты, пришедшие после последней проверки в главном цикле, новым пробуждением не обозначатся. */
    const uint16_t head = purrgo_gnss_ring_head_from_ndtr(&gnss_ring,
                                                          __HAL_DMA_GET_COUNTER(hlpuart1.hdmarx));
    if (head != gnss_ring.tail)
    {
        return false;
    }

    /*
     * RXNE разрешается под PRIMASK: байт, оказавшийся в RDR уже сейчас, тоже даст прерывание, и WFI
     * вернётся сразу. Сам байт во сне забирает LPDMA1.
     */
    __HAL_UART_ENABLE_IT(&hlpuart1, UART_IT_RXNE);
    return true;
}

void purrgo_gnss_stop_disarm(void)
{
    if (!gnss_powered)
    {
        return;
    }
    /*
     * Ожидающее прерывание LPUART1 в NVIC не сбрасывается: так же его мог вызвать флаг ошибки приёма
     * (ORE/FE), и HAL_UART_IRQHandler() после __enable_irq() должен его обработать. С выключенным RXNEIE
     * лишний вызов обработчика безвреден (в режиме DMA у HAL нет RxISR).
     */
    __HAL_UART_DISABLE_IT(&hlpuart1, UART_IT_RXNE);
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
