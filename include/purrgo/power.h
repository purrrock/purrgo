#ifndef PURRGO_POWER_H
#define PURRGO_POWER_H

/**
 * Initiates the system power-off sequence (implemented in the portable core,
 * src/core/power.c). Order:
 *   1. render the final "POWER OFF" screen into the framebuffer;
 *   2. stop the track logger (flush and close the track file);
 *   3. save the configuration;
 *   4. close the persistent map files;
 *   5. purrgo_platform_storage_unmount();
 *   6. push the final screen to the display and put the display to sleep;
 *   7. purrgo_platform_enter_standby().
 *
 * On hardware the function does not return (the last step never returns);
 * on the PC the last step exits the process.
 */
void purrgo_system_power_off(void);

/*
 * Platform hooks used by purrgo_system_power_off().
 */

/* Unmount the storage so all metadata is written and the card is safe to lose power. */
void purrgo_platform_storage_unmount(void);

/* Enter the hardware low-power state (STM32: STANDBY with wake-up pin; PC: exit). */
void purrgo_platform_enter_standby(void);

#endif /* PURRGO_POWER_H */
