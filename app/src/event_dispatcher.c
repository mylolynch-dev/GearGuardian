/**
 * @file event_dispatcher.c
 * @brief Central event queue and dispatcher thread.
 *
 * This module owns the app_event_queue k_msgq and runs the highest-priority
 * application thread.  It dequeues events one at a time and routes them to:
 *   - state_machine_handle_event() for all mode/state transitions
 *   - motion_classifier_feed()     for IMU sample events
 *   - fault_manager_handle()       for fault events
 *   - mode_manager_enter()         for mode change events
 *
 * ISR discipline: this thread never performs I2C, file I/O, or sleeps.
 * It must complete each dispatch within a bounded time window so that the
 * queue does not fill during high-frequency IMU sampling.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_events.h"
#include "app_config.h"
#include "state_machine.h"
#include "mode_manager.h"
#include "fault_manager.h"
#include "motion_classifier.h"
#include "ui_service.h"
#include "logger_service.h"

LOG_MODULE_REGISTER(event_dispatcher, LOG_LEVEL_DBG);

/* ---------------------------------------------------------------------------
 * Message queue — the central event bus.
 *
 * All threads and ISRs post to this queue.  Only this thread consumes it.
 * Queue depth of 16 at ~56 bytes each = ~896 bytes in .bss.
 * --------------------------------------------------------------------------- */
K_MSGQ_DEFINE(app_event_queue, sizeof(app_event_t), 16, 4);

/* ---------------------------------------------------------------------------
 * Thread stack and definition
 * --------------------------------------------------------------------------- */
K_THREAD_STACK_DEFINE(s_event_stack, APP_STACK_EVENT_DISPATCHER_SZ);
static struct k_thread s_event_thread;

/* ---------------------------------------------------------------------------
 * Forward declarations
 * --------------------------------------------------------------------------- */
static void dispatcher_thread(void *p1, void *p2, void *p3);
static void dispatch_event(const app_event_t *evt);

/* ===========================================================================
 * Module initialization
 *
 * Called from startup.c (or SYS_INIT) to start the dispatcher thread.
 * We use SYS_INIT at APPLICATION level so the thread is ready before main()
 * posts the first event.
 * =========================================================================== */

static int event_dispatcher_init(void)
{
    motion_classifier_init();

    k_thread_create(&s_event_thread,
                    s_event_stack,
                    K_THREAD_STACK_SIZEOF(s_event_stack),
                    dispatcher_thread,
                    NULL, NULL, NULL,
                    APP_PRIO_EVENT_DISPATCHER,
                    0,
                    K_NO_WAIT);

    k_thread_name_set(&s_event_thread, "event_dispatcher");
    return 0;
}

SYS_INIT(event_dispatcher_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

/* ===========================================================================
 * Dispatcher thread body
 * =========================================================================== */

static void dispatcher_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    app_event_t evt;

    LOG_INF("Event dispatcher thread started");

    while (true) {
        /* Block indefinitely until an event arrives */
        k_msgq_get(&app_event_queue, &evt, K_FOREVER);
        dispatch_event(&evt);
    }
}

/* ===========================================================================
 * Event routing
 * =========================================================================== */

static void dispatch_event(const app_event_t *evt)
{
    switch (evt->type) {
    /* --- Boot completion: enter the selected operating mode --- */
    case EVT_BOOT_COMPLETE:
        LOG_INF("EVT_BOOT_COMPLETE → mode %d, faults 0x%08X",
                evt->payload.mode.next_mode,
                evt->payload.mode.fault_flags);
        mode_manager_enter(evt->payload.mode.next_mode);
        break;

    /* --- Mode change request (e.g. fatal fault → safe mode) --- */
    case EVT_MODE_CHANGE:
        LOG_INF("EVT_MODE_CHANGE → mode %d", evt->payload.mode.next_mode);
        mode_manager_enter(evt->payload.mode.next_mode);
        break;

    /* --- Fault event: accumulate and optionally enter safe mode --- */
    case EVT_FAULT:
        LOG_WRN("EVT_FAULT: id=%d fatal=%d",
                evt->payload.fault.fault_id,
                evt->payload.fault.is_fatal);
        fault_manager_handle(evt->payload.fault.fault_id,
                             evt->payload.fault.is_fatal);
        break;

    /* --- IMU sample: feed motion classifier, update state --- */
    case EVT_IMU_SAMPLE_READY:
        /* Pass the raw sample to the classifier.
         * The classifier posts EVT_MOTION_DETECTED/CLEARED if thresholds cross. */
        motion_classifier_feed((const icm20948_sample_t *)&evt->payload.imu);

        /* Update last known IMU values in shared state */
        k_mutex_lock(&g_app_state_mutex, K_FOREVER);
        g_app_state.last_ax = evt->payload.imu.ax;
        g_app_state.last_ay = evt->payload.imu.ay;
        g_app_state.last_az = evt->payload.imu.az;
        g_app_state.last_gx = evt->payload.imu.gx;
        g_app_state.last_gy = evt->payload.imu.gy;
        g_app_state.last_gz = evt->payload.imu.gz;
        k_mutex_unlock(&g_app_state_mutex);
        break;

    /* --- IMU fault --- */
    case EVT_IMU_FAULT:
        LOG_ERR("EVT_IMU_FAULT — I2C read failure");
        fault_manager_report(FAULT_IMU_READ);
        break;

    /* --- Reed switch and motion: forward to state machine --- */
    case EVT_REED_OPEN:
    case EVT_REED_CLOSE:
    case EVT_MOTION_DETECTED:
    case EVT_MOTION_CLEARED:
        state_machine_handle_event(evt);
        break;

    /* --- Mode button: forward to state machine --- */
    case EVT_MODE_BUTTON_PRESS:
        LOG_INF("EVT_MODE_BUTTON_PRESS");
        state_machine_handle_event(evt);
        break;

    /* --- Alarm control: forward to state machine --- */
    case EVT_ALARM_TRIGGER:
    case EVT_ALARM_SILENCE:
    case EVT_COOLDOWN_EXPIRE:
        state_machine_handle_event(evt);
        break;

    /* --- Log entries: forward to logger service --- */
    case EVT_LOG_ENTRY:
        /* Logger service has its own queue; this path re-routes log requests
         * that were posted as events rather than direct logger_service_post() */
        logger_service_post_str(evt->payload.log.text);
        break;

    case EVT_NONE:
        /* Spurious empty event — ignore */
        break;

    default:
        LOG_WRN("Unknown event type %d — ignored", (int)evt->type);
        break;
    }
}
