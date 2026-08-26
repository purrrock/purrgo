
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
 * Hardware profile
 * ============================================================================
 *
 * Выбор профиля осуществляется раскомментированием одной из строк ниже.
 * Защита #ifndef удалена намеренно, чтобы исключить управление через CMake.
 */

#define PURRGO_HW_PROFILE_DEVELOPMENT  1
#define PURRGO_HW_PROFILE_PROTOTYPE    2
#define PURRGO_HW_PROFILE_RELEASE      3

// ---> ТЕКУЩИЙ АКТИВНЫЙ ПРОФИЛЬ <---
#define PURRGO_HW_PROFILE PURRGO_HW_PROFILE_DEVELOPMENT
// #define PURRGO_HW_PROFILE PURRGO_HW_PROFILE_PROTOTYPE
// #define PURRGO_HW_PROFILE PURRGO_HW_PROFILE_RELEASE


/*
 * ============================================================================
 * MCU / execution platform
 * ============================================================================
 */

#define PURRGO_PLATFORM_PC             1
#define PURRGO_MCU_STM32F446RE         2
#define PURRGO_MCU_STM32U5             3

#if PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_DEVELOPMENT
#define PURRGO_HW_MCU PURRGO_PLATFORM_PC
#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_PROTOTYPE
#define PURRGO_HW_MCU PURRGO_MCU_STM32F446RE
#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_RELEASE
#define PURRGO_HW_MCU PURRGO_MCU_STM32U5
#else
#error "Unknown PURRGO_HW_PROFILE"
#endif


/*
 * ============================================================================
 * GNSS receiver
 * ============================================================================
 */

#define PURRGO_GNSS_MOCK             1
#define PURRGO_GNSS_GY_NEO6MV2       2
#define PURRGO_GNSS_UBLOX_M10        3

#if PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_DEVELOPMENT
#define PURRGO_HW_GNSS PURRGO_GNSS_MOCK
#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_PROTOTYPE
#define PURRGO_HW_GNSS PURRGO_GNSS_GY_NEO6MV2
#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_RELEASE
#define PURRGO_HW_GNSS PURRGO_GNSS_UBLOX_M10
#else
#error "Unknown PURRGO_HW_PROFILE"
#endif


/*
 * ============================================================================
 * Display resolution
 * ============================================================================
 */

#if PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_DEVELOPMENT

#define PURRGO_HW_DISPLAY_WIDTH_PX      176
#define PURRGO_HW_DISPLAY_HEIGHT_PX     264

#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_PROTOTYPE

/* GMT024-08-SPI8P_LCM: TFT 240x320 2.4" ST7789 */
#define PURRGO_HW_DISPLAY_WIDTH_PX      240
#define PURRGO_HW_DISPLAY_HEIGHT_PX     320

#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_RELEASE

/* Release: E-Ink 2.9" 128x296 SPI */
#define PURRGO_HW_DISPLAY_WIDTH_PX      176
#define PURRGO_HW_DISPLAY_HEIGHT_PX     264

#else
#error "Unknown PURRGO_HW_PROFILE"
#endif


/*
 * --------------------------------------------------------------------------
 * Display diagonal
 * --------------------------------------------------------------------------
 */

#ifndef PURRGO_HW_DISPLAY_DIAGONAL_MM

#if PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_DEVELOPMENT
#define PURRGO_HW_DISPLAY_DIAGONAL_MM   74 /* Эмуляция габаритов целевого дисплея */
#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_PROTOTYPE
#define PURRGO_HW_DISPLAY_DIAGONAL_MM   61 /* 2.4 дюйма = ~60.96 мм */
#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_RELEASE
#define PURRGO_HW_DISPLAY_DIAGONAL_MM   74 /* 2.9 дюйма = ~73.66 мм */
#else
#error "PURRGO_HW_DISPLAY_DIAGONAL_MM must be defined"
#endif

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


/*
 * ============================================================================
 * Profile consistency checks
 * ============================================================================
 */

#if PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_DEVELOPMENT
#if PURRGO_HW_MCU != PURRGO_PLATFORM_PC
#error "DEVELOPMENT profile must use the PC platform"
#endif
#if PURRGO_HW_GNSS != PURRGO_GNSS_MOCK
#error "DEVELOPMENT profile must use the mock GNSS receiver"
#endif

#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_PROTOTYPE
#if PURRGO_HW_MCU != PURRGO_MCU_STM32F446RE
#error "PROTOTYPE profile must use STM32F446RE"
#endif
#if PURRGO_HW_GNSS != PURRGO_GNSS_GY_NEO6MV2
#error "PROTOTYPE profile must use GY-NEO6MV2"
#endif

#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_RELEASE
#if PURRGO_HW_MCU != PURRGO_MCU_STM32U5
#error "RELEASE profile must use STM32U5"
#endif
#if PURRGO_HW_GNSS != PURRGO_GNSS_UBLOX_M10
#error "RELEASE profile must use u-blox M10"
#endif
#endif

#endif /* PURRGO_HARDWARE_CONFIG_H */
