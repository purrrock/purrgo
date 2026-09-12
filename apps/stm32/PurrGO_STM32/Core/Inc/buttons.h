/*
 * STM32 button event driver for PurrGO.
 *
 * Temporary hardware configuration:
 *
 *   KEY1 -> PB3
 *   KEY2 -> PB4
 *   KEY3 -> PB5
 *   KEY4 -> PB6
 *
 * Buttons are connected between GPIO pin and GND.
 * GPIO inputs therefore use the internal pull-up:
 *
 *   released = GPIO_PIN_SET
 *   pressed  = GPIO_PIN_RESET
 *
 * The driver provides:
 *
 *   - mechanical button debounce;
 *   - SHORT press detection;
 *   - LONG press detection;
 *   - one event per physical button action;
 *   - an event queue so simultaneous button events are not lost.
 */

#ifndef PURRGO_STM32_BUTTONS_H
#define PURRGO_STM32_BUTTONS_H

#include <stdbool.h>
#include "purrgo/app_fsm.h"

/**
 * @brief Initialize the STM32 button driver.
 *
 * GPIO configuration is performed by CubeMX-generated
 * MX_GPIO_Init(). This function initializes the driver's
 * internal state from the current GPIO levels.
 */
void purrgo_stm32_buttons_init(void);

/**
 * @brief Update the button state and generate button events.
 *
 * This function must be called periodically from the main loop.
 * The recommended polling period is 10 ms.
 *
 * Debounce and SHORT/LONG detection are performed here.
 */
void purrgo_stm32_buttons_update(void);

/**
 * @brief Get the next pending button event.
 *
 * @param event Pointer where the event will be written.
 *
 * @return true if an event was available, false if the queue is empty.
 *
 * Events are removed from the queue when returned.
 */
bool purrgo_stm32_buttons_get_event(purrgo_btn_t *event);

/**
 * @brief Read the current debounced physical state of a button.
 *
 * This function is retained for code that needs the current
 * button level rather than an event.
 *
 * @param button Button identifier.
 *
 * @return true if the corresponding physical button is pressed.
 */
bool purrgo_stm32_button_is_pressed(purrgo_btn_t button);

#endif /* PURRGO_STM32_BUTTONS_H */