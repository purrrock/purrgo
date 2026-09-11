/*
 * STM32 debug button emulator for PurrGO.
 *
 * This module allows operating the device during debugging
 * without connecting physical buttons. It receives ASCII characters
 * from the diagnostic UART (USART2) and injects corresponding
 * button events into the existing FSM via purrgo_app_handle_button().
 *
 * It is a debug-only alternative input source.
 * UART processing is non-blocking so it doesn't affect the main loop.
 * The FSM is not modified and treats these events exactly as if they
 * came from a physical button.
 */
#define DEBUG_BUTTONS_MAX_BYTES_PER_LOOP 16U

#include "debug_buttons.h"
#include "purrgo/app_fsm.h"
#include "purrgo/logger.h"
#include "usart.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>


void purrgo_debug_buttons_init(void)
{
    /*
     * USART2 is already initialized in main.c by MX_USART2_UART_Init().
     * There is no extra initialization needed here for basic non-blocking RX.
     */
}

void purrgo_debug_buttons_process(void)
{
    uint8_t rx_char;

    /*
     * We use a non-blocking check to see if a character has been received.
     * We can check the RXNE (Read Data Register Not Empty) flag directly
     * to avoid blocking calls like HAL_UART_Receive.
     */
    uint16_t bytes_processed = 0U;

    while (
    bytes_processed < DEBUG_BUTTONS_MAX_BYTES_PER_LOOP &&
    __HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) == SET
    )
    {
        /*
         * Read the received data.
         * The UART data register (DR) read clears the RXNE flag.
         */
        rx_char = (uint8_t)(huart2.Instance->DR & (uint8_t)0x00FF);
        bytes_processed++;
        purrgo_btn_t btn;
        const char *log_msg = NULL;

        switch (rx_char)
        {
            case '1':
                btn = PURRGO_BTN_KEY1_SHORT;
                log_msg = "DEBUG KEY1 SHORT\r\n";
                break;
            case '2':
                btn = PURRGO_BTN_KEY2_SHORT;
                log_msg = "DEBUG KEY2 SHORT\r\n";
                break;
            case '3':
                btn = PURRGO_BTN_KEY3_SHORT;
                log_msg = "DEBUG KEY3 SHORT\r\n";
                break;
            case '4':
                btn = PURRGO_BTN_KEY4_SHORT;
                log_msg = "DEBUG KEY4 SHORT\r\n";
                break;
            case '5':
                btn = PURRGO_BTN_KEY1_LONG;
                log_msg = "DEBUG KEY1 LONG\r\n";
                break;
            case '6':
                btn = PURRGO_BTN_KEY2_LONG;
                log_msg = "DEBUG KEY2 LONG\r\n";
                break;
            case '7':
                btn = PURRGO_BTN_KEY3_LONG;
                log_msg = "DEBUG KEY3 LONG\r\n";
                break;
            case '8':
                btn = PURRGO_BTN_KEY4_LONG;
                log_msg = "DEBUG KEY4 LONG\r\n";
                break;
            default:
                /* Ignore CR/LF, spaces, and other unsupported characters */
                continue;
        }

        /* Log recognized button event */
        // purrgo_logger_write(log_msg);

        /* Inject the event into the FSM */
        purrgo_app_handle_button(btn);
    }
}
