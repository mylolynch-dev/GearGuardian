/**
 * @file buzzer.c
 * @brief Active buzzer driver — GPIO control and pattern sequences.
 *
 * Uses the "buzzer" DT alias defined in the overlay.
 *
 * All pattern functions are blocking (k_sleep).  They must be called
 * from the alarm_service thread or diagnostic mode handler only —
 * never from an ISR or the event_dispatcher thread.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "buzzer.h"

LOG_MODULE_REGISTER(buzzer, LOG_LEVEL_INF);

static const struct gpio_dt_spec s_buzzer_gpio =
    GPIO_DT_SPEC_GET(DT_ALIAS(buzzer), gpios);

/* ===========================================================================
 * Lifecycle
 * =========================================================================== */

int buzzer_init(void)
{
    if (!device_is_ready(s_buzzer_gpio.port)) {
        LOG_ERR("Buzzer GPIO port not ready");
        return -ENODEV;
    }

    int rc = gpio_pin_configure_dt(&s_buzzer_gpio, GPIO_OUTPUT_INACTIVE);
    if (rc != 0) {
        LOG_ERR("Failed to configure buzzer GPIO: %d", rc);
        return rc;
    }

    LOG_INF("Buzzer initialized on pin %d", s_buzzer_gpio.pin);
    return 0;
}

/* ===========================================================================
 * Primitive on/off
 * =========================================================================== */

void buzzer_on(void)
{
    gpio_pin_set_dt(&s_buzzer_gpio, 1);
}

void buzzer_off(void)
{
    gpio_pin_set_dt(&s_buzzer_gpio, 0);
}

/* ===========================================================================
 * Pattern sequences (blocking)
 * =========================================================================== */

void buzzer_chirp(uint32_t duration_ms)
{
    buzzer_on();
    k_sleep(K_MSEC(duration_ms));
    buzzer_off();
}

void buzzer_alarm_pattern(void)
{
    /* 3 × (500ms on / 200ms off) ≈ 2.1 seconds total */
    for (int i = 0; i < 3; i++) {
        buzzer_on();
        k_sleep(K_MSEC(500));
        buzzer_off();
        k_sleep(K_MSEC(200));
    }
}

void buzzer_diag_pattern(void)
{
    /* 2 × short chirp with gap ≈ 400ms total */
    for (int i = 0; i < 2; i++) {
        buzzer_chirp(100);
        k_sleep(K_MSEC(100));
    }
}

void buzzer_sos_pattern(void)
{
    /* S = 3 × 200ms on / 100ms off
     * O = 3 × 600ms on / 100ms off
     * S = 3 × 200ms on / 100ms off
     * Gap between letters: 300ms
     * Total ≈ 6 seconds
     */

    /* S — three short */
    for (int i = 0; i < 3; i++) {
        buzzer_on();
        k_sleep(K_MSEC(200));
        buzzer_off();
        k_sleep(K_MSEC(100));
    }
    k_sleep(K_MSEC(300));

    /* O — three long */
    for (int i = 0; i < 3; i++) {
        buzzer_on();
        k_sleep(K_MSEC(600));
        buzzer_off();
        k_sleep(K_MSEC(100));
    }
    k_sleep(K_MSEC(300));

    /* S — three short */
    for (int i = 0; i < 3; i++) {
        buzzer_on();
        k_sleep(K_MSEC(200));
        buzzer_off();
        k_sleep(K_MSEC(100));
    }
}
