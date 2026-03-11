/**
 * @file reed_switch.c
 * @brief Reed switch GPIO interrupt driver with debounce.
 *
 * Uses Zephyr's gpio_dt_spec pattern with the "reed-switch" DT alias.
 *
 * ISR discipline:
 *   The gpio_callback fires in interrupt context.  It submits a delayable
 *   work item (5 ms debounce) and returns immediately.  The work handler
 *   runs in the system workqueue context, reads the pin, and posts an event.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "reed_switch.h"
#include "app_events.h"
#include "app_config.h"

LOG_MODULE_REGISTER(reed_switch, LOG_LEVEL_INF);

/* ---------------------------------------------------------------------------
 * DT spec — "reed-switch" alias is defined in the overlay
 * --------------------------------------------------------------------------- */
static const struct gpio_dt_spec s_reed_gpio =
    GPIO_DT_SPEC_GET(DT_ALIAS(reed_switch), gpios);

/* ---------------------------------------------------------------------------
 * Debounce work item
 * --------------------------------------------------------------------------- */
static void debounce_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(s_debounce_work, debounce_work_fn);

/* ---------------------------------------------------------------------------
 * GPIO interrupt callback
 * --------------------------------------------------------------------------- */
static struct gpio_callback s_gpio_cb;

static void reed_switch_isr(const struct device *dev,
                             struct gpio_callback *cb,
                             uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    /* Submit debounce work — do not read pin or post events here */
    k_work_reschedule(&s_debounce_work, K_MSEC(APP_REED_DEBOUNCE_MS));
}

/* ---------------------------------------------------------------------------
 * Debounce handler (runs in system workqueue, not ISR context)
 * --------------------------------------------------------------------------- */
static void debounce_work_fn(struct k_work *work)
{
    ARG_UNUSED(work);

    int val = gpio_pin_get_dt(&s_reed_gpio);
    if (val < 0) {
        LOG_ERR("gpio_pin_get_dt failed: %d", val);
        return;
    }

    /* val == 1 means active (switch closed, magnet present, active-low config) */
    bool closed = (val == 1);

    LOG_DBG("Reed switch: %s", closed ? "CLOSED" : "OPEN");

    app_event_t evt = {
        .type = closed ? EVT_REED_CLOSE : EVT_REED_OPEN,
    };
    int rc = app_event_post(&evt);
    if (rc != 0) {
        LOG_WRN("Reed event queue full — event dropped");
    }
}

/* ===========================================================================
 * Public API
 * =========================================================================== */

int reed_switch_init(void)
{
    if (!device_is_ready(s_reed_gpio.port)) {
        LOG_ERR("Reed switch GPIO port not ready");
        return -ENODEV;
    }

    int rc = gpio_pin_configure_dt(&s_reed_gpio, GPIO_INPUT);
    if (rc != 0) {
        LOG_ERR("Failed to configure reed switch GPIO: %d", rc);
        return rc;
    }

    rc = gpio_pin_interrupt_configure_dt(&s_reed_gpio,
                                         GPIO_INT_EDGE_BOTH);
    if (rc != 0) {
        LOG_ERR("Failed to configure reed switch interrupt: %d", rc);
        return rc;
    }

    gpio_init_callback(&s_gpio_cb, reed_switch_isr,
                       BIT(s_reed_gpio.pin));
    gpio_add_callback(s_reed_gpio.port, &s_gpio_cb);

    LOG_INF("Reed switch initialized on pin %d", s_reed_gpio.pin);
    return 0;
}

bool reed_switch_is_closed(void)
{
    if (!device_is_ready(s_reed_gpio.port)) {
        return false;
    }

    /* Configure as input if not already done */
    gpio_pin_configure_dt(&s_reed_gpio, GPIO_INPUT);

    int val = gpio_pin_get_dt(&s_reed_gpio);
    return (val == 1); /* active = closed */
}
