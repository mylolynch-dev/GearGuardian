/**
 * @file logger_service.c
 * @brief SD card logging service — dedicated thread, owns all SD writes.
 *
 * Architecture:
 *   - All log requests from other modules arrive via logger_service_post()
 *   - The logger thread dequeues records and calls sdlog_write()
 *   - Every LOG_FLUSH_INTERVAL_RECORDS records, sdlog_flush() is called
 *   - SD errors are handled locally; non-fatal errors do NOT crash the system
 *
 * No other module calls sdlog_write() directly.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "logger_service.h"
#include "app_config.h"
#include "app_state.h"
#include "sdlog.h"
#include "fault_manager.h"

LOG_MODULE_REGISTER(logger_service, LOG_LEVEL_INF);

/* ---------------------------------------------------------------------------
 * Logger message queue
 *
 * Separate from app_event_queue so that log record posting never blocks the
 * main event dispatcher queue.  Only the logger thread consumes this queue.
 * --------------------------------------------------------------------------- */
K_MSGQ_DEFINE(s_logger_queue,
              sizeof(log_record_t),
              APP_LOG_QUEUE_DEPTH,
              4);

/* ---------------------------------------------------------------------------
 * Thread
 * --------------------------------------------------------------------------- */
K_THREAD_STACK_DEFINE(s_logger_stack, APP_STACK_LOGGER_SZ);
static struct k_thread s_logger_thread;

#define LOG_FLUSH_INTERVAL_RECORDS  8  /* Flush every N records */

static void logger_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    LOG_INF("Logger service thread started");

    log_record_t rec;
    uint32_t records_since_flush = 0;

    while (true) {
        k_msgq_get(&s_logger_queue, &rec, K_FOREVER);

        int rc = sdlog_write(&rec);
        if (rc != 0) {
            LOG_ERR("sdlog_write failed (%d)", rc);
            fault_manager_report(FAULT_SD_WRITE);
        } else {
            records_since_flush++;
            if (records_since_flush >= LOG_FLUSH_INTERVAL_RECORDS) {
                sdlog_flush();
                records_since_flush = 0;
            }
        }
    }
}

static int logger_service_init(void)
{
    k_thread_create(&s_logger_thread,
                    s_logger_stack,
                    K_THREAD_STACK_SIZEOF(s_logger_stack),
                    logger_thread_fn,
                    NULL, NULL, NULL,
                    APP_PRIO_LOGGER,
                    0,
                    K_NO_WAIT);

    k_thread_name_set(&s_logger_thread, "logger_svc");
    return 0;
}

SYS_INIT(logger_service_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

/* ===========================================================================
 * Public API
 * =========================================================================== */

int logger_service_post(const log_record_t *rec)
{
    if (!rec) {
        return -EINVAL;
    }
    return k_msgq_put(&s_logger_queue, rec, K_NO_WAIT);
}

void logger_service_post_str(const char *summary)
{
    if (!summary) {
        return;
    }

    log_record_t rec = {0};
    rec.timestamp_ms = k_uptime_get();

    /* Snapshot current mode/substate without blocking */
    rec.mode     = (uint8_t)g_app_state.current_mode;
    rec.substate = (uint8_t)g_app_state.substate;
    rec.fault_flags = g_app_state.fault_flags;

    strncpy(rec.summary, summary, sizeof(rec.summary) - 1);
    rec.summary[sizeof(rec.summary) - 1] = '\0';

    int rc = logger_service_post(&rec);
    if (rc != 0) {
        LOG_WRN("logger queue full, record dropped: %s", summary);
    }
}
