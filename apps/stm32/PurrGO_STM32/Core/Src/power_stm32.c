#include "purrgo/power.h"
#include "purrgo/logger.h"
#include "purrgo/track_logger.h"
#include "purrgo/map.h"
#include "purrgo/config.h"
#include "purrgo/display_hal.h"
#include "main.h"
#include "fatfs.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_pwr.h"

void purrgo_system_power_off(void) {
    PURRGO_LOG("Powering off...\n\r");

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
     * 5. Put display into low-power mode.
     */
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
