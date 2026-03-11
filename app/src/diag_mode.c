/**
 * @file diag_mode.c
 * @brief Diagnostic mode — hardware self-test and status display.
 *
 * Entered when the mode button is held at boot.  Runs a sequential test
 * routine that checks each peripheral and displays results on the OLED.
 *
 * The diag_mode_enter() function runs the entire test sequence synchronously
 * from the event_dispatcher thread.  Each test step:
 *   1. Prints result to LOG (visible over UART)
 *   2. Updates the OLED display
 *   3. Optionally writes to SD card
 *
 * After the test sequence, the system stays in DIAGNOSTIC mode and the
 * sensor thread streams live IMU data to the OLED at APP_DIAG_SAMPLE_INTERVAL_MS.
 *
 * TODO (Phase 5): Add continuous loop showing live IMU + reed state.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "diag_mode.h"
#include "app_config.h"
#include "app_faults.h"
#include "boot_metadata.h"
#include "ui_service.h"
#include "logger_service.h"
#include "buzzer.h"
#include "oled.h"
#include "sdlog.h"
#include "icm20948.h"
#include "reed_switch.h"

/* Access the IMU handle initialized in startup.c */
extern icm20948_dev_t g_imu_dev;

LOG_MODULE_REGISTER(diag_mode, LOG_LEVEL_INF);

void diag_mode_enter(void)
{
    LOG_INF("=== DIAGNOSTIC MODE ===");

    /* --- Step 1: Show diagnostic header on OLED --- */
    oled_screen_diagnostic();
    k_sleep(K_MSEC(1000));

    /* --- Step 2: Show firmware version and boot info --- */
    boot_meta_t meta;
    boot_meta_load(&meta);

    LOG_INF("FW Version : %s", APP_FW_VERSION_STR);
    LOG_INF("Boot count : %u", meta.boot_count);
    LOG_INF("Consec fail: %u", meta.consecutive_failures);
    LOG_INF("Last faults: 0x%08X", meta.last_fault_flags);

    /* --- Step 3: ICM-20948 WHO_AM_I probe --- */
    int imu_ok = icm20948_probe(&g_imu_dev);
    if (imu_ok == 0) {
        LOG_INF("IMU  WHO_AM_I: PASS (0xEA)");
    } else {
        LOG_ERR("IMU  WHO_AM_I: FAIL (%d)", imu_ok);
    }

    /* --- Step 4: Reed switch state --- */
    bool reed_closed = reed_switch_is_closed();
    LOG_INF("Reed switch: %s", reed_closed ? "CLOSED" : "OPEN");

    /* --- Step 5: Buzzer diagnostic pattern --- */
    LOG_INF("Buzzer test: firing diag pattern");
    buzzer_diag_pattern();

    /* --- Step 6: SD card test write --- */
    int sd_ok = sdlog_diag_test_write();
    if (sd_ok == 0) {
        LOG_INF("SD write   : PASS");
    } else {
        LOG_ERR("SD write   : FAIL (%d)", sd_ok);
    }

    logger_service_post_str("DIAGNOSTIC mode entered");

    LOG_INF("Diagnostic sequence complete. Live IMU data will stream.");
    /* TODO (Phase 5): Start a periodic timer to update OLED with live IMU data.
     * The sensor thread is already running; subscribe to EVT_IMU_SAMPLE_READY
     * to drive continuous display updates in this mode. */
}

void diag_mode_exit(void)
{
    LOG_INF("Exiting DIAGNOSTIC mode");
}
