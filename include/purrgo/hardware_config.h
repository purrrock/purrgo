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
 * Слои конфигурации UBX-CFG-VALSET (битовая маска: 0x01 RAM, 0x02 BBR,
 * 0x04 Flash). По умолчанию только RAM: настройка отправляется при каждом
 * старте, а ресурс Flash модуля не расходуется. Значение 0x07 сохранит
 * настройку в Flash модуля.
 */
#ifndef PURRGO_GNSS_UBX_LAYERS
#define PURRGO_GNSS_UBX_LAYERS 0x01
#endif

/*
 * Повторная отправка настройки через столько мс после старта (0 - не
 * повторять). Модуль, запитанный одновременно с МК, может ещё не принимать
 * команды в момент первой отправки; настройка идемпотентна.
 */
#ifndef PURRGO_GNSS_CONFIG_RETRY_MS
#define PURRGO_GNSS_CONFIG_RETRY_MS 2000
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
 * R_TOP и R_BOTTOM - фактические сопротивления резисторов (измерить
 * мультиметром). CAL_PERMILLE - поправка в тысячных: сравнить U_блока в логе
 * (PURRGO_LOG_LEVEL >= 3) с мультиметром, CAL = 1000 * U_мультиметр / U_лога.
 */
#ifndef PURRGO_HW_BATT_DIV_R_TOP_OHM
#define PURRGO_HW_BATT_DIV_R_TOP_OHM     1000000u
#endif
#ifndef PURRGO_HW_BATT_DIV_R_BOTTOM_OHM
#define PURRGO_HW_BATT_DIV_R_BOTTOM_OHM  1000000u
#endif
#ifndef PURRGO_HW_BATT_CAL_PERMILLE
#define PURRGO_HW_BATT_CAL_PERMILLE      1000u
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
