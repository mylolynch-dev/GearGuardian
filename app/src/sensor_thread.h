/**
 * @file sensor_thread.h
 * @brief IMU sensor sampling thread — public interface.
 *
 * The sensor thread periodically calls icm20948_read_sample() and posts
 * EVT_IMU_SAMPLE_READY events to the app_event_queue.  It is the only
 * thread that accesses the ICM-20948 driver after startup.
 */

#ifndef SENSOR_THREAD_H
#define SENSOR_THREAD_H

/**
 * @brief Initialize and start the sensor sampling thread.
 *
 * Called via SYS_INIT at APPLICATION level.  The thread blocks on k_sleep()
 * between samples; it uses APP_NORMAL_SAMPLE_INTERVAL_MS in NORMAL/ARMED
 * mode and APP_DIAG_SAMPLE_INTERVAL_MS in DIAGNOSTIC mode.
 */
void sensor_thread_start(void);

#endif /* SENSOR_THREAD_H */
