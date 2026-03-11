/**
 * @file motion_classifier.h
 * @brief Threshold-based motion detection from IMU samples.
 */

#ifndef MOTION_CLASSIFIER_H
#define MOTION_CLASSIFIER_H

#include "icm20948.h"

/**
 * @brief Initialize the classifier (clears sliding window).
 */
void motion_classifier_init(void);

/**
 * @brief Feed one IMU sample into the classifier.
 *
 * Updates the sliding window.  If the RMS accel magnitude deviation
 * from the window mean exceeds APP_MOTION_THRESHOLD, posts
 * EVT_MOTION_DETECTED.  If previously detected and now below
 * APP_MOTION_CLEAR_THRESHOLD, posts EVT_MOTION_CLEARED.
 *
 * Called by event_dispatcher on EVT_IMU_SAMPLE_READY.
 *
 * @param sample  Pointer to the latest raw IMU sample.
 */
void motion_classifier_feed(const icm20948_sample_t *sample);

#endif /* MOTION_CLASSIFIER_H */
