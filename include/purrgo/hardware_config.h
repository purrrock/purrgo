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
 *                                настройками по умолчанию);
 *   PURRGO_GNSS_RECEIVER_CASIC - CASIC AT6558R (команды $PCAS...). Отладочный
 *                                модуль; приёмник Release (G10A F30, u-blox M10)
 *                                настраивается командами UBX - отдельная
 *                                реализация в gnss_receiver.c.
 */
#define PURRGO_GNSS_RECEIVER_NONE   0
#define PURRGO_GNSS_RECEIVER_CASIC  1

#ifndef PURRGO_HW_GNSS_RECEIVER
#define PURRGO_HW_GNSS_RECEIVER PURRGO_GNSS_RECEIVER_CASIC
#endif

#if (PURRGO_HW_GNSS_RECEIVER != PURRGO_GNSS_RECEIVER_NONE) && \
    (PURRGO_HW_GNSS_RECEIVER != PURRGO_GNSS_RECEIVER_CASIC)
#error "Invalid PURRGO_HW_GNSS_RECEIVER"
#endif


/*
 * Аппаратный backend дисплея. Единственный поддерживаемый - E-Ink
 * (Waveshare 2.7" e-Paper V2). Отладочный TFT ST7789 удалён из проекта.
 */
#define PURRGO_DISPLAY_BACKEND_EINK   1

#ifndef PURRGO_HW_DISPLAY_BACKEND
#define PURRGO_HW_DISPLAY_BACKEND PURRGO_DISPLAY_BACKEND_EINK
#endif

#if (PURRGO_HW_DISPLAY_BACKEND != PURRGO_DISPLAY_BACKEND_EINK)
#error "Invalid PURRGO_HW_DISPLAY_BACKEND"
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
