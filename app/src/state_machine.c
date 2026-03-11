/**
 * @file state_machine.c
 * @brief Normal-mode sub-state machine and shared application state.
 *
 * This module owns:
 *   - g_app_state (the shared state struct)
 *   - g_app_state_mutex (the protecting mutex)
 *   - All sub-state transition logic for MODE_NORMAL
 *
 * State transitions:
 *
 *   DISARMED ──[EVT_REED_CLOSE]──────► ARMING
 *   ARMING   ──[arming timer fires]──► ARMED
 *   ARMING   ──[EVT_REED_OPEN]───────► DISARMED  (abort arming)
 *   ARMED    ──[EVT_REED_OPEN]───────► ALARM
 *   ARMED    ──[EVT_MOTION_DETECTED]─► ALARM
 *   ALARM    ──[EVT_ALARM_SILENCE]───► COOLDOWN
 *   COOLDOWN ──[cooldown timer fires]► DISARMED
 *
 * The k_timer callbacks post lightweight events to app_event_queue so the
 * actual state change happens in the dispatcher thread context — not in the
 * timer ISR context.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "state_machine.h"
#include "app_config.h"
#include "app_events.h"
#include "app_state.h"
#include "app_modes.h"
#include "ui_service.h"
#include "logger_service.h"
#include "alarm_service.h"

LOG_MODULE_REGISTER(state_machine, LOG_LEVEL_INF);

/* ---------------------------------------------------------------------------
 * Shared state — owned by this module
 * --------------------------------------------------------------------------- */

app_state_t    g_app_state;
struct k_mutex g_app_state_mutex;

/* ---------------------------------------------------------------------------
 * Timers
 *
 * arming_timer: fired after APP_ARMING_DELAY_MS to transition ARMING → ARMED
 * cooldown_timer: fired after APP_ALARM_COOLDOWN_MS to transition COOLDOWN → DISARMED
 *
 * Both callbacks only post to the event queue — no state modification in ISR.
 * --------------------------------------------------------------------------- */

static void arming_timer_expiry(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    app_event_t evt = {
        .type = EVT_COOLDOWN_EXPIRE,  /* Reused: signals "arming complete" */
    };
    /* Set payload so the state machine can distinguish arming vs cooldown */
    evt.payload.raw[0] = 1; /* 1 = arming complete, 0 = cooldown complete */
    app_event_post_isr(&evt);
}

static void cooldown_timer_expiry(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    app_event_t evt = {
        .type = EVT_COOLDOWN_EXPIRE,
    };
    evt.payload.raw[0] = 0; /* 0 = cooldown complete */
    app_event_post_isr(&evt);
}

K_TIMER_DEFINE(s_arming_timer,   arming_timer_expiry,   NULL);
K_TIMER_DEFINE(s_cooldown_timer, cooldown_timer_expiry, NULL);

/* ---------------------------------------------------------------------------
 * Transition helpers
 * --------------------------------------------------------------------------- */

static void enter_substate(app_normal_substate_t next, const char *reason)
{
    k_mutex_lock(&g_app_state_mutex, K_FOREVER);
    app_normal_substate_t prev = g_app_state.substate;
    g_app_state.substate       = next;
    k_mutex_unlock(&g_app_state_mutex);

    LOG_INF("Substate: %s → %s (%s)",
            app_substate_names[prev],
            app_substate_names[next],
            reason);

    ui_service_request_update();
    logger_service_post_str(reason);
}

/* ===========================================================================
 * Public API
 * =========================================================================== */

void state_machine_init(void)
{
    k_mutex_init(&g_app_state_mutex);
    memset(&g_app_state, 0, sizeof(g_app_state));
    g_app_state.current_mode = MODE_BOOT;
    g_app_state.substate     = SUBSTATE_DISARMED;
}

void state_machine_set_substate(app_normal_substate_t next)
{
    enter_substate(next, "explicit transition");
}

void state_machine_handle_event(const app_event_t *evt)
{
    app_mode_t mode = app_state_get_mode();

    /* Only handle normal-mode substates here.
     * Mode transitions are handled by mode_manager. */
    if (mode != MODE_NORMAL) {
        return;
    }

    app_normal_substate_t substate = app_state_get_substate();

    switch (substate) {

    /* -------------------------------------------------------------------
     * DISARMED: waiting for bag to be closed to start arming
     * ------------------------------------------------------------------- */
    case SUBSTATE_DISARMED:
        if (evt->type == EVT_REED_CLOSE) {
            enter_substate(SUBSTATE_ARMING, "Reed closed: arming started");
            k_timer_start(&s_arming_timer, K_MSEC(APP_ARMING_DELAY_MS), K_NO_WAIT);
        }
        break;

    /* -------------------------------------------------------------------
     * ARMING: countdown before the device becomes armed
     * Reed opening during arming aborts the countdown.
     * ------------------------------------------------------------------- */
    case SUBSTATE_ARMING:
        if (evt->type == EVT_REED_OPEN) {
            k_timer_stop(&s_arming_timer);
            enter_substate(SUBSTATE_DISARMED, "Reed opened: arming aborted");
        } else if (evt->type == EVT_COOLDOWN_EXPIRE && evt->payload.raw[0] == 1) {
            enter_substate(SUBSTATE_ARMED, "Arming complete");
        }
        break;

    /* -------------------------------------------------------------------
     * ARMED: actively monitoring
     * Any reed opening or sustained motion triggers ALARM.
     * ------------------------------------------------------------------- */
    case SUBSTATE_ARMED:
        if (evt->type == EVT_REED_OPEN) {
            enter_substate(SUBSTATE_ALARM, "ALARM: reed switch opened");
            alarm_service_trigger();
            /* Start auto-silence timer */
            k_timer_start(&s_cooldown_timer, K_MSEC(APP_ALARM_COOLDOWN_MS), K_NO_WAIT);

        } else if (evt->type == EVT_MOTION_DETECTED) {
            enter_substate(SUBSTATE_ALARM, "ALARM: motion detected");
            alarm_service_trigger();
            k_timer_start(&s_cooldown_timer, K_MSEC(APP_ALARM_COOLDOWN_MS), K_NO_WAIT);
        }
        break;

    /* -------------------------------------------------------------------
     * ALARM: alarm is sounding
     * Silenced by EVT_ALARM_SILENCE (manual or timer) or mode button.
     * ------------------------------------------------------------------- */
    case SUBSTATE_ALARM:
        if (evt->type == EVT_ALARM_SILENCE ||
            evt->type == EVT_MODE_BUTTON_PRESS) {
            k_timer_stop(&s_cooldown_timer);
            alarm_service_silence();
            enter_substate(SUBSTATE_COOLDOWN, "Alarm silenced: entering cooldown");
            k_timer_start(&s_cooldown_timer, K_MSEC(APP_ALARM_COOLDOWN_MS), K_NO_WAIT);

        } else if (evt->type == EVT_COOLDOWN_EXPIRE && evt->payload.raw[0] == 0) {
            /* Auto-timeout: alarm ran for full cooldown duration */
            alarm_service_silence();
            enter_substate(SUBSTATE_COOLDOWN, "Alarm auto-silenced after timeout");
            k_timer_start(&s_cooldown_timer, K_MSEC(APP_ALARM_COOLDOWN_MS), K_NO_WAIT);
        }
        break;

    /* -------------------------------------------------------------------
     * COOLDOWN: lockout period before re-arming is allowed
     * ------------------------------------------------------------------- */
    case SUBSTATE_COOLDOWN:
        if (evt->type == EVT_COOLDOWN_EXPIRE && evt->payload.raw[0] == 0) {
            enter_substate(SUBSTATE_DISARMED, "Cooldown complete: disarmed");
        }
        break;

    default:
        LOG_WRN("Unhandled substate %d in event handler", (int)substate);
        break;
    }
}
