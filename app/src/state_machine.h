/**
 * @file state_machine.h
 * @brief Normal-mode sub-state machine and shared app state.
 *
 * state_machine.c owns g_app_state and g_app_state_mutex.
 * All writes to app_state go through the transition functions below.
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "app_events.h"
#include "app_state.h"

/**
 * @brief Initialize the state machine and shared state.
 *
 * Zeros g_app_state, initializes the mutex, and sets mode to MODE_BOOT.
 * Must be called once before any threads start.
 */
void state_machine_init(void);

/**
 * @brief Dispatch one event to the state machine.
 *
 * Called by event_dispatcher on every event dequeued from app_event_queue.
 * Routes the event to the appropriate handler based on current mode and
 * substate.
 *
 * @param evt  The event to handle.
 */
void state_machine_handle_event(const app_event_t *evt);

/**
 * @brief Request a normal-mode substate transition.
 *
 * Acquires the mutex, updates substate, releases mutex, signals UI,
 * and posts a log entry.
 *
 * @param next  Target substate.
 */
void state_machine_set_substate(app_normal_substate_t next);

#endif /* STATE_MACHINE_H */
