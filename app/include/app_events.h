/**
 * @file app_events.h
 * @brief Strongly-typed event system for Gear Guardian.
 *
 * All inter-thread and ISR→thread communication goes through a single
 * Zephyr message queue (k_msgq).  Events are fixed-size structs that can
 * be copied safely across context boundaries without heap allocation.
 *
 * ISR rule: ISRs must only call k_msgq_put() — no I2C, no mutex, no sleep.
 *
 * Queue:
 *   The queue instance (app_event_queue) is defined in event_dispatcher.c.
 *   Post from anywhere via app_event_post() or app_event_post_isr().
 *   The event dispatcher thread is the sole consumer.
 *
 * Payload sizing:
 *   The union is sized to the largest branch (log text = 40 bytes).
 *   Total struct size ≈ 56 bytes.  Queue depth of 16 → ~896 bytes of
 *   static buffer in .bss.
 */

#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>
#include "app_faults.h"
#include "app_modes.h"

/* ===========================================================================
 * Event type enum
 * =========================================================================== */

typedef enum app_event_type {
    EVT_NONE = 0,

    /* --- Hardware events (posted by ISRs or driver callbacks) --- */
    EVT_REED_OPEN,          /**< Reed switch opened (tamper / bag opened)     */
    EVT_REED_CLOSE,         /**< Reed switch closed (bag closed / at rest)    */
    EVT_MODE_BUTTON_PRESS,  /**< Mode button pressed (arm/disarm or diag)     */
    EVT_IMU_SAMPLE_READY,   /**< Sensor thread has a fresh accel/gyro reading */
    EVT_IMU_FAULT,          /**< I2C read failure in sensor thread            */

    /* --- Motion classifier outputs --- */
    EVT_MOTION_DETECTED,    /**< Sustained/spike motion threshold exceeded    */
    EVT_MOTION_CLEARED,     /**< Accel magnitude dropped below clear threshold*/

    /* --- Alarm control --- */
    EVT_ALARM_TRIGGER,      /**< Request: start buzzer alarm                  */
    EVT_ALARM_SILENCE,      /**< Request: stop buzzer alarm (manual or timer) */
    EVT_COOLDOWN_EXPIRE,    /**< k_timer callback: cooldown or arming elapsed */

    /* --- Mode and state transitions --- */
    EVT_MODE_CHANGE,        /**< Request: transition to a new operating mode  */
    EVT_BOOT_COMPLETE,      /**< startup_run() finished; payload has target mode */

    /* --- Fault reporting --- */
    EVT_FAULT,              /**< A driver or service reported a fault         */

    /* --- Logger --- */
    EVT_LOG_ENTRY,          /**< logger_service: enqueue a CSV log record     */

    EVT_TYPE_COUNT          /**< Sentinel — keep last                         */
} app_event_type_t;

/* ===========================================================================
 * Payload branches
 *
 * Each branch is used by the event types noted in the comment.
 * Unused payload fields in a posted event are left zero.
 * =========================================================================== */

/** Payload for EVT_IMU_SAMPLE_READY. */
struct evt_payload_imu {
    int16_t ax, ay, az;    /**< Raw accelerometer counts (big-endian from device, converted) */
    int16_t gx, gy, gz;    /**< Raw gyroscope counts                                        */
    int16_t temp_raw;      /**< Raw temperature counts                                      */
    int64_t timestamp_ms;  /**< k_uptime_get() at time of read                              */
};

/** Payload for EVT_FAULT. */
struct evt_payload_fault {
    app_fault_id_t fault_id;  /**< Specific fault that occurred    */
    bool           is_fatal;  /**< True if safe mode should follow */
};

/** Payload for EVT_MODE_CHANGE and EVT_BOOT_COMPLETE. */
struct evt_payload_mode {
    app_mode_t next_mode;          /**< Requested/selected mode     */
    uint32_t   fault_flags;        /**< Active faults at transition */
};

/** Payload for EVT_LOG_ENTRY. */
struct evt_payload_log {
    char text[40];                 /**< Null-terminated log summary */
};

/* ===========================================================================
 * Event payload union
 *
 * Sized to 52 bytes by the raw[] member; total struct app_event ≈ 56 bytes.
 * =========================================================================== */

union app_event_payload {
    struct evt_payload_imu   imu;
    struct evt_payload_fault fault;
    struct evt_payload_mode  mode;
    struct evt_payload_log   log;
    uint8_t                  raw[52]; /**< Ensures union is always 52 bytes */
};

/* ===========================================================================
 * Event struct
 * =========================================================================== */

/**
 * @brief A single Gear Guardian application event.
 *
 * Instances are allocated on the stack of the posting thread/ISR, then
 * copied by value into the k_msgq.  No heap allocation required.
 */
typedef struct app_event {
    app_event_type_t     type;    /**< Discriminates which payload branch is valid */
    union app_event_payload payload;
} app_event_t;

/* ===========================================================================
 * Message queue (defined in event_dispatcher.c)
 * =========================================================================== */

/** Central event queue.  All events flow through here. */
extern struct k_msgq app_event_queue;

/* ===========================================================================
 * Posting helpers
 * =========================================================================== */

/**
 * @brief Post an event from a thread context.
 *
 * Uses K_NO_WAIT — does not block.  If the queue is full the event is
 * dropped and -ENOMSG is returned.  Callers should log queue-full
 * conditions as a sign that queue depth or dispatcher latency needs tuning.
 *
 * @param evt  Pointer to a stack-allocated event.  Copied by value.
 * @return 0 on success, -ENOMSG if queue is full.
 */
static inline int app_event_post(const app_event_t *evt)
{
    return k_msgq_put(&app_event_queue, evt, K_NO_WAIT);
}

/**
 * @brief Post an event from an ISR.
 *
 * Identical to app_event_post(); k_msgq_put is ISR-safe in Zephyr.
 * Provided as a named alias so code review can quickly identify ISR call sites.
 *
 * @param evt  Pointer to a stack-allocated event.  Copied by value.
 * @return 0 on success, -ENOMSG if queue is full.
 */
static inline int app_event_post_isr(const app_event_t *evt)
{
    return k_msgq_put(&app_event_queue, evt, K_NO_WAIT);
}

/**
 * @brief Convenience: post a simple event with no payload.
 *
 * @param type  Event type; payload will be zeroed.
 * @return 0 on success, -ENOMSG if queue is full.
 */
static inline int app_event_post_simple(app_event_type_t type)
{
    app_event_t evt = {
        .type = type,
        .payload = { .raw = {0} }
    };
    return app_event_post(&evt);
}

#endif /* APP_EVENTS_H */
