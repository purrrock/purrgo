#ifndef STM32_APP_H
#define STM32_APP_H

/**
 * @brief Initializes the STM32 PurrGO application.
 *
 * This function orchestrates the startup of the application layer.
 * It handles the initialization of UART diagnostics, FatFs, GNSS,
 * physical buttons, display, graphics context, application FSM,
 * and shows the splash screen.
 *
 * It must be called after the HAL and CubeMX peripheral initializations.
 */
void purrgo_stm32_init(void);

/**
 * @brief Processes the STM32 PurrGO application tasks.
 *
 * This function runs one iteration of the application's main loop.
 * It processes incoming GNSS bytes, updates the core application FSM,
 * polls and handles button events, performs UI rendering if needed,
 * and enters low-power sleep mode until the next event.
 *
 * It must be called continuously within the main infinite loop.
 */
void purrgo_stm32_process(void);

#endif /* STM32_APP_H */
