#ifndef PURRGO_DEBUG_BUTTONS_H
#define PURRGO_DEBUG_BUTTONS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the debug button emulator.
 *
 * This emulator allows injecting button events via the diagnostic UART
 * for testing without physical buttons.
 */
void purrgo_debug_buttons_init(void);

/**
 * @brief Process debug button events from UART.
 *
 * This function should be called frequently from the main application loop.
 * It reads characters non-blockingly and maps them to PurrGO button events.
 */
void purrgo_debug_buttons_process(void);

#ifdef __cplusplus
}
#endif

#endif /* PURRGO_DEBUG_BUTTONS_H */
