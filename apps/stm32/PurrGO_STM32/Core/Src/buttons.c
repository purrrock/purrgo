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
 * The driver implements:
 *
 *   - time-based debounce;
 *   - SHORT press detection;
 *   - LONG press detection;
 *   - an event queue.
 *
 * A SHORT event is generated when a debounced button is released
 * before PURRGO_BTN_LONG_PRESS_MS.
 *
 * A LONG event is generated once when a debounced button remains
 * pressed for PURRGO_BTN_LONG_PRESS_MS.
 *
 * Releasing a button after a LONG event does not generate a SHORT
 * event.
 */

#include "buttons.h"
#include "main.h"
#include "purrgo/logger.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Mechanical button debounce interval.
 *
 * A new electrical state must remain unchanged for this amount
 * of time before it becomes the debounced stable state.
 */
#define PURRGO_BUTTON_DEBOUNCE_MS 20U

/*
 * Maximum number of pending button events.
 *
 * The queue is deliberately larger than the number of physical
 * buttons so that several events generated close together are
 * not immediately lost before the main loop consumes them.
 */
#define PURRGO_BUTTON_EVENT_QUEUE_SIZE 16U

/*
 * State of one physical button.
 */
typedef struct
{
    /* STM32 GPIO used by this button. */
    GPIO_TypeDef *gpio_port;

    /* STM32 GPIO pin used by this button. */
    uint16_t gpio_pin;

    /*
     * Most recently sampled electrical state.
     *
     * This value is not yet necessarily debounced.
     */
    GPIO_PinState raw_state;

    /*
     * Debounced state reported by the driver.
     */
    GPIO_PinState stable_state;

    /*
     * HAL_GetTick() value at which raw_state last changed.
     */
    uint32_t last_change_time;

    /*
     * Time at which the current debounced press started.
     *
     * Valid only while stable_state == GPIO_PIN_RESET.
     */
    uint32_t press_start_time;

    /*
     * Set after LONG event has been generated.
     *
     * This prevents repeated LONG events while the button
     * remains physically pressed.
     */
    bool long_event_generated;

} purrgo_button_hw_t;


/*
 * Physical button mapping.
 *
 * These pins are deliberately specified directly rather than
 * depending on CubeMX-generated KEY1_Pin macros.
 */
static purrgo_button_hw_t buttons[] =
{
    {
        .gpio_port = GPIOB,
        .gpio_pin = GPIO_PIN_3,
        .raw_state = GPIO_PIN_SET,
        .stable_state = GPIO_PIN_SET,
        .last_change_time = 0U,
        .press_start_time = 0U,
        .long_event_generated = false
    },

    {
        .gpio_port = GPIOB,
        .gpio_pin = GPIO_PIN_4,
        .raw_state = GPIO_PIN_SET,
        .stable_state = GPIO_PIN_SET,
        .last_change_time = 0U,
        .press_start_time = 0U,
        .long_event_generated = false
    },

    {
        .gpio_port = GPIOB,
        .gpio_pin = GPIO_PIN_5,
        .raw_state = GPIO_PIN_SET,
        .stable_state = GPIO_PIN_SET,
        .last_change_time = 0U,
        .press_start_time = 0U,
        .long_event_generated = false
    },

    {
        .gpio_port = GPIOB,
        .gpio_pin = GPIO_PIN_6,
        .raw_state = GPIO_PIN_SET,
        .stable_state = GPIO_PIN_SET,
        .last_change_time = 0U,
        .press_start_time = 0U,
        .long_event_generated = false
    }
};


/*
 * Event queue.
 *
 * The queue contains only actual application events:
 *
 *   KEY1_SHORT
 *   KEY2_SHORT
 *   KEY3_SHORT
 *   KEY4_SHORT
 *   KEY1_LONG
 *   KEY2_LONG
 *   KEY3_LONG
 *   KEY4_LONG
 */
static purrgo_btn_t event_queue[PURRGO_BUTTON_EVENT_QUEUE_SIZE];

static size_t event_queue_head = 0U;
static size_t event_queue_tail = 0U;
static size_t event_queue_count = 0U;


/*
 * Convert a physical button index to the corresponding SHORT event.
 */
static purrgo_btn_t button_short_event(size_t index)
{
    switch (index)
    {
        case 0U:
            return PURRGO_BTN_KEY1_SHORT;

        case 1U:
            return PURRGO_BTN_KEY2_SHORT;

        case 2U:
            return PURRGO_BTN_KEY3_SHORT;

        case 3U:
            return PURRGO_BTN_KEY4_SHORT;

        default:
            /*
             * This value is unreachable when called with a valid
             * physical button index.
             *
             * There is no PURRGO_BTN_NONE value in app_fsm.h.
             */
            return PURRGO_BTN_KEY1_SHORT;
    }
}


/*
 * Convert a physical button index to the corresponding LONG event.
 */
static purrgo_btn_t button_long_event(size_t index)
{
    switch (index)
    {
        case 0U:
            return PURRGO_BTN_KEY1_LONG;

        case 1U:
            return PURRGO_BTN_KEY2_LONG;

        case 2U:
            return PURRGO_BTN_KEY3_LONG;

        case 3U:
            return PURRGO_BTN_KEY4_LONG;

        default:
            /*
             * This value is unreachable when called with a valid
             * physical button index.
             */
            return PURRGO_BTN_KEY1_LONG;
    }
}


/*
 * Convert a PurrGO button identifier to a physical button index.
 *
 * This is needed only by purrgo_stm32_button_is_pressed().
 *
 * SHORT and LONG identifiers refer to the same physical input.
 */
static int button_to_index(purrgo_btn_t button)
{
    switch (button)
    {
        case PURRGO_BTN_KEY1_SHORT:
        case PURRGO_BTN_KEY1_LONG:
            return 0;

        case PURRGO_BTN_KEY2_SHORT:
        case PURRGO_BTN_KEY2_LONG:
            return 1;

        case PURRGO_BTN_KEY3_SHORT:
        case PURRGO_BTN_KEY3_LONG:
            return 2;

        case PURRGO_BTN_KEY4_SHORT:
        case PURRGO_BTN_KEY4_LONG:
            return 3;

        default:
            /*
             * UP/DOWN/LEFT/RIGHT/PLUS/MINUS/MENU/OK are not
             * physical GPIO inputs in this temporary configuration.
             */
            return -1;
    }
}


/*
 * Put one event into the event queue.
 */
static void queue_event(purrgo_btn_t event)
{
    /*
     * Do not overwrite an event that has not yet been consumed.
     */
    if (event_queue_count >= PURRGO_BUTTON_EVENT_QUEUE_SIZE)
    {
        PURRGO_LOG("BUTTON EVENT QUEUE FULL\r\n");
        return;
    }

    event_queue[event_queue_tail] = event;

    event_queue_tail++;

    if (event_queue_tail >= PURRGO_BUTTON_EVENT_QUEUE_SIZE)
    {
        event_queue_tail = 0U;
    }

    event_queue_count++;

    /*
     * Log the event at the moment it is generated.
     *
     * The application therefore sees exactly the same event
     * that is reported here.
     */
    switch (event)
    {
        case PURRGO_BTN_KEY1_SHORT:
            PURRGO_LOG("BUTTON KEY1 SHORT\r\n");
            break;

        case PURRGO_BTN_KEY2_SHORT:
            PURRGO_LOG("BUTTON KEY2 SHORT\r\n");
            break;

        case PURRGO_BTN_KEY3_SHORT:
            PURRGO_LOG("BUTTON KEY3 SHORT\r\n");
            break;

        case PURRGO_BTN_KEY4_SHORT:
            PURRGO_LOG("BUTTON KEY4 SHORT\r\n");
            break;

        case PURRGO_BTN_KEY1_LONG:
            PURRGO_LOG("BUTTON KEY1 LONG\r\n");
            break;

        case PURRGO_BTN_KEY2_LONG:
            PURRGO_LOG("BUTTON KEY2 LONG\r\n");
            break;

        case PURRGO_BTN_KEY3_LONG:
            PURRGO_LOG("BUTTON KEY3 LONG\r\n");
            break;

        case PURRGO_BTN_KEY4_LONG:
            PURRGO_LOG("BUTTON KEY4 LONG\r\n");
            break;

        default:
            break;
    }
}


/*
 * Update debounce and press-duration state for one physical button.
 */
static void update_button(
    purrgo_button_hw_t *button,
    size_t index,
    uint32_t now
)
{
    GPIO_PinState current_state;

    current_state = HAL_GPIO_ReadPin(
        button->gpio_port,
        button->gpio_pin
    );

    /*
     * The raw electrical state changed.
     *
     * Restart the debounce timer. The new state will not be
     * accepted until it remains unchanged for the full debounce
     * interval.
     */
    if (current_state != button->raw_state)
    {
        button->raw_state = current_state;
        button->last_change_time = now;

        return;
    }

    /*
     * Raw state has remained unchanged.
     *
     * Check whether it has been stable long enough to accept it.
     */
    if (button->stable_state != button->raw_state)
    {
        if ((uint32_t)(now - button->last_change_time)
            >= PURRGO_BUTTON_DEBOUNCE_MS)
        {
            GPIO_PinState previous_state;

            previous_state = button->stable_state;
            button->stable_state = button->raw_state;

            /*
             * Stable transition: released -> pressed.
             */
            if (
                previous_state == GPIO_PIN_SET &&
                button->stable_state == GPIO_PIN_RESET
            )
            {
                /*
                 * Start measuring the press duration.
                 */
                button->press_start_time = now;

                /*
                 * No LONG event has been generated for this press.
                 */
                button->long_event_generated = false;
            }

            /*
             * Stable transition: pressed -> released.
             */
            else if (
                previous_state == GPIO_PIN_RESET &&
                button->stable_state == GPIO_PIN_SET
            )
            {
                /*
                 * If LONG was not generated, this was a SHORT press.
                 */
                if (!button->long_event_generated)
                {
                    queue_event(button_short_event(index));
                }
            }
        }
    }

    /*
     * Check for a long press.
     *
     * The check is performed only while the debounced button
     * remains pressed and only once per physical press.
     */
    if (
        button->stable_state == GPIO_PIN_RESET &&
        !button->long_event_generated
    )
    {
        if (
            (uint32_t)(now - button->press_start_time)
            >= PURRGO_BTN_LONG_PRESS_MS
        )
        {
            queue_event(button_long_event(index));

            /*
             * Prevent another LONG event until the button is released.
             */
            button->long_event_generated = true;
        }
    }
}


void purrgo_stm32_buttons_init(void)
{
    uint32_t now;
    size_t i;

    now = HAL_GetTick();

    /*
     * Initialize software state from the actual electrical state.
     *
     * This prevents a button that happens to be held during reset
     * from producing a false SHORT/LONG transition caused only by
     * driver initialization.
     */
    for (i = 0U; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
    {
        GPIO_PinState state;

        state = HAL_GPIO_ReadPin(
            buttons[i].gpio_port,
            buttons[i].gpio_pin
        );

        buttons[i].raw_state = state;
        buttons[i].stable_state = state;
        buttons[i].last_change_time = now;
        buttons[i].press_start_time = now;
        buttons[i].long_event_generated = false;
    }

    /*
     * Empty the event queue.
     */
    event_queue_head = 0U;
    event_queue_tail = 0U;
    event_queue_count = 0U;

    PURRGO_LOG("BUTTONS INIT\r\n");
}


void purrgo_stm32_buttons_update(void)
{
    uint32_t now;
    size_t i;

    now = HAL_GetTick();

    /*
     * Update every physical button on every polling cycle.
     *
     * This is important: long-press timing must continue even when
     * no event was generated during the previous polling cycle.
     */
    for (i = 0U; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
    {
        update_button(&buttons[i], i, now);
    }
}


bool purrgo_stm32_buttons_get_event(purrgo_btn_t *event)
{
    /*
     * NULL is not a valid output pointer.
     */
    if (event == NULL)
    {
        return false;
    }

    /*
     * No pending event.
     */
    if (event_queue_count == 0U)
    {
        return false;
    }

    /*
     * Return the oldest event in FIFO order.
     */
    *event = event_queue[event_queue_head];

    event_queue_head++;

    if (event_queue_head >= PURRGO_BUTTON_EVENT_QUEUE_SIZE)
    {
        event_queue_head = 0U;
    }

    event_queue_count--;

    return true;
}


bool purrgo_stm32_button_is_pressed(purrgo_btn_t button)
{
    int index;

    index = button_to_index(button);

    if (index < 0)
    {
        return false;
    }

    /*
     * Return the already debounced state.
     *
     * State update itself is performed by
     * purrgo_stm32_buttons_update().
     */
    return (buttons[index].stable_state == GPIO_PIN_RESET);
}