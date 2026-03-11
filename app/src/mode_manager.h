/**
 * @file mode_manager.h
 * @brief Top-level mode entry and exit coordination.
 *
 * mode_manager.c is called by the event_dispatcher when it receives
 * EVT_BOOT_COMPLETE or EVT_MODE_CHANGE.  It tears down the previous mode
 * and initializes the next one.
 */

#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include "app_modes.h"

/**
 * @brief Transition the system to the specified mode.
 *
 * This function:
 *  1. Calls the exit handler for the current mode (if any)
 *  2. Updates g_app_state.current_mode under mutex
 *  3. Calls the entry handler for the new mode
 *  4. Signals the UI semaphore
 *
 * Must be called from the event_dispatcher thread only.
 *
 * @param mode  Target operating mode.
 */
void mode_manager_enter(app_mode_t mode);

/**
 * @brief Return the current operating mode (non-locking fast path).
 *
 * Reads g_app_state.current_mode without a mutex (enum-sized atomic read
 * is safe on 32-bit architectures for read-only consumers).
 *
 * @return Current mode.
 */
app_mode_t mode_manager_current(void);

/* Human-readable mode and substate name strings (defined in mode_manager.c) */
extern const char *const app_mode_names[MODE_COUNT];
extern const char *const app_substate_names[SUBSTATE_COUNT];

#endif /* MODE_MANAGER_H */
