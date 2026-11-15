/*
 * STM32 button event driver for PurrGO.
 *
 * Hardware configuration (buttons of the Waveshare 2.7" e-Paper HAT):
 *
 *   KEY1 -> PA0  (K1)
 *   KEY2 -> PA1  (K2)
 *   KEY3 -> PC14 (K3)
 *   KEY4 -> PC15 (K4)
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
 *
 * Buttons are served in a hybrid way:
 *
 *   - EXTI (both edges, configured in CubeMX) only reports that something
 *     happened on a button pin: the interrupt handler sets a flag and does
 *     nothing else;
 *   - debounce, SHORT/LONG and the K1+K4 combination stay time based and are
 *     computed in purrgo_stm32_buttons_update(), which the main loop calls
 *     every 10 ms while purrgo_stm32_buttons_poll_needed() is true.
 *
 * The EXTI lines of K1..K4 (both edges, pull-up) and their NVIC vectors must be
 * enabled in CubeMX: without them a button press is never noticed.
 */

#ifndef PURRGO_STM32_BUTTONS_H
#define PURRGO_STM32_BUTTONS_H

#include <stdbool.h>
#include "purrgo/app_fsm.h"

/**
 * @brief Initialize the STM32 button driver.
 *
 * GPIO and EXTI configuration is performed by CubeMX-generated
 * MX_GPIO_Init(). This function initializes the driver's
 * internal state from the current GPIO levels.
 */
void purrgo_stm32_buttons_init(void);

/**
 * @brief Update the button state and generate button events.
 *
 * This function must be called from the main loop every 10 ms while
 * purrgo_stm32_buttons_poll_needed() returns true.
 *
 * Debounce and SHORT/LONG detection are performed here.
 */
void purrgo_stm32_buttons_update(void);

/**
 * @brief Whether purrgo_stm32_buttons_update() has to be called now.
 *
 * true  - an EXTI edge is pending, or a button is pressed / still being
 *         debounced / an event is queued (time based work is in progress);
 * false - all buttons are released and stable and no edge was seen: there is
 *         nothing to compute until the next EXTI edge.
 */
bool purrgo_stm32_buttons_poll_needed(void);

/**
 * @brief Ask for a poll on the next main loop pass.
 *
 * For code that consumes button edges itself, for example after waking up from
 * Stop 2 (the edge that woke the MCU is cleared there, the driver must still
 * look at the pin level). Safe to call from any context.
 */
void purrgo_stm32_buttons_request_poll(void);

/* true - все кнопки отпущены, debounce закончен, событий нет (можно в Stop 2). */
bool purrgo_stm32_buttons_idle(void);

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