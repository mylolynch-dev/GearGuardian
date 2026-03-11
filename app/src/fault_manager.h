/**
 * @file fault_manager.h
 * @brief Fault accumulation, severity classification, and safe mode trigger.
 */

#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include <stdint.h>
#include "app_faults.h"

/**
 * @brief Report a fault from any thread context.
 *
 * Constructs an EVT_FAULT event and posts it to the app_event_queue.
 * Thread-safe; does not block.
 *
 * @param fault_id  The specific fault to report.
 */
void fault_manager_report(app_fault_id_t fault_id);

/**
 * @brief Process an EVT_FAULT event (called by event_dispatcher).
 *
 * Adds fault_id to g_app_state.fault_flags.
 * If the fault is fatal, posts EVT_MODE_CHANGE → MODE_SAFE.
 *
 * @param fault_id   The fault ID from the event payload.
 * @param is_fatal   True if safe mode should be entered.
 */
void fault_manager_handle(app_fault_id_t fault_id, bool is_fatal);

/**
 * @brief Return true if any currently active fault is fatal.
 */
bool fault_manager_any_fatal(void);

/**
 * @brief Return the combined fault flags bitmask.
 */
uint32_t fault_manager_get_flags(void);

/**
 * @brief Return a short string name for a fault ID (for logging/display).
 *
 * Returns "NONE" for FAULT_NONE, "UNKNOWN" for unrecognized IDs.
 */
const char *app_fault_name(app_fault_id_t fault);

#endif /* FAULT_MANAGER_H */
