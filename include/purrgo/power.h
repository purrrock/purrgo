#ifndef PURRGO_POWER_H
#define PURRGO_POWER_H

/**
 * Initiates the system power-off sequence.
 *
 * This sequence safely stops normal application activity, closes all necessary
 * handles and files, puts peripherals in low-power states, and enters
 * a hardware specific low power state (or exits the application on PC).
 */
void purrgo_system_power_off(void);

#endif /* PURRGO_POWER_H */
