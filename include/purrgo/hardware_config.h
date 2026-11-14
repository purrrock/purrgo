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

// #define PURRGO_GNSS_MOCK             1
// #define PURRGO_HW_GNSS PURRGO_GNSS_MOCK

/*
 * Модель приёмника: определяет, какие настроечные команды ему отправляются при
 * старте (см. purrgo/gnss_receiver.h).
 *
 *   PURRGO_GNSS_RECEIVER_NONE  - ничего не отправлять (приёмник работает с
 *                                настройками по умолчанию, например отладочный
 *                                GY-NEO6MV2);
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
 * Режим PSMOO (ON/OFF, значение 1, для периодов решений дольше 10 с) не поддержан
 * намеренно: между фиксами приёмник молчит, а ядро (таймаут фикса, запись трека раз в
 * секунду) и связь с модулем (gnss_link: контроль молчания) рассчитаны на поток данных
 * каждую секунду. Для PSMOO к тому же нужен RTC в модуле, а настройку придётся хранить
 * и в слое BBR (RAM очищается в состоянии "off").
 *
 * Порядок настройки (последними, после всей остальной конфигурации; OPERATEMODE - самым
 * последним ключом, как рекомендует manual):
 *  1. Набор систем (см. ниже): в PSM не поддержаны BeiDou B1C и SBAS. Без этого на
 *     G10A-F30 с заводским набором (GPS, Galileo, GLONASS, BeiDou B1C, QZSS, SBAS)
 *     кадр PSM был отклонён модулем (NAK, psmState оставался 0).
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
 * Набор спутниковых систем (CFG-SIGNAL-*), только для приёмника UBX.
 *
 * Заводской набор G10A-F30: GPS, Galileo, GLONASS, BeiDou B1C, QZSS, SBAS. В режиме
 * энергосбережения (PSM) по integration manual MAX-M10S (UBX-20053088, раздел 3.6.2):
 *   - BeiDou B1C в PSM не поддерживается (B1I поддерживается, но его нельзя включать
 *     одновременно с B1C и GLONASS L1OF - даташит G10A-F30, сноска 1);
 *   - SBAS в PSM не принимается, manual рекомендует его отключить.
 * Поэтому при PURRGO_GNSS_PSM != OFF автоматически выключаются BeiDou и SBAS: в PSM они
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
 *
 * Смена набора перезапускает ГНСС-подсистему приёмника, поэтому команда уходит
 * только при настройке после включения, до первого фикса.
 */
#define PURRGO_GNSS_SIG_FACTORY 0
#define PURRGO_GNSS_SIG_GPS_GAL 1

#ifndef PURRGO_GNSS_SIGNALS
#define PURRGO_GNSS_SIGNALS PURRGO_GNSS_SIG_FACTORY
#endif
#if (PURRGO_GNSS_SIGNALS != PURRGO_GNSS_SIG_FACTORY) && (PURRGO_GNSS_SIGNALS != PURRGO_GNSS_SIG_GPS_GAL)
#error "PURRGO_GNSS_SIGNALS must be PURRGO_GNSS_SIG_FACTORY (0) or PURRGO_GNSS_SIG_GPS_GAL (1)"
#endif
#if (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_UBX) && (PURRGO_GNSS_SIGNALS != PURRGO_GNSS_SIG_FACTORY)
#define PURRGO_GNSS_SIGNALS_ACTIVE 1
#else
#define PURRGO_GNSS_SIGNALS_ACTIVE 0
#endif

/*
 * Что выключает кадр CFG-SIGNAL (0/1): SBAS и BeiDou - вместе с PSM, GLONASS и BeiDou -
 * при PURRGO_GNSS_SIG_GPS_GAL. Кадр отправляется, если выключается хоть одна система.
 */
#define PURRGO_GNSS_SIG_OFF_SBAS (PURRGO_GNSS_PSM_ACTIVE)
#define PURRGO_GNSS_SIG_OFF_BDS  ((PURRGO_GNSS_PSM_ACTIVE) || (PURRGO_GNSS_SIGNALS_ACTIVE))
#define PURRGO_GNSS_SIG_OFF_GLO  (PURRGO_GNSS_SIGNALS_ACTIVE)
#define PURRGO_GNSS_SIGNAL_COUNT (PURRGO_GNSS_SIG_OFF_SBAS + PURRGO_GNSS_SIG_OFF_BDS + PURRGO_GNSS_SIG_OFF_GLO)

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
 *     - при старте опрос UBX-MON-VER, версия прошивки пишется в лог;
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
 * UBX-SEC-SIG (глушение и спуфинг) есть только в SPG 5.30 и новее. В SPG 5.10
 * (G10A-F30, "ROM SPG 5.10") ключа CFG-MSGOUT-UBX_SEC_SIG_UART1 нет, модуль отвечает NAK.
 * Включать (-DPURRGO_GNSS_SEC_SIG=1) только для модуля с прошивкой 5.30+; в UI строка
 * SIG при выключенном сообщении показывает "---".
 */
#ifndef PURRGO_GNSS_SEC_SIG
#define PURRGO_GNSS_SEC_SIG 0
#endif

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
 * или PVT не приходит), работает прежний фильтр по HDOP. Порог подобран без
 * полевых данных: сверить на записанных треках (pg:hacc в GPX).
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
 * Скорость UART связи с GNSS-модулем (только STM32, apps/stm32/common/gnss_link).
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


#endif /* PURRGO_HARDWARE_CONFIG_H */
