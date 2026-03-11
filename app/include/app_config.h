/**
 * @file app_config.h
 * @brief Compile-time configuration constants for Gear Guardian.
 *
 * This is the single place to tune hardware pin assignments, timing
 * parameters, thread stack sizes, thread priorities, and threshold values.
 *
 * Pin assignments marked "TODO: confirm wiring" must be verified against
 * your physical wiring before flashing to hardware.  They are collected
 * here so you only have to change one file — not hunt through driver code.
 *
 * All constants here are plain C preprocessor macros (no types, no includes
 * of project headers) so this file can be included anywhere without risk
 * of circular dependencies.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ===========================================================================
 * Firmware identity
 * =========================================================================== */

#define APP_FW_VERSION_MAJOR  0
#define APP_FW_VERSION_MINOR  1
#define APP_FW_VERSION_PATCH  0
#define APP_FW_VERSION_STR    "0.1.0-dev"

/* ===========================================================================
 * I2C — ICM-20948 IMU
 *
 * AD0 pin on the SparkFun ICM-20948 breakout sets the I2C address:
 *   AD0 = GND → 0x68
 *   AD0 = VCC → 0x69  (SparkFun default pull-up)
 * TODO: confirm AD0 wiring on your breakout board.
 * =========================================================================== */

#define APP_ICM20948_I2C_ADDR       0x69   /**< I2C address (AD0 high = 0x69) */
#define APP_ICM20948_WHO_AM_I_VAL   0xEAU  /**< Expected WHO_AM_I register value */
#define APP_I2C_TIMEOUT_MS          10     /**< Per-transaction I2C timeout      */

/* ===========================================================================
 * Timing constants (milliseconds unless noted)
 * =========================================================================== */

/** Time the system waits for reed switch closure before arming completes. */
#define APP_ARMING_DELAY_MS             5000

/** After alarm silences, system stays in COOLDOWN for this long. */
#define APP_ALARM_COOLDOWN_MS           30000

/** IMU sample interval in NORMAL (ARMED) mode. */
#define APP_NORMAL_SAMPLE_INTERVAL_MS   100

/** IMU sample interval in DIAGNOSTIC mode (slower, for display readability). */
#define APP_DIAG_SAMPLE_INTERVAL_MS     500

/** Reed switch debounce window. */
#define APP_REED_DEBOUNCE_MS            5

/** Mode button hold time required to enter DIAGNOSTIC mode at boot. */
#define APP_MODE_BTN_HOLD_MS            2000

/* ===========================================================================
 * Boot health thresholds
 * =========================================================================== */

/**
 * Number of consecutive boot failures before the system enters SAFE MODE
 * instead of NORMAL MODE on the next boot.
 */
#define APP_BOOT_FAIL_THRESHOLD         3

/* ===========================================================================
 * Motion classifier thresholds
 *
 * Raw ICM-20948 accel units at ±2g full scale:
 *   1g ≈ 16384 LSB
 * Tune APP_MOTION_THRESHOLD empirically by logging idle and active data.
 * =========================================================================== */

/** Motion detection window size (number of samples). */
#define APP_MOTION_WINDOW_SIZE          8

/**
 * RMS accel-magnitude delta from window mean that constitutes "motion".
 * At ±2g FS: 16384 LSB = 1g.  Start conservative (~0.1g = 1638).
 * TODO: tune after logging on hardware.
 */
#define APP_MOTION_THRESHOLD            1638

/**
 * Delta must drop below this value for motion_classifier to post
 * EVT_MOTION_CLEARED.  Set slightly below threshold to add hysteresis.
 */
#define APP_MOTION_CLEAR_THRESHOLD      1000

/* ===========================================================================
 * SD / Logger
 * =========================================================================== */

/** Full path to the log file on the SD card FATFS volume. */
#define APP_LOG_FILENAME                "/SD:/gg_log.csv"

/** Depth of the logger service message queue. */
#define APP_LOG_QUEUE_DEPTH             16

/* ===========================================================================
 * OLED display geometry
 * =========================================================================== */

#define APP_OLED_WIDTH                  128  /**< Pixels wide  */
#define APP_OLED_HEIGHT                 64   /**< Pixels tall  */

/* ===========================================================================
 * Thread stack sizes (bytes)
 * =========================================================================== */

#define APP_STACK_MAIN_SZ               2048
#define APP_STACK_EVENT_DISPATCHER_SZ   2048
#define APP_STACK_SENSOR_SZ             2048
#define APP_STACK_LOGGER_SZ             2048
#define APP_STACK_UI_SZ                 1536

/* ===========================================================================
 * Thread priorities
 *
 * Lower numerical value = higher priority in Zephyr's preemptive scheduler.
 * All values are in the preemptive range (0..CONFIG_NUM_PREEMPT_PRIORITIES-1).
 * =========================================================================== */

#define APP_PRIO_EVENT_DISPATCHER       2   /**< Highest app priority; must stay responsive */
#define APP_PRIO_SENSOR                 3   /**< IMU sampler; slightly below dispatcher     */
#define APP_PRIO_LOGGER                 5   /**< SD writes; lower than sensor               */
#define APP_PRIO_UI                     6   /**< OLED renderer; lowest app priority         */

/* ===========================================================================
 * Hardware GPIO — pin numbers
 *
 * These map to the devicetree aliases defined in boards/esp32s3_devkitc.overlay.
 * The DT aliases are: "reed-switch", "mode-btn", "buzzer".
 * The actual GPIO numbers are set in the overlay — NOT hardcoded here — so
 * that driver code uses DT_ALIAS() and these constants are only needed for
 * documentation or fallback logic.
 *
 * TODO: verify all pin assignments match your physical wiring.
 * =========================================================================== */

/* Reference documentation only; real assignments are in the .overlay file:
 *
 *  Reed switch input : GPIO4  (gpio0, active-low, pull-up)
 *  Mode button input : GPIO0  (gpio0, active-low, pull-up — BOOT button)
 *  Buzzer output     : GPIO5  (gpio0, active-high)
 *  OLED CS           : GPIO10 (spi2)
 *  OLED DC           : GPIO13 (gpio0)
 *  OLED RESET        : GPIO14 (gpio0)
 *  SD CS             : GPIO38 (gpio1 pin 6)  ← 38-32=6
 *  ICM-20948 SDA     : GPIO8  (i2c0)
 *  ICM-20948 SCL     : GPIO9  (i2c0)
 */

#endif /* APP_CONFIG_H */
