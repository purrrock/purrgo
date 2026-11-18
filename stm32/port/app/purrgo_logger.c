/*
 * PurrGO STM32 logger backend.
 *
 * Этот файл содержит платформенную реализацию диагностического
 * вывода для STM32.
 *
 * Ядро PurrGO (src/core) не должно зависеть от HAL и от конкретного
 * UART. Поэтому работа с HAL_UART_Transmit() находится здесь,
 * в платформенной части STM32.
 */

#include "purrgo/logger.h"

#include "usart.h"
#include "main.h"

#include "purrgo/fs_hal.h"

#include <stdarg.h>
#include <string.h>
#include "purrgo/purrgo_format.h"

/*
 * Максимальный размер одного сформированного сообщения.
 *
 * Один вызов purrgo_logger_write() формирует сообщение во временном
 * буфере, после чего целиком передаёт его через UART.
 */
#define PURRGO_LOG_BUFFER_SIZE 256U

/* Мигать светодиодом PC13 на каждое сообщение: по умолчанию только при INFO/DEBUG. */
#ifndef PURRGO_LOG_LED
#define PURRGO_LOG_LED (PURRGO_LOG_LEVEL >= PURRGO_LOG_LEVEL_INFO)
#endif


/* ------------------------------------------------------------------------- */
/* Запись лога на SD (purrgo_logger_sd_*, см. purrgo/logger.h)                */
/* ------------------------------------------------------------------------- */

#define SD_LOG_DIR           "0:/PURRGO/LOGS"
#define SD_LOG_PATH          "0:/PURRGO/LOGS/LOG.TXT"
#define SD_LOG_OLD_PATH      "0:/PURRGO/LOGS/LOG.OLD"

#define SD_LOG_RING_SIZE     4096U        /* кольцо сообщений между вызовами сервиса */
#define SD_LOG_CHUNK         256U         /* порция записи в файл */
#define SD_LOG_FLUSH_MS      2000U        /* сбрасывать на карту не реже, чем раз в 2 с */
#define SD_LOG_FLUSH_FILL    (SD_LOG_RING_SIZE / 2U)   /* и раньше, если кольцо заполнено наполовину */
#define SD_LOG_SYNC_MS       10000U       /* физическая запись (f_sync) не чаще, чем раз в 10 с */
#define SD_LOG_RETRY_MS      5000U        /* повтор открытия, если карты нет или она не готова */
#define SD_LOG_MAX_BYTES     (2UL * 1024UL * 1024UL)   /* предел размера файла за сессию */

static bool sd_log_enabled = false;       /* настройка (меню, PURRGO.CFG) */
static bool sd_log_stopped = false;       /* предел размера: ждём повторного включения */
static bool sd_log_session = false;       /* LOG.TXT этой сессии уже создан: после сбоя карты дописываем в него */
static purrgo_file_t *sd_log_file = NULL;
static uint32_t sd_log_bytes = 0U;        /* записано в открытый файл */
static uint32_t sd_log_retry_ms = 0U;
static uint32_t sd_log_flush_ms = 0U;
static uint32_t sd_log_sync_ms = 0U;
static bool sd_log_unsynced = false;
static bool sd_log_in_service = false;    /* сообщения самого сервиса (ошибки fs) в кольцо не попадают */

static char sd_ring[SD_LOG_RING_SIZE];
static volatile uint32_t sd_ring_head = 0U;     /* куда пишет purrgo_logger_write */
static volatile uint32_t sd_ring_tail = 0U;     /* откуда читает сервис */
static volatile uint32_t sd_dropped = 0U;       /* сообщений потеряно из-за переполнения кольца */

static uint32_t sd_ring_fill(void)
{
    return (sd_ring_head - sd_ring_tail) % SD_LOG_RING_SIZE;
}

/*
 * Положить сообщение (с отметкой времени) в кольцо. Вызывается из purrgo_logger_write, возможно из
 * прерывания: индекс головы меняется под запретом прерываний. При нехватке места сообщение целиком
 * отбрасывается и учитывается (сервис напишет, сколько потеряно).
 */
static void sd_ring_push(const char *msg, uint32_t len)
{
    char prefix[24];
    const uint32_t tick = HAL_GetTick();
    const int pl = purrgo_snprintf(prefix, sizeof prefix, "[%lu.%03lu] ",
                                   (unsigned long)(tick / 1000U), (unsigned long)(tick % 1000U));
    const uint32_t plen = (pl > 0) ? (uint32_t)pl : 0U;

    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    const uint32_t free_bytes = (SD_LOG_RING_SIZE - 1U) - sd_ring_fill();

    if ((plen + len) > free_bytes)
    {
        sd_dropped++;
    }
    else
    {
        uint32_t head = sd_ring_head;

        for (uint32_t i = 0U; i < plen; i++)
        {
            sd_ring[head] = prefix[i];
            head = (head + 1U) % SD_LOG_RING_SIZE;
        }
        for (uint32_t i = 0U; i < len; i++)
        {
            sd_ring[head] = msg[i];
            head = (head + 1U) % SD_LOG_RING_SIZE;
        }
        sd_ring_head = head;
    }

    if (primask == 0U)
    {
        __enable_irq();
    }
}

/* Забрать из кольца до size байт; возвращает число байт. */
static uint32_t sd_ring_pop(char *out, uint32_t size)
{
    uint32_t n = 0U;

    while ((n < size) && (sd_ring_tail != sd_ring_head))
    {
        out[n++] = sd_ring[sd_ring_tail];
        sd_ring_tail = (sd_ring_tail + 1U) % SD_LOG_RING_SIZE;
    }
    return n;
}

static void sd_log_close(void)
{
    if (sd_log_file != NULL)
    {
        (void)purrgo_fs_close(sd_log_file);
        sd_log_file = NULL;
    }
    sd_log_unsynced = false;
}

/*
 * Закрыть файл после сбоя. permanent - до повторного включения (предел размера); иначе (ошибка записи,
 * карту вынули) запись повторится через SD_LOG_RETRY_MS: файл откроется в режиме дозаписи, когда
 * каталог на карте снова доступен.
 */
static void sd_log_fail(const char *why, bool permanent, uint32_t now_ms)
{
    sd_log_close();
    if (permanent)
    {
        sd_log_stopped = true;
    }
    else
    {
        sd_log_retry_ms = now_ms + SD_LOG_RETRY_MS;
    }
    /* Сообщение только в UART: кольцо во время сервиса не пополняется. */
    purrgo_logger_write("SD LOG: %s (%s)\r\n", permanent ? "stopped" : "write failed, will retry", why);
}

static bool sd_log_write(const char *data, uint32_t len)
{
    if ((sd_log_file == NULL) || (len == 0U))
    {
        return true;
    }
    if (purrgo_fs_write(sd_log_file, (const uint8_t *)data, len) != len)
    {
        return false;
    }
    sd_log_bytes += len;
    sd_log_unsynced = true;
    return true;
}

/* Открыть файл: прошлый LOG.TXT становится LOG.OLD. false - попробовать позже. */
static bool sd_log_open(uint32_t now_ms)
{
    /* Каталога нет - карта не смонтирована (проверка тихая, без записи об ошибке в лог). */
    if (!purrgo_fs_exists(SD_LOG_DIR))
    {
        sd_log_retry_ms = now_ms + SD_LOG_RETRY_MS;
        return false;
    }

    /*
     * Первое открытие после включения: прошлый LOG.TXT становится LOG.OLD, создаётся новый. После
     * сбоя карты в той же сессии файл дописывается (FS_WRITE_APPEND), чтобы не потерять уже записанное.
     */
    if (!sd_log_session && purrgo_fs_exists(SD_LOG_PATH))
    {
        (void)purrgo_fs_replace(SD_LOG_PATH, SD_LOG_OLD_PATH);
    }

    sd_log_file = purrgo_fs_open(SD_LOG_PATH, sd_log_session ? FS_WRITE_APPEND : FS_WRITE_CREATE);
    if (sd_log_file == NULL)
    {
        sd_log_retry_ms = now_ms + SD_LOG_RETRY_MS;
        return false;
    }

    char head[64];
    const int n = purrgo_snprintf(head, sizeof head, "=== PURRGO log, uptime %lu ms%s ===\r\n",
                                  (unsigned long)now_ms, sd_log_session ? ", card back" : "");

    sd_log_session = true;
    sd_log_flush_ms = now_ms;
    sd_log_sync_ms = now_ms;
    if ((n > 0) && !sd_log_write(head, (uint32_t)n))
    {
        sd_log_fail("write error", false, now_ms);
        return false;
    }
    return true;
}

/*
 * Дописать накопленное, при необходимости добавить завершающую строку trailer, синхронизировать и закрыть
 * файл; запись выключается (настройка в PURRGO.CFG не меняется). Если файл не открыт (карты нет, ждём
 * повтор), накопленное просто отбрасывается.
 */
static void sd_log_finish(const char *trailer)
{
    sd_log_in_service = true;
    if (sd_log_file != NULL)
    {
        char chunk[SD_LOG_CHUNK];
        uint32_t n;
        bool ok = true;

        const uint32_t lost = sd_dropped;
        if (lost != 0U)
        {
            const int m = purrgo_snprintf(chunk, sizeof chunk, "[SD LOG: %lu messages dropped]\r\n",
                                          (unsigned long)lost);
            sd_dropped = 0U;
            ok = !((m > 0) && !sd_log_write(chunk, (uint32_t)m));
        }
        while (ok && ((n = sd_ring_pop(chunk, sizeof chunk)) != 0U))
        {
            ok = sd_log_write(chunk, n);
        }
        if (ok && (trailer != NULL))
        {
            (void)sd_log_write(trailer, (uint32_t)strlen(trailer));
        }
        (void)purrgo_fs_sync(sd_log_file);
    }
    sd_log_close();
    sd_log_in_service = false;
    sd_log_enabled = false;
    sd_ring_tail = sd_ring_head;
}

void purrgo_logger_sd_set_enabled(bool enabled)
{
    if (enabled == sd_log_enabled)
    {
        return;
    }

    if (enabled)
    {
        sd_log_enabled = true;
        sd_log_stopped = false;
        sd_log_session = false;
        sd_log_retry_ms = 0U;
        sd_dropped = 0U;
        sd_ring_tail = sd_ring_head;      /* накопленного раньше не писать */
        purrgo_logger_write("SD LOG: enabled\r\n");
    }
    else
    {
        sd_log_finish(NULL);
        purrgo_logger_write("SD LOG: disabled\r\n");
    }
}

void purrgo_logger_sd_shutdown(void)
{
    if (!sd_log_enabled)
    {
        return;
    }

    char line[64];
    const uint32_t tick = HAL_GetTick();
    const int n = purrgo_snprintf(line, sizeof line, "=== PURRGO power off, uptime %lu ms ===\r\n",
                                  (unsigned long)tick);
    sd_log_finish((n > 0) ? line : NULL);
}

bool purrgo_logger_sd_enabled(void)
{
    return sd_log_enabled;
}

void purrgo_logger_sd_service(uint32_t now_ms)
{
    if (!sd_log_enabled || sd_log_stopped)
    {
        return;
    }

    sd_log_in_service = true;

    if (sd_log_file == NULL)
    {
        if ((int32_t)(now_ms - sd_log_retry_ms) >= 0)
        {
            (void)sd_log_open(now_ms);
        }
        /* Пока карты нет, кольцо не растёт бесконечно: старое молча вытесняется. */
        if ((sd_log_file == NULL) && (sd_ring_fill() > (SD_LOG_RING_SIZE / 2U)))
        {
            char drop[SD_LOG_CHUNK];

            (void)sd_ring_pop(drop, sizeof drop);
        }
        sd_log_in_service = false;
        return;
    }

    if (((uint32_t)(now_ms - sd_log_flush_ms) >= SD_LOG_FLUSH_MS) || (sd_ring_fill() >= SD_LOG_FLUSH_FILL))
    {
        char chunk[SD_LOG_CHUNK];
        uint32_t n;

        sd_log_flush_ms = now_ms;

        const uint32_t lost = sd_dropped;
        if (lost != 0U)
        {
            const int m = purrgo_snprintf(chunk, sizeof chunk, "[SD LOG: %lu messages dropped]\r\n",
                                          (unsigned long)lost);
            sd_dropped = 0U;
            if ((m > 0) && !sd_log_write(chunk, (uint32_t)m))
            {
                sd_log_fail("write error", false, now_ms);
                sd_log_in_service = false;
                return;
            }
        }

        while ((n = sd_ring_pop(chunk, sizeof chunk)) != 0U)
        {
            if (!sd_log_write(chunk, n))
            {
                sd_log_fail("write error", false, now_ms);
                sd_log_in_service = false;
                return;
            }
            if (sd_log_bytes >= SD_LOG_MAX_BYTES)
            {
                sd_log_fail("size limit", true, now_ms);
                sd_log_in_service = false;
                return;
            }
        }
    }

    if (sd_log_unsynced && ((uint32_t)(now_ms - sd_log_sync_ms) >= SD_LOG_SYNC_MS))
    {
        sd_log_sync_ms = now_ms;
        if (!purrgo_fs_sync(sd_log_file))
        {
            sd_log_fail("sync error", false, now_ms);
        }
        else
        {
            sd_log_unsynced = false;
        }
    }

    sd_log_in_service = false;
}

void purrgo_logger_init(void)
{
    /*
     * USART1 уже инициализируется в main():
     *
     *     MX_USART1_UART_Init();
     *
     * Поэтому повторно настраивать UART здесь нельзя и не требуется.
     *
     * Функция пока является пустой. Она существует для того, чтобы
     * у платформенного логгера была собственная точка инициализации,
     * если она понадобится позже.
     */
}


void purrgo_logger_write(const char *format, ...)
{
    /*
     * Локальный буфер сообщения.
     *
     * Он находится в стеке текущего вызова функции и после выхода
     * из неё больше не используется.
     */
    char buffer[PURRGO_LOG_BUFFER_SIZE];

    va_list args;

    /*
     * Инициализируем обработку переменного количества аргументов.
     */
    va_start(args, format);

    /*
     * Формируем строку аналогично printf().
     *
     * purrgo_vsnprintf() гарантирует, что в buffer не будет записано
     * больше sizeof(buffer) байт с учётом завершающего '\0'.
     */
    int length = purrgo_vsnprintf(
        buffer,
        sizeof(buffer),
        format,
        args
    );

    /*
     * Завершаем обработку переменных аргументов.
     */
    va_end(args);

    /*
     * Ошибка форматирования.
     *
     * В этом случае передавать данные в UART нельзя.
     */
    if (length < 0)
    {
        return;
    }

    /*
     * Если сформированная строка не помещается в буфер,
     * purrgo_vsnprintf() возвращает требуемую длину, не включая '\0'.
     *
     * Мы передадим только реально записанную часть буфера.
     */
    if ((size_t)length >= sizeof(buffer))
    {
        length = (int)(sizeof(buffer) - 1U);
    }

    /*
     * Передаём сообщение через USART1.
     *
     * huart1 объявлен в usart.c и доступен через usart.h.
     *
     * HAL_UART_Transmit() в данном варианте работает блокирующе:
     * функция не возвращается, пока передача не завершится
     * или HAL не сообщит об ошибке.
     *
     * Для первоначальной диагностической версии это допустимо:
     * сейчас нам важнее надёжно увидеть сообщения при запуске
     * микроконтроллера.
     */
    (void)HAL_UART_Transmit(
        &huart1,
        (uint8_t *)buffer,
        (uint16_t)length,
        100U
    );

    /* Копия в кольцо для записи на SD (не во время самого сервиса: иначе его сообщения зациклились бы). */
    if (sd_log_enabled && !sd_log_in_service && !sd_log_stopped)
    {
        sd_ring_push(buffer, (uint32_t)length);
    }

#if PURRGO_LOG_LED
    /*
     * Индикация активности: Alive_LED (PC13) переключается после каждого
     * сообщения. Только для отладки (PURRGO_LOG_LED, по умолчанию - при уровне
     * INFO и выше): светодиод потребляет ток и мигает на каждое сообщение.
     */
    HAL_GPIO_TogglePin(Alive_LED_GPIO_Port, Alive_LED_Pin);
#endif
}
