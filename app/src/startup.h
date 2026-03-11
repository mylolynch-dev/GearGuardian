/**
 * @file startup.h
 * @brief Boot coordinator — public interface.
 *
 * startup_run() is the single entry point called from main().  It sequences
 * all driver and service initializations and determines which operating mode
 * to enter.  After it returns, the RTOS threads handle everything.
 */

#ifndef STARTUP_H
#define STARTUP_H

#include "app_modes.h"

/**
 * @brief Execute the full startup sequence.
 *
 * Call once from main().  Never returns to main().
 *
 * Sequence:
 *   1. Init boot metadata (NVS)
 *   2. Load persistent boot meta, increment boot count
 *   3. Check reset reason (hwinfo)
 *   4. Read mode button — if held, select DIAGNOSTIC mode
 *   5. Check consecutive_failures >= threshold — if so, select SAFE mode
 *   6. Init all drivers (IMU, OLED, SD)
 *   7. Record faults in boot meta
 *   8. Post EVT_BOOT_COMPLETE with the selected mode
 *   9. Call boot_meta_record_clean_boot() or boot_meta_record_failed_boot()
 *  10. Enter main loop (k_sleep forever — threads handle the rest)
 */
void startup_run(void);

/**
 * @brief Read the mode button GPIO synchronously (before scheduler event flow).
 *
 * Called during the boot sequence before threads are dispatching events.
 * Returns true if the button is held (active-low = pin reads 0).
 *
 * @return true if the mode button is currently pressed.
 */
bool startup_mode_button_held(void);

#endif /* STARTUP_H */
