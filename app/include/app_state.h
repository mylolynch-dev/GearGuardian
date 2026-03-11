/**
 * @file app_state.h
 * @brief Shared application state struct and accessor interface.
 *
 * There is one global app_state instance (g_app_state) protected by a
 * k_mutex (g_app_state_mutex).  Only state_machine.c writes to it.
 * All other modules read via the inline accessors below, which acquire
 * the mutex for a single field read.
 *
 * For display rendering (ui_service) it is acceptable to snapshot the
 * entire struct under one mutex lock rather than calling multiple accessors.
 *
 * Threading notes:
 *   - state_machine.c: acquires mutex, writes, releases, signals UI semaphore
 *   - event_dispatcher.c: acquires mutex for brief reads only
 *   - ui_service.c: snapshots entire struct under one lock for rendering
 *   - ISRs: must NOT access g_app_state (use events instead)
 */

#ifndef APP_STATE_H
#define APP_STATE_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>
#include "app_modes.h"

/* ===========================================================================
 * Application state struct
 * =========================================================================== */

/**
 * @brief Complete runtime state of the Gear Guardian application.
 *
 * Protected by g_app_state_mutex.  Access only through the accessors below
 * unless you hold the mutex explicitly.
 */
typedef struct app_state {
    /* --- Mode / substate --- */
    app_mode_t           current_mode;    /**< Current top-level operating mode      */
    app_normal_substate_t substate;       /**< Sub-state (valid only in MODE_NORMAL) */

    /* --- Sensor state --- */
    bool     reed_closed;                 /**< Last known reed switch state           */
    int16_t  last_ax, last_ay, last_az;  /**< Most recent raw accel sample           */
    int16_t  last_gx, last_gy, last_gz;  /**< Most recent raw gyro sample            */

    /* --- Alarm state --- */
    bool     alarm_active;               /**< True while buzzer is sounding          */

    /* --- Boot health (mirrored from boot_meta for fast access) --- */
    uint32_t boot_count;                 /**< Lifetime boot counter                  */
    uint32_t consecutive_boot_failures;  /**< Consecutive failed boots               */

    /* --- Fault tracking --- */
    uint32_t fault_flags;                /**< OR of app_fault_id_t for active faults */
} app_state_t;

/* ===========================================================================
 * Global instance (defined in state_machine.c)
 * =========================================================================== */

extern app_state_t   g_app_state;
extern struct k_mutex g_app_state_mutex;

/* ===========================================================================
 * Inline accessor helpers
 *
 * These acquire the mutex for the minimum time needed to read one field.
 * For multi-field snapshots, lock the mutex in the caller directly.
 * =========================================================================== */

static inline app_mode_t app_state_get_mode(void)
{
    app_mode_t m;
    k_mutex_lock(&g_app_state_mutex, K_FOREVER);
    m = g_app_state.current_mode;
    k_mutex_unlock(&g_app_state_mutex);
    return m;
}

static inline app_normal_substate_t app_state_get_substate(void)
{
    app_normal_substate_t s;
    k_mutex_lock(&g_app_state_mutex, K_FOREVER);
    s = g_app_state.substate;
    k_mutex_unlock(&g_app_state_mutex);
    return s;
}

static inline bool app_state_alarm_active(void)
{
    bool a;
    k_mutex_lock(&g_app_state_mutex, K_FOREVER);
    a = g_app_state.alarm_active;
    k_mutex_unlock(&g_app_state_mutex);
    return a;
}

static inline bool app_state_reed_closed(void)
{
    bool c;
    k_mutex_lock(&g_app_state_mutex, K_FOREVER);
    c = g_app_state.reed_closed;
    k_mutex_unlock(&g_app_state_mutex);
    return c;
}

static inline uint32_t app_state_get_fault_flags(void)
{
    uint32_t f;
    k_mutex_lock(&g_app_state_mutex, K_FOREVER);
    f = g_app_state.fault_flags;
    k_mutex_unlock(&g_app_state_mutex);
    return f;
}

/**
 * @brief Snapshot the entire app_state struct in one mutex lock.
 *
 * Use in ui_service for rendering — cheaper than multiple accessor calls.
 *
 * @param[out] snap  Destination buffer; must not be NULL.
 */
static inline void app_state_snapshot(app_state_t *snap)
{
    k_mutex_lock(&g_app_state_mutex, K_FOREVER);
    *snap = g_app_state;
    k_mutex_unlock(&g_app_state_mutex);
}

#endif /* APP_STATE_H */
