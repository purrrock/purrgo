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


#define PURRGO_DISPLAY_BACKEND_ST7789 0
#define PURRGO_DISPLAY_BACKEND_EINK   1

#ifndef PURRGO_HW_DISPLAY_BACKEND
#define PURRGO_HW_DISPLAY_BACKEND PURRGO_DISPLAY_BACKEND_EINK
// #define PURRGO_HW_DISPLAY_BACKEND PURRGO_DISPLAY_BACKEND_ST7789
#endif

#if (PURRGO_HW_DISPLAY_BACKEND != PURRGO_DISPLAY_BACKEND_ST7789) && \
    (PURRGO_HW_DISPLAY_BACKEND != PURRGO_DISPLAY_BACKEND_EINK)
#error "Invalid PURRGO_HW_DISPLAY_BACKEND"
#endif

/*
 * --------------------------------------------------------------------------
 * E-Ink: режим передачи на панель (действует только для backend E-Ink)
 * --------------------------------------------------------------------------
 *
 * PURRGO_EINK_MODE_4GRAY
 *   4 уровня серого. Панель не поддерживает частичное обновление в этом
 *   режиме, каждое обновление - полное (секунды, мерцание).
 *
 * PURRGO_EINK_MODE_1BIT
 *   Чёрно-белый режим: белый остаётся белым, любой другой цвет PurrGO
 *   выводится чёрным. Поддерживается частичное обновление.
 *
 * Внутренний framebuffer PurrGO в обоих режимах остаётся 2-битным, поэтому
 * ядро (gfx, карта, UI) от выбора режима не зависит.
 */
#define PURRGO_EINK_MODE_4GRAY 0
#define PURRGO_EINK_MODE_1BIT  1

#ifndef PURRGO_HW_EINK_MODE
#define PURRGO_HW_EINK_MODE PURRGO_EINK_MODE_1BIT
// #define PURRGO_HW_EINK_MODE PURRGO_EINK_MODE_4GRAY
#endif

#if (PURRGO_HW_EINK_MODE != PURRGO_EINK_MODE_4GRAY) && \
    (PURRGO_HW_EINK_MODE != PURRGO_EINK_MODE_1BIT)
#error "Invalid PURRGO_HW_EINK_MODE"
#endif

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
