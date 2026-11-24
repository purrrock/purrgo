/*
 * Аппаратный тест GNSS_BBR (-DPURRGO_HW_TEST=GNSS_BBR, см. hw_test.h).
 *
 * Вопрос: что приёмник G10A-F30 (u-blox M10) сохраняет между фиксами
 *   (1) при выключении по EN = Low на 15 с (идея "выключать между точками");
 *   (2) в режиме PSMOO (состояние "off" между фиксами).
 * Сохраняется ли слой конфигурации BBR, часы, эфемериды и альманах (то есть возможен ли горячий старт).
 *
 * Ход теста (навигатор не запускается; результаты - в UART1 строками "HWTEST GNSS_BBR ..." и на дисплее):
 *
 *   0. Связь. Приёмник опрашивается UBX-NAV-STATUS на 38400 и на 115200 бод (в BBR мог остаться след
 *      прошивки). Состояние настройки читается и чистится от следов прошлых запусков.
 *   1. ACQ. Ожидание первого фикса (PURRGO_HWTEST_GNSS_ACQ_TIMEOUT_S), затем выдержка, чтобы приёмник
 *      набрал эфемериды. Снимок "BASE": msss, эфемериды/альманах (UBX-NAV-ORB), маркер.
 *   2. EN. PURRGO_HWTEST_GNSS_EN_CYCLES раз: в RAM и BBR пишется настройка "NMEA выключен, NAV-PVT и NAV-STATUS
 *      включены" и маркер (CFG-ITFM-ENABLE = 1, по умолчанию 0), затем EN = Low на PURRGO_HWTEST_GNSS_EN_OFF_S и
 *      EN = High. Первые 2.5 с после включения тест только слушает (ничего не отправляет): если пошли UBX PVT/STATUS и
 *      нет NMEA, настройка пережила выключение; если пошёл заводской NMEA, потеряна. Затем снимок: маркер в слоях
 *      BBR и RAM (UBX-CFG-VALGET), msss (миллисекунды с запуска/сброса приёмника: если меньше, чем до выключения,
 *      питание ядра пропадало), число спутников с эфемеридами и альманахом (UBX-NAV-ORB), достоверность времени.
 *      Время до фикса берётся из момента приёма первого PVT с фиксом (hot start около 1-3 с, cold start 25-30 с).
 *      Результат каждого пункта: yes / NO / ? (не удалось определить, например приёмник не ответил на опрос).
 *   3. PSMOO. Набор систем как в рабочей прошивке (SBAS, GLONASS и BeiDou B1C выключены, B1I включён; после смены
 *      ГНСС перезапускается, тест ждёт фикс), затем вывод UBX-NAV-PVT / NAV-STATUS и параметры PSMOO в RAM и BBR
 *      (как у трекера) и режим PSMOO последним ключом. При отказе (NAK) на режим тест сам ищет причину: пробует
 *      только RAM и, как контроль, PSMCT в RAM (принимается ли PSM вообще).
 *      Наблюдение пассивное (тест ничего не отправляет, любая передача будит приёмник): по потоку PVT/STATUS на
 *      каждом всплеске определяются интервал между началами всплесков, длительность работы, msss первого STATUS
 *      (малое значение - ядро стартовало заново после "off"), ttff, ошибки UART и возврат NMEA (настройка потеряна).
 *      В конце один опрос проверяет маркер и режим PSMOO в BBR. Тишина дольше периода будит приёмник опросом.
 *   4. Откат. Из BBR удаляются все ключи теста, в RAM возвращается полный режим.
 *
 * Настройки (-D при сборке, значения по умолчанию ниже):
 *   PURRGO_HWTEST_GNSS_PHASES            1 = только EN, 2 = только PSMOO, 3 = обе (по умолчанию 3)
 *   PURRGO_HWTEST_GNSS_ACQ_TIMEOUT_S     300
 *   PURRGO_HWTEST_GNSS_ACQ_HOLD_S        20     выдержка после первого фикса
 *   PURRGO_HWTEST_GNSS_EN_CYCLES         3
 *   PURRGO_HWTEST_GNSS_EN_OFF_S          15
 *   PURRGO_HWTEST_GNSS_EN_FIX_TIMEOUT_S  90
 *   PURRGO_HWTEST_GNSS_PSMOO_PERIOD_S    15
 *   PURRGO_HWTEST_GNSS_PSMOO_ONTIME_S    2
 *   PURRGO_HWTEST_GNSS_HOT_MS            15000  фикс не позже этого после EN High = горячий старт
 *   PURRGO_HWTEST_GNSS_PSMOO_CYCLES      6      сколько всплесков наблюдать
 *   PURRGO_HWTEST_GNSS_PSMOO_BAUD        0      115200: скорость модуля пишется в RAM и BBR (как в рабочей прошивке)
 *
 * Тест нужно запускать на улице (или у окна с хорошим небом): без фикса сравнение эфемерид и времени фикса
 * теряет смысл (тест об этом напишет).
 */

#include "hw_test.h"

#if defined(PURRGO_HW_TEST_ENABLED) && defined(PURRGO_HW_TEST_GNSS_BBR)

#include "main.h"
#include "usart.h"
#include "gnss_raw.h"
#include "display_stm32.h"
#include "purrgo/display_hal.h"
#include "purrgo/gfx_renderer.h"
#include "purrgo/gfx_text.h"
#include "purrgo/gnss_io.h"
#include "purrgo/gnss_receiver.h"
#include "purrgo/gnss_ubx.h"
#include "purrgo/logger.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef PURRGO_HWTEST_GNSS_PHASES
#define PURRGO_HWTEST_GNSS_PHASES 3
#endif
#ifndef PURRGO_HWTEST_GNSS_ACQ_TIMEOUT_S
#define PURRGO_HWTEST_GNSS_ACQ_TIMEOUT_S 300U
#endif
#ifndef PURRGO_HWTEST_GNSS_ACQ_HOLD_S
#define PURRGO_HWTEST_GNSS_ACQ_HOLD_S 20U
#endif
#ifndef PURRGO_HWTEST_GNSS_EN_CYCLES
#define PURRGO_HWTEST_GNSS_EN_CYCLES 3U
#endif
#ifndef PURRGO_HWTEST_GNSS_EN_OFF_S
#define PURRGO_HWTEST_GNSS_EN_OFF_S 15U
#endif
#ifndef PURRGO_HWTEST_GNSS_EN_FIX_TIMEOUT_S
#define PURRGO_HWTEST_GNSS_EN_FIX_TIMEOUT_S 90U
#endif
#ifndef PURRGO_HWTEST_GNSS_PSMOO_PERIOD_S
#define PURRGO_HWTEST_GNSS_PSMOO_PERIOD_S 15U
#endif
#ifndef PURRGO_HWTEST_GNSS_PSMOO_ONTIME_S
#define PURRGO_HWTEST_GNSS_PSMOO_ONTIME_S 2U
#endif
#ifndef PURRGO_HWTEST_GNSS_PSMOO_CYCLES
#define PURRGO_HWTEST_GNSS_PSMOO_CYCLES 6U
#endif
/* Фикс не позже стольких мс после EN High считается горячим стартом (холодный старт при том же небе - 60-80 с). */
#ifndef PURRGO_HWTEST_GNSS_HOT_MS
#define PURRGO_HWTEST_GNSS_HOT_MS 15000U
#endif
/* 0 - скорость модуля не меняется (38400); 115200 - как в рабочей прошивке: скорость пишется в RAM и BBR. */
#ifndef PURRGO_HWTEST_GNSS_PSMOO_BAUD
#define PURRGO_HWTEST_GNSS_PSMOO_BAUD 0U
#endif
#if (PURRGO_HWTEST_GNSS_PSMOO_BAUD != 0U) && (PURRGO_HWTEST_GNSS_PSMOO_BAUD != 115200U)
#error "PURRGO_HWTEST_GNSS_PSMOO_BAUD must be 0 or 115200"
#endif

#define HT(...) purrgo_logger_write(__VA_ARGS__)

/* ------------------------------------------------------------------------- */
/* Константы протокола                                                        */
/* ------------------------------------------------------------------------- */

#define CLS_CFG        0x06U
#define ID_VALSET      0x8AU
#define ID_VALGET      0x8BU
#define ID_VALDEL      0x8CU
#define ID_NAV_ORB     0x34U

#define LAYER_RAM      0x01U
#define LAYER_BBR      0x02U
#define LAYER_FLASH    0x04U
#define LAYER_RAM_BBR  (LAYER_RAM | LAYER_BBR)

/* Ключи CFG (UBX-M10 SPG 5.10 Interface Description). */
#define KEY_MARKER         0x1041000DU   /* CFG-ITFM-ENABLE, L: по умолчанию 0; маркер "BBR жив" */
#define KEY_NMEA_UART1     0x10740002U   /* CFG-UART1OUTPROT-NMEA, L */
#define KEY_SBAS           0x10310020U   /* CFG-SIGNAL-SBAS_ENA, L: по умолчанию 1; для PSM нужно 0 */
#define KEY_CANARY         0x20410002U   /* CFG-ITFM-CWTHRESHOLD, U1: по умолчанию 15; "канарейка" только для слоя RAM */
#define CANARY_VALUE       14U
#define CANARY_DEFAULT     15U
#define KEY_UART_BAUD      0x40520001U   /* CFG-UART1-BAUDRATE, U4 */
#define KEY_SIG_GLO        0x10310025U   /* CFG-SIGNAL-GLO_ENA, L: по умолчанию 0 */
#define KEY_SIG_BDS        0x10310022U   /* CFG-SIGNAL-BDS_ENA, L: по умолчанию 0 */
#define KEY_SIG_BDS_B1     0x1031000DU   /* CFG-SIGNAL-BDS_B1_ENA (B1I), L: по умолчанию 0 */
#define KEY_SIG_BDS_B1C    0x1031000FU   /* CFG-SIGNAL-BDS_B1C_ENA, L: по умолчанию 1; в PSM не поддерживается */
#define KEY_MSGOUT_PVT     0x20910007U   /* CFG-MSGOUT-UBX_NAV_PVT_UART1, U1 */
#define KEY_MSGOUT_STATUS  0x2091001BU   /* CFG-MSGOUT-UBX_NAV_STATUS_UART1, U1 */
#define KEY_PM_OPMODE      0x20D00001U   /* CFG-PM-OPERATEMODE, E1: 0 FULL, 1 PSMOO, 2 PSMCT */
#define KEY_PM_NOOFF       0x10D00008U   /* CFG-PM-DONOTENTEROFF, L */
#define KEY_PM_POSPERIOD   0x40D00002U   /* CFG-PM-POSUPDATEPERIOD, U4 */
#define KEY_PM_ACQPERIOD   0x40D00003U   /* CFG-PM-ACQPERIOD, U4 */
#define KEY_PM_ONTIME      0x30D00005U   /* CFG-PM-ONTIME, U2 */

#define BAUD_FACTORY       38400U
#define BAUD_ALT           115200U

/* ------------------------------------------------------------------------- */
/* Приём и разбор UBX                                                         */
/* ------------------------------------------------------------------------- */

static purrgo_ubx_parser_t s_parser;

/* Последние значения и счётчики сообщений (счётчик растёт на каждое принятое сообщение). */
static volatile uint32_t s_seq_status;
static volatile uint32_t s_seq_pvt;
static volatile uint32_t s_seq_orb;
static volatile uint32_t s_seq_valget, s_nak_valget;
static volatile uint32_t s_ack_valset, s_nak_valset;
static volatile uint32_t s_ack_valdel, s_nak_valdel;

static uint32_t s_nmea_dollars;     /* сколько раз принят '$' (начало NMEA-предложения) */
static uint32_t s_bytes;            /* принято байт с последнего сброса */
static uint32_t s_t_first_byte;     /* HAL_GetTick() первого принятого байта (0 - не было) */
static uint32_t s_t_first_fix;      /* HAL_GetTick() первого PVT с фиксом (0 - не было) */

static struct {
    uint32_t msss, ttff;
    uint8_t  gps_fix, flags;
} s_st;

static struct {
    uint8_t  fix_type, num_sv, valid, psm;
    bool     fix_ok;
    uint16_t year;
    uint8_t  mon, day, hour, min, sec;
    uint32_t hacc_mm;
} s_pvt;

static struct {
    int sv, eph, alm;
} s_orb;

#define VG_MAX 8U
static struct {
    uint8_t  layer;
    unsigned n;
    uint32_t key[VG_MAX];
    uint32_t val[VG_MAX];
} s_vg;

static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | (rd16(p + 2) << 16); }

/* Размер значения по ключу (биты 30..28): 1 и 2 - один байт, 3 - два, 4 - четыре; 0 - неподдерживаемый. */
static unsigned key_size(uint32_t key)
{
    switch ((key >> 28) & 0x7U)
    {
    case 1U: case 2U: return 1U;
    case 3U:          return 2U;
    case 4U:          return 4U;
    default:          return 0U;
    }
}

static void on_frame(void)
{
    const uint8_t *p = s_parser.payload;
    const unsigned n = s_parser.length;
    const uint8_t cls = s_parser.msg_class;
    const uint8_t id = s_parser.msg_id;

    if ((cls == PURRGO_UBX_CLASS_NAV) && (id == PURRGO_UBX_ID_NAV_STATUS) && (n >= 16U))
    {
        s_st.gps_fix = p[4];
        s_st.flags = p[5];
        s_st.ttff = rd32(&p[8]);
        s_st.msss = rd32(&p[12]);
        s_seq_status++;
    }
    else if ((cls == PURRGO_UBX_CLASS_NAV) && (id == PURRGO_UBX_ID_NAV_PVT) && (n >= 92U))
    {
        s_pvt.year = (uint16_t)rd16(&p[4]);
        s_pvt.mon = p[6];
        s_pvt.day = p[7];
        s_pvt.hour = p[8];
        s_pvt.min = p[9];
        s_pvt.sec = p[10];
        s_pvt.valid = p[11];
        s_pvt.fix_type = p[20];
        s_pvt.fix_ok = ((p[21] & 0x01U) != 0U);
        s_pvt.psm = (uint8_t)((p[21] >> 2) & 0x07U);
        s_pvt.num_sv = p[23];
        s_pvt.hacc_mm = rd32(&p[40]);
        if (s_pvt.fix_ok && (s_pvt.fix_type >= 2U) && (s_t_first_fix == 0U))
        {
            s_t_first_fix = HAL_GetTick();
        }
        s_seq_pvt++;
    }
    else if ((cls == PURRGO_UBX_CLASS_NAV) && (id == ID_NAV_ORB) && (n >= 8U))
    {
        unsigned sv = p[5];
        int eph = 0;
        int alm = 0;

        if (sv > (n - 8U) / 6U)
        {
            sv = (n - 8U) / 6U;
        }
        for (unsigned i = 0U; i < sv; i++)
        {
            const uint8_t e = p[8U + (6U * i) + 3U];
            const uint8_t a = p[8U + (6U * i) + 4U];

            /* Источник известен (биты 7..5 != 0) и данные ещё пригодны (биты 4..0 != 0). */
            if ((((e >> 5) & 0x07U) != 0U) && ((e & 0x1FU) != 0U)) { eph++; }
            if ((((a >> 5) & 0x07U) != 0U) && ((a & 0x1FU) != 0U)) { alm++; }
        }
        s_orb.sv = (int)sv;
        s_orb.eph = eph;
        s_orb.alm = alm;
        s_seq_orb++;
    }
    else if ((cls == CLS_CFG) && (id == ID_VALGET) && (n >= 4U))
    {
        unsigned off = 4U;

        s_vg.layer = p[1];
        s_vg.n = 0U;
        while ((off + 4U) <= n)
        {
            const uint32_t key = rd32(&p[off]);
            const unsigned sz = key_size(key);
            uint32_t val = 0U;

            if ((sz == 0U) || ((off + 4U + sz) > n))
            {
                break;
            }
            for (unsigned i = 0U; i < sz; i++)
            {
                val |= (uint32_t)p[off + 4U + i] << (8U * i);
            }
            if (s_vg.n < VG_MAX)
            {
                s_vg.key[s_vg.n] = key;
                s_vg.val[s_vg.n] = val;
                s_vg.n++;
            }
            off += 4U + sz;
        }
        s_seq_valget++;
    }
    else if ((cls == PURRGO_UBX_CLASS_ACK) && (n >= 2U) &&
             ((id == PURRGO_UBX_ID_ACK_ACK) || (id == PURRGO_UBX_ID_ACK_NAK)) && (p[0] == CLS_CFG))
    {
        const bool ack = (id == PURRGO_UBX_ID_ACK_ACK);

        switch (p[1])
        {
        case ID_VALSET: if (ack) { s_ack_valset++; } else { s_nak_valset++; } break;
        case ID_VALDEL: if (ack) { s_ack_valdel++; } else { s_nak_valdel++; } break;
        case ID_VALGET: if (!ack) { s_nak_valget++; } break;
        default: break;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Экран                                                                      */
/* ------------------------------------------------------------------------- */

/* Шрифт 5x7: шаг 6 px по горизонтали, строки через 9 px: на панели 176 x 264 помещается 28 x 29. */
#define SCR_COLS      28U
#define SCR_HEAD_ROWS 6U
#define SCR_RES_ROWS  21U
#define SCR_ROW_STEP  9
#define SCR_MIN_MS    6000U   /* не чаще (полное обновление E-Ink длится секунды) */

static gfx_context_t s_gfx;
static char s_head[SCR_HEAD_ROWS][SCR_COLS + 1U];
static char s_res[SCR_RES_ROWS][SCR_COLS + 1U];
static bool s_scr_dirty;
static bool s_scr_ready;
static uint32_t s_t_scr;

static void head_set(unsigned row, const char *fmt, ...)
{
    va_list ap;

    if (row >= SCR_HEAD_ROWS)
    {
        return;
    }
    va_start(ap, fmt);
    (void)vsnprintf(s_head[row], sizeof s_head[row], fmt, ap);
    va_end(ap);
    s_scr_dirty = true;
}

/* Новая строка в список результатов (старые уходят вверх). */
static void res_add(const char *fmt, ...)
{
    va_list ap;

    for (unsigned i = 1U; i < SCR_RES_ROWS; i++)
    {
        memcpy(s_res[i - 1U], s_res[i], sizeof s_res[i]);
    }
    va_start(ap, fmt);
    (void)vsnprintf(s_res[SCR_RES_ROWS - 1U], sizeof s_res[0], fmt, ap);
    va_end(ap);
    s_scr_dirty = true;
}

static void scr_refresh(bool force)
{
    const uint32_t now = HAL_GetTick();

    if (!s_scr_ready || !s_scr_dirty)
    {
        return;
    }
    if (!force && ((uint32_t)(now - s_t_scr) < SCR_MIN_MS))
    {
        return;
    }
    if (!display_stm32_ready_for_render())
    {
        return;
    }

    gfx_set_color(&s_gfx, COLOR_BLACK, COLOR_WHITE);
    gfx_clear(&s_gfx);
    for (unsigned i = 0U; i < SCR_HEAD_ROWS; i++)
    {
        gfx_draw_string(&s_gfx, 2, (int16_t)(2 + ((int)i * SCR_ROW_STEP)), s_head[i]);
    }
    for (unsigned i = 0U; i < SCR_RES_ROWS; i++)
    {
        gfx_draw_string(&s_gfx, 2, (int16_t)(2 + (((int)SCR_HEAD_ROWS + 1 + (int)i) * SCR_ROW_STEP)), s_res[i]);
    }
    display_refresh_region(0, 0, (int16_t)DISPLAY_WIDTH, (int16_t)DISPLAY_HEIGHT);
    display_flush();

    s_t_scr = now;
    s_scr_dirty = false;
}

static void scr_init(void)
{
    display_init();
    if (gfx_init(&s_gfx, DISPLAY_WIDTH, DISPLAY_HEIGHT, (void *)display_get_framebuffer(),
                 stm32_draw_pixel_cb, stm32_read_pixel_cb))
    {
        gfx_set_clear_callback(&s_gfx, stm32_clear_cb);
        gfx_set_fill_span_callback(&s_gfx, stm32_fill_span_cb);
        display_set_mode(DISPLAY_MODE_MONO);
        s_scr_ready = true;
    }
    else
    {
        HT("HWTEST GNSS_BBR: gfx_init failed, no display output\r\n");
    }
}

/* ------------------------------------------------------------------------- */
/* Ожидание, отправка, запросы                                                */
/* ------------------------------------------------------------------------- */

static void pump(void)
{
    uint8_t b;

    while (purrgo_gnss_read_byte(&b))
    {
        if (s_bytes == 0U)
        {
            s_t_first_byte = HAL_GetTick();
        }
        s_bytes++;

        const purrgo_ubx_feed_t r = purrgo_ubx_parser_feed(&s_parser, b);

        if (r == PURRGO_UBX_FEED_FRAME)
        {
            on_frame();
        }
        else if ((r == PURRGO_UBX_FEED_NOT_UBX) && (b == (uint8_t)'$'))
        {
            s_nmea_dollars++;
        }
    }
    display_stm32_poll();
}

/* Один шаг ожидания: разбор входа, дисплей, пауза ~1 мс. */
static void idle_step(void)
{
    pump();
    scr_refresh(false);
    HAL_Delay(1);
}

static void delay_ms(uint32_t ms)
{
    const uint32_t t0 = HAL_GetTick();

    while ((uint32_t)(HAL_GetTick() - t0) < ms)
    {
        idle_step();
    }
}

static bool tx_raw(const uint8_t *data, size_t len)
{
    return HAL_UART_Transmit(&hlpuart1, (uint8_t *)(uintptr_t)data, (uint16_t)len, 1000U) == HAL_OK;
}

static bool tx_msg(uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len)
{
    uint8_t frame[96];
    const size_t n = purrgo_ubx_build_frame(frame, sizeof frame, cls, id, payload, len);

    return (n != 0U) && tx_raw(frame, n);
}

/* Дождаться изменения счётчика *seq (нового сообщения) не дольше timeout_ms. */
static bool wait_change(const volatile uint32_t *seq, uint32_t old, uint32_t timeout_ms)
{
    const uint32_t t0 = HAL_GetTick();

    while ((*seq == old) && ((uint32_t)(HAL_GetTick() - t0) < timeout_ms))
    {
        idle_step();
    }
    return (*seq != old);
}

static bool poll_msg(uint8_t cls, uint8_t id, const volatile uint32_t *seq, uint32_t timeout_ms)
{
    const uint32_t old = *seq;

    if (!tx_msg(cls, id, NULL, 0U))
    {
        return false;
    }
    return wait_change(seq, old, timeout_ms);
}

/*
 * Запрос с повторами. Ответ на опрос может стоять в очереди за NMEA на 38400 бод (секунда и больше), поэтому
 * таймаут щедрый. Перед повтором разборщик сбрасывается: оборванный кадр не должен съесть следующий.
 */
static bool poll_retry(uint8_t cls, uint8_t id, const volatile uint32_t *seq, uint32_t per_try_ms, unsigned tries)
{
    for (unsigned i = 0U; i < tries; i++)
    {
        if (i != 0U)
        {
            purrgo_ubx_parser_init(&s_parser);
        }
        if (poll_msg(cls, id, seq, per_try_ms))
        {
            return true;
        }
    }
    return false;
}

/* UBX-CFG-VALGET: 1 - ответ получен, 0 - NAK, -1 - тишина. vals[i] = значение или -1 (ключа нет в слое). */
static int valget(uint8_t layer, const uint32_t *keys, unsigned count, int32_t *vals)
{
    uint8_t pl[4U + (4U * 4U)];

    if ((count == 0U) || (count > 4U))
    {
        return -1;
    }
    pl[0] = 0U;
    pl[1] = layer;
    pl[2] = 0U;
    pl[3] = 0U;
    for (unsigned i = 0U; i < count; i++)
    {
        pl[4U + (4U * i) + 0U] = (uint8_t)(keys[i] & 0xFFU);
        pl[4U + (4U * i) + 1U] = (uint8_t)((keys[i] >> 8) & 0xFFU);
        pl[4U + (4U * i) + 2U] = (uint8_t)((keys[i] >> 16) & 0xFFU);
        pl[4U + (4U * i) + 3U] = (uint8_t)((keys[i] >> 24) & 0xFFU);
        vals[i] = -1;
    }

    const uint32_t old_reply = s_seq_valget;
    const uint32_t old_nak = s_nak_valget;
    const uint32_t t0 = HAL_GetTick();

    if (!tx_msg(CLS_CFG, ID_VALGET, pl, (uint16_t)(4U + (4U * count))))
    {
        return -1;
    }
    while ((s_seq_valget == old_reply) && (s_nak_valget == old_nak) &&
           ((uint32_t)(HAL_GetTick() - t0) < 1500U))
    {
        idle_step();
    }

    if (s_seq_valget != old_reply)
    {
        for (unsigned i = 0U; i < count; i++)
        {
            for (unsigned j = 0U; j < s_vg.n; j++)
            {
                if (s_vg.key[j] == keys[i])
                {
                    vals[i] = (int32_t)s_vg.val[j];
                }
            }
        }
        return 1;
    }
    return (s_nak_valget != old_nak) ? 0 : -1;
}

/* Отправить кадр настройки и дождаться ACK/NAK: 1 ACK, 0 NAK, -1 тишина. */
static int cfg_wait(const uint8_t *frame, size_t len, volatile uint32_t *ack, volatile uint32_t *nak)
{
    const uint32_t old_ack = *ack;
    const uint32_t old_nak = *nak;
    const uint32_t t0 = HAL_GetTick();

    if ((len == 0U) || !tx_raw(frame, len))
    {
        return -1;
    }
    while ((*ack == old_ack) && (*nak == old_nak) && ((uint32_t)(HAL_GetTick() - t0) < 1000U))
    {
        idle_step();
    }
    if (*ack != old_ack) { return 1; }
    return (*nak != old_nak) ? 0 : -1;
}

static int valset(uint8_t layers, const purrgo_ubx_cfg_item_t *items, size_t count)
{
    uint8_t frame[128];
    const size_t n = purrgo_ubx_build_valset(frame, sizeof frame, layers, items, count);

    return cfg_wait(frame, n, &s_ack_valset, &s_nak_valset);
}

static int valdel_bbr(const uint32_t *keys, size_t count)
{
    uint8_t frame[96];
    const size_t n = purrgo_ubx_build_valdel(frame, sizeof frame, LAYER_BBR, keys, count);

    return cfg_wait(frame, n, &s_ack_valdel, &s_nak_valdel);
}

static const char *rc_text(int rc)
{
    return (rc > 0) ? "ACK" : ((rc == 0) ? "NAK" : "NONE");
}

/* ------------------------------------------------------------------------- */
/* Снимок состояния приёмника                                                 */
/* ------------------------------------------------------------------------- */

typedef struct {
    bool     alive;                 /* ответил на UBX-NAV-STATUS */
    uint32_t msss, ttff;
    bool     pvt;
    uint8_t  fix_type, num_sv, valid;
    bool     fix_ok;
    uint16_t year;
    uint8_t  mon, day, hour, min, sec;
    bool     orb;
    int      orb_sv, eph, alm;
    int32_t  bbr[3], ram[3];        /* [0] маркер, [1] OPERATEMODE, [2] канарейка; -1 - ключа нет в слое */
    int      bbr_rc, ram_rc;        /* результат VALGET: 1 ответ, 0 NAK, -1 тишина */
} snap_t;

static const uint32_t k_check_keys[3] = { KEY_MARKER, KEY_PM_OPMODE, KEY_CANARY };

static void take_snapshot(snap_t *s)
{
    memset(s, 0, sizeof *s);
    s->bbr[0] = s->bbr[1] = s->bbr[2] = s->ram[0] = s->ram[1] = s->ram[2] = -1;
    s->bbr_rc = s->ram_rc = -1;

    s->alive = poll_retry(PURRGO_UBX_CLASS_NAV, PURRGO_UBX_ID_NAV_STATUS, &s_seq_status, 1500U, 3U);
    if (!s->alive)
    {
        return;
    }
    s->msss = s_st.msss;
    s->ttff = s_st.ttff;

    if (poll_retry(PURRGO_UBX_CLASS_NAV, PURRGO_UBX_ID_NAV_PVT, &s_seq_pvt, 1500U, 2U))
    {
        s->pvt = true;
        s->fix_type = s_pvt.fix_type;
        s->fix_ok = s_pvt.fix_ok;
        s->num_sv = s_pvt.num_sv;
        s->valid = s_pvt.valid;
        s->year = s_pvt.year;
        s->mon = s_pvt.mon;
        s->day = s_pvt.day;
        s->hour = s_pvt.hour;
        s->min = s_pvt.min;
        s->sec = s_pvt.sec;
    }
    if (poll_retry(PURRGO_UBX_CLASS_NAV, ID_NAV_ORB, &s_seq_orb, 2500U, 2U))
    {
        s->orb = true;
        s->orb_sv = s_orb.sv;
        s->eph = s_orb.eph;
        s->alm = s_orb.alm;
    }
    s->bbr_rc = valget(0x01U, k_check_keys, 3U, s->bbr);   /* слой BBR */
    if (s->bbr_rc < 0)
    {
        s->bbr_rc = valget(0x01U, k_check_keys, 3U, s->bbr);
    }
    s->ram_rc = valget(0x00U, k_check_keys, 3U, s->ram);   /* слой RAM */
    if (s->ram_rc < 0)
    {
        s->ram_rc = valget(0x00U, k_check_keys, 3U, s->ram);
    }
}

static void log_snap(const char *tag, const snap_t *s)
{
    if (!s->alive)
    {
        HT("HWTEST GNSS_BBR %s: module SILENT (no reply to UBX-NAV-STATUS)\r\n", tag);
        return;
    }
    HT("HWTEST GNSS_BBR %s: msss=%lu ms, rx-ttff=%lu ms, fix=%u ok=%u sv=%u, time valid=0x%02X"
       " %04u-%02u-%02u %02u:%02u:%02u\r\n",
       tag, (unsigned long)s->msss, (unsigned long)s->ttff, (unsigned)s->fix_type, (unsigned)s->fix_ok,
       (unsigned)s->num_sv, (unsigned)s->valid, (unsigned)s->year, (unsigned)s->mon, (unsigned)s->day,
       (unsigned)s->hour, (unsigned)s->min, (unsigned)s->sec);
    HT("HWTEST GNSS_BBR %s: orbit db: %d SV, eph usable %d, alm usable %d%s | marker BBR=%ld RAM=%ld"
       " | OPERATEMODE BBR=%ld RAM=%ld | RAM canary=%ld (set %u, default %u) | VALGET rc BBR=%s RAM=%s"
       " (-1 = key absent in layer)\r\n",
       tag, s->orb_sv, s->eph, s->alm, s->orb ? "" : " (NAV-ORB no reply)", (long)s->bbr[0], (long)s->ram[0],
       (long)s->bbr[1], (long)s->ram[1], (long)s->ram[2], (unsigned)CANARY_VALUE, (unsigned)CANARY_DEFAULT,
       rc_text(s->bbr_rc), rc_text(s->ram_rc));
}

/* ------------------------------------------------------------------------- */
/* Связь и состояние настройки                                                */
/* ------------------------------------------------------------------------- */

static uint32_t s_baud = BAUD_FACTORY;

/*
 * Дождаться жизни приёмника: любой UBX-кадр (в том числе периодический NAV-PVT/STATUS) или ответ на опрос
 * UBX-NAV-STATUS. Скорость МК не меняется первые 3 с (модулю нужно время на старт), затем скорости чередуются
 * (после сброса настройки модуль на 38400, а в BBR могла остаться другая). reply_ms - время до первого признака
 * жизни от t_start.
 */
static bool link_up(uint32_t t_start, uint32_t timeout_ms, uint32_t *reply_ms)
{
    const uint32_t seq0 = s_seq_status + s_seq_pvt;
    uint32_t t_toggle = t_start + 3000U;

    while ((uint32_t)(HAL_GetTick() - t_start) < timeout_ms)
    {
        const bool got = ((s_seq_status + s_seq_pvt) != seq0) ||
                         poll_msg(PURRGO_UBX_CLASS_NAV, PURRGO_UBX_ID_NAV_STATUS, &s_seq_status, 700U);

        if (got)
        {
            if (reply_ms != NULL)
            {
                *reply_ms = (uint32_t)(HAL_GetTick() - t_start);
            }
            return true;
        }
        if ((int32_t)(HAL_GetTick() - t_toggle) >= 0)
        {
            s_baud = (s_baud == BAUD_FACTORY) ? BAUD_ALT : BAUD_FACTORY;
            (void)purrgo_gnss_raw_set_baud(s_baud);
            t_toggle = HAL_GetTick() + 1500U;
        }
    }
    return false;
}

/* Разбудить приёмник из "off" PSMOO: любой фронт на RX (manual MAX-M10S, 3.6.2.5). */
static void wake_receiver(void)
{
    static const uint8_t wake[] = { 0xFF, 0xFF, 0xFF, 0xFF };

    (void)tx_raw(wake, sizeof wake);
    delay_ms(300U);
}

static int valdel_layers(uint8_t layers, const uint32_t *keys, size_t count)
{
    uint8_t frame[96];
    const size_t n = purrgo_ubx_build_valdel(frame, sizeof frame, layers, keys, count);

    return cfg_wait(frame, n, &s_ack_valdel, &s_nak_valdel);
}

/*
 * Есть ли у модуля слой Flash: запись маркера в слой Flash принимается (ACK) только если флэш есть; сразу после
 * этого значение читается обратно и удаляется. NAK на запись - флэша нет (или слой недоступен).
 */
static void flash_probe(void)
{
    static const purrgo_ubx_cfg_item_t mk = { KEY_MARKER, 1U };
    static const uint32_t key = KEY_MARKER;
    int32_t v[1] = { -1 };

    const int g0 = valget(0x02U, &key, 1U, v);
    const int w = valset(LAYER_FLASH, &mk, 1U);
    int g1 = -2;
    int d = -2;

    v[0] = -1;
    if (w == 1)
    {
        g1 = valget(0x02U, &key, 1U, v);
        d = valdel_layers(LAYER_FLASH, &key, 1U);
    }
    HT("HWTEST GNSS_BBR flash probe: VALGET(flash) %s; VALSET(flash) %s; readback %ld (rc %s); VALDEL(flash) %s -> %s\r\n",
       rc_text(g0), rc_text(w), (long)v[0], (g1 == -2) ? "-" : rc_text(g1), (d == -2) ? "-" : rc_text(d),
       (w == 1) ? "module HAS a flash configuration layer" : "no flash layer (write rejected)");
    res_add("flash layer: %s", (w == 1) ? "YES" : "no");
}

/*
 * Сменить скорость модуля и МК. ACK на команду смены скорости может прийти на старой или на новой скорости,
 * поэтому он не ждётся: после паузы скорость МК меняется и связь проверяется опросом.
 */
static bool set_module_baud(uint32_t baud, uint8_t layers)
{
    const purrgo_ubx_cfg_item_t item = { KEY_UART_BAUD, baud };
    uint8_t frame[64];
    const size_t n = purrgo_ubx_build_valset(frame, sizeof frame, layers, &item, 1U);

    if ((n == 0U) || !tx_raw(frame, n))
    {
        return false;
    }
    delay_ms(250U);
    (void)purrgo_gnss_raw_set_baud(baud);
    s_baud = baud;
    purrgo_ubx_parser_init(&s_parser);
    return link_up(HAL_GetTick(), 6000U, NULL);
}

/* Вернуть приёмник в обычное состояние: ключи теста из BBR удалены, в RAM полный режим и NMEA. */
static void cleanup_receiver(const char *why)
{
    static const purrgo_ubx_cfg_item_t ram_restore[] = {
        { KEY_PM_OPMODE, 0U }, { KEY_NMEA_UART1, 1U }, { KEY_MSGOUT_PVT, 0U }, { KEY_CANARY, CANARY_DEFAULT },
        { KEY_MSGOUT_STATUS, 0U }, { KEY_MARKER, 0U },
        /* Заводской набор систем; сначала выключения, потом включения (B1I несовместим с B1C и GLONASS). */
        { KEY_SIG_GLO, 0U }, { KEY_SIG_BDS, 0U }, { KEY_SIG_BDS_B1, 0U }, { KEY_SIG_BDS_B1C, 1U }, { KEY_SBAS, 1U },
    };
    static const uint32_t del1[] = {
        KEY_MARKER, KEY_PM_OPMODE, KEY_PM_POSPERIOD, KEY_PM_ACQPERIOD, KEY_PM_ONTIME, KEY_PM_NOOFF,
        KEY_MSGOUT_PVT, KEY_MSGOUT_STATUS,
    };
    static const uint32_t del2[] = {
        KEY_NMEA_UART1, KEY_SBAS, KEY_SIG_GLO, KEY_SIG_BDS, KEY_SIG_BDS_B1, KEY_SIG_BDS_B1C, KEY_UART_BAUD,
    };

    wake_receiver();
    if (s_baud != BAUD_FACTORY)
    {
        /* Модуль мог остаться на рабочей скорости (BBR прошлого запуска или PSMOO_BAUD): вернуть заводскую. */
        HT("HWTEST GNSS_BBR cleanup (%s): module at %lu baud, restoring %u\r\n", why, (unsigned long)s_baud,
           (unsigned)BAUD_FACTORY);
        (void)set_module_baud(BAUD_FACTORY, LAYER_RAM);
    }
    const int a = valset(LAYER_RAM, ram_restore, sizeof ram_restore / sizeof ram_restore[0]);
    const int b = valdel_bbr(del1, sizeof del1 / sizeof del1[0]);
    const int c = valdel_bbr(del2, sizeof del2 / sizeof del2[0]);

    HT("HWTEST GNSS_BBR cleanup (%s): RAM restore %s, BBR delete %s / %s\r\n", why, rc_text(a), rc_text(b),
       rc_text(c));
}

/*
 * Ждать фикса: опрос UBX-NAV-PVT раз в секунду, плюс периодический NAV-PVT, если он включён. Время фикса берётся
 * из момента приёма первого PVT с фиксом (s_t_first_fix), а не из момента опроса. true - фикс есть;
 * *t_ms - время от t_start.
 */
static bool wait_fix(uint32_t t_start, uint32_t timeout_s, const char *tag, uint32_t *t_ms)
{
    uint32_t last_log = 0U;

    while ((uint32_t)(HAL_GetTick() - t_start) < (timeout_s * 1000U))
    {
        const uint32_t t_poll = HAL_GetTick();
        const bool got = poll_msg(PURRGO_UBX_CLASS_NAV, PURRGO_UBX_ID_NAV_PVT, &s_seq_pvt, 1200U);

        if (s_t_first_fix != 0U)
        {
            *t_ms = (s_t_first_fix > t_start) ? (uint32_t)(s_t_first_fix - t_start) : 0U;
            return true;
        }
        const uint32_t el = (uint32_t)(HAL_GetTick() - t_start) / 1000U;

        if ((el >= last_log + 5U) || (last_log == 0U))
        {
            last_log = el;
            HT("HWTEST GNSS_BBR %s: waiting for fix %lu s: fix=%u sv=%u psm=%u%s\r\n", tag, (unsigned long)el,
               (unsigned)s_pvt.fix_type, (unsigned)s_pvt.num_sv, (unsigned)s_pvt.psm, got ? "" : " (no PVT)");
            head_set(2, "wait fix %lu s", (unsigned long)el);
            head_set(3, "fix=%u sv=%u", (unsigned)s_pvt.fix_type, (unsigned)s_pvt.num_sv);
        }
        while ((uint32_t)(HAL_GetTick() - t_poll) < 1000U)
        {
            idle_step();
        }
    }
    return false;
}

/* ------------------------------------------------------------------------- */
/* Фаза 2: EN = Low                                                           */
/* ------------------------------------------------------------------------- */

/* Трёхзначный результат: 1 да, 0 нет, -1 не удалось определить (приёмник не ответил и т.п.). */
static const char *tri_text(int v)
{
    return (v > 0) ? "yes" : ((v == 0) ? "NO" : "?");
}

static char yn_c(int v)
{
    return (v > 0) ? 'Y' : ((v == 0) ? 'N' : '?');
}

static void count_yn(int v, unsigned *yes, unsigned *no, unsigned *unk)
{
    if (v > 0) { (*yes)++; } else if (v == 0) { (*no)++; } else { (*unk)++; }
}

typedef struct {
    int cfg;       /* настройка (NMEA выключен, PVT включён) пережила EN Low: по приёму без единого запроса */
    int bbr;       /* маркер в слое BBR по VALGET */
    int ram;       /* маркер в слое RAM по VALGET (RAM собирается из BBR при старте, поэтому это не доказательство) */
    int ram_only;  /* значение, записанное ТОЛЬКО в RAM, пережило EN Low: настоящая проверка RAM */
    int reset;     /* ядро перезапускалось (msss стал меньше) */
    int eph;       /* эфемериды сохранились */
    int hot;       /* фикс не позже 6 с после EN High */
    uint32_t fix_ms;
    uint32_t rx_ttff_ms;
} en_result_t;

static void run_en_phase(void)
{
    /* Настройка, по которой сразу видно, пережила ли она EN Low: NMEA выключен, PVT и STATUS включены. */
    static const purrgo_ubx_cfg_item_t persist_cfg[] = {
        { KEY_NMEA_UART1, 0U }, { KEY_MSGOUT_PVT, 1U }, { KEY_MSGOUT_STATUS, 1U }, { KEY_MARKER, 1U },
    };
    const unsigned n = (unsigned)PURRGO_HWTEST_GNSS_EN_CYCLES;
    unsigned cfg_y = 0U, cfg_n = 0U, cfg_u = 0U;
    unsigned bbr_y = 0U, bbr_n = 0U, bbr_u = 0U;
    unsigned rmo_y = 0U, rmo_n = 0U, rmo_u = 0U;
    unsigned rst_y = 0U, rst_n = 0U, rst_u = 0U;
    unsigned eph_y = 0U, eph_n = 0U, eph_u = 0U;
    unsigned hot_y = 0U, hot_n = 0U, hot_u = 0U;
    unsigned dead_n = 0U;

    HT("HWTEST GNSS_BBR === phase EN: %u cycles, EN low %u s ===\r\n", n, (unsigned)PURRGO_HWTEST_GNSS_EN_OFF_S);
    head_set(0, "GNSS_BBR  phase EN");

    for (unsigned k = 0U; k < n; k++)
    {
        snap_t before;
        snap_t after;
        en_result_t r;
        uint32_t fix_ms = 0U;

        memset(&r, 0, sizeof r);
        r.cfg = r.bbr = r.ram = r.ram_only = r.reset = r.eph = r.hot = -1;
        head_set(1, "EN cycle %u/%u", k + 1U, n);

        /* Настройка и маркер в RAM и BBR; снимок до выключения. */
        const int mk = valset(LAYER_RAM_BBR, persist_cfg, sizeof persist_cfg / sizeof persist_cfg[0]);

        HT("HWTEST GNSS_BBR EN#%u: persistent config (NMEA off, PVT+STATUS on, marker) RAM+BBR -> %s\r\n", k + 1U,
           rc_text(mk));
        {
            static const purrgo_ubx_cfg_item_t canary = { KEY_CANARY, CANARY_VALUE };

            HT("HWTEST GNSS_BBR EN#%u: RAM-only canary -> %s\r\n", k + 1U, rc_text(valset(LAYER_RAM, &canary, 1U)));
        }
        delay_ms(1500U);   /* дать накопиться периодическим сообщениям (и убедиться, что NMEA ушёл) */
        take_snapshot(&before);
        log_snap("before-off", &before);

        /* EN = Low. */
        purrgo_gnss_raw_en(false);
        const uint32_t t_off = HAL_GetTick();

        HT("HWTEST GNSS_BBR EN#%u: EN low for %u s\r\n", k + 1U, (unsigned)PURRGO_HWTEST_GNSS_EN_OFF_S);
        while ((uint32_t)(HAL_GetTick() - t_off) < (PURRGO_HWTEST_GNSS_EN_OFF_S * 1000U))
        {
            head_set(2, "EN low %lu/%u s", (unsigned long)((HAL_GetTick() - t_off) / 1000U),
                     (unsigned)PURRGO_HWTEST_GNSS_EN_OFF_S);
            idle_step();
        }

        /* EN = High; все времена считаются от этого момента. */
        const uint32_t t_on = HAL_GetTick();

        s_bytes = 0U;
        s_t_first_byte = 0U;
        s_t_first_fix = 0U;
        s_nmea_dollars = 0U;
        s_baud = BAUD_FACTORY;
        const uint32_t pvt0 = s_seq_pvt;
        const uint32_t sta0 = s_seq_status;

        purrgo_gnss_raw_en(true);
        purrgo_ubx_parser_init(&s_parser);

        /* Пассивное окно: 2.5 с только слушаем, ничего не отправляя. */
        delay_ms(2500U);
        const uint32_t frames = (s_seq_pvt - pvt0) + (s_seq_status - sta0);
        const uint32_t dollars = s_nmea_dollars;

        if ((frames > 0U) && (dollars == 0U)) { r.cfg = 1; }
        else if ((dollars > 0U) && (frames == 0U)) { r.cfg = 0; }
        HT("HWTEST GNSS_BBR EN#%u: passive window 2.5 s after EN high: first byte after %ld ms, UBX PVT/STATUS frames=%lu,"
           " NMEA '$'=%lu, bytes=%lu, first fix seen after %ld ms, UART errors total=%lu -> config %s\r\n", k + 1U,
           (s_t_first_byte != 0U) ? (long)(s_t_first_byte - t_on) : -1L, (unsigned long)frames,
           (unsigned long)dollars, (unsigned long)s_bytes,
           (s_t_first_fix != 0U) ? (long)(s_t_first_fix - t_on) : -1L, (unsigned long)purrgo_gnss_raw_errors(),
           (r.cfg > 0) ? "RETAINED (survived EN low)" : ((r.cfg == 0) ? "LOST (factory NMEA again)" : "unknown"));

        uint32_t reply_ms = 0U;
        const bool alive = link_up(t_on, 20000U, &reply_ms);

        if (!alive)
        {
            dead_n++;
            HT("HWTEST GNSS_BBR EN#%u: NO REPLY in 20 s after EN high\r\n", k + 1U);
            res_add("E%u cfg:%c NO REPLY", k + 1U, yn_c(r.cfg));
            count_yn(r.cfg, &cfg_y, &cfg_n, &cfg_u);
            bbr_u++; rst_u++; eph_u++; hot_u++;
            continue;
        }

        take_snapshot(&after);
        log_snap("after-on", &after);

        if (after.alive)
        {
            r.reset = (after.msss < before.msss) ? 1 : 0;
            if (after.bbr_rc > 0) { r.bbr = (after.bbr[0] == 1) ? 1 : 0; }
            if (after.ram_rc > 0) { r.ram = (after.ram[0] == 1) ? 1 : 0; r.ram_only = (after.ram[2] == (int32_t)CANARY_VALUE) ? 1 : 0; }
            if (after.orb && before.orb)
            {
                r.eph = ((after.eph > 0) && ((after.eph * 2) >= before.eph)) ? 1 : 0;
            }
        }

        if (wait_fix(t_on, PURRGO_HWTEST_GNSS_EN_FIX_TIMEOUT_S, "EN", &fix_ms))
        {
            r.fix_ms = fix_ms;
            r.hot = (fix_ms <= (uint32_t)PURRGO_HWTEST_GNSS_HOT_MS) ? 1 : 0;
            if (poll_retry(PURRGO_UBX_CLASS_NAV, PURRGO_UBX_ID_NAV_STATUS, &s_seq_status, 1500U, 3U))
            {
                r.rx_ttff_ms = s_st.ttff;
            }
        }
        else
        {
            r.hot = 0;
            r.fix_ms = PURRGO_HWTEST_GNSS_EN_FIX_TIMEOUT_S * 1000U;
        }

        HT("HWTEST GNSS_BBR EN#%u RESULT: config retained %s | marker BBR %s, RAM %s | RAM-only value kept %s | core reset %s (msss %lu -> %lu) |"
           " ephemerides kept %s (eph %d -> %d, alm %d -> %d) | time valid after start 0x%02X | fix %s%lu ms after EN"
           " high (receiver ttff %lu ms) -> hot start %s\r\n", k + 1U, tri_text(r.cfg), tri_text(r.bbr), tri_text(r.ram), tri_text(r.ram_only), tri_text(r.reset),
           (unsigned long)before.msss, (unsigned long)after.msss, tri_text(r.eph), before.eph, after.eph, before.alm,
           after.alm, (unsigned)after.valid, (r.hot > 0) ? "" : ">", (unsigned long)r.fix_ms,
           (unsigned long)r.rx_ttff_ms, tri_text(r.hot));

        res_add("E%u cfg:%c bbr:%c ram:%c rst:%c", k + 1U, yn_c(r.cfg), yn_c(r.bbr), yn_c(r.ram_only), yn_c(r.reset));
        res_add("  eph:%c %d>%d fix %s%lu.%lus", yn_c(r.eph), before.eph, after.eph, (r.hot > 0) ? "" : ">",
                (unsigned long)(r.fix_ms / 1000U), (unsigned long)((r.fix_ms % 1000U) / 100U));
        scr_refresh(true);

        count_yn(r.cfg, &cfg_y, &cfg_n, &cfg_u);
        count_yn(r.bbr, &bbr_y, &bbr_n, &bbr_u);
        count_yn(r.ram_only, &rmo_y, &rmo_n, &rmo_u);
        count_yn(r.reset, &rst_y, &rst_n, &rst_u);
        count_yn(r.eph, &eph_y, &eph_n, &eph_u);
        count_yn(r.hot, &hot_y, &hot_n, &hot_u);
    }

    HT("HWTEST GNSS_BBR (hot start = fix within %lu ms after EN high; override: PURRGO_HWTEST_GNSS_HOT_MS)\r\n",
       (unsigned long)PURRGO_HWTEST_GNSS_HOT_MS);
    HT("HWTEST GNSS_BBR === EN SUMMARY (yes/no/unknown of %u cycles): config retained %u/%u/%u, marker in BBR %u/%u/%u,"
       " RAM-only value kept %u/%u/%u, core reset %u/%u/%u, ephemerides kept %u/%u/%u, hot start %u/%u/%u,"
       " no reply %u ===\r\n", n, cfg_y, cfg_n, cfg_u, bbr_y, bbr_n, bbr_u, rmo_y, rmo_n, rmo_u, rst_y, rst_n, rst_u, eph_y, eph_n, eph_u, hot_y, hot_n, hot_u, dead_n);
    if (hot_y == n)
    {
        HT("HWTEST GNSS_BBR EN VERDICT: nav data survives EN low (hot start every cycle). Configuration: %s\r\n",
           (cfg_y == n) ? "also kept (BBR)" : ((cfg_n == n) ? "LOST - must be re-sent after every EN high" : "unclear, see cycles"));
    }
    else if ((hot_n == n) && (eph_n == n))
    {
        HT("HWTEST GNSS_BBR EN VERDICT: ephemerides are lost on EN low - every fix is a cold start\r\n");
    }
    else
    {
        HT("HWTEST GNSS_BBR EN VERDICT: mixed or incomplete result, see per-cycle lines above\r\n");
    }
    res_add("EN cfg%u/%u eph%u/%u hot%u/%u", cfg_y, n, eph_y, n, hot_y, n);
    scr_refresh(true);
}

/* ------------------------------------------------------------------------- */
/* Фаза 3: PSMOO                                                              */
/* ------------------------------------------------------------------------- */

static void run_psmoo_phase(void)
{
    static const purrgo_ubx_cfg_item_t out_cfg[] = {
        { KEY_NMEA_UART1, 0U }, { KEY_MSGOUT_PVT, 1U }, { KEY_MSGOUT_STATUS, 1U }, { KEY_MARKER, 1U },
    };
    static const purrgo_ubx_cfg_item_t pm_params[] = {
        { KEY_PM_POSPERIOD, (uint32_t)PURRGO_HWTEST_GNSS_PSMOO_PERIOD_S },
        { KEY_PM_ACQPERIOD, (uint32_t)PURRGO_HWTEST_GNSS_PSMOO_PERIOD_S },
        { KEY_PM_ONTIME,    (uint32_t)PURRGO_HWTEST_GNSS_PSMOO_ONTIME_S },
        { KEY_PM_NOOFF,     0U },
    };
    static const purrgo_ubx_cfg_item_t pm_mode = { KEY_PM_OPMODE, 1U };

    const uint32_t silence_ms = ((2U * PURRGO_HWTEST_GNSS_PSMOO_PERIOD_S) + 15U) * 1000U;
    unsigned nmea_back = 0U;
    unsigned silences = 0U;
    bool mode_in_bbr = true;

    HT("HWTEST GNSS_BBR === phase PSMOO: period %u s, on-time %u s, %u bursts ===\r\n",
       (unsigned)PURRGO_HWTEST_GNSS_PSMOO_PERIOD_S, (unsigned)PURRGO_HWTEST_GNSS_PSMOO_ONTIME_S,
       (unsigned)PURRGO_HWTEST_GNSS_PSMOO_CYCLES);
    head_set(0, "GNSS_BBR  phase PSMOO");
    head_set(1, "period %us ontime %us", (unsigned)PURRGO_HWTEST_GNSS_PSMOO_PERIOD_S,
             (unsigned)PURRGO_HWTEST_GNSS_PSMOO_ONTIME_S);

    /* Приёмник должен работать (после фазы EN он включён; без фазы EN - просто работает). */
    if (!link_up(HAL_GetTick(), 15000U, NULL))
    {
        HT("HWTEST GNSS_BBR PSMOO: module does not answer, phase skipped\r\n");
        res_add("PSMOO: no module reply");
        return;
    }

    /*
     * Набор систем как в рабочей прошивке (gnss_receiver.c, PURRGO_GNSS_SIGNALS = GPS_GAL_BDS): SBAS выключен
     * (manual: в PSM он не обрабатывается), GLONASS выключен, BeiDou B1C выключен (в PSM не поддерживается;
     * по умолчанию он включён), B1I включён. Порядок тоже как там: сначала выключения, потом включения.
     * Смена набора перезапускает ГНСС, поэтому после неё ждём связь и фикс (рабочий трекер входит в PSMOO с фиксом).
     */
    static const purrgo_ubx_cfg_item_t signals[] = {
        { KEY_SBAS, 0U }, { KEY_SIG_GLO, 0U }, { KEY_SIG_BDS_B1C, 0U }, { KEY_SIG_BDS_B1, 1U }, { KEY_SIG_BDS, 1U },
    };
    const int c0 = valset(LAYER_RAM_BBR, signals, sizeof signals / sizeof signals[0]);

    delay_ms(3000U);
    (void)link_up(HAL_GetTick(), 10000U, NULL);

    uint32_t pre_fix_ms = 0U;

    s_t_first_fix = 0U;
    const bool pre_fix = wait_fix(HAL_GetTick(), 90U, "PSM-pre", &pre_fix_ms);

    HT("HWTEST GNSS_BBR PSMOO: signals (SBAS/GLO/B1C off, B1I on) RAM+BBR -> %s; fix %s (%lu ms)\r\n", rc_text(c0),
       pre_fix ? "obtained" : "NOT obtained in 90 s", (unsigned long)pre_fix_ms);

#if PURRGO_HWTEST_GNSS_PSMOO_BAUD != 0U
    /* Как в рабочей прошивке: скорость UART пишется и в BBR (иначе после "off" модуль вернулся бы на 38400). */
    const bool baud_ok = set_module_baud(PURRGO_HWTEST_GNSS_PSMOO_BAUD, LAYER_RAM_BBR);

    HT("HWTEST GNSS_BBR PSMOO: module baud -> %lu (RAM+BBR): link %s\r\n",
       (unsigned long)PURRGO_HWTEST_GNSS_PSMOO_BAUD, baud_ok ? "OK" : "LOST");
    res_add("baud %lu: %s", (unsigned long)PURRGO_HWTEST_GNSS_PSMOO_BAUD, baud_ok ? "OK" : "LOST");
#endif
    const int c1 = valset(LAYER_RAM_BBR, out_cfg, sizeof out_cfg / sizeof out_cfg[0]);
    const int c2 = valset(LAYER_RAM_BBR, pm_params, sizeof pm_params / sizeof pm_params[0]);
    int c3 = valset(LAYER_RAM_BBR, &pm_mode, 1U);

    if (c3 == 0)
    {
        /*
         * Диагностика отказа. (a) только RAM; (b) контроль: PSMCT в RAM (в рабочей прошивке принимается):
         * если принят, PSM как таковой возможен и отклоняется именно PSMOO; если нет, мешает общее
         * условие (набор систем, состояние приёмника).
         */
        static const purrgo_ubx_cfg_item_t ct_mode = { KEY_PM_OPMODE, 2U };
        static const purrgo_ubx_cfg_item_t full_mode = { KEY_PM_OPMODE, 0U };
        const int c3r = valset(LAYER_RAM, &pm_mode, 1U);

        HT("HWTEST GNSS_BBR PSMOO: OPERATEMODE=PSMOO rejected for RAM+BBR; RAM only -> %s\r\n", rc_text(c3r));
        if (c3r == 1)
        {
            mode_in_bbr = false;
            c3 = 1;
        }
        else
        {
            const int ct = valset(LAYER_RAM, &ct_mode, 1U);

            HT("HWTEST GNSS_BBR PSMOO: control OPERATEMODE=PSMCT (RAM) -> %s: %s\r\n", rc_text(ct),
               (ct == 1) ? "PSM in general is accepted, only PSMOO is rejected"
                         : "PSM is rejected altogether (precondition: signals, time, receiver state)");
            res_add("control PSMCT: %s", rc_text(ct));
            if (ct == 1)
            {
                (void)valset(LAYER_RAM, &full_mode, 1U);
            }
        }
    }

    HT("HWTEST GNSS_BBR PSMOO: config: signals %s, output %s, params %s, mode %s (%s)\r\n", rc_text(c0),
       rc_text(c1), rc_text(c2), rc_text(c3), mode_in_bbr ? "RAM+BBR" : "RAM only");
    res_add("sig %s out %s par %s", rc_text(c0), rc_text(c1), rc_text(c2));
    res_add("mode %s (%s)", rc_text(c3), mode_in_bbr ? "BBR" : "RAM");
    if ((c1 != 1) || (c2 != 1) || (c3 != 1))
    {
        HT("HWTEST GNSS_BBR PSMOO: configuration not accepted, phase aborted\r\n");
        return;
    }

    /*
     * Наблюдение ПАССИВНОЕ: тест ничего не отправляет, пока идут всплески (любая передача на RX будит приёмник из
     * "off" и удерживает его: прошлый прогон показал, что опросы меняли ритм). Всплеск - это серия периодических
     * PVT/STATUS; пауза больше 3 с - приёмник ушёл в "off". Из потока берутся: интервал между началами
     * всплесков (сетка периода), длительность работы, msss первого STATUS всплеска (малое значение - ядро
     * стартовало заново после "off"), ttff, состояние PSM, спутники, ошибки UART, возврат NMEA (настройка потеряна).
     * Маркер и режим в BBR проверяются опросом один раз в конце.
     */
    {
        static const purrgo_ubx_cfg_item_t canary = { KEY_CANARY, CANARY_VALUE };

        HT("HWTEST GNSS_BBR PSMOO: RAM-only canary -> %s\r\n", rc_text(valset(LAYER_RAM, &canary, 1U)));
    }
    s_nmea_dollars = 0U;
    const uint32_t t_start = HAL_GetTick();
    const uint32_t total_ms = ((PURRGO_HWTEST_GNSS_PSMOO_CYCLES + 2U) * PURRGO_HWTEST_GNSS_PSMOO_PERIOD_S + 180U) * 1000U;
    uint32_t seen_pvt = s_seq_pvt;
    uint32_t seen_sta = s_seq_status;
    uint32_t t_last_frame = t_start;
    uint32_t t_prev_burst = 0U;
    uint32_t t_burst = 0U;
    uint32_t gap_ms = 0U;
    uint32_t msss_first = 0U;
    uint32_t frames = 0U;
    uint32_t err_mark = purrgo_gnss_raw_errors();
    uint32_t err_all = 0U;
    bool in_burst = false;
    bool sta_seen = false;
    unsigned done = 0U;

    while (done < PURRGO_HWTEST_GNSS_PSMOO_CYCLES)
    {
        idle_step();

        const uint32_t now = HAL_GetTick();

        if ((uint32_t)(now - t_start) > total_ms)
        {
            HT("HWTEST GNSS_BBR PSMOO: overall timeout, %u bursts completed\r\n", done);
            break;
        }
        if (s_nmea_dollars != 0U)
        {
            nmea_back++;
            HT("HWTEST GNSS_BBR PSMOO: NMEA '$' x%lu appeared - receiver is back to factory output (configuration lost)\r\n",
               (unsigned long)s_nmea_dollars);
            s_nmea_dollars = 0U;
        }

        const bool got_pvt = (s_seq_pvt != seen_pvt);
        const bool got_sta = (s_seq_status != seen_sta);

        if (got_pvt || got_sta)
        {
            seen_pvt = s_seq_pvt;
            seen_sta = s_seq_status;
            t_last_frame = now;
            if (!in_burst)
            {
                in_burst = true;
                sta_seen = false;
                frames = 0U;
                t_burst = now;
                gap_ms = (t_prev_burst != 0U) ? (uint32_t)(now - t_prev_burst) : 0U;
            }
            if (got_pvt)
            {
                frames++;
            }
            if (got_sta && !sta_seen)
            {
                sta_seen = true;
                msss_first = s_st.msss;
            }
        }
        else if (in_burst && ((uint32_t)(now - t_last_frame) > 3000U))
        {
            const uint32_t on_ms = (uint32_t)(t_last_frame - t_burst);
            const uint32_t errs = purrgo_gnss_raw_errors() - err_mark;

            err_mark = purrgo_gnss_raw_errors();
            err_all += errs;
            done++;
            in_burst = false;
            t_prev_burst = t_burst;
            HT("HWTEST GNSS_BBR PSMOO burst %u: t=%lu s, start-to-start %lu ms (period %u s), on %lu ms, PVT frames %lu,"
               " msss at first STATUS %lu ms (small = core restarted), last msss %lu ms, receiver ttff %lu ms,"
               " psm=%u fix=%u sv=%u, UART errors %lu\r\n", done, (unsigned long)((t_burst - t_start) / 1000U),
               (unsigned long)gap_ms, (unsigned)PURRGO_HWTEST_GNSS_PSMOO_PERIOD_S, (unsigned long)on_ms,
               (unsigned long)frames, (unsigned long)msss_first, (unsigned long)s_st.msss, (unsigned long)s_st.ttff,
               (unsigned)s_pvt.psm, (unsigned)s_pvt.fix_type, (unsigned)s_pvt.num_sv, (unsigned long)errs);
            res_add("B%u g%lu.%lu on%lu.%lu t%lu sv%u", done, (unsigned long)(gap_ms / 1000U),
                    (unsigned long)((gap_ms % 1000U) / 100U), (unsigned long)(on_ms / 1000U),
                    (unsigned long)((on_ms % 1000U) / 100U), (unsigned long)s_st.ttff, (unsigned)s_pvt.num_sv);
            head_set(2, "burst %u/%u psm=%u", done, (unsigned)PURRGO_HWTEST_GNSS_PSMOO_CYCLES, (unsigned)s_pvt.psm);
            head_set(3, "fix=%u sv=%u nmea=%u", (unsigned)s_pvt.fix_type, (unsigned)s_pvt.num_sv, nmea_back);
            scr_refresh(false);
        }
        else if (!in_burst && ((uint32_t)(now - t_last_frame) > silence_ms))
        {
            /* Настоящая тишина: опрос будит приёмник и показывает, жив ли он и на какой скорости. */
            snap_t s;

            silences++;
            HT("HWTEST GNSS_BBR PSMOO: SILENT for %lu ms (expected a burst every %u s), probing (wakes the receiver)\r\n",
               (unsigned long)(now - t_last_frame), (unsigned)PURRGO_HWTEST_GNSS_PSMOO_PERIOD_S);
            wake_receiver();
            const bool alive = link_up(HAL_GetTick(), 8000U, NULL);

            take_snapshot(&s);
            log_snap("after-silence", &s);
            HT("HWTEST GNSS_BBR PSMOO: after silence module is %s at %lu baud, NMEA '$' seen=%lu\r\n",
               alive ? "ALIVE" : "DEAD", (unsigned long)s_baud, (unsigned long)s_nmea_dollars);
            res_add("silence: %s %lu bd", alive ? "alive" : "DEAD", (unsigned long)s_baud);
            scr_refresh(true);
            if (!alive || (silences >= 2U))
            {
                break;
            }
            seen_pvt = s_seq_pvt;
            seen_sta = s_seq_status;
            t_last_frame = HAL_GetTick();
            in_burst = false;
        }
    }

    /* Итоговая проверка опросом (будит приёмник): маркер и режим в BBR и RAM. */
    snap_t fin;

    wake_receiver();
    (void)link_up(HAL_GetTick(), 8000U, NULL);
    take_snapshot(&fin);
    log_snap("PSMOO-final", &fin);

    const int32_t op_bbr = fin.bbr[1];
    const bool final_ok = fin.alive && (fin.bbr_rc > 0) && (fin.bbr[0] == 1) && (mode_in_bbr ? (op_bbr == 1) : (fin.ram[1] == 1));
    const unsigned n = (unsigned)PURRGO_HWTEST_GNSS_PSMOO_CYCLES;

    const bool canary_kept = fin.alive && (fin.ram_rc > 0) && (fin.ram[2] == (int32_t)CANARY_VALUE);

    HT("HWTEST GNSS_BBR === PSMOO SUMMARY: %u/%u bursts, NMEA reappeared %u, real silences %u, UART errors %lu,"
       " final check: marker BBR=%ld, OPERATEMODE BBR=%ld RAM=%ld (%s); RAM-only value after the off cycles: %s ===\r\n",
       done, n, nmea_back, silences, (unsigned long)err_all, (long)fin.bbr[0], (long)op_bbr, (long)fin.ram[1],
       final_ok ? "config kept" : "NOT confirmed", canary_kept ? "KEPT (RAM survives PSMOO off)" : "LOST (RAM is cleared)");
    if ((done == n) && (nmea_back == 0U) && (silences == 0U) && final_ok)
    {
        HT("HWTEST GNSS_BBR PSMOO VERDICT: PSMOO ran %u off/on cycles, configuration (BBR) survived\r\n", n);
    }
    else
    {
        HT("HWTEST GNSS_BBR PSMOO VERDICT: configuration or the receiver was LOST or the run is incomplete (see above)\r\n");
    }
    res_add("PSMOO %u/%u nm%u sil%u bbr:%c ram:%c", done, n, nmea_back, silences, final_ok ? 'Y' : 'N',
            canary_kept ? 'Y' : 'N');
    scr_refresh(true);
}

/* ------------------------------------------------------------------------- */
/* Точка входа                                                                */
/* ------------------------------------------------------------------------- */

void purrgo_hw_test_gnss_bbr(void)
{
    snap_t base;
    uint32_t fix_ms = 0U;

    HT("HWTEST GNSS_BBR start (phases mask %d)\r\n", (int)PURRGO_HWTEST_GNSS_PHASES);
    scr_init();
    head_set(0, "GNSS_BBR  start");
    scr_refresh(true);

    purrgo_ubx_parser_init(&s_parser);
    if (!purrgo_gnss_raw_start(BAUD_FACTORY))
    {
        HT("HWTEST GNSS_BBR: raw receive start failed\r\n");
        head_set(1, "RX start FAILED");
        scr_refresh(true);
        goto done;
    }
    s_baud = BAUD_FACTORY;

    /* 0. Связь. */
    uint32_t reply_ms = 0U;

    if (!link_up(HAL_GetTick(), 20000U, &reply_ms))
    {
        HT("HWTEST GNSS_BBR: no UBX reply at %u or %u baud in 20 s (bytes received: %lu, NMEA '$': %lu). "
           "Check wiring/EN.\r\n", (unsigned)BAUD_FACTORY, (unsigned)BAUD_ALT, (unsigned long)s_bytes,
           (unsigned long)s_nmea_dollars);
        head_set(1, "NO UBX REPLY");
        head_set(2, "rx bytes %lu", (unsigned long)s_bytes);
        scr_refresh(true);
        goto done;
    }
    HT("HWTEST GNSS_BBR: module answers at %lu baud (first reply after %lu ms)\r\n", (unsigned long)s_baud,
       (unsigned long)reply_ms);

    take_snapshot(&base);
    log_snap("initial", &base);
    HT("HWTEST GNSS_BBR: initial state above shows leftovers of earlier runs (marker/OPERATEMODE in BBR)\r\n");
    cleanup_receiver("initial");
    flash_probe();
    {
        /* NMEA на 38400 бод задерживает ответы на опросы на секунду и больше: на время теста он выключается. */
        static const purrgo_ubx_cfg_item_t nmea_off = { KEY_NMEA_UART1, 0U };

        HT("HWTEST GNSS_BBR: NMEA off (RAM) -> %s\r\n", rc_text(valset(LAYER_RAM, &nmea_off, 1U)));
    }
    s_t_first_fix = 0U;

    /* 1. ACQ. */
    head_set(0, "GNSS_BBR  acquire");
    HT("HWTEST GNSS_BBR === phase ACQ: waiting for a fix (up to %u s) ===\r\n",
       (unsigned)PURRGO_HWTEST_GNSS_ACQ_TIMEOUT_S);
    const bool fixed = wait_fix(HAL_GetTick(), PURRGO_HWTEST_GNSS_ACQ_TIMEOUT_S, "ACQ", &fix_ms);

    if (fixed)
    {
        HT("HWTEST GNSS_BBR ACQ: first fix after %lu ms; holding %u s to collect ephemerides\r\n",
           (unsigned long)fix_ms, (unsigned)PURRGO_HWTEST_GNSS_ACQ_HOLD_S);
        res_add("ACQ fix %lus", (unsigned long)(fix_ms / 1000U));
        delay_ms(PURRGO_HWTEST_GNSS_ACQ_HOLD_S * 1000U);
    }
    else
    {
        HT("HWTEST GNSS_BBR ACQ: NO FIX in %u s. Results below are not meaningful (go outside / near a window)\r\n",
           (unsigned)PURRGO_HWTEST_GNSS_ACQ_TIMEOUT_S);
        res_add("ACQ: NO FIX (bad sky)");
    }
    take_snapshot(&base);
    log_snap("BASE", &base);
    res_add("BASE sv%u eph%d alm%d", (unsigned)base.num_sv, base.eph, base.alm);
    scr_refresh(true);

    /* 2, 3. Основные фазы. */
#if (PURRGO_HWTEST_GNSS_PHASES & 1)
    run_en_phase();
#endif
#if (PURRGO_HWTEST_GNSS_PHASES & 2)
    run_psmoo_phase();
#endif

    /* 4. Откат. */
    head_set(0, "GNSS_BBR  cleanup");
    cleanup_receiver("final");
    {
        snap_t fin;

        take_snapshot(&fin);
        log_snap("final", &fin);
    }
    head_set(0, "GNSS_BBR  DONE");
    head_set(2, "see result lines below");
    res_add("DONE");
    HT("HWTEST GNSS_BBR done\r\n");
    scr_refresh(true);

done:
    for (;;)
    {
        idle_step();
    }
}

#endif /* PURRGO_HW_TEST_ENABLED && PURRGO_HW_TEST_GNSS_BBR */
