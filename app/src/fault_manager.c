/**
 * @file fault_manager.c
 * @brief Fault accumulation, severity classification, and safe mode trigger.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "fault_manager.h"
#include "app_events.h"
#include "app_state.h"
#include "app_modes.h"

LOG_MODULE_REGISTER(fault_manager, LOG_LEVEL_WRN);

/* ---------------------------------------------------------------------------
 * Fault name string table
 * --------------------------------------------------------------------------- */

static const struct {
    app_fault_id_t id;
    const char    *name;
} s_fault_names[] = {
    { FAULT_NONE,       "NONE"       },
    { FAULT_NVS_INIT,   "NVS_INIT"   },
    { FAULT_IMU_INIT,   "IMU_INIT"   },
    { FAULT_OLED_INIT,  "OLED_INIT"  },
    { FAULT_SD_MOUNT,   "SD_MOUNT"   },
    { FAULT_IMU_READ,   "IMU_READ"   },
    { FAULT_SD_WRITE,   "SD_WRITE"   },
    { FAULT_BOOT_LOOP,  "BOOT_LOOP"  },
    { FAULT_WATCHDOG,   "WATCHDOG"   },
};

const char *app_fault_name(app_fault_id_t fault)
{
    for (size_t i = 0; i < ARRAY_SIZE(s_fault_names); i++) {
        if (s_fault_names[i].id == fault) {
            return s_fault_names[i].name;
        }
    }
    return "UNKNOWN";
}

/* ===========================================================================
 * Public API
 * =========================================================================== */

void fault_manager_report(app_fault_id_t fault_id)
{
    app_event_t evt = {
        .type = EVT_FAULT,
        .payload.fault = {
            .fault_id = fault_id,
            .is_fatal = app_fault_is_fatal(fault_id),
        }
    };

    int rc = app_event_post(&evt);
    if (rc != 0) {
        /* Can't post the fault — at minimum log it */
        LOG_ERR("fault_manager_report: queue full, fault %s dropped",
                app_fault_name(fault_id));
    }
}

void fault_manager_handle(app_fault_id_t fault_id, bool is_fatal)
{
    LOG_WRN("Fault: %s (fatal=%d)", app_fault_name(fault_id), (int)is_fatal);

    /* Accumulate fault flags in shared state */
    k_mutex_lock(&g_app_state_mutex, K_FOREVER);
    g_app_state.fault_flags |= (uint32_t)fault_id;
    k_mutex_unlock(&g_app_state_mutex);

    /* Trigger safe mode if fatal */
    if (is_fatal && app_state_get_mode() != MODE_SAFE) {
        LOG_ERR("Fatal fault — requesting SAFE MODE");
        app_event_t mode_evt = {
            .type = EVT_MODE_CHANGE,
            .payload.mode = {
                .next_mode   = MODE_SAFE,
                .fault_flags = fault_manager_get_flags(),
            }
        };
        app_event_post(&mode_evt);
    }
}

bool fault_manager_any_fatal(void)
{
    return app_faults_any_fatal(app_state_get_fault_flags());
}

uint32_t fault_manager_get_flags(void)
{
    return app_state_get_fault_flags();
}
