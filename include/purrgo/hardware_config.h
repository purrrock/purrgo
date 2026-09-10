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

#define PURRGO_GNSS_MOCK             1
#define PURRGO_HW_GNSS PURRGO_GNSS_MOCK

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

#if PURRGO_HW_DISPLAY_BPP < 2
#error "PurrGo requires at least 2 bits per pixel (4 logical shades)"
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
