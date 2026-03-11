/**
 * @file reed_switch.h
 * @brief Reed switch driver — GPIO interrupt + debounce.
 *
 * The reed switch is wired normally-closed: when the bag is sealed the
 * magnet holds the switch closed (GPIO reads LOW with active-low config).
 * When the bag is opened the magnet moves away and the switch opens.
 *
 * ISR discipline:
 *   The GPIO interrupt callback only submits a k_work_delayable item.
 *   After the debounce delay the work handler reads the pin and posts
 *   EVT_REED_OPEN or EVT_REED_CLOSE to the app_event_queue.
 *
 * Hardware note:
 *   The GPIO node is defined in the devicetree overlay under the alias
 *   "reed-switch".  Pin number and active polarity are set there —
 *   not hardcoded in this driver.
 */

#ifndef REED_SWITCH_H
#define REED_SWITCH_H

#include <stdbool.h>

/**
 * @brief Initialize the reed switch GPIO and register the interrupt.
 *
 * Configures the GPIO pin as input with pull-up and registers an edge
 * interrupt on both rising and falling edges.  The interrupt is not enabled
 * until this function returns successfully.
 *
 * Must be called after the Zephyr scheduler is running (work queue is live).
 *
 * @return 0 on success, negative errno on GPIO configuration failure.
 */
int reed_switch_init(void);

/**
 * @brief Read the current reed switch state synchronously (no interrupt).
 *
 * Useful at boot to determine the initial state before interrupts fire.
 * Does not require reed_switch_init() to have been called first.
 *
 * @return true  if the switch is closed (magnet present, bag sealed).
 * @return false if the switch is open  (magnet absent, bag opened).
 */
bool reed_switch_is_closed(void);

#endif /* REED_SWITCH_H */
