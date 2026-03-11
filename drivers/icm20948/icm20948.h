/**
 * @file icm20948.h
 * @brief ICM-20948 9-axis IMU driver — public API.
 *
 * This driver is written from scratch using Zephyr I2C primitives and the
 * ICM-20948 datasheet (TDK InvenSense DS-000189).  No vendor convenience
 * library is used.
 *
 * Bring-up stages:
 *   Stage 1 — icm20948_probe()        : verify WHO_AM_I over I2C
 *   Stage 2 — icm20948_init()         : reset + wake + configure
 *   Stage 3 — icm20948_read_sample()  : burst-read accel+gyro+temp
 *   Stage 4 — icm20948_convert_*()    : convert raw counts to engineering units
 *
 * All functions return 0 on success or a negative Zephyr errno value.
 * -EIO   : I2C transaction failed
 * -ENODEV: WHO_AM_I did not match expected value
 * -ETIMEDOUT: reset did not complete within timeout
 *
 * Thread safety:
 *   The driver does not protect its internal bank-select state with a mutex.
 *   Callers must ensure that only one thread accesses the driver at a time.
 *   In Gear Guardian, only the sensor thread calls into this driver after init.
 */

#ifndef ICM20948_H
#define ICM20948_H

#include <zephyr/device.h>
#include <stdint.h>
#include <stdbool.h>

/* ===========================================================================
 * Configuration struct
 * =========================================================================== */

/**
 * @brief ICM-20948 sensor configuration.
 *
 * Pass NULL to icm20948_init() to use the safe defaults defined in the .c file.
 * All FS values reference the ICM20948_ACCEL_FS_* and ICM20948_GYRO_FS_*
 * enumerations below.
 */
typedef struct icm20948_cfg {
    uint8_t accel_fs;       /**< Accel full-scale (ICM20948_ACCEL_FS_*) */
    uint8_t gyro_fs;        /**< Gyro full-scale  (ICM20948_GYRO_FS_*)  */
    uint8_t accel_dlpf;     /**< Accel DLPF config bits [5:3] in ACCEL_CONFIG */
    uint8_t gyro_dlpf;      /**< Gyro  DLPF config bits [5:3] in GYRO_CONFIG_1 */
    uint8_t accel_odr_div;  /**< Written to ACCEL_SMPLRT_DIV_2 (low byte)      */
    uint8_t gyro_odr_div;   /**< Written to GYRO_SMPLRT_DIV                     */
} icm20948_cfg_t;

/* Full-scale range selectors (maps directly to register bitfields) */
#define ICM20948_ACCEL_FS_2G    0U
#define ICM20948_ACCEL_FS_4G    1U
#define ICM20948_ACCEL_FS_8G    2U
#define ICM20948_ACCEL_FS_16G   3U

#define ICM20948_GYRO_FS_250DPS  0U
#define ICM20948_GYRO_FS_500DPS  1U
#define ICM20948_GYRO_FS_1000DPS 2U
#define ICM20948_GYRO_FS_2000DPS 3U

/* ===========================================================================
 * Raw sample struct
 * =========================================================================== */

/**
 * @brief Raw IMU sample — integer counts directly from device registers.
 *
 * Convert to engineering units using icm20948_convert_accel() or
 * icm20948_convert_gyro().
 */
typedef struct icm20948_sample {
    int16_t accel_x, accel_y, accel_z;  /**< Raw accelerometer counts */
    int16_t gyro_x,  gyro_y,  gyro_z;   /**< Raw gyroscope counts     */
    int16_t temp_raw;                    /**< Raw temperature counts   */
} icm20948_sample_t;

/* ===========================================================================
 * Engineering unit structs
 * =========================================================================== */

typedef struct icm20948_accel_g {
    float x, y, z;  /**< Acceleration in g */
} icm20948_accel_g_t;

typedef struct icm20948_gyro_dps {
    float x, y, z;  /**< Angular rate in degrees per second */
} icm20948_gyro_dps_t;

/* ===========================================================================
 * Driver handle
 *
 * Bundles the I2C device pointer, address, and current configuration so
 * callers do not need to pass them individually to every function.
 * =========================================================================== */

typedef struct icm20948_dev {
    const struct device *i2c_dev;   /**< Zephyr I2C device (from DEVICE_DT_GET) */
    uint8_t              i2c_addr;  /**< 0x68 or 0x69                           */
    icm20948_cfg_t       cfg;       /**< Active sensor configuration             */
    uint8_t              cur_bank;  /**< Cached current register bank (0–3)      */
} icm20948_dev_t;

/* ===========================================================================
 * Stage 1: Probe
 * =========================================================================== */

/**
 * @brief Read WHO_AM_I and verify it equals 0xEA.
 *
 * Useful as a quick "is the device present on the bus?" check before
 * attempting a full init sequence.  Does not change device state.
 *
 * @param dev  Initialized icm20948_dev_t handle.
 * @return 0 if WHO_AM_I == 0xEA, -ENODEV otherwise, -EIO on I2C error.
 */
int icm20948_probe(icm20948_dev_t *dev);

/* ===========================================================================
 * Stage 2: Initialize
 * =========================================================================== */

/**
 * @brief Full device initialization sequence.
 *
 * Steps:
 *   1. Select Bank 0
 *   2. Read WHO_AM_I; fail with -ENODEV if unexpected
 *   3. Issue soft reset (PWR_MGMT_1 bit 7); wait for reset to complete
 *   4. Wake device: write PWR_MGMT_1 = 0x01 (auto-clock, not sleep)
 *   5. Select Bank 2; write accel/gyro FS and DLPF config
 *   6. Write sample rate divisors
 *   7. Return to Bank 0
 *
 * @param dev    Handle with i2c_dev and i2c_addr set by caller.
 *               On success, dev->cfg and dev->cur_bank are updated.
 * @param cfg    Desired configuration; pass NULL for safe defaults.
 * @return 0 on success.
 *         -EIO on I2C error.
 *         -ENODEV if WHO_AM_I does not match.
 *         -ETIMEDOUT if reset does not complete.
 */
int icm20948_init(icm20948_dev_t *dev, const icm20948_cfg_t *cfg);

/* ===========================================================================
 * Stage 3: Read samples
 * =========================================================================== */

/**
 * @brief Burst-read one accel + temp + gyro sample.
 *
 * Ensures Bank 0 is selected, then issues a 14-byte burst read starting at
 * ACCEL_XOUT_H (0x2D).  Byte order from device is big-endian.
 *
 * @param dev   Handle (must have been initialized via icm20948_init).
 * @param out   Destination for the raw sample.
 * @return 0 on success, -EIO on I2C error.
 */
int icm20948_read_sample(icm20948_dev_t *dev, icm20948_sample_t *out);

/* ===========================================================================
 * Stage 4: Engineering unit conversion
 * =========================================================================== */

/**
 * @brief Convert raw accel counts to g.
 *
 * @param dev   Handle (uses dev->cfg.accel_fs for scaling).
 * @param raw   Pointer to a filled icm20948_sample_t.
 * @param[out] out  Converted values in g.
 */
void icm20948_convert_accel(const icm20948_dev_t *dev,
                             const icm20948_sample_t *raw,
                             icm20948_accel_g_t *out);

/**
 * @brief Convert raw gyro counts to degrees per second.
 *
 * @param dev   Handle (uses dev->cfg.gyro_fs for scaling).
 * @param raw   Pointer to a filled icm20948_sample_t.
 * @param[out] out  Converted values in deg/s.
 */
void icm20948_convert_gyro(const icm20948_dev_t *dev,
                            const icm20948_sample_t *raw,
                            icm20948_gyro_dps_t *out);

/**
 * @brief Convert raw temperature count to degrees Celsius.
 *
 * Formula: T_degC = (raw_temp / 333.87) + 21.0
 *
 * @param raw_temp  Raw temperature register value.
 * @return Temperature in degrees Celsius.
 */
float icm20948_convert_temp(int16_t raw_temp);

/* ===========================================================================
 * Low-level register access (exposed for diagnostics and testing)
 * =========================================================================== */

/**
 * @brief Select a register bank (0–3).
 *
 * Writes to REG_BANK_SEL (0x7F).  Skips the write if the requested bank
 * is already selected (cached in dev->cur_bank).
 *
 * @param dev   Driver handle.
 * @param bank  ICM20948_BANK_0 … ICM20948_BANK_3.
 * @return 0 on success, -EIO on I2C error.
 */
int icm20948_select_bank(icm20948_dev_t *dev, uint8_t bank);

/**
 * @brief Write one byte to a register in the currently selected bank.
 *
 * @param dev  Driver handle.
 * @param reg  Register address within the current bank.
 * @param val  Byte value to write.
 * @return 0 on success, -EIO on I2C error.
 */
int icm20948_write_reg(icm20948_dev_t *dev, uint8_t reg, uint8_t val);

/**
 * @brief Read one byte from a register in the currently selected bank.
 *
 * @param dev  Driver handle.
 * @param reg  Register address within the current bank.
 * @param[out] val  Byte value read from register.
 * @return 0 on success, -EIO on I2C error.
 */
int icm20948_read_reg(icm20948_dev_t *dev, uint8_t reg, uint8_t *val);

#endif /* ICM20948_H */
