/**
 * @file motion_classifier.c
 * @brief Threshold-based motion detection from ICM-20948 accelerometer data.
 *
 * Algorithm:
 *   Maintains a circular buffer of N raw accel samples (N = APP_MOTION_WINDOW_SIZE).
 *   On each new sample:
 *     1. Compute the 3D acceleration magnitude: M = sqrt(ax²+ay²+az²)
 *     2. Maintain a rolling mean of M over the window
 *     3. Compute the absolute deviation from mean: delta = |M - mean|
 *     4. If delta > APP_MOTION_THRESHOLD  → post EVT_MOTION_DETECTED
 *     5. If delta < APP_MOTION_CLEAR_THRESHOLD → post EVT_MOTION_CLEARED
 *
 * Avoids floating-point division in the hot path by using integer arithmetic
 * for the mean and squared comparison for threshold crossing.
 *
 * TODO (Phase 6): Tune APP_MOTION_THRESHOLD and APP_MOTION_CLEAR_THRESHOLD
 * after logging idle and active IMU data on hardware.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <math.h>

#include "motion_classifier.h"
#include "app_config.h"
#include "app_events.h"

LOG_MODULE_REGISTER(motion_classifier, LOG_LEVEL_DBG);

/* ---------------------------------------------------------------------------
 * State
 * --------------------------------------------------------------------------- */

/** Circular buffer of accel magnitude values (integer counts). */
static int32_t  s_window[APP_MOTION_WINDOW_SIZE];
static uint8_t  s_window_idx     = 0;
static uint8_t  s_window_count   = 0;  /* Samples in buffer before first full wrap */
static bool     s_motion_active  = false; /* True if EVT_MOTION_DETECTED was posted */

/* ===========================================================================
 * Public API
 * =========================================================================== */

void motion_classifier_init(void)
{
    memset(s_window, 0, sizeof(s_window));
    s_window_idx    = 0;
    s_window_count  = 0;
    s_motion_active = false;
    LOG_DBG("Motion classifier initialized (window=%d)", APP_MOTION_WINDOW_SIZE);
}

void motion_classifier_feed(const icm20948_sample_t *sample)
{
    /* --- Step 1: Compute accel magnitude (integer approximation) ---
     *
     * Use integer sqrt to avoid float dependency in the hot path.
     * At ±2g FS: 1g ≈ 16384 LSB. Max magnitude ≈ 16384*sqrt(3) ≈ 28377.
     * This fits comfortably in int32_t.
     */
    int32_t ax = sample->accel_x;
    int32_t ay = sample->accel_y;
    int32_t az = sample->accel_z;

    /* Use floating point sqrt — FPU is available on ESP32-S3 (Xtensa LX7) */
    float mag_f = sqrtf((float)(ax*ax + ay*ay + az*az));
    int32_t mag = (int32_t)mag_f;

    /* --- Step 2: Add to circular window --- */
    s_window[s_window_idx] = mag;
    s_window_idx = (s_window_idx + 1) % APP_MOTION_WINDOW_SIZE;
    if (s_window_count < APP_MOTION_WINDOW_SIZE) {
        s_window_count++;
    }

    /* Don't classify until we have a full window */
    if (s_window_count < APP_MOTION_WINDOW_SIZE) {
        return;
    }

    /* --- Step 3: Compute rolling mean --- */
    int64_t sum = 0;
    for (int i = 0; i < APP_MOTION_WINDOW_SIZE; i++) {
        sum += s_window[i];
    }
    int32_t mean = (int32_t)(sum / APP_MOTION_WINDOW_SIZE);

    /* --- Step 4: Compute absolute deviation of most recent sample --- */
    int32_t delta = (mag > mean) ? (mag - mean) : (mean - mag);

    LOG_DBG("mag=%d mean=%d delta=%d threshold=%d",
            mag, mean, delta, APP_MOTION_THRESHOLD);

    /* --- Step 5: Threshold crossing --- */
    if (!s_motion_active && delta > APP_MOTION_THRESHOLD) {
        s_motion_active = true;
        LOG_INF("Motion detected: delta=%d > threshold=%d", delta, APP_MOTION_THRESHOLD);
        app_event_post_simple(EVT_MOTION_DETECTED);

    } else if (s_motion_active && delta < APP_MOTION_CLEAR_THRESHOLD) {
        s_motion_active = false;
        LOG_INF("Motion cleared: delta=%d < clear_threshold=%d",
                delta, APP_MOTION_CLEAR_THRESHOLD);
        app_event_post_simple(EVT_MOTION_CLEARED);
    }
}
