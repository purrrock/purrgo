/*
 * PurrGo hardware configuration
 *
 * This file describes the physical hardware configuration for which
 * the current firmware/application is built.
 *
 * IMPORTANT:
 *   This is NOT user configuration.
 *
 *   Values in this file describe hardware characteristics that are
 *   known at compile time and normally cannot be changed by the user
 *   while the firmware is running.
 *
 * Examples:
 *   - build/hardware target
 *   - MCU
 *   - GNSS receiver type
 *   - display resolution
 *   - display diagonal
 *   - display color depth
 *   - display orientation
 *
 * User-adjustable settings such as timezone, logging mode, units,
 * map settings, etc. belong to the runtime configuration subsystem
 * and must NOT be placed here.
 */

#ifndef PURRGO_HARDWARE_CONFIG_H
#define PURRGO_HARDWARE_CONFIG_H


/*
 * ============================================================================
 * Hardware profile
 * ============================================================================
 *
 * Select the physical hardware profile used by this build.
 *
 * DEVELOPMENT:
 *     PC development / emulator environment.
 *
 * PROTOTYPE:
 *     STM32F446RE prototype hardware.
 *
 * RELEASE:
 *     Final hardware platform.
 *
 * The profile is intentionally a compile-time choice.
 */

#define PURRGO_HW_PROFILE_DEVELOPMENT  1
#define PURRGO_HW_PROFILE_PROTOTYPE    2
#define PURRGO_HW_PROFILE_RELEASE      3

#ifndef PURRGO_HW_PROFILE
#define PURRGO_HW_PROFILE PURRGO_HW_PROFILE_DEVELOPMENT
#endif


/*
 * ============================================================================
 * MCU / execution platform
 * ============================================================================
 *
 * This identifies the execution platform for the selected hardware profile.
 *
 * The portable PurrGo core must NOT depend directly on these definitions.
 *
 * Platform-specific code may use this information to select the appropriate
 * HAL, drivers, peripherals, or emulator backend.
 */

#define PURRGO_PLATFORM_PC             1
#define PURRGO_MCU_STM32F446RE         2
#define PURRGO_MCU_STM32U5             3

#ifndef PURRGO_HW_MCU

#if PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_DEVELOPMENT

#define PURRGO_HW_MCU PURRGO_PLATFORM_PC

#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_PROTOTYPE

#define PURRGO_HW_MCU PURRGO_MCU_STM32F446RE

#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_RELEASE

#define PURRGO_HW_MCU PURRGO_MCU_STM32U5

#else

#error "Unknown PURRGO_HW_PROFILE"

#endif

#endif /* PURRGO_HW_MCU */


/*
 * ============================================================================
 * GNSS receiver
 * ============================================================================
 *
 * This describes the actual GNSS receiver used by the selected hardware
 * profile.
 *
 * It does NOT describe the generic GNSS API.
 *
 * The generic navigation code must remain independent of the receiver model.
 *
 * DEVELOPMENT uses a mock receiver because the PC emulator does not have
 * physical GNSS hardware.
 */

#define PURRGO_GNSS_MOCK             1
#define PURRGO_GNSS_GY_NEO6MV2       2
#define PURRGO_GNSS_UBLOX_M10        3

#ifndef PURRGO_HW_GNSS

#if PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_DEVELOPMENT

#define PURRGO_HW_GNSS PURRGO_GNSS_MOCK

#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_PROTOTYPE

#define PURRGO_HW_GNSS PURRGO_GNSS_GY_NEO6MV2

#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_RELEASE

#define PURRGO_HW_GNSS PURRGO_GNSS_UBLOX_M10

#else

#error "Unknown PURRGO_HW_PROFILE"

#endif

#endif /* PURRGO_HW_GNSS */


/*
 * ============================================================================
 * Display
 * ============================================================================
 *
 * Physical display characteristics.
 *
 * Width and height are expressed in physical display pixels.
 *
 * The diagonal is expressed in millimetres.
 *
 * BPP means bits per pixel in the logical PurrGo framebuffer.
 *
 * IMPORTANT:
 *
 *   These values describe the display itself.
 *   They must not be confused with the map viewport dimensions.
 *
 *   The map viewport is calculated by the UI/layout subsystem from the
 *   display dimensions and UI layout parameters.
 *
 *   No application code should contain hard-coded physical display
 *   dimensions such as 128 or 296.
 */


/*
 * --------------------------------------------------------------------------
 * Display resolution
 * --------------------------------------------------------------------------
 *
 * The current PC emulator uses a 128 x 296 logical display.
 *
 * These values are therefore part of the DEVELOPMENT profile.
 *
 * For a physical prototype, these values MUST be replaced with the
 * characteristics of the actual display used on the STM32F446RE hardware.
 */

#if PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_DEVELOPMENT

#define PURRGO_HW_DISPLAY_WIDTH_PX      128
#define PURRGO_HW_DISPLAY_HEIGHT_PX     296

#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_PROTOTYPE

/*
 * The prototype display has not been fixed here yet.
 *
 * Do not invent its physical characteristics.
 *
 * Define these values when the actual prototype display is selected.
 */

#ifndef PURRGO_HW_DISPLAY_WIDTH_PX
#error "Prototype display width is not defined"
#endif

#ifndef PURRGO_HW_DISPLAY_HEIGHT_PX
#error "Prototype display height is not defined"
#endif

#elif PURRGO_HW_PROFILE == PURRGO_HW_PROFILE_RELEASE

/*
 * The release display has not been fixed here yet.
 *
 * Do not invent its physical characteristics.
 */

#ifndef PURRGO_HW_DISPLAY_WIDTH_PX
#error "Release display width is not defined"
#endif

#ifndef PURRGO_HW_DISPLAY_HEIGHT_PX
#error "Release display height is not defined"
#endif

#else

#error "Unknown PURRGO_HW_PROFILE"

#endif


/*
 * --------------------------------------------------------------------------
 * Display diagonal
 * --------------------------------------------------------------------------
 *
 * The diagonal is a physical characteristic of the actual display.
 *
 * It MUST be specified in millimetres from the display datasheet.
 *
 * PPI / pixel density must be calculated from:
 *
 *     width_px
 *     height_px
 *     diagonal_mm
 *
 * Do not hard-code PPI separately because that would create two sources
 * of truth.
 *
 * The calculation itself belongs to the display/graphics subsystem.
 *
 * We deliberately do not provide a guessed value here.
 */

#ifndef PURRGO_HW_DISPLAY_DIAGONAL_MM

/*
 * The PC emulator has a logical 128 x 296 display, but its physical
 * diagonal is not known from the logical framebuffer dimensions.
 *
 * Therefore the diagonal must be supplied explicitly when physical
 * display scaling is required.
 */
#error "PURRGO_HW_DISPLAY_DIAGONAL_MM must be defined"

#endif


/*
 * --------------------------------------------------------------------------
 * Display color depth
 * --------------------------------------------------------------------------
 *
 * PurrGo currently requires at least 2 bits per pixel.
 *
 * 2 BPP provides four logical intensity levels:
 *
 *     0 = black
 *     1 = dark gray
 *     2 = light gray
 *     3 = white
 *
 * The physical mapping from these logical levels to the actual display
 * hardware is platform/display-driver specific.
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
 *
 * Orientation is a logical display-layout property.
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
 * Display physical parameters
 * ============================================================================
 *
 * Pixel density must be derived from:
 *
 *     width_px
 *     height_px
 *     diagonal_mm
 *
 * Do not store a separate PPI constant.
 *
 * The display/graphics subsystem is responsible for calculating:
 *
 *     pixel density
 *     physical pixel size
 *     physical dimensions
 *
 * from the hardware parameters above.
 */


/*
 * ============================================================================
 * Compile-time validation
 * ============================================================================
 */

#if PURRGO_HW_DISPLAY_WIDTH_PX == 0
#error "Display width must be greater than zero"
#endif

#if PURRGO_HW_DISPLAY_HEIGHT_PX == 0
#error "Display height must be greater than zero"
#endif

#if PURRGO_HW_DISPLAY_DIAGONAL_MM == 0
#error "Display diagonal must be greater than zero"
#endif

#if PURRGO_HW_DISPLAY_BPP < 2
#error "Display BPP must provide at least four logical shades"
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

#else

#error "Unknown PURRGO_HW_PROFILE"

#endif


#endif /* PURRGO_HARDWARE_CONFIG_H */