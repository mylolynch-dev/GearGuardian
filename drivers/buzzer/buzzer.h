/**
 * @file buzzer.h
 * @brief Active buzzer driver — GPIO control and pattern sequences.
 *
 * The buzzer is an active type (contains its own oscillator) so it emits
 * a tone whenever the GPIO output is driven high.  No PWM required for V1.
 *
 * Pattern functions block the calling thread for their duration using
 * k_sleep().  They must be called from the alarm_service thread (or any
 * thread where blocking is acceptable) — NOT from an ISR or the event
 * dispatcher thread.
 *
 * Hardware note:
 *   The GPIO node is defined in the devicetree overlay under the alias
 *   "buzzer".  Pin number and active polarity are set there.
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

/**
 * @brief Initialize the buzzer GPIO pin as output, drive low (silent).
 *
 * @return 0 on success, negative errno on GPIO init failure.
 */
int buzzer_init(void);

/**
 * @brief Drive the buzzer ON (continuous tone).
 *
 * Non-blocking.  Caller must call buzzer_off() to silence.
 */
void buzzer_on(void);

/**
 * @brief Drive the buzzer OFF (silent).
 *
 * Non-blocking.
 */
void buzzer_off(void);

/**
 * @brief Sound a single chirp of the given duration.
 *
 * Blocks the calling thread for duration_ms.
 *
 * @param duration_ms  How long the tone lasts, in milliseconds.
 */
void buzzer_chirp(uint32_t duration_ms);

/**
 * @brief Sound the alarm pattern: 3 × (500ms on / 200ms off).
 *
 * Blocks ~2.1 seconds.  Intended to be called in a loop by alarm_service
 * while the alarm state is active.
 */
void buzzer_alarm_pattern(void);

/**
 * @brief Sound the diagnostic acknowledgment pattern: 2 × 100ms chirps.
 *
 * Blocks ~400ms.  Used in DIAGNOSTIC mode to confirm a test step.
 */
void buzzer_diag_pattern(void);

/**
 * @brief Sound the SOS / safe-mode distress pattern.
 *
 * S = 3 × 200ms   O = 3 × 600ms   S = 3 × 200ms
 * Blocks ~6 seconds.  Played once in safe_mode_enter().
 */
void buzzer_sos_pattern(void);

#endif /* BUZZER_H */
