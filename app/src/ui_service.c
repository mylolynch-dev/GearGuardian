/**
 * @file ui_service.c
 * @brief OLED UI service — semaphore-driven screen updates.
 *
 * The UI thread wakes on a semaphore signal, snapshots the current app state,
 * and calls the appropriate oled_screen_*() function to redraw the display.
 *
 * Other modules call ui_service_request_update() to signal a redraw.
 * Multiple signals before the thread wakes collapse into one redraw (the
 * semaphore count saturates at 1 via K_SEM_MAX_LIMIT=1 if needed, but
 * standard give/take with a sleeping thread achieves the same effect here).
 *
 * A periodic 1-second timer also signals the semaphore so the display
 * refreshes even when no state changes occur (useful for countdown timers
 * and live IMU data in diagnostic mode).
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ui_service.h"
#include "app_config.h"
#include "app_state.h"
#include "app_modes.h"
#include "oled.h"

LOG_MODULE_REGISTER(ui_service, LOG_LEVEL_INF);

/* ---------------------------------------------------------------------------
 * Semaphore — given when a redraw is needed
 * --------------------------------------------------------------------------- */
K_SEM_DEFINE(s_ui_update_sem, 0, 1);

/* ---------------------------------------------------------------------------
 * Periodic refresh timer (1 Hz heartbeat for countdown display etc.)
 * --------------------------------------------------------------------------- */
static void ui_heartbeat_timer_fn(struct k_timer *t)
{
    ARG_UNUSED(t);
    k_sem_give(&s_ui_update_sem);
}

K_TIMER_DEFINE(s_ui_heartbeat_timer, ui_heartbeat_timer_fn, NULL);

/* ---------------------------------------------------------------------------
 * Thread
 * --------------------------------------------------------------------------- */
K_THREAD_STACK_DEFINE(s_ui_stack, APP_STACK_UI_SZ);
static struct k_thread s_ui_thread;

static void ui_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    LOG_INF("UI service thread started");

    /* Start the 1 Hz heartbeat refresh */
    k_timer_start(&s_ui_heartbeat_timer, K_SECONDS(1), K_SECONDS(1));

    app_state_t snap;

    while (true) {
        k_sem_take(&s_ui_update_sem, K_FOREVER);

        /* Snapshot shared state in one mutex lock */
        app_state_snapshot(&snap);

        /* Route to the correct screen renderer */
        switch (snap.current_mode) {
        case MODE_BOOT:
            oled_screen_boot();
            break;

        case MODE_DIAGNOSTIC:
            oled_screen_diagnostic();
            /* In diagnostic mode: if IMU data is fresh, show live values */
            if (snap.last_ax != 0 || snap.last_ay != 0 || snap.last_az != 0) {
                oled_screen_imu_data(snap.last_ax, snap.last_ay, snap.last_az,
                                     snap.last_gx, snap.last_gy, snap.last_gz);
            }
            break;

        case MODE_SAFE:
            oled_screen_safe(snap.fault_flags);
            break;

        case MODE_NORMAL:
            switch (snap.substate) {
            case SUBSTATE_DISARMED:
                oled_screen_disarmed(snap.boot_count);
                break;
            case SUBSTATE_ARMING:
                /* TODO: pass remaining arming time when timer API is available */
                oled_screen_arming(0);
                break;
            case SUBSTATE_ARMED:
                oled_screen_armed();
                break;
            case SUBSTATE_ALARM:
                oled_screen_alarm();
                break;
            case SUBSTATE_COOLDOWN:
                /* Reuse disarmed screen with different message — TODO Phase 5 */
                oled_screen_disarmed(snap.boot_count);
                break;
            default:
                break;
            }
            break;

        default:
            break;
        }
    }
}

static int ui_service_init(void)
{
    k_thread_create(&s_ui_thread,
                    s_ui_stack,
                    K_THREAD_STACK_SIZEOF(s_ui_stack),
                    ui_thread_fn,
                    NULL, NULL, NULL,
                    APP_PRIO_UI,
                    0,
                    K_NO_WAIT);

    k_thread_name_set(&s_ui_thread, "ui_service");
    return 0;
}

SYS_INIT(ui_service_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

/* ===========================================================================
 * Public API
 * =========================================================================== */

void ui_service_request_update(void)
{
    k_sem_give(&s_ui_update_sem);
}
