/**
 * @file alarm_service.h
 * @brief Buzzer alarm sequencer — public control interface.
 */

#ifndef ALARM_SERVICE_H
#define ALARM_SERVICE_H

/**
 * @brief Start the alarm (called by state machine on ALARM entry).
 *
 * Posts EVT_ALARM_TRIGGER to the event queue; the alarm_service thread
 * picks it up and begins the alarm pattern loop.
 */
void alarm_service_trigger(void);

/**
 * @brief Silence the alarm (called by state machine on COOLDOWN entry).
 *
 * Posts EVT_ALARM_SILENCE; alarm_service thread stops the pattern loop.
 */
void alarm_service_silence(void);

#endif /* ALARM_SERVICE_H */
