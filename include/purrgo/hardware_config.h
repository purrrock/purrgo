/* file: purrgo/hardware_config.h
 * PurrGo hardware configuration
 *
 * This file describes the physical hardware configuration for which
 * the current firmware/application is built.
 *
 * IMPORTANT:
 *   This is NOT user configuration.
 */

#ifndef PURRGO_HARDWARE_CONFIG_H
#define PURRGO_HARDWARE_CONFIG_H

/*
 * ============================================================================
 * GNSS receiver
 * ============================================================================
 */

/*
 * Модель приёмника: определяет, какие настроечные команды ему отправляются при
 * старте (см. purrgo/gnss_receiver.h).
 *
 *   PURRGO_GNSS_RECEIVER_NONE  - ничего не отправлять (приёмник работает с
 *                                настройками по умолчани.
 *   PURRGO_GNSS_RECEIVER_UBX   - u-blox M10: Release-модуль G10A F30
 *                                (UBX-M10050-KB), настройка бинарными
 *                                сообщениями UBX-CFG-VALSET. По умолчанию.
 */
#define PURRGO_GNSS_RECEIVER_NONE   0
#define PURRGO_GNSS_RECEIVER_UBX    1

#ifndef PURRGO_HW_GNSS_RECEIVER
#define PURRGO_HW_GNSS_RECEIVER PURRGO_GNSS_RECEIVER_UBX
#endif

#if (PURRGO_HW_GNSS_RECEIVER != PURRGO_GNSS_RECEIVER_NONE) && \
    (PURRGO_HW_GNSS_RECEIVER != PURRGO_GNSS_RECEIVER_UBX)
#error "Invalid PURRGO_HW_GNSS_RECEIVER"
#endif

/* Период навигационных решений GNSS, мс (1000 = 1 Гц). */
#ifndef PURRGO_GNSS_UPDATE_PERIOD_MS
#define PURRGO_GNSS_UPDATE_PERIOD_MS 1000
#endif

/*
 * Энергосбережение приёмника u-blox M10 (CFG-PM-OPERATEMODE; MAX-M10S Integration
 * manual UBX-20053088, раздел 3.6, и UBX-M10 SPG 5.10 Interface Description).
 *
 *   PURRGO_GNSS_PSM_OFF (0) - полная мощность (заводской режим).
 *   PURRGO_GNSS_PSM_CT  (2) - PSMCT, циклическое слежение: приёмник продолжает
 *                             выдавать решения каждую эпоху (NMEA и UBX идут как
 *                             обычно), между измерениями уходит в состояние POT
 *                             (Power optimized tracking), без полного отключения.
 *                             По умолчанию.
 *
 * Ожидаемый выигрыш по даташиту MAX-M10S (UBX-20035208, табл. 15, питание 3,0 В,
 * 1 Гц, ток чипа VCC + V_IO): слежение в непрерывном режиме GPS+GAL+GLO+BDS B1C
 * 10 + 2,3 = 12,3 мА, в PSMCT GPS+GAL (или +GLO) 5 + 2 = 7 мА, то есть около
 * 5 мА. Ток модуля G10A-F30 целиком больше (светодиоды, стабилизатор и т. п.).
 *
 * Период решений в PSMCT задаётся CFG-RATE-MEAS (PURRGO_GNSS_UPDATE_PERIOD_MS):
 * поддержаны 500 мс, 1 с, а также 2..10 с с шагом 1 с (4 Гц и выше PSMCT не поддерживает).
 *
 * Режим PSMOO (ON/OFF, значение 1, для периодов решений дольше 10 с) в навигации (карта,
 * путевой компьютер) не используется намеренно, поэтому PURRGO_GNSS_PSM принимает только
 * OFF и CT: между фиксами приёмник молчит, а ядро (таймаут фикса, запись трека раз в
 * секунду) и связь с модулем (gnss_link: контроль молчания) рассчитаны на поток данных
 * каждую секунду. PSMOO включается только в режиме "трекер с отключённым дисплеем"
 * (PURRGO_GNSS_TRACKER_MODE_OO, блок ниже; описание: docs/gnss-ubx.md, раздел
 * "Энергосбережение приёмника (PSM)"). Для PSMOO нужен RTC в модуле, а настройка
 * хранится и в слое BBR (RAM очищается в состоянии "off").
 *
 * Порядок настройки (последними, после всей остальной конфигурации; OPERATEMODE - самым
 * последним ключом, как рекомендует manual):
 *  1. Набор систем (см. ниже).
 *  2. CFG-PM-DONOTENTEROFF = 1: при потере фикса приёмник остаётся в Acquisition, а не
 *     уходит в "Inactive for search"; иначе поток NMEA замолкает, а настройка в слое RAM
 *     стирается (так сказано в manual: RAM очищается при входе в это состояние).
 *  3. CFG-PM-OPERATEMODE = 2 (PSMCT). CFG-PM-UPDATEEPH не отправляется: по умолчанию 1
 *     (эфемериды обновляются раз в ~30 минут, приёмник на это время возвращается в
 *     непрерывный режим).
 * Каждый шаг - отдельный UBX-CFG-VALSET: NAK называет точный шаг в логе (группы
 * "Signals", "PSM no-off", "PSM mode"), отказ оставляет приёмник на полной мощности, а
 * остальная настройка не страдает. Результат: UBX-NAV-PVT.psmState = 4 (Power optimized
 * tracking), в логе "GNSS: PSM state 4". Выключить: -DPURRGO_GNSS_PSM=0.
 */
#define PURRGO_GNSS_PSM_OFF 0
#define PURRGO_GNSS_PSM_CT  2

#ifndef PURRGO_GNSS_PSM
#define PURRGO_GNSS_PSM PURRGO_GNSS_PSM_CT
#endif
#if (PURRGO_GNSS_PSM != PURRGO_GNSS_PSM_OFF) && (PURRGO_GNSS_PSM != PURRGO_GNSS_PSM_CT)
#error "PURRGO_GNSS_PSM must be PURRGO_GNSS_PSM_OFF (0) or PURRGO_GNSS_PSM_CT (2)"
#endif

/* Кадры настройки PSM уходят только приёмнику UBX. */
#if (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_UBX) && (PURRGO_GNSS_PSM != PURRGO_GNSS_PSM_OFF)
#define PURRGO_GNSS_PSM_ACTIVE 1
#define PURRGO_GNSS_PSM_FRAMES 2   /* DONOTENTEROFF и OPERATEMODE */
#else
#define PURRGO_GNSS_PSM_ACTIVE 0
#define PURRGO_GNSS_PSM_FRAMES 0
#endif

/* PSMCT работает с периодом решений 500 мс, 1 с или 2..10 с с шагом 1 с (CFG-RATE-MEAS). */
#if PURRGO_GNSS_PSM_ACTIVE && (PURRGO_GNSS_UPDATE_PERIOD_MS != 500) && \
    !((PURRGO_GNSS_UPDATE_PERIOD_MS >= 1000) && (PURRGO_GNSS_UPDATE_PERIOD_MS <= 10000) && \
      ((PURRGO_GNSS_UPDATE_PERIOD_MS % 1000) == 0))
#error "PSMCT supports PURRGO_GNSS_UPDATE_PERIOD_MS of 500 or 1000..10000 in 1000 ms steps"
#endif

/*
 * Режим приёмника в "трекере с отключённым дисплеем" (TRACKER (SCREEN OFF)).
 *
 * В этом режиме экран не обновляется, а запись трека не требует фикса каждую секунду. При входе в режим
 * приёмнику отправляется профиль трекера (purrgo_gnss_set_profile), при выходе (K1+K4) - профиль навигации.
 *
 *   PURRGO_GNSS_TRACKER_MODE_OFF (0) - приёмник не трогать (работает как на карте).
 *   PURRGO_GNSS_TRACKER_MODE_CT  (1) - PSMCT с редкими решениями: тот же циклический режим, что в навигации,
 *                                     но период решений PURRGO_GNSS_TRACKER_PERIOD_S (по умолчанию 10 с, предел
 *                                     PSMCT; CFG-RATE-MEAS). Настройка только в RAM, приёмник не уходит в "off".
 *   PURRGO_GNSS_TRACKER_MODE_OO  (2) - PSMOO (ON/OFF): приёмник просыпается раз в период, ищет фикс и засыпает.
 *                                     Сбои этого режима в рабочей прошивке не объяснены (docs/gnss-retention.md).
 *   PURRGO_GNSS_TRACKER_MODE_EN  (3) - цикл по EN: прошивка сама включает приёмник (EN = High) раз в период,
 *                                     ждёт фикс (не дольше таймаута), записывает точку и выключает (EN = Low).
 *                                     Скорость UART 38400, настройка только в RAM после каждого включения (BBR не
 *                                     трогается). Политика таймаутов и пауз - purrgo/gnss_duty.h: после промахов
 *                                     паузы растут, при долгом поиске (слабое небо) приёмник не выключается и
 *                                     работает как PSMCT (PURRGO_GNSS_TRACKER_CT_PERIOD_S).
 *                                     По умолчанию.
 *   PURRGO_GNSS_TRACKER_PERIOD_S  период, с. MODE_CT: период решений, 1..10 (по умолчанию 10); MODE_OO: 5..3600 (по
 *                                 умолчанию 15; по manual PSMOO - для периодов дольше 10 с); MODE_EN: период между
 *                                 началами включений (по умолчанию 15). Время ядра в трекере: в режимах CT и OO фикс
 *                                 считается устаревшим по PURRGO_GNSS_TRACKER_FIX_TIMEOUT_MS, в MODE_EN срок задаёт
 *                                 автомат (purrgo_gnss_tracker_fix_timeout_ms()).
 *   PURRGO_GNSS_TRACKER_ONTIME_S  только MODE_OO: сколько секунд приёмник остаётся в слежении после фикса и выдаёт
 *                                 решения (по умолчанию 2).
 *
 * MODE_EN (результаты теста GNSS_BBR, docs/gnss-retention.md): EN = Low стирает RAM приёмника, но BBR, эфемериды
 * и время сохраняются, поэтому после включения старт горячий (rx-ttff около 1 с на хорошем небе, 7-11 с при 4-5
 * спутниках). Включённое время на точку: загрузка ~0,3 с + время до фикса.
 *
 * MODE_OO по manual (MAX-M10S, 3.6.2.4): в состоянии "off" RAM приёмника очищается вместе с настройкой, поэтому всё,
 * что должно сохраниться (скорость UART, набор сообщений, параметры PSM), отправляется в слои RAM и BBR
 * (PURRGO_GNSS_UBX_LAYERS_PERSIST); RTC и BBR живут на питании backup-домена. DONOTENTEROFF = 0, ACQPERIOD = период.
 */
#define PURRGO_GNSS_TRACKER_MODE_OFF 0
#define PURRGO_GNSS_TRACKER_MODE_CT  1
#define PURRGO_GNSS_TRACKER_MODE_OO  2
#define PURRGO_GNSS_TRACKER_MODE_EN  3

#ifndef PURRGO_GNSS_TRACKER_MODE
#define PURRGO_GNSS_TRACKER_MODE PURRGO_GNSS_TRACKER_MODE_EN // цикл по EN
#endif
#if (PURRGO_GNSS_TRACKER_MODE != PURRGO_GNSS_TRACKER_MODE_OFF) && \
    (PURRGO_GNSS_TRACKER_MODE != PURRGO_GNSS_TRACKER_MODE_CT) && \
    (PURRGO_GNSS_TRACKER_MODE != PURRGO_GNSS_TRACKER_MODE_OO) && \
    (PURRGO_GNSS_TRACKER_MODE != PURRGO_GNSS_TRACKER_MODE_EN)
#error "PURRGO_GNSS_TRACKER_MODE must be PURRGO_GNSS_TRACKER_MODE_OFF (0), _CT (1), _OO (2) or _EN (3)"
#endif

#ifndef PURRGO_GNSS_TRACKER_PERIOD_S
#if (PURRGO_GNSS_TRACKER_MODE == PURRGO_GNSS_TRACKER_MODE_OO) || (PURRGO_GNSS_TRACKER_MODE == PURRGO_GNSS_TRACKER_MODE_EN)
#define PURRGO_GNSS_TRACKER_PERIOD_S 15
#else
#define PURRGO_GNSS_TRACKER_PERIOD_S 10
#endif
#endif

/*
 * Политика цикла по EN (PURRGO_GNSS_TRACKER_MODE_EN, purrgo/gnss_duty.h). PURRGO_GNSS_TRACKER_PERIOD_S - период
 * между началами включений.
 *   PURRGO_GNSS_DUTY_FIX_TIMEOUT_S   сколько ждать фикс при горячем старте (BBR жив): по логам GNSS_BBR rx-ttff
 *                                    не превышал ~11 с даже при 4-5 спутниках.
 *   PURRGO_GNSS_DUTY_COLD_TIMEOUT_S  то же при холодном старте: первое включение без известного фикса, фикса не было
 *                                    дольше PURRGO_GNSS_DUTY_COLD_AGE_S или приёмник сообщил, что времени не знает
 *                                    (холодный старт на слабом небе занимал 65-82 с).
 *   PURRGO_GNSS_DUTY_COLD_AGE_S      возраст последнего фикса, после которого эфемериды считаются устаревшими
 *                                    (живут 2-4 ч).
 *   PURRGO_GNSS_DUTY_HOLD_MS         после первого фикса ждать точность hAcc не хуже PURRGO_GNSS_DUTY_ACCEPT_HACC_M
 *                                    не дольше этого времени, потом принять любой фикс (0 - принимать сразу).
 *   PURRGO_GNSS_DUTY_MIN_FIX_TYPE    минимальный тип фикса (2 - 2D, 3 - 3D).
 *   PURRGO_GNSS_DUTY_MIN_PAUSE_S     наименьшая пауза с EN = Low.
 *   PURRGO_GNSS_DUTY_BACKOFF_MAX_S   пауза после промахов удваивается (период, 2 периода, ...) до этого предела.
 *   PURRGO_GNSS_DUTY_SLOW_FIX_S      фикс дольше этого времени считается медленным: если медленных подряд
 *   PURRGO_GNSS_DUTY_SLOW_COUNT      штук, приёмник дешевле не выключать (рабочий ток ~13 мА при поиске против
 *                                    ~7 мА в PSMCT: граница около 8 с на период 15 с) и он остаётся включённым в
 *                                    PSMCT с периодом PURRGO_GNSS_TRACKER_CT_PERIOD_S. 0 - режим выключен.
 *   PURRGO_GNSS_DUTY_CONT_RETRY_S    через это время непрерывной работы снова пробуется цикл по EN.
 *   PURRGO_GNSS_DUTY_CONT_LOSS_S     фикса нет столько секунд - непрерывный режим кончается, дальше паузы как после
 *                                    промаха.
 */
#ifndef PURRGO_GNSS_DUTY_FIX_TIMEOUT_S
#define PURRGO_GNSS_DUTY_FIX_TIMEOUT_S 25
#endif
/*
 * Состояние линий UART, пока GNSS выключен по EN. 1 (по умолчанию) - обе в Hi-Z (Analog без подтяжки): у входа RXD
 * модуля есть внутренняя подтяжка к V_IO около 17 кОм, поэтому TX микроконтроллера в Low давал бы ток около 190 мкА,
 * а в High подпитывал бы модуль через защитные диоды (datasheet MAX-M10S, 3.2 и таблица Digital IO).
 * 0 - прежнее поведение: PA2 в Low (для сравнения замером тока).
 */
#ifndef PURRGO_GNSS_OFF_TX_HIZ
#define PURRGO_GNSS_OFF_TX_HIZ 1
#endif
#ifndef PURRGO_GNSS_DUTY_COLD_TIMEOUT_S
#define PURRGO_GNSS_DUTY_COLD_TIMEOUT_S 90
#endif
#ifndef PURRGO_GNSS_DUTY_COLD_AGE_S
#define PURRGO_GNSS_DUTY_COLD_AGE_S 7200
#endif
#ifndef PURRGO_GNSS_DUTY_HOLD_MS
#define PURRGO_GNSS_DUTY_HOLD_MS 3000
#endif
#ifndef PURRGO_GNSS_DUTY_ACCEPT_HACC_M
#define PURRGO_GNSS_DUTY_ACCEPT_HACC_M 30
#endif
#ifndef PURRGO_GNSS_DUTY_MIN_FIX_TYPE
#define PURRGO_GNSS_DUTY_MIN_FIX_TYPE 3
#endif
#ifndef PURRGO_GNSS_DUTY_MIN_PAUSE_S
#define PURRGO_GNSS_DUTY_MIN_PAUSE_S 1
#endif
#ifndef PURRGO_GNSS_DUTY_BACKOFF_MAX_S
#define PURRGO_GNSS_DUTY_BACKOFF_MAX_S 300
#endif
#ifndef PURRGO_GNSS_DUTY_SLOW_FIX_S
#define PURRGO_GNSS_DUTY_SLOW_FIX_S 8
#endif
#ifndef PURRGO_GNSS_DUTY_SLOW_COUNT
#define PURRGO_GNSS_DUTY_SLOW_COUNT 3
#endif
#ifndef PURRGO_GNSS_DUTY_CONT_RETRY_S
#define PURRGO_GNSS_DUTY_CONT_RETRY_S 600
#endif
#ifndef PURRGO_GNSS_DUTY_CONT_LOSS_S
#define PURRGO_GNSS_DUTY_CONT_LOSS_S 60
#endif
/* Период решений в непрерывном режиме (PSMCT, предел 10 с). */
#ifndef PURRGO_GNSS_TRACKER_CT_PERIOD_S
#define PURRGO_GNSS_TRACKER_CT_PERIOD_S 10
#endif
#ifndef PURRGO_GNSS_TRACKER_ONTIME_S
#define PURRGO_GNSS_TRACKER_ONTIME_S 2
#endif

#if (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_UBX) && (PURRGO_GNSS_TRACKER_MODE != PURRGO_GNSS_TRACKER_MODE_OFF)
#define PURRGO_GNSS_TRACKER_ACTIVE 1
#if PURRGO_GNSS_TRACKER_MODE == PURRGO_GNSS_TRACKER_MODE_EN
#define PURRGO_GNSS_TRACKER_EN_ACTIVE 1
#define PURRGO_GNSS_TRACKER_PM_FRAMES 1   /* профиль EN: один кадр OPERATEMODE = FULL (непрерывный профиль CT - два) */
/* Срок действия фикса в трекере меняется с паузами и берётся у политики: purrgo_gnss_tracker_fix_timeout_ms(). */
#else
#define PURRGO_GNSS_TRACKER_EN_ACTIVE 0
#define PURRGO_GNSS_TRACKER_PM_FRAMES 2   /* PSMCT: DONOTENTEROFF и OPERATEMODE; PSMOO: параметры и OPERATEMODE */
/*
 * Фикс в трекере моложе этого срока считается действующим: два периода (пропущенный цикл) и запас
 * на поиск спутников после потери сигнала - 30 с.
 */
#define PURRGO_GNSS_TRACKER_FIX_TIMEOUT_MS \
    ((2U * (unsigned)PURRGO_GNSS_TRACKER_PERIOD_S + 30U) * 1000U)
#endif
#if PURRGO_GNSS_TRACKER_MODE == PURRGO_GNSS_TRACKER_MODE_EN
/* Профили EN и CT выбираются во время работы (purrgo/gnss_duty.h); PSMOO и PSMCT "трекера" как режим не активны. */
#define PURRGO_GNSS_TRACKER_PSMOO_ACTIVE 0
#define PURRGO_GNSS_TRACKER_PSMCT_ACTIVE 0
#define PURRGO_GNSS_TRACKER_BAUD_FRAME 0
#if (PURRGO_GNSS_TRACKER_CT_PERIOD_S < 1) || (PURRGO_GNSS_TRACKER_CT_PERIOD_S > 10)
#error "PURRGO_GNSS_TRACKER_CT_PERIOD_S must be 1..10 for PSMCT (CFG-RATE-MEAS)"
#endif
#if PURRGO_GNSS_TRACKER_PERIOD_S < 1
#error "PURRGO_GNSS_TRACKER_PERIOD_S must be >= 1"
#endif
#elif PURRGO_GNSS_TRACKER_MODE == PURRGO_GNSS_TRACKER_MODE_OO
#define PURRGO_GNSS_TRACKER_PSMOO_ACTIVE 1
#define PURRGO_GNSS_TRACKER_PSMCT_ACTIVE 0
#define PURRGO_GNSS_TRACKER_BAUD_FRAME 1  /* скорость UART в слой BBR (после пробуждения RAM пуста) */
#if (PURRGO_GNSS_TRACKER_PERIOD_S < 5) || (PURRGO_GNSS_TRACKER_PERIOD_S > 3600)
#error "PURRGO_GNSS_TRACKER_PERIOD_S must be 5..3600 for PSMOO (CFG-PM-POSUPDATEPERIOD >= 5 s)"
#endif
#if (PURRGO_GNSS_TRACKER_ONTIME_S < 0) || (PURRGO_GNSS_TRACKER_ONTIME_S >= PURRGO_GNSS_TRACKER_PERIOD_S)
#error "PURRGO_GNSS_TRACKER_ONTIME_S must be 0..PURRGO_GNSS_TRACKER_PERIOD_S-1"
#endif
#else
#define PURRGO_GNSS_TRACKER_PSMOO_ACTIVE 0
#define PURRGO_GNSS_TRACKER_PSMCT_ACTIVE 1
#define PURRGO_GNSS_TRACKER_BAUD_FRAME 0
#if (PURRGO_GNSS_TRACKER_PERIOD_S < 1) || (PURRGO_GNSS_TRACKER_PERIOD_S > 10)
#error "PURRGO_GNSS_TRACKER_PERIOD_S must be 1..10 for PSMCT (CFG-RATE-MEAS)"
#endif
#endif
#else
#define PURRGO_GNSS_TRACKER_ACTIVE 0
#define PURRGO_GNSS_TRACKER_EN_ACTIVE 0
#define PURRGO_GNSS_TRACKER_PSMOO_ACTIVE 0
#define PURRGO_GNSS_TRACKER_PSMCT_ACTIVE 0
#define PURRGO_GNSS_TRACKER_PM_FRAMES 0
#define PURRGO_GNSS_TRACKER_BAUD_FRAME 0
#endif

/* Приёмнику могут включить любой режим PSM (PSMCT в навигации, PSMCT или PSMOO в трекере). */
#define PURRGO_GNSS_PSM_ANY ((PURRGO_GNSS_PSM_ACTIVE) || (PURRGO_GNSS_TRACKER_ACTIVE))

/*
 * Кадры PM в профиле навигации: при PSMCT - два (DONOTENTEROFF и OPERATEMODE); если PSMCT выключен, но
 * в трекере режим энергосбережения возможен - один (OPERATEMODE = FULL), чтобы возврат из трекера
 * однозначно возвращал полную мощность.
 */
#if PURRGO_GNSS_PSM_ACTIVE
#define PURRGO_GNSS_NAV_PM_FRAMES 2
#elif PURRGO_GNSS_TRACKER_ACTIVE
#define PURRGO_GNSS_NAV_PM_FRAMES 1
#else
#define PURRGO_GNSS_NAV_PM_FRAMES 0
#endif

/*
 * Набор спутниковых систем (CFG-SIGNAL-*), только для приёмника UBX.
 *
 * Заводской набор G10A-F30: GPS, Galileo, GLONASS, BeiDou B1C, QZSS, SBAS. В режиме
 * энергосбережения (PSM) по integration manual MAX-M10S (UBX-20053088, раздел 3.6.2):
 *   - BeiDou B1C в PSM не поддерживается (B1I поддерживается, но его нельзя включать
 *     одновременно с B1C и GLONASS L1OF - даташит G10A-F30, сноска 1);
 *   - SBAS в PSM не принимается, manual рекомендует его отключить.
 * Поэтому при PURRGO_GNSS_PSM != OFF (и при PSMOO в трекере) автоматически выключаются BeiDou и SBAS: в PSM они
 * не работают. GPS, Galileo, GLONASS и QZSS остаются (по даташиту ток слежения в PSM
 * одинаков для GPS+GAL и GPS+GAL+GLO).
 *
 * Дополнительно, сверх этого (меньше систем - меньше каналов и ток, но и меньше
 * видимых спутников в плотной застройке и под листвой):
 *
 *   PURRGO_GNSS_SIG_FACTORY (0) - ничего сверх требований PSM. По умолчанию.
 *   PURRGO_GNSS_SIG_GPS_GAL (1) - GPS и Galileo (и QZSS, ему нужен GPS): GLONASS и
 *                                 BeiDou выключаются. Заметно только в непрерывном
 *                                 режиме (даташит: на ~1 мА меньше); в PSMCT по
 *                                 даташиту выигрыша от выключения GLONASS нет.
 *   PURRGO_GNSS_SIG_GPS_GAL_BDS (2) - GPS, Galileo и BeiDou B1I (и QZSS): вместо GLONASS.
 *                                 B1I в PSM поддерживается, но не совместим с GLONASS L1OF
 *                                 и BeiDou B1C (даташит G10A-F30, сноска 1), поэтому кадр
 *                                 CFG-SIGNAL выключает GLONASS и B1C и включает B1I, всё в
 *                                 одном кадре VALSET (сначала выключения). По даташиту
 *                                 MAX-M10S ток слежения в PSM у GPS+GAL+GLO и GPS+GAL+BDS B1I
 *                                 одинаковый, холодный старт у B1I дольше (27 с против
 *                                 23 с); что даёт больше спутников, зависит от региона.
 *
 * Смена набора перезапускает ГНСС-подсистему приёмника, поэтому команда уходит
 * только при настройке после включения, до первого фикса.
 */
#define PURRGO_GNSS_SIG_FACTORY     0
#define PURRGO_GNSS_SIG_GPS_GAL     1
#define PURRGO_GNSS_SIG_GPS_GAL_BDS 2

#ifndef PURRGO_GNSS_SIGNALS
#define PURRGO_GNSS_SIGNALS PURRGO_GNSS_SIG_GPS_GAL_BDS
#endif
#if (PURRGO_GNSS_SIGNALS != PURRGO_GNSS_SIG_FACTORY) && (PURRGO_GNSS_SIGNALS != PURRGO_GNSS_SIG_GPS_GAL) && \
    (PURRGO_GNSS_SIGNALS != PURRGO_GNSS_SIG_GPS_GAL_BDS)
#error "PURRGO_GNSS_SIGNALS must be PURRGO_GNSS_SIG_FACTORY (0), PURRGO_GNSS_SIG_GPS_GAL (1) or PURRGO_GNSS_SIG_GPS_GAL_BDS (2)"
#endif
#if (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_UBX) && (PURRGO_GNSS_SIGNALS != PURRGO_GNSS_SIG_FACTORY)
#define PURRGO_GNSS_SIGNALS_ACTIVE 1
#else
#define PURRGO_GNSS_SIGNALS_ACTIVE 0
#endif

/*
 * Что делает кадр CFG-SIGNAL (0/1): SBAS и BeiDou выключаются вместе с PSM, GLONASS и BeiDou - при
 * PURRGO_GNSS_SIG_GPS_GAL, GLONASS и BeiDou B1C выключаются, а BeiDou B1I включается - при
 * PURRGO_GNSS_SIG_GPS_GAL_BDS (BeiDou при этом остаётся включённым, PURRGO_GNSS_SIG_OFF_BDS = 0).
 * Кадр отправляется, если меняется хоть одна система.
 */
#if PURRGO_GNSS_SIGNALS_ACTIVE && (PURRGO_GNSS_SIGNALS == PURRGO_GNSS_SIG_GPS_GAL_BDS)
#define PURRGO_GNSS_SIG_BDS_B1I 1
#else
#define PURRGO_GNSS_SIG_BDS_B1I 0
#endif
#define PURRGO_GNSS_SIG_OFF_SBAS (PURRGO_GNSS_PSM_ANY)
#define PURRGO_GNSS_SIG_OFF_BDS  ((((PURRGO_GNSS_PSM_ANY) || (PURRGO_GNSS_SIGNALS_ACTIVE))) && !(PURRGO_GNSS_SIG_BDS_B1I))
#define PURRGO_GNSS_SIG_OFF_GLO  (PURRGO_GNSS_SIGNALS_ACTIVE)
/* Число пар ключ-значение в кадре: B1I - три ключа (BDS_B1C = 0, BDS_B1 = 1, BDS = 1). */
#define PURRGO_GNSS_SIGNAL_COUNT (PURRGO_GNSS_SIG_OFF_SBAS + PURRGO_GNSS_SIG_OFF_BDS + PURRGO_GNSS_SIG_OFF_GLO + \
                                  3 * (PURRGO_GNSS_SIG_BDS_B1I))

#if PURRGO_GNSS_SIGNAL_COUNT != 0
#define PURRGO_GNSS_SIGNAL_FRAME 1
#else
#define PURRGO_GNSS_SIGNAL_FRAME 0
#endif

/*
 * Слои конфигурации UBX-CFG-VALSET (битовая маска: 0x01 RAM, 0x02 BBR,
 * 0x04 Flash). По умолчанию только RAM: настройка отправляется при каждом
 * старте, а ресурс Flash модуля не расходуется. Значение 0x07 сохранит
 * настройку в Flash модуля.
 */
#ifndef PURRGO_GNSS_UBX_LAYERS
#define PURRGO_GNSS_UBX_LAYERS 0x01
#endif

/*
 * Слои для настройки, которая должна пережить состояние "off" PSMOO (RAM приёмника в нём очищается):
 * основные слои плюс BBR.
 */
#define PURRGO_GNSS_UBX_LAYERS_PERSIST ((PURRGO_GNSS_UBX_LAYERS) | 0x02)

/*
 * Цикл по EN (PURRGO_GNSS_TRACKER_MODE_EN): настройка профиля EN пишется в слои RAM и BBR. RAM при EN = Low стирается,
 * а BBR и навигационные данные остаются (docs/gnss-retention.md, раздел 4.2): после EN = High модуль сам начинает
 * выдавать NAV-PVT/STATUS и молчит по NMEA, поэтому настройку после каждого включения слать не нужно.
 *   PURRGO_GNSS_TRACKER_EN_BBR      1 - так (по умолчанию); 0 - как раньше: настройка только в RAM и после каждого
 *                                   включения заново.
 *   PURRGO_GNSS_EN_CFG_WAIT_MS      сколько ждать после установления связи первый кадр NAV-PVT, прежде чем решить,
 *                                   что настройка в BBR потеряна (нет резервного питания, сброс) и отправить её снова.
 *                                   Кадр идёт раз в эпоху (1 Гц), поэтому не меньше ~1200.
 *   PURRGO_GNSS_EN_CFG_SETTLE_MS    после отправки настройки модуль не выключается, пока не пройдёт столько мс:
 *                                   запись в BBR применяется не мгновенно, а выключение по EN сразу после фикса
 *                                   оборвало бы её.
 * Первое включение после перезапуска МК всегда отправляет настройку (BBR мог остаться от другой версии прошивки).
 * Если после этого NAV-PVT не пришёл, отправка после каждого включения возвращается до перезапуска МК.
 */
#ifndef PURRGO_GNSS_TRACKER_EN_BBR
#define PURRGO_GNSS_TRACKER_EN_BBR 1
#endif
#ifndef PURRGO_GNSS_EN_CFG_WAIT_MS
#define PURRGO_GNSS_EN_CFG_WAIT_MS 1500
#endif
#ifndef PURRGO_GNSS_EN_CFG_SETTLE_MS
#define PURRGO_GNSS_EN_CFG_SETTLE_MS 200
#endif

/*
 * Трекер и питание SD: если период трекера не меньше этого значения (с), файл трека между записями закрывается и
 * карта сразу отключается (purrgo_logger_set_release_between_writes(), purrgo_fs_idle_hint()), а не после тайм-аута
 * простоя. Следующая запись (буфер GPX заполнился или пришёл срок синхронизации) снова включает карту. При более
 * коротком периоде включение карты (десятки - сотни мс) не окупается, файл остаётся открытым. 0 - никогда не закрывать.
 */
#ifndef PURRGO_TRACKER_SD_RELEASE_MIN_PERIOD_S
#define PURRGO_TRACKER_SD_RELEASE_MIN_PERIOD_S 15
#endif

/*
 * ============================================================================
 * Расширения UBX (u-blox M10, SPG 5.x): PURRGO_GNSS_UBX
 * ============================================================================
 *
 *   PURRGO_GNSS_UBX = 0 - только NMEA (GGA, RMC, GSA), как раньше. Точность
 *                         (hAcc), одометр модуля, индикаторы помех/глушения/
 *                         спуфинга и версия прошивки недоступны; в логе трека
 *                         нет pg:hacc, фильтр точек работает по HDOP, строки
 *                         TRP/ACC/SIG/RF на экране Trip computer не рисуются.
 *   PURRGO_GNSS_UBX = 1 - дополнительно UBX (по умолчанию для приёмника UBX):
 *     - опрос UBX-MON-VER при старте (только при PURRGO_GNSS_POLL_VERSION = 1), версия в лог;
 *     - UBX-NAV-PVT (время, позиция, скорость, оценки точности hAcc/vAcc/sAcc);
 *       пока PVT приходит, он главнее RMC/GGA/GSA (те дают только HDOP/VDOP и
 *       резерв при пропаже PVT);
 *     - UBX-NAV-ODO: дистанция с начала записи трека (при старте нового трека
 *       отправляется UBX-NAV-RESETODO);
 *     - UBX-SEC-SIG (глушение, спуфинг) и UBX-MON-RF (уровень помех, антенна).
 *   Включается автоматически только вместе с PURRGO_HW_GNSS_RECEIVER =
 *   PURRGO_GNSS_RECEIVER_UBX (для GY-NEO6MV2 и т.п. ставьте 0).
 *   Все данные выдаются модулем раз в PURRGO_GNSS_UPDATE_PERIOD_MS, а ядро берёт
 *   их и показывает не чаще раза в секунду.
 */
#ifndef PURRGO_GNSS_UBX
#if (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_UBX)
#define PURRGO_GNSS_UBX 1
#else
#define PURRGO_GNSS_UBX 0
#endif
#endif

#if (PURRGO_GNSS_UBX != 0) && (PURRGO_GNSS_UBX != 1)
#error "PURRGO_GNSS_UBX must be 0 or 1"
#endif
#if (PURRGO_GNSS_UBX == 1) && (PURRGO_HW_GNSS_RECEIVER != PURRGO_GNSS_RECEIVER_UBX)
#error "PURRGO_GNSS_UBX=1 requires PURRGO_HW_GNSS_RECEIVER = PURRGO_GNSS_RECEIVER_UBX"
#endif

/*
 * NMEA на UART приёмника u-blox: PURRGO_GNSS_UBX_NMEA (только при PURRGO_GNSS_UBX = 1).
 *
 *   0 (по умолчанию) - NMEA полностью выключен: после установления связи с модулем (скорость
 *     UART определена по NMEA заводской настройки, gnss_link) последним кадром настройки
 *     уходит CFG-UART1OUTPROT-NMEA = 0 (PURRGO_GNSS_NMEA_OFF = 1). Остаётся только UBX:
 *     меньше нагрузка на UART (около 300-500 байт/с) и между бинарными кадрами нет текста.
 *     HDOP/VDOP берутся из UBX-NAV-DOP, фикс и число спутников - из UBX-NAV-PVT.
 *     Если модуль отклонит кадр (NAK), останется прежний сокращённый NMEA (GGA, RMC, GSA):
 *     он по-прежнему разбирается как запасной путь, как и NMEA до настройки.
 *   1 - NMEA остаётся включённым (GGA, RMC, GSA, 1 Гц) рядом с UBX, как раньше; полезно для
 *     отладки (u-center, внешние программы).
 *
 * Модуль помнит настройку только в слое RAM (PURRGO_GNSS_UBX_LAYERS) и после выключения по EN
 * возвращается к заводской (NMEA, 38400): поиск скорости и настройка проходят заново. После
 * сброса одного МК (модуль не обесточен) NMEA у модуля уже выключен, поэтому gnss_link
 * принимает за признак связи и корректный кадр UBX, а не только предложение NMEA.
 */
#ifndef PURRGO_GNSS_UBX_NMEA
#define PURRGO_GNSS_UBX_NMEA 0
#endif
#if (PURRGO_GNSS_UBX_NMEA != 0) && (PURRGO_GNSS_UBX_NMEA != 1)
#error "PURRGO_GNSS_UBX_NMEA must be 0 or 1"
#endif
#if PURRGO_GNSS_UBX && !PURRGO_GNSS_UBX_NMEA
#define PURRGO_GNSS_NMEA_OFF 1
#define PURRGO_GNSS_NMEA_OFF_FRAMES 1   /* кадр CFG-UART1OUTPROT-NMEA */
#else
#define PURRGO_GNSS_NMEA_OFF 0
#define PURRGO_GNSS_NMEA_OFF_FRAMES 0
#endif
/* UBX-NAV-DOP (HDOP/VDOP) включается кадром настройки вместе с остальными сообщениями UBX. */
#define PURRGO_GNSS_DOP_FRAMES (PURRGO_GNSS_UBX)

/*
 * Сигнал 1PPS (TIMEPULSE) модуля, только при PURRGO_GNSS_UBX = 1. Прошивка 1PPS не использует (вывод
 * 1PPS модуля к МК не подключается), а светодиод PPS на плате модуля мигает и тратит ток.
 * Отдельным кадром VALSET (слой PURRGO_GNSS_UBX_LAYERS, по умолчанию RAM, поэтому после каждого
 * включения модуля):
 *   PURRGO_GNSS_TIMEPULSE_FACTORY (0)  заводское поведение: импульс 100 мс раз в секунду при валидном времени.
 *   PURRGO_GNSS_TIMEPULSE_QUIET   (1)  CFG-TP-TP1_ENA = 1, CFG-TP-LEN_TP1 = 0, CFG-TP-LEN_LOCK_TP1 = 0:
 *                                      вывод включён, но импульсов нет (длина 0, как у модуля до захвата
 *                                      времени), на выводе остаётся пассивный уровень, светодиод не горит.
 *   PURRGO_GNSS_TIMEPULSE_DISABLED (2) CFG-TP-TP1_ENA = 0: вывод отпускается. На плате G10A F30 в этом
 *                                      режиме светодиод PPS горит постоянно (замер), поэтому не по умолчанию.
 */
#define PURRGO_GNSS_TIMEPULSE_FACTORY  0
#define PURRGO_GNSS_TIMEPULSE_QUIET    1
#define PURRGO_GNSS_TIMEPULSE_DISABLED 2
#ifndef PURRGO_GNSS_TIMEPULSE
#define PURRGO_GNSS_TIMEPULSE PURRGO_GNSS_TIMEPULSE_QUIET
#endif
#if (PURRGO_GNSS_TIMEPULSE < 0) || (PURRGO_GNSS_TIMEPULSE > 2)
#error "PURRGO_GNSS_TIMEPULSE must be 0 (factory), 1 (quiet) or 2 (disabled)"
#endif
#define PURRGO_GNSS_TIMEPULSE_FRAMES (PURRGO_GNSS_UBX && (PURRGO_GNSS_TIMEPULSE != PURRGO_GNSS_TIMEPULSE_FACTORY))

/*
 * UBX-SEC-SIG (глушение и спуфинг) есть только в SPG 5.30 и новее. В SPG 5.10
 * (G10A-F30, "ROM SPG 5.10") ключа CFG-MSGOUT-UBX_SEC_SIG_UART1 нет, модуль отвечает NAK.
 * Включать (-DPURRGO_GNSS_SEC_SIG=1) только для модуля с прошивкой 5.30+; без него строка
 * SIG строится из MON-RF и NAV-STATUS (см. PURRGO_GNSS_ITFM ниже).
 */
#ifndef PURRGO_GNSS_SEC_SIG
#define PURRGO_GNSS_SEC_SIG 0
#endif

/*
 * Замена SEC-SIG и контроля антенны для SPG 5.10 (G10A-F30), только при PURRGO_GNSS_UBX = 1.
 * Каждый флаг: 1 - включить, 0 - выключить (для замеров тока и отладки).
 *
 *   PURRGO_GNSS_ITFM       CFG-ITFM-ENABLE = 1: монитор помех модуля (по умолчанию выключен).
 *                          Состояние берётся из UBX-MON-RF.flags.jammingState (в строке SIG: JAM).
 *                          Опорный спектр монитор снимает после первого хорошего фикса, до этого
 *                          состояние "--" (неизвестно).
 *   PURRGO_GNSS_NAV_STATUS UBX-NAV-STATUS (16 байт/с): flags2.spoofDetState (в строке SIG: SPF).
 *                          Детектор спуфинга в модуле включён всегда.
 *   PURRGO_GNSS_NAV_SAT    UBX-NAV-SAT (8 + 12 * N байт/с): средний C/N0 спутников решения
 *                          (строка RF с C/N0 убрана из Trip
 *                          computer, по умолчанию 0). Модуль выдаёт все известные спутники, в том
 *                          числе не используемые в решении.
 *
 * При PURRGO_GNSS_SEC_SIG = 1 (SPG 5.30+) в строке SIG остаётся SEC-SIG, а эти источники
 * используются только пока SEC-SIG не приходит.
 * Контроль антенны (antStatus в MON-RF) у G10A-F30 недоступен: нужны внешняя схема и свободные
 * выводы PIO, а у модуля 6 выводов и встроенная антенна. Поле всегда "DONTKNOW"; на экране
 * состояние антенны не выводится.
 */
#ifndef PURRGO_GNSS_ITFM
#define PURRGO_GNSS_ITFM 1
#endif
#ifndef PURRGO_GNSS_NAV_STATUS
#define PURRGO_GNSS_NAV_STATUS 1
#endif
#ifndef PURRGO_GNSS_NAV_SAT
#define PURRGO_GNSS_NAV_SAT 0       /* C/N0 больше нигде не выводится (строку RF убрали): команда не отправляется */
#endif
#if (PURRGO_GNSS_ITFM != 0 && PURRGO_GNSS_ITFM != 1) || \
    (PURRGO_GNSS_NAV_STATUS != 0 && PURRGO_GNSS_NAV_STATUS != 1) || \
    (PURRGO_GNSS_NAV_SAT != 0 && PURRGO_GNSS_NAV_SAT != 1)
#error "PURRGO_GNSS_ITFM, PURRGO_GNSS_NAV_STATUS and PURRGO_GNSS_NAV_SAT must be 0 or 1"
#endif
/*
 * Отладочный вывод GNSS в лог (по умолчанию выключен, чтобы не забивать лог).
 *   PURRGO_GNSS_LOG_PERIODIC = 1 - периодически (раз в PVT_LOG_INTERVAL) писать строки
 *                                  "GNSS: PVT ...", "GNSS: RF ...", "GNSS: STATUS ...",
 *                                  "GNSS: SAT ..." (и смену состояния помех/спуфинга).
 *   PURRGO_GNSS_POLL_VERSION = 1 - при настройке запрашивать версию прошивки (UBX-MON-VER)
 *                                  и писать ответ в лог. При 0 запрос не отправляется.
 * Включить: #define PURRGO_GNSS_LOG_PERIODIC 1 до этого места или -D в сборке.
 */
#ifndef PURRGO_GNSS_LOG_PERIODIC
#define PURRGO_GNSS_LOG_PERIODIC 0
#endif
#ifndef PURRGO_GNSS_POLL_VERSION
#define PURRGO_GNSS_POLL_VERSION 0
#endif
#if (PURRGO_GNSS_LOG_PERIODIC != 0 && PURRGO_GNSS_LOG_PERIODIC != 1) || \
    (PURRGO_GNSS_POLL_VERSION != 0 && PURRGO_GNSS_POLL_VERSION != 1)
#error "PURRGO_GNSS_LOG_PERIODIC and PURRGO_GNSS_POLL_VERSION must be 0 or 1"
#endif
/* Кадр опроса MON-VER в настройке (учитывается в тестах). */
#define PURRGO_GNSS_VERSION_FRAMES (PURRGO_GNSS_UBX && PURRGO_GNSS_POLL_VERSION)

/* Кадры настройки VALSET (по одному на ключ), только для приёмника с UBX-расширениями. */
#define PURRGO_GNSS_ITFM_FRAMES       (PURRGO_GNSS_UBX && PURRGO_GNSS_ITFM)
#define PURRGO_GNSS_NAV_STATUS_FRAMES (PURRGO_GNSS_UBX && PURRGO_GNSS_NAV_STATUS)
#define PURRGO_GNSS_NAV_SAT_FRAMES    (PURRGO_GNSS_UBX && PURRGO_GNSS_NAV_SAT)
#define PURRGO_GNSS_DIAG_FRAMES \
    (PURRGO_GNSS_ITFM_FRAMES + PURRGO_GNSS_NAV_STATUS_FRAMES + PURRGO_GNSS_NAV_SAT_FRAMES)

/*
 * Модель динамики приёмника (CFG-NAVSPG-DYNMODEL), только при PURRGO_GNSS_UBX = 1.
 * Отправляется отдельной командой UBX-CFG-VALSET: если модуль не знает значения
 * (NAK), остальная настройка не страдает, а модель остаётся прежней.
 *
 * Модели, перечисленные в даташите G10A-F30: Portable, Stationary, Pedestrian,
 * Automotive, Sea, Airborne 1g/2g/4g, Wrist. Велосипедной модели у приёмника нет:
 * значение BIKE (10) в SPG 5.30 - МОТОЦИКЛ ("не во всех продуктах"), в даташите
 * модуля его нет. Для велосипеда подходит Portable (универсальная, до 310 м/с);
 * Pedestrian (до 30 м/с, сильнее сглаживает) - вариант для очень медленной езды.
 * Выбор: -DPURRGO_GNSS_DYNMODEL=<число или PURRGO_GNSS_DYN_*>.
 */
#define PURRGO_GNSS_DYN_PORTABLE     0
#define PURRGO_GNSS_DYN_STATIONARY   2
#define PURRGO_GNSS_DYN_PEDESTRIAN   3
#define PURRGO_GNSS_DYN_AUTOMOTIVE   4
#define PURRGO_GNSS_DYN_SEA          5
#define PURRGO_GNSS_DYN_AIRBORNE_1G  6
#define PURRGO_GNSS_DYN_AIRBORNE_2G  7
#define PURRGO_GNSS_DYN_AIRBORNE_4G  8
#define PURRGO_GNSS_DYN_WRIST        9
#define PURRGO_GNSS_DYN_BICYCLE      PURRGO_GNSS_DYN_PORTABLE

#ifndef PURRGO_GNSS_DYNMODEL
#define PURRGO_GNSS_DYNMODEL PURRGO_GNSS_DYN_BICYCLE
#endif
#if (PURRGO_GNSS_DYNMODEL < 0) || (PURRGO_GNSS_DYNMODEL > 15)
#error "PURRGO_GNSS_DYNMODEL must be 0..15 (see CFG-NAVSPG-DYNMODEL)"
#endif

/*
 * Профиль одометра (CFG-ODO-PROFILE): 0 RUN, 1 CYCL (велосипед), 2 SWIM, 3 CAR.
 * Одометр включается ключом CFG-ODO-USE_ODO (по умолчанию он выключен).
 */
#define PURRGO_GNSS_ODO_RUN   0
#define PURRGO_GNSS_ODO_CYCL  1
#define PURRGO_GNSS_ODO_SWIM  2
#define PURRGO_GNSS_ODO_CAR   3

#ifndef PURRGO_GNSS_ODO_PROFILE
#define PURRGO_GNSS_ODO_PROFILE PURRGO_GNSS_ODO_CYCL
#endif
#if (PURRGO_GNSS_ODO_PROFILE < 0) || (PURRGO_GNSS_ODO_PROFILE > 4)
#error "PURRGO_GNSS_ODO_PROFILE must be 0..4 (see CFG-ODO-PROFILE)"
#endif

/*
 * Фильтр точек трека по оценке горизонтальной точности hAcc (UBX-NAV-PVT), метры:
 * точка с hAcc выше порога в трек не пишется. Когда hAcc недоступна (NMEA-режим
 * или PVT не приходит), работает прежний фильтр по HDOP.
 */
#ifndef PURRGO_GNSS_TRACK_MAX_HACC_M
#define PURRGO_GNSS_TRACK_MAX_HACC_M 20U
#endif

/*
 * Повторная отправка настройки (набор NMEA) через столько мс после установления
 * связи с модулем (0 - не повторять). Связь установлена, когда от модуля принято
 * корректное предложение NMEA на рабочей скорости (см. PURRGO_GNSS_BAUD_*);
 * повтор нужен на случай потерянной команды, настройка идемпотентна.
 */
#ifndef PURRGO_GNSS_CONFIG_RETRY_MS
#define PURRGO_GNSS_CONFIG_RETRY_MS 2000
#endif

/*
 * Скорость UART связи с GNSS-модулем (только STM32, stm32/port/gnss).
 *
 * PURRGO_GNSS_BAUD_DEFAULT - скорость модуля после включения (G10A F30: 38400,
 * настройка слоя RAM пропадает при выключении по EN).
 * PURRGO_GNSS_BAUD_TARGET  - рабочая скорость. Если она равна DEFAULT, скорость
 * не меняется, но настройка NMEA всё равно отправляется только после приёма
 * корректного предложения от модуля.
 *
 * Порядок при включении: слушать DEFAULT; при корректном NMEA отправить команду
 * смены скорости (UBX-CFG-VALSET CFG-UART1-BAUDRATE, слой PURRGO_GNSS_UBX_LAYERS),
 * переключить UART на TARGET и дождаться NMEA; если NMEA нет - вернуться на
 * DEFAULT и повторить. Если модуль уже работает на TARGET (сброс МК без
 * выключения модуля или настройка сохранена в BBR/Flash), он находится слушанием
 * TARGET. После PURRGO_GNSS_LINK_ATTEMPTS неудач остаётся DEFAULT (или ошибка,
 * если модуль молчит).
 *
 * PURRGO_GNSS_LINK_PROBE_MS - сколько слушать на одной скорости; модуль выдаёт
 * NMEA раз в секунду, поэтому меньше 1100 мс ставить нельзя.
 */
#ifndef PURRGO_GNSS_BAUD_DEFAULT
#define PURRGO_GNSS_BAUD_DEFAULT 38400U
#endif
#ifndef PURRGO_GNSS_BAUD_TARGET
#define PURRGO_GNSS_BAUD_TARGET 115200U
#endif
#ifndef PURRGO_GNSS_LINK_PROBE_MS
#define PURRGO_GNSS_LINK_PROBE_MS 1300U
#endif
#ifndef PURRGO_GNSS_LINK_ATTEMPTS
#define PURRGO_GNSS_LINK_ATTEMPTS 3U
#endif

#if (PURRGO_GNSS_BAUD_DEFAULT < 9600U) || (PURRGO_GNSS_BAUD_DEFAULT > 921600U) || \
    (PURRGO_GNSS_BAUD_TARGET < 9600U) || (PURRGO_GNSS_BAUD_TARGET > 921600U)
#error "PURRGO_GNSS_BAUD_DEFAULT/TARGET must be in 9600..921600"
#endif
#if (PURRGO_GNSS_LINK_PROBE_MS < 1100U)
#error "PURRGO_GNSS_LINK_PROBE_MS must be >= 1100 (NMEA at 1 Hz)"
#endif
#if (PURRGO_GNSS_LINK_ATTEMPTS < 1U)
#error "PURRGO_GNSS_LINK_ATTEMPTS must be >= 1"
#endif


/*
 * ============================================================================
 * Измерение напряжения батареи (PB1 = ADC1_IN16, метка V_MAIN_BAT)
 * ============================================================================
 *
 * Делитель из двух резисторов между плюсом батарейного отсека и GND, средняя
 * точка на PB1, конденсатор 100 нФ между PB1 и GND. Напряжение блока:
 *
 *   U_блока = U_PB1 * (R_TOP + R_BOTTOM) / R_BOTTOM * CAL / 1000
 *
 * R_TOP и R_BOTTOM - фактические сопротивления резисторов. CAL_PERMILLE - поправка в тысячных: сравнить U_блока в логе
 * (PURRGO_LOG_LEVEL >= 3) с мультиметром, CAL = 1000 * U_мультиметр / U_лога.
 *
 * При питании 5 В (источник PURRGO_POWER_SRC_EXTERNAL_5V: сигнал ORing или ручной
 * выбор в меню) батарея не измеряется: АЦП не опрашивается вообще, чтобы его
 * регулятор и VREFINT зря не включались (purrgo_battery_read(), purrgo/battery.h).
 */
#ifndef PURRGO_HW_BATT_DIV_R_TOP_OHM
#define PURRGO_HW_BATT_DIV_R_TOP_OHM     1004000u
#endif
#ifndef PURRGO_HW_BATT_DIV_R_BOTTOM_OHM
#define PURRGO_HW_BATT_DIV_R_BOTTOM_OHM  997000u
#endif
#ifndef PURRGO_HW_BATT_CAL_PERMILLE
#define PURRGO_HW_BATT_CAL_PERMILLE      1004u
#endif

/* Не чаще одного измерения за столько мс; между ними возвращается прошлое значение. */
#ifndef PURRGO_HW_BATT_PERIOD_MS
#define PURRGO_HW_BATT_PERIOD_MS         10000u
#endif

/*
 * ============================================================================
 * Модуль ORing LM66200 (WeAct) и определение питания 5 В (PA8, метка PWR_SRC)
 * ============================================================================
 *
 * Схема: VI1 = BAT+ (после SW1), VI2 = 5 В отдельного USB-C, VO = VIN TPS63802.
 * Вывод ST модуля (на PA8, вход без подтяжки, EXTI): высокий - работает вход VI1
 * (батареи), низкий - вход VI2 (5 В, горит синий светодиод модуля). Определение
 * включено по умолчанию (модуль установлен, PA8 подключён к ST). Если ST не подключён, вход
 * плавает и показывал бы случайный уровень (при «низком» ядро приняло бы питание за 5 В
 * и отключило бы контроль батарей): в такой сборке задать -DPURRGO_HW_ORING_PRESENT=0.
 *
 * Уровень ST - напряжение VI1, то есть до ~5,4 В у свежих Li-FeS2: вывод должен быть
 * 5V-tolerant (PA8 - FT_hv) и без внутренней подтяжки (подтяжка к VDD дала бы ток
 * в VDD при ST выше 3,3 В).
 */
#ifndef PURRGO_HW_ORING_PRESENT
#define PURRGO_HW_ORING_PRESENT          1
#endif

/*
 * Режим передачи на панель E-Ink (4 gray / 1 bit) выбирается не при сборке, а
 * по экрану: ядро вызывает display_set_mode() (purrgo/display_hal.h). Внутренний
 * framebuffer PurrGO в обоих режимах остаётся 2-битным.
 */

/*
 * ============================================================================
 * Display resolution
 * ============================================================================
 */

#define PURRGO_HW_DISPLAY_WIDTH_PX      176
#define PURRGO_HW_DISPLAY_HEIGHT_PX     264

/*
 * Область карты на экране: между верхней и нижней строками состояния
 * (по 9 px каждая). Используется и отрисовкой (ui_map.c), и контроллером карты
 * (шаг сдвига по вертикали считается от высоты области).
 */
#define PURRGO_MAP_STATUS_BAR_H_PX      9
#define PURRGO_MAP_VIEWPORT_HEIGHT_PX \
    (PURRGO_HW_DISPLAY_HEIGHT_PX - 2 * PURRGO_MAP_STATUS_BAR_H_PX)

/*
 * Перерисовка карты на экране карты (сдвиг кнопками, автоследование, масштаб):
 *   1 - частичное обновление по маске изменившихся пикселей (E-Ink 4 gray: ~1,3 с, без
 *       мигания; после EPD4G_PARTIALS_BEFORE_FULL частичных подряд драйвер сам делает
 *       полное обновление, чтобы сбросить следы);
 *   0 - полное обновление (~3 с, весь экран мигает), как раньше.
 * Вход на экран карты, смена режима панели и первое обновление всегда выполняются полными.
 */
#ifndef PURRGO_MAP_SHIFT_PARTIAL_REFRESH
#define PURRGO_MAP_SHIFT_PARTIAL_REFRESH 1
#endif

/*
 * Сколько перерисовок карты подряд выводятся частично, затем одно полное (сброс следов).
 * Сдвиг меняет почти весь кадр, поэтому лимит строже общего лимита драйвера (10 частичных):
 * на железе после 5 сдвигов подряд были видны следы.
 */
#ifndef PURRGO_MAP_SHIFT_PARTIALS_BEFORE_FULL
#define PURRGO_MAP_SHIFT_PARTIALS_BEFORE_FULL 4
#endif

/*
 * Шаг ручного сдвига карты за одно нажатие:
 *   по горизонтали - 1/PURRGO_MAP_PAN_STEP_X_DIV ширины окна карты (1/3),
 *   по вертикали   - 1/PURRGO_MAP_PAN_STEP_Y_DIV высоты окна карты (1/3).
 */
#ifndef PURRGO_MAP_PAN_STEP_X_DIV
#define PURRGO_MAP_PAN_STEP_X_DIV 3
#endif
#ifndef PURRGO_MAP_PAN_STEP_Y_DIV
#define PURRGO_MAP_PAN_STEP_Y_DIV 3
#endif

/*
 * --------------------------------------------------------------------------
 * Display diagonal
 * --------------------------------------------------------------------------
 */

#ifndef PURRGO_HW_DISPLAY_DIAGONAL_MM
#define PURRGO_HW_DISPLAY_DIAGONAL_MM   74 /* 2.7 дюйма = ~73.66 мм */
#endif

/*
 * --------------------------------------------------------------------------
 * Display color depth
 * --------------------------------------------------------------------------
 */

#ifndef PURRGO_HW_DISPLAY_BPP
#define PURRGO_HW_DISPLAY_BPP 2
#endif

/*
 * --------------------------------------------------------------------------
 * Display orientation
 * --------------------------------------------------------------------------
 */

#define PURRGO_DISPLAY_ORIENTATION_0    0
#define PURRGO_DISPLAY_ORIENTATION_90   1
#define PURRGO_DISPLAY_ORIENTATION_180  2
#define PURRGO_DISPLAY_ORIENTATION_270  3

#ifndef PURRGO_HW_DISPLAY_ORIENTATION
#define PURRGO_HW_DISPLAY_ORIENTATION PURRGO_DISPLAY_ORIENTATION_0
#endif


/*
 * --------------------------------------------------------------------------
 * Звуковые сигналы (пьезоизлучатель, purrgo/sound.h)
 * --------------------------------------------------------------------------
 *
 * Пьезоизлучатель включён между PA6 (TIM16_CH1) и PB6 (TIM16_CH1N).
 *
 * HIGH: выводы переключаются в противофазе (мост), на излучателе +-3,3 В, то есть 6,6 В размаха.
 * LOW: переключается только PA6, а PB6 держится в Low (GPIO): размах 3,3 В, вдвое меньше,
 * то есть примерно на 6 дБ тише при той же форме сигнала (PURRGO_SOUND_LOW_SINGLE_ENDED).
 * Громкость не регулируется скважностью как основным способом: теория (амплитуда основной
 * гармоники пропорциональна sin(pi * D)) обещает падение, но у реального излучателя с резонансом
 * корпуса узкие импульсы звучат почти так же громко, зато меняют тембр. Скважности ниже
 * (особенно 10 % и 4 % у HIGH) - уже подобранные по месту значения, их можно менять независимо.
 */

/* Частота тона, Гц: около резонанса излучателя (пьезодиски 2-4,6 кГц). */
#ifndef PURRGO_SOUND_FREQ_HZ
#define PURRGO_SOUND_FREQ_HZ 4000U
#endif

/* LOW: 1 - питать излучатель одним выводом (PA6), PB6 в Low: размах вдвое меньше, около -6 дБ к HIGH; 0 - как HIGH (мост). */
#ifndef PURRGO_SOUND_LOW_SINGLE_ENDED
#define PURRGO_SOUND_LOW_SINGLE_ENDED 1
#endif

/*
 * Скважность, % периода (1..50), по классам сигнала (LOUD - ERROR и ARRIVED, MID - INFO,
 * QUIET - щелчок) и по настройке SOUND меню: HIGH - значения без суффикса, LOW - с суффиксом _LOW.
 * По умолчанию LOW повторяет скважности HIGH: тогда разница громкости - ровно уменьшенный размах
 * (PURRGO_SOUND_LOW_SINGLE_ENDED). Если LOW нужен ещё тише, уменьшайте скважности _LOW.
 */
#ifndef PURRGO_SOUND_DUTY_LOUD_PCT
#define PURRGO_SOUND_DUTY_LOUD_PCT 50U
#endif
#ifndef PURRGO_SOUND_DUTY_MID_PCT
#define PURRGO_SOUND_DUTY_MID_PCT 10U
#endif
#ifndef PURRGO_SOUND_DUTY_QUIET_PCT
#define PURRGO_SOUND_DUTY_QUIET_PCT 4U
#endif
#ifndef PURRGO_SOUND_DUTY_LOUD_LOW_PCT
#define PURRGO_SOUND_DUTY_LOUD_LOW_PCT PURRGO_SOUND_DUTY_LOUD_PCT
#endif
#ifndef PURRGO_SOUND_DUTY_MID_LOW_PCT
#define PURRGO_SOUND_DUTY_MID_LOW_PCT PURRGO_SOUND_DUTY_MID_PCT
#endif
#ifndef PURRGO_SOUND_DUTY_QUIET_LOW_PCT
#define PURRGO_SOUND_DUTY_QUIET_LOW_PCT PURRGO_SOUND_DUTY_QUIET_PCT
#endif

/* Длительности, мс. ERROR - один длинный; ARRIVED - два сигнала с паузой; INFO - короткий; CLICK - щелчок. */
#ifndef PURRGO_SOUND_ERROR_MS
#define PURRGO_SOUND_ERROR_MS 1200U
#endif
#ifndef PURRGO_SOUND_ARRIVED_MS
#define PURRGO_SOUND_ARRIVED_MS 200U
#endif
#ifndef PURRGO_SOUND_ARRIVED_GAP_MS
#define PURRGO_SOUND_ARRIVED_GAP_MS 150U
#endif
#ifndef PURRGO_SOUND_INFO_MS
#define PURRGO_SOUND_INFO_MS 100U
#endif
#ifndef PURRGO_SOUND_CLICK_MS
#define PURRGO_SOUND_CLICK_MS 3U
#endif

/* 1 - короткий сигнал INFO при первом фиксе GNSS после включения (отладка и удобство); 0 - не подавать. */
#ifndef PURRGO_SOUND_FIX_BEEP
#define PURRGO_SOUND_FIX_BEEP 1
#endif


#endif /* PURRGO_HARDWARE_CONFIG_H */
