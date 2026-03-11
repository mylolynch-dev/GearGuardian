/**
 * @file sensor_thread.c
 * @brief IMU sensor sampling thread.
 *
 * Runs at priority APP_PRIO_SENSOR.  Each iteration:
 *   1. Sleep for the appropriate sample interval (mode-dependent)
 *   2. Call icm20948_read_sample()
 *   3. On success: post EVT_IMU_SAMPLE_READY with raw accel/gyro data
 *   4. On failure: post EVT_IMU_FAULT
 *
 * The g_imu_dev handle is initialized by startup.c before this thread runs.
 * This thread is the sole consumer of the ICM-20948 driver after init.
 *
 * TODO (Phase 3): Add CONFIG_WATCHDOG feeding here once watchdog is enabled.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "sensor_thread.h"
#include "app_config.h"
#include "app_events.h"
#include "app_state.h"
#include "app_modes.h"
#include "icm20948.h"

LOG_MODULE_REGISTER(sensor_thread, LOG_LEVEL_DBG);

/* The IMU handle is initialized in startup.c */
extern icm20948_dev_t g_imu_dev;

K_THREAD_STACK_DEFINE(s_sensor_stack, APP_STACK_SENSOR_SZ);
static struct k_thread s_sensor_thread;

static void sensor_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    LOG_INF("Sensor thread started");

    icm20948_sample_t sample;

    while (true) {
        /* Use a slower rate in diagnostic mode for display readability */
        app_mode_t mode = app_state_get_mode();
        uint32_t interval = (mode == MODE_DIAGNOSTIC)
                          ? APP_DIAG_SAMPLE_INTERVAL_MS
                          : APP_NORMAL_SAMPLE_INTERVAL_MS;

        k_sleep(K_MSEC(interval));

        /* Only sample when in a mode that benefits from it */
        if (mode == MODE_SAFE || mode == MODE_BOOT) {
            continue;
        }

        int rc = icm20948_read_sample(&g_imu_dev, &sample);
        if (rc != 0) {
            LOG_ERR("icm20948_read_sample failed: %d", rc);
            app_event_post_simple(EVT_IMU_FAULT);
            continue;
        }

        app_event_t evt = {
            .type = EVT_IMU_SAMPLE_READY,
            .payload.imu = {
                .ax           = sample.accel_x,
                .ay           = sample.accel_y,
                .az           = sample.accel_z,
                .gx           = sample.gyro_x,
                .gy           = sample.gyro_y,
                .gz           = sample.gyro_z,
                .timestamp_ms = k_uptime_get(),
            }
        };

        int post_rc = app_event_post(&evt);
        if (post_rc != 0) {
            LOG_WRN("sensor: event queue full — sample dropped");
        }
    }
}

static int sensor_thread_sys_init(void)
{
    k_thread_create(&s_sensor_thread,
                    s_sensor_stack,
                    K_THREAD_STACK_SIZEOF(s_sensor_stack),
                    sensor_thread_fn,
                    NULL, NULL, NULL,
                    APP_PRIO_SENSOR,
                    0,
                    K_NO_WAIT);

    k_thread_name_set(&s_sensor_thread, "sensor");
    return 0;
}

SYS_INIT(sensor_thread_sys_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
