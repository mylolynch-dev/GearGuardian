/**
 * @file alarm_service.c
 * @brief Buzzer alarm sequencer — dedicated thread for pattern playback.
 *
 * The alarm_service thread waits on a semaphore.  When signaled to start, it
 * loops the buzzer_alarm_pattern() until silenced.  Control signals arrive
 * via two semaphores (trigger and silence) rather than a message queue, which
 * simplifies the silencing logic.
 *
 * buzzer_alarm_pattern() and buzzer_sos_pattern() are blocking calls in
 * buzzer.c.  They are called from this thread — not from any ISR or the
 * event_dispatcher thread.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "alarm_service.h"
#include "app_config.h"
#include "app_events.h"
#include "buzzer.h"

LOG_MODULE_REGISTER(alarm_service, LOG_LEVEL_INF);

/* ---------------------------------------------------------------------------
 * Control semaphores
 * --------------------------------------------------------------------------- */

/** Given by alarm_service_trigger(); thread wakes and starts pattern loop. */
K_SEM_DEFINE(s_alarm_trigger_sem, 0, 1);

/** Set to true by alarm_service_silence() to break the pattern loop. */
static volatile bool s_alarm_active = false;

/* ---------------------------------------------------------------------------
 * Thread
 * --------------------------------------------------------------------------- */

/* Alarm thread runs at same priority as logger (priority 5) to avoid
 * starving the event dispatcher. */
K_THREAD_STACK_DEFINE(s_alarm_stack, 1024);
static struct k_thread s_alarm_thread;

static void alarm_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    LOG_INF("Alarm service thread started");

    while (true) {
        /* Wait indefinitely for an alarm trigger */
        k_sem_take(&s_alarm_trigger_sem, K_FOREVER);

        LOG_WRN("Alarm triggered — sounding pattern");
        s_alarm_active = true;

        /* Loop alarm pattern until silenced */
        while (s_alarm_active) {
            buzzer_alarm_pattern();
            /* buzzer_alarm_pattern() takes ~2.1 seconds per call.
             * After it returns, check if we've been silenced before looping. */
        }

        buzzer_off();
        LOG_INF("Alarm silenced");
    }
}

static int alarm_service_init(void)
{
    k_thread_create(&s_alarm_thread,
                    s_alarm_stack,
                    K_THREAD_STACK_SIZEOF(s_alarm_stack),
                    alarm_thread_fn,
                    NULL, NULL, NULL,
                    APP_PRIO_LOGGER,  /* Same priority as logger */
                    0,
                    K_NO_WAIT);

    k_thread_name_set(&s_alarm_thread, "alarm_svc");
    return 0;
}

SYS_INIT(alarm_service_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

/* ===========================================================================
 * Public API
 * =========================================================================== */

void alarm_service_trigger(void)
{
    s_alarm_active = true;
    k_sem_give(&s_alarm_trigger_sem);
}

void alarm_service_silence(void)
{
    s_alarm_active = false;
    /* The alarm thread will check s_alarm_active after the current
     * buzzer_alarm_pattern() call completes and exit the loop. */
}
