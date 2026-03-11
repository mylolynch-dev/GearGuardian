/**
 * @file main.c
 * @brief Gear Guardian firmware entry point.
 *
 * main() is called by the Zephyr kernel after all static initialization is
 * complete.  It hands off immediately to startup_run(), which sequences the
 * full boot process.
 *
 * The main thread does NOT idle after startup_run() — startup_run() posts
 * EVT_BOOT_COMPLETE and then enters a permanent sleep so that the five RTOS
 * service threads (event_dispatcher, sensor, logger, ui, alarm) take over.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "startup.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("Gear Guardian v%s starting", CONFIG_BOARD);
    startup_run();

    /* startup_run() never returns — it loops forever in k_sleep().
     * This return is unreachable but satisfies the compiler. */
    return 0;
}
