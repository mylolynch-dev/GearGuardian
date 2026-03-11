/**
 * @file app_faults.h
 * @brief Fault code definitions and severity classification for Gear Guardian.
 *
 * All modules report faults by posting an EVT_FAULT event whose payload
 * contains one of these fault IDs. The fault manager accumulates them in a
 * bitmask. Fatal faults trigger safe-mode entry; recoverable faults are
 * logged and displayed in diagnostic mode.
 *
 * Fault IDs are powers-of-two so they can be OR-combined into a bitmask
 * (stored in boot_meta.last_fault_flags and app_state.fault_flags).
 */

#ifndef APP_FAULTS_H
#define APP_FAULTS_H

#include <zephyr/sys/util.h>  /* BIT() macro */
#include <stdbool.h>
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Fault ID enum
 * --------------------------------------------------------------------------- */

/**
 * @brief Bitmask-compatible fault identifiers.
 *
 * Add new faults here. Keep the list in rough init-order so that the bitmask
 * value reflects where in startup the fault occurred.
 */
typedef enum app_fault_id {
    FAULT_NONE          = 0,

    /* --- Boot / init stage faults --- */
    FAULT_NVS_INIT      = BIT(0),   /**< NVS filesystem failed to open */
    FAULT_IMU_INIT      = BIT(1),   /**< ICM-20948 init or WHO_AM_I check failed */
    FAULT_OLED_INIT     = BIT(2),   /**< SSD1306 display init failed */
    FAULT_SD_MOUNT      = BIT(3),   /**< microSD FATFS mount failed */

    /* --- Runtime faults --- */
    FAULT_IMU_READ      = BIT(4),   /**< I2C read failure during sampling */
    FAULT_SD_WRITE      = BIT(5),   /**< FATFS write failed (logger thread) */

    /* --- System faults --- */
    FAULT_BOOT_LOOP     = BIT(6),   /**< Consecutive boot failure threshold exceeded */
    FAULT_WATCHDOG      = BIT(7),   /**< Reset caused by watchdog expiry */

} app_fault_id_t;

/* ---------------------------------------------------------------------------
 * Fatal fault mask
 *
 * If any fault in this mask is set after init, startup_run() will steer
 * the system into SAFE MODE instead of NORMAL MODE.
 * --------------------------------------------------------------------------- */

/** Faults that are unrecoverable and require safe mode. */
#define APP_FATAL_FAULT_MASK  (FAULT_IMU_INIT | FAULT_BOOT_LOOP)

/* ---------------------------------------------------------------------------
 * Severity helpers
 * --------------------------------------------------------------------------- */

/**
 * @brief Return true if the given fault ID is fatal (drives safe mode entry).
 *
 * @param fault  A single fault ID (not a combined bitmask).
 */
static inline bool app_fault_is_fatal(app_fault_id_t fault)
{
    return (fault & APP_FATAL_FAULT_MASK) != 0;
}

/**
 * @brief Return true if any fault in the combined flags bitmask is fatal.
 *
 * @param flags  OR-combination of app_fault_id_t values.
 */
static inline bool app_faults_any_fatal(uint32_t flags)
{
    return (flags & APP_FATAL_FAULT_MASK) != 0;
}

/**
 * @brief Return a short human-readable string for a fault ID.
 *
 * Defined in fault_manager.c. Returns "UNKNOWN" for unrecognized IDs.
 */
const char *app_fault_name(app_fault_id_t fault);

#endif /* APP_FAULTS_H */
