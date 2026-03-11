/**
 * @file mode_manager.c
 * @brief Top-level mode entry/exit coordination and name string tables.
 *
 * Called by event_dispatcher when EVT_BOOT_COMPLETE or EVT_MODE_CHANGE is
 * received.  Calls the exit handler of the current mode and the entry handler
 * of the new mode.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "mode_manager.h"
#include "app_state.h"
#include "app_events.h"
#include "ui_service.h"
#include "logger_service.h"

/* Forward declarations for mode entry/exit functions */
#include "normal_mode.h"
#include "diag_mode.h"
#include "safe_mode.h"

LOG_MODULE_REGISTER(mode_manager, LOG_LEVEL_INF);

/* ---------------------------------------------------------------------------
 * Human-readable name string tables (extern declarations in app_modes.h)
 * --------------------------------------------------------------------------- */

const char *const app_mode_names[MODE_COUNT] = {
    [MODE_BOOT]       = "BOOT",
    [MODE_NORMAL]     = "NORMAL",
    [MODE_DIAGNOSTIC] = "DIAGNOSTIC",
    [MODE_SAFE]       = "SAFE",
};

const char *const app_substate_names[SUBSTATE_COUNT] = {
    [SUBSTATE_DISARMED] = "DISARMED",
    [SUBSTATE_ARMING]   = "ARMING",
    [SUBSTATE_ARMED]    = "ARMED",
    [SUBSTATE_ALARM]    = "ALARM",
    [SUBSTATE_COOLDOWN] = "COOLDOWN",
};

/* ===========================================================================
 * Public API
 * =========================================================================== */

app_mode_t mode_manager_current(void)
{
    return g_app_state.current_mode; /* enum-sized atomic read on 32-bit MCU */
}

void mode_manager_enter(app_mode_t new_mode)
{
    app_mode_t prev_mode = mode_manager_current();

    if (prev_mode == new_mode) {
        return; /* Already in target mode; no-op */
    }

    LOG_INF("Mode transition: %s → %s",
            app_mode_names[prev_mode], app_mode_names[new_mode]);

    /* --- Exit current mode --- */
    switch (prev_mode) {
    case MODE_NORMAL:
        normal_mode_exit();
        break;
    case MODE_DIAGNOSTIC:
        diag_mode_exit();
        break;
    case MODE_SAFE:
        /* Safe mode is a terminal state in V1; no exit handling */
        break;
    case MODE_BOOT:
        /* No cleanup needed from BOOT */
        break;
    default:
        break;
    }

    /* --- Update shared state --- */
    k_mutex_lock(&g_app_state_mutex, K_FOREVER);
    g_app_state.current_mode = new_mode;
    k_mutex_unlock(&g_app_state_mutex);

    /* --- Log the transition --- */
    char msg[48];
    snprintf(msg, sizeof(msg), "Mode change: %s->%s",
             app_mode_names[prev_mode], app_mode_names[new_mode]);
    logger_service_post_str(msg);

    /* --- Enter new mode --- */
    switch (new_mode) {
    case MODE_NORMAL:
        normal_mode_enter();
        break;
    case MODE_DIAGNOSTIC:
        diag_mode_enter();
        break;
    case MODE_SAFE:
        safe_mode_enter(g_app_state.fault_flags);
        break;
    case MODE_BOOT:
        /* Should never transition TO boot after leaving it */
        LOG_WRN("Unexpected transition to MODE_BOOT");
        break;
    default:
        break;
    }

    ui_service_request_update();
}
