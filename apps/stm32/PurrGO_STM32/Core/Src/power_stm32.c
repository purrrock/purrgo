#include "purrgo/power.h"
#include "purrgo/logger.h"
#include "purrgo/track_logger.h"
#include "purrgo/map.h"
#include "purrgo/config.h"
#include "purrgo/display_hal.h"
#include "purrgo/app_ui.h"
#include "main.h"
#include "fatfs.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_pwr.h"

void purrgo_system_power_off(void) {
    PURRGO_LOG("Powering off...\n\r");

    /*
     * 0. Финальный экран: последняя карта с треком и маркером и инверсные
     * строки "POWER OFF". Рисуется во framebuffer ДО остановки логгера
     * (трек - в его RAM-буфере) и до размонтирования SD (с неё читается
     * карта). На дисплей экран уходит позже, после сохранения данных:
     * полный refresh e-ink занимает секунды, а данные важнее.
     */
    bool power_off_screen = purrgo_app_ui_render_power_off();

    /*
     * 1. Stop GPX logging if active.
     * This flushes the buffers and closes the active .gpx file.
     */
    purrgo_logger_stop();

    /*
     * 2. Save configuration to ensure PURRGO.CFG is closed and synced.
     */
    purrgo_config_save();

    /*
     * 3. Close persistent map files (idx, mlp, db).
     * Necessary to free FATFS structures before unmounting.
     */
    purrgo_map_shutdown();

    /*
     * 4. Unmount the FATFS filesystem to ensure all metadata is written
     * and the SD card is in a safe state.
     */
    f_mount(NULL, "0:", 1);
    PURRGO_LOG("FatFs dismount\r\n");
    /*
     * 5. Show the final screen (the image stays on e-ink without power) and
     * put the display into low-power mode.
     */
    if (power_off_screen) {
        display_refresh();
        PURRGO_LOG("Power-off screen shown\r\n");
    }
    display_sleep();
    PURRGO_LOG("Display off\r\n");
    
       /* Ensure Wakeup pin is enabled to allow turning back on (usually WKUP pin, PA0). */
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);

    /* Clear the Wakeup flag if already set. */
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

    /* Enter STANDBY mode. */
    PURRGO_LOG("Entering STANDBY mode.\n\r");
    HAL_PWR_EnterSTANDBYMode();

    /* Device should reset upon wakeup. */
    while (1) {
    }
}
