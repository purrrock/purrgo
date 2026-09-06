/*
 * STM32 button driver stub for PurrGO.
 *
 * This is a temporary STM32 hardware stub. Physical buttons are not connected yet.
 * The driver intentionally always reports "no button pressed".
 * It is intended to be replaced by the real STM32 button driver later.
 */

#ifndef PURRGO_STM32_BUTTONS_H
#define PURRGO_STM32_BUTTONS_H

#include <stdbool.h>
#include "purrgo/app_fsm.h"

/**
 * @brief Initializes the STM32 button driver.
 *
 * Currently a stub that performs no hardware initialization.
 */
void purrgo_stm32_buttons_init(void);

/**
 * @brief Reads the current state of a specific button.
 *
 * @param button The button to query.
 * @return true if the button is physically pressed, false otherwise.
 *         (The stub implementation always returns false).
 */
bool purrgo_stm32_button_is_pressed(purrgo_btn_t button);

#endif /* PURRGO_STM32_BUTTONS_H */
