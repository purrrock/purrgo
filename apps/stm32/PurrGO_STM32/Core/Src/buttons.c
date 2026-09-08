/*
 * STM32 button driver stub for PurrGO.
 *
 * This is a temporary STM32 hardware stub. Physical buttons are not connected yet.
 * The driver intentionally always reports "no button pressed".
 * It is intended to be replaced by the real STM32 button driver later.
 */

#include "buttons.h"
#include <stddef.h>

void purrgo_stm32_buttons_init(void)
{
    /*
     * Stub: No GPIO initialization is performed since physical buttons
     * are not connected yet.
     */
}

bool purrgo_stm32_button_is_pressed(purrgo_btn_t button)
{
    (void)button;
    /*
     * Stub: Always report that no button is pressed.
     */
    return false;
}
