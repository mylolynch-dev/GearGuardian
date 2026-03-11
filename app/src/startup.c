/**
 * @file startup.c
 * @brief Boot coordinator — sequences init, determines operating mode.
 *
 * This module is the single owner of the boot sequence.  It runs entirely
 * on the main thread (before handing off to the RTOS service threads via
 * event posting) and follows a strict initialization order:
 *
 *   1. boot_meta_init()      — open NVS flash partition
 *   2. boot_meta_load()      — read persistent boot counter and fault history
 *   3. hwinfo_get_reset_cause() — check for watchdog / unexpected reset
 *   4. Increment boot_count and save
 *   5. Determine boot mode (diagnostic / safe / normal)
 *   6. icm20948_init()       — IMU (fatal if fails)
 *   7. oled_init()           — display (non-fatal)
 *   8. sdlog_init()          — SD card (non-fatal)
 *   9. Post EVT_BOOT_COMPLETE
 *  10. Record boot result in NVS
 *  11. Sleep forever (threads take over)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>

/* Zephyr hwinfo for reset-cause detection */
#include <zephyr/drivers/hwinfo.h>

#include "startup.h"
#include "app_config.h"
#include "app_events.h"
#include "app_modes.h"
#include "app_faults.h"
#include "boot_metadata.h"
#include "fault_manager.h"
#include "state_machine.h"
#include "ui_service.h"

/* Drivers */
#include "icm20948.h"
#include "reed_switch.h"
#include "buzzer.h"
#include "oled.h"
#include "sdlog.h"

LOG_MODULE_REGISTER(startup, LOG_LEVEL_INF);

/* ---------------------------------------------------------------------------
 * Mode button DT spec
 *
 * The "mode-btn" alias is defined in boards/esp32s3_devkitc.overlay.
 * GPIO0 (BOOT button) on DevKitC-1 is active-low with pull-up.
 * --------------------------------------------------------------------------- */
static const struct gpio_dt_spec s_mode_btn =
    GPIO_DT_SPEC_GET(DT_ALIAS(mode_btn), gpios);

/* ---------------------------------------------------------------------------
 * ICM-20948 driver handle
 *
 * Populated during startup_run().  Made accessible to sensor_thread via
 * a shared pointer.  sensor_thread.c declares an extern for this.
 * --------------------------------------------------------------------------- */
icm20948_dev_t g_imu_dev;

/* ---------------------------------------------------------------------------
 * Forward declarations
 * --------------------------------------------------------------------------- */
static app_mode_t determine_boot_mode(const boot_meta_t *meta,
                                      uint32_t reset_cause);
static uint32_t   init_drivers(void);
static void       post_boot_complete(app_mode_t mode, uint32_t fault_flags);

/* ===========================================================================
 * Public API
 * =========================================================================== */

bool startup_mode_button_held(void)
{
    if (!device_is_ready(s_mode_btn.port)) {
        LOG_WRN("Mode button GPIO not ready — assuming not held");
        return false;
    }

    /* Configure as input before reading (overlay sets pull-up and active-low) */
    int rc = gpio_pin_configure_dt(&s_mode_btn, GPIO_INPUT);
    if (rc != 0) {
        LOG_ERR("Failed to configure mode button GPIO: %d", rc);
        return false;
    }

    /* Read low = button pressed (active-low) */
    int val = gpio_pin_get_dt(&s_mode_btn);
    return (val == 1); /* gpio_pin_get_dt returns 1 when in active state */
}

void startup_run(void)
{
    int rc;

    LOG_INF("=== Gear Guardian boot sequence start ===");

    /* -----------------------------------------------------------------------
     * Step 1: Initialize state machine (sets up mutex and initial state)
     * ----------------------------------------------------------------------- */
    state_machine_init();

    /* -----------------------------------------------------------------------
     * Step 2: Boot metadata — NVS init + load
     * ----------------------------------------------------------------------- */
    rc = boot_meta_init();
    if (rc != 0) {
        LOG_ERR("NVS init failed (%d) — boot metadata unavailable", rc);
        fault_manager_report(FAULT_NVS_INIT);
        /* Non-fatal for boot; continue with RAM-only metadata */
    }

    boot_meta_t meta;
    rc = boot_meta_load(&meta);
    if (rc != 0) {
        LOG_WRN("boot_meta_load failed (%d) — using defaults", rc);
        boot_meta_reset(&meta);
    }

    meta.boot_count++;
    LOG_INF("Boot count: %u  consecutive failures: %u  last faults: 0x%08X",
            meta.boot_count, meta.consecutive_failures, meta.last_fault_flags);
    boot_meta_save(&meta);

    /* -----------------------------------------------------------------------
     * Step 3: Check hardware reset cause
     * ----------------------------------------------------------------------- */
    uint32_t reset_cause = 0;

#if defined(CONFIG_HWINFO)
    rc = hwinfo_get_reset_cause(&reset_cause);
    if (rc == 0) {
        hwinfo_clear_reset_cause();
        LOG_INF("Reset cause: 0x%08X", reset_cause);

        if (reset_cause & RESET_WATCHDOG) {
            LOG_WRN("Watchdog reset detected — recording failed boot");
            boot_meta_record_failed_boot(FAULT_WATCHDOG);
            /* Reload after recording */
            boot_meta_load(&meta);
        }
    } else {
        LOG_WRN("hwinfo_get_reset_cause returned %d", rc);
    }
#else
    LOG_WRN("CONFIG_HWINFO not enabled — reset cause unknown");
#endif

    /* -----------------------------------------------------------------------
     * Step 4: Check mode button (synchronous read before ISRs are armed)
     * ----------------------------------------------------------------------- */
    bool diag_requested = startup_mode_button_held();
    if (diag_requested) {
        LOG_INF("Mode button held at boot — DIAGNOSTIC MODE selected");
    }

    /* -----------------------------------------------------------------------
     * Step 5: Determine boot mode
     * ----------------------------------------------------------------------- */
    app_mode_t boot_mode = determine_boot_mode(&meta, reset_cause);
    LOG_INF("Selected boot mode: %d", (int)boot_mode);

    /* -----------------------------------------------------------------------
     * Step 6: Initialize drivers
     * ----------------------------------------------------------------------- */
    uint32_t fault_flags = init_drivers();

    /* If init produced fatal faults and we weren't already headed to safe mode,
     * override the mode selection */
    if (app_faults_any_fatal(fault_flags) && boot_mode == MODE_NORMAL) {
        LOG_WRN("Fatal init fault — overriding to SAFE MODE");
        boot_mode = MODE_SAFE;
    }

    /* -----------------------------------------------------------------------
     * Step 7: Record boot result in NVS
     * ----------------------------------------------------------------------- */
    if (fault_flags != 0) {
        boot_meta_record_failed_boot(fault_flags);
    } else {
        boot_meta_record_clean_boot();
    }

    /* -----------------------------------------------------------------------
     * Step 8: Post boot complete event — threads will handle the rest
     * ----------------------------------------------------------------------- */
    post_boot_complete(boot_mode, fault_flags);

    LOG_INF("=== Gear Guardian boot sequence complete — entering %s ===",
            app_mode_names[boot_mode]);

    /* -----------------------------------------------------------------------
     * Step 9: Main thread sleeps forever; RTOS threads handle everything
     * ----------------------------------------------------------------------- */
    while (true) {
        k_sleep(K_SECONDS(60));
    }
}

/* ===========================================================================
 * Private helpers
 * =========================================================================== */

/**
 * @brief Determine which mode to enter based on boot metadata and context.
 */
static app_mode_t determine_boot_mode(const boot_meta_t *meta,
                                      uint32_t reset_cause)
{
    /* Diagnostic mode: mode button was held at boot */
    if (startup_mode_button_held()) {
        return MODE_DIAGNOSTIC;
    }

    /* Safe mode: too many consecutive failures */
    if (meta->consecutive_failures >= APP_BOOT_FAIL_THRESHOLD) {
        LOG_WRN("Consecutive boot failures (%u >= %u) — entering SAFE MODE",
                meta->consecutive_failures, APP_BOOT_FAIL_THRESHOLD);
        return MODE_SAFE;
    }

    return MODE_NORMAL;
}

/**
 * @brief Initialize all peripheral drivers in order.
 *
 * @return Bitmask of app_fault_id_t for any faults encountered.
 */
static uint32_t init_drivers(void)
{
    uint32_t faults = 0;
    int rc;

    /* --- Reed switch (non-fatal; system can run without it) --- */
    rc = reed_switch_init();
    if (rc != 0) {
        LOG_WRN("Reed switch init failed (%d) — tamper detection disabled", rc);
        /* Not in our fault enum as a standalone fault; log only */
    }

    /* --- Buzzer (non-fatal) --- */
    rc = buzzer_init();
    if (rc != 0) {
        LOG_WRN("Buzzer init failed (%d) — audio alerts disabled", rc);
    }

    /* --- OLED display (non-fatal) --- */
    rc = oled_init();
    if (rc != 0) {
        LOG_ERR("OLED init failed (%d)", rc);
        faults |= FAULT_OLED_INIT;
        fault_manager_report(FAULT_OLED_INIT);
    } else {
        oled_screen_boot();
    }

    /* --- ICM-20948 IMU (FATAL — motion detection is core functionality) --- */
    const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C0 device not ready");
        faults |= FAULT_IMU_INIT;
        fault_manager_report(FAULT_IMU_INIT);
    } else {
        g_imu_dev.i2c_dev   = i2c_dev;
        g_imu_dev.i2c_addr  = APP_ICM20948_I2C_ADDR;

        rc = icm20948_init(&g_imu_dev, NULL /* use defaults */);
        if (rc != 0) {
            LOG_ERR("ICM-20948 init failed (%d) — WHO_AM_I or I2C issue", rc);
            faults |= FAULT_IMU_INIT;
            fault_manager_report(FAULT_IMU_INIT);
        } else {
            LOG_INF("ICM-20948 initialized OK");
        }
    }

    /* --- microSD card (non-fatal) --- */
    rc = sdlog_init();
    if (rc != 0) {
        LOG_ERR("SD card init failed (%d)", rc);
        faults |= FAULT_SD_MOUNT;
        fault_manager_report(FAULT_SD_MOUNT);
    } else {
        LOG_INF("SD card mounted OK");
    }

    return faults;
}

/**
 * @brief Post EVT_BOOT_COMPLETE with the selected mode and active fault flags.
 */
static void post_boot_complete(app_mode_t mode, uint32_t fault_flags)
{
    app_event_t evt = {
        .type = EVT_BOOT_COMPLETE,
        .payload.mode = {
            .next_mode   = mode,
            .fault_flags = fault_flags,
        }
    };

    int rc = app_event_post(&evt);
    if (rc != 0) {
        LOG_ERR("Failed to post EVT_BOOT_COMPLETE (%d) — queue full at boot", rc);
    }
}
