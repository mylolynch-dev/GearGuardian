/**
 * @file icm20948.c
 * @brief ICM-20948 IMU driver — handwritten from datasheet.
 *
 * Source: TDK InvenSense ICM-20948 datasheet DS-000189 rev 1.3.
 *
 * This driver uses only Zephyr's I2C primitives:
 *   i2c_write()         — write N bytes to the device
 *   i2c_write_read()    — set register address, then read N bytes
 *
 * No vendor library, no HAL abstraction layer beyond Zephyr's own I2C API.
 *
 * Register bank model:
 *   The ICM-20948 has four register banks (0–3).  REG_BANK_SEL (0x7F)
 *   is accessible from all banks and selects the active bank.  The driver
 *   caches the current bank in icm20948_dev_t.cur_bank to avoid redundant
 *   I2C transactions.
 *
 * Bring-up sequence (test each step before moving to the next):
 *   1. Verify I2C bus is ready (device_is_ready())
 *   2. Call icm20948_probe() — WHO_AM_I must return 0xEA
 *   3. Call icm20948_init() — reset, wake, configure
 *   4. Call icm20948_read_sample() in a loop — verify values change on tilt
 *   5. Call icm20948_convert_accel() — verify ~1g on whichever axis is vertical
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <math.h>
#include <string.h>

#include "icm20948.h"
#include "icm20948_regs.h"

LOG_MODULE_REGISTER(icm20948, LOG_LEVEL_INF);

/* ===========================================================================
 * Low-level I2C helpers
 * =========================================================================== */

/**
 * @brief Write a single byte to a register in the current bank.
 */
int icm20948_write_reg(icm20948_dev_t *dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    int rc = i2c_write(dev->i2c_dev, buf, sizeof(buf), dev->i2c_addr);
    if (rc != 0) {
        LOG_ERR("i2c_write failed: reg=0x%02X val=0x%02X rc=%d", reg, val, rc);
    }
    return rc;
}

/**
 * @brief Read a single byte from a register in the current bank.
 */
int icm20948_read_reg(icm20948_dev_t *dev, uint8_t reg, uint8_t *val)
{
    int rc = i2c_write_read(dev->i2c_dev,
                            dev->i2c_addr,
                            &reg, 1,
                            val,  1);
    if (rc != 0) {
        LOG_ERR("i2c_write_read failed: reg=0x%02X rc=%d", reg, rc);
    }
    return rc;
}

/**
 * @brief Select a register bank (0–3).
 *
 * Skips the I2C write if the bank is already selected.
 */
int icm20948_select_bank(icm20948_dev_t *dev, uint8_t bank)
{
    if (dev->cur_bank == bank) {
        return 0; /* Already in the right bank */
    }

    /* REG_BANK_SEL is always at 0x7F regardless of current bank */
    uint8_t bank_bits = bank & 0x30U; /* Bits [5:4] select the bank */
    uint8_t buf[2] = {ICM20948_REG_BANK_SEL, bank_bits};

    int rc = i2c_write(dev->i2c_dev, buf, sizeof(buf), dev->i2c_addr);
    if (rc != 0) {
        LOG_ERR("Bank select failed: bank=%d rc=%d", (int)(bank >> 4), rc);
        return rc;
    }

    dev->cur_bank = bank;
    return 0;
}

/* ===========================================================================
 * Stage 1: Probe (WHO_AM_I check)
 * =========================================================================== */

int icm20948_probe(icm20948_dev_t *dev)
{
    int rc;

    /* WHO_AM_I is in Bank 0 */
    rc = icm20948_select_bank(dev, ICM20948_BANK_0);
    if (rc != 0) {
        return rc;
    }

    uint8_t who_am_i = 0;
    rc = icm20948_read_reg(dev, B0_WHO_AM_I, &who_am_i);
    if (rc != 0) {
        LOG_ERR("Failed to read WHO_AM_I");
        return rc;
    }

    LOG_INF("WHO_AM_I = 0x%02X (expected 0x%02X)", who_am_i, ICM20948_WHO_AM_I_RESPONSE);

    if (who_am_i != ICM20948_WHO_AM_I_RESPONSE) {
        LOG_ERR("WHO_AM_I mismatch: got 0x%02X, expected 0x%02X",
                who_am_i, ICM20948_WHO_AM_I_RESPONSE);
        return -ENODEV;
    }

    return 0;
}

/* ===========================================================================
 * Stage 2: Initialize
 * =========================================================================== */

/* Default configuration: ±2g accel, ±250 dps gyro, DLPF enabled */
static const icm20948_cfg_t s_default_cfg = {
    .accel_fs      = ICM20948_ACCEL_FS_2G,
    .gyro_fs       = ICM20948_GYRO_FS_250DPS,
    .accel_dlpf    = 0x01U,  /* ACCEL_DLPFCFG = 1 → 111 Hz BW */
    .gyro_dlpf     = 0x01U,  /* GYRO_DLPFCFG  = 1 → 119 Hz BW */
    .accel_odr_div = 0x00U,  /* Max ODR (1125 Hz / (1 + 0) = 1125 Hz) */
    .gyro_odr_div  = 0x00U,  /* Max ODR */
};

int icm20948_init(icm20948_dev_t *dev, const icm20948_cfg_t *cfg)
{
    int rc;

    if (cfg == NULL) {
        cfg = &s_default_cfg;
    }

    /* -----------------------------------------------------------------------
     * Step 1: Select Bank 0 and probe WHO_AM_I
     * ----------------------------------------------------------------------- */
    dev->cur_bank = 0xFF; /* Force a bank select on first access */
    rc = icm20948_probe(dev);
    if (rc != 0) {
        return rc;
    }

    /* -----------------------------------------------------------------------
     * Step 2: Soft reset — set DEVICE_RESET bit in PWR_MGMT_1
     * ----------------------------------------------------------------------- */
    rc = icm20948_write_reg(dev, B0_PWR_MGMT_1, PWR_MGMT_1_DEVICE_RESET);
    if (rc != 0) {
        LOG_ERR("Failed to write device reset");
        return rc;
    }

    /* Wait for reset to take effect (datasheet: 1 ms minimum, 11 ms typical) */
    k_sleep(K_MSEC(ICM20948_RESET_WAIT_MS));

    /* -----------------------------------------------------------------------
     * Step 3: Poll until DEVICE_RESET bit clears
     * ----------------------------------------------------------------------- */
    uint8_t pwr_mgmt_1 = 0;
    int64_t deadline   = k_uptime_get() + ICM20948_RESET_TIMEOUT_MS;

    do {
        k_sleep(K_MSEC(2));
        rc = icm20948_read_reg(dev, B0_PWR_MGMT_1, &pwr_mgmt_1);
        if (rc != 0) {
            LOG_ERR("Failed to poll PWR_MGMT_1 during reset");
            return rc;
        }
    } while ((pwr_mgmt_1 & PWR_MGMT_1_DEVICE_RESET) &&
             (k_uptime_get() < deadline));

    if (pwr_mgmt_1 & PWR_MGMT_1_DEVICE_RESET) {
        LOG_ERR("ICM-20948 reset timed out");
        return -ETIMEDOUT;
    }

    LOG_DBG("ICM-20948 reset complete (PWR_MGMT_1=0x%02X)", pwr_mgmt_1);

    /* -----------------------------------------------------------------------
     * Step 4: Wake device and select auto clock source
     * The device wakes from sleep with CLKSEL = 1 (auto = recommended)
     * ----------------------------------------------------------------------- */
    rc = icm20948_write_reg(dev, B0_PWR_MGMT_1, PWR_MGMT_1_CLKSEL_AUTO);
    if (rc != 0) {
        LOG_ERR("Failed to write PWR_MGMT_1 wake");
        return rc;
    }

    /* Small delay after wake (datasheet 3.2: "wait at least 100 µs") */
    k_sleep(K_MSEC(1));

    /* -----------------------------------------------------------------------
     * Step 5: Configure gyroscope (Bank 2)
     * ----------------------------------------------------------------------- */
    rc = icm20948_select_bank(dev, ICM20948_BANK_2);
    if (rc != 0) {
        return rc;
    }

    /* GYRO_CONFIG_1: DLPF enable + FS select + DLPFCFG */
    uint8_t gyro_config = GYRO_FCHOICE_DLPF_ENABLE
                        | ((cfg->gyro_fs  & 0x03U) << 1)
                        | ((cfg->gyro_dlpf & 0x07U) << 3);
    rc = icm20948_write_reg(dev, B2_GYRO_CONFIG_1, gyro_config);
    if (rc != 0) {
        return rc;
    }

    /* GYRO_SMPLRT_DIV: sample rate divisor */
    rc = icm20948_write_reg(dev, B2_GYRO_SMPLRT_DIV, cfg->gyro_odr_div);
    if (rc != 0) {
        return rc;
    }

    /* -----------------------------------------------------------------------
     * Step 6: Configure accelerometer (still Bank 2)
     * ----------------------------------------------------------------------- */

    /* ACCEL_CONFIG: DLPF enable + FS select + DLPFCFG */
    uint8_t accel_config = ACCEL_FCHOICE_DLPF_ENABLE
                         | ((cfg->accel_fs   & 0x03U) << 1)
                         | ((cfg->accel_dlpf & 0x07U) << 3);
    rc = icm20948_write_reg(dev, B2_ACCEL_CONFIG, accel_config);
    if (rc != 0) {
        return rc;
    }

    /* ACCEL_SMPLRT_DIV_1 (high byte) = 0, ACCEL_SMPLRT_DIV_2 (low byte) */
    rc = icm20948_write_reg(dev, B2_ACCEL_SMPLRT_DIV_1, 0x00U);
    if (rc != 0) {
        return rc;
    }
    rc = icm20948_write_reg(dev, B2_ACCEL_SMPLRT_DIV_2, cfg->accel_odr_div);
    if (rc != 0) {
        return rc;
    }

    /* -----------------------------------------------------------------------
     * Step 7: Return to Bank 0 (where accel/gyro output registers live)
     * ----------------------------------------------------------------------- */
    rc = icm20948_select_bank(dev, ICM20948_BANK_0);
    if (rc != 0) {
        return rc;
    }

    /* Save active configuration */
    dev->cfg = *cfg;

    LOG_INF("ICM-20948 initialized: accel_fs=%d gyro_fs=%d",
            cfg->accel_fs, cfg->gyro_fs);
    return 0;
}

/* ===========================================================================
 * Stage 3: Read samples
 * =========================================================================== */

int icm20948_read_sample(icm20948_dev_t *dev, icm20948_sample_t *out)
{
    int rc;

    /* Ensure we are in Bank 0 (output registers are Bank 0 only) */
    rc = icm20948_select_bank(dev, ICM20948_BANK_0);
    if (rc != 0) {
        return rc;
    }

    /* Burst read: 14 bytes starting at ACCEL_XOUT_H (0x2D)
     *
     * Byte layout from device (big-endian, signed):
     *  [0] ACCEL_XOUT_H  [1] ACCEL_XOUT_L
     *  [2] ACCEL_YOUT_H  [3] ACCEL_YOUT_L
     *  [4] ACCEL_ZOUT_H  [5] ACCEL_ZOUT_L
     *  [6] TEMP_OUT_H    [7] TEMP_OUT_L
     *  [8] GYRO_XOUT_H   [9] GYRO_XOUT_L
     * [10] GYRO_YOUT_H  [11] GYRO_YOUT_L
     * [12] GYRO_ZOUT_H  [13] GYRO_ZOUT_L
     */
    uint8_t raw[ICM20948_BURST_READ_LEN];
    uint8_t start_reg = B0_BURST_READ_START;

    rc = i2c_write_read(dev->i2c_dev,
                        dev->i2c_addr,
                        &start_reg, 1,
                        raw, sizeof(raw));
    if (rc != 0) {
        LOG_ERR("Burst read failed: rc=%d", rc);
        return rc;
    }

    /* Parse big-endian int16 values */
    out->accel_x = (int16_t)((raw[0]  << 8) | raw[1]);
    out->accel_y = (int16_t)((raw[2]  << 8) | raw[3]);
    out->accel_z = (int16_t)((raw[4]  << 8) | raw[5]);
    out->temp_raw = (int16_t)((raw[6] << 8) | raw[7]);
    out->gyro_x  = (int16_t)((raw[8]  << 8) | raw[9]);
    out->gyro_y  = (int16_t)((raw[10] << 8) | raw[11]);
    out->gyro_z  = (int16_t)((raw[12] << 8) | raw[13]);

    return 0;
}

/* ===========================================================================
 * Stage 4: Engineering unit conversion
 * =========================================================================== */

static float accel_sensitivity(uint8_t accel_fs)
{
    switch (accel_fs) {
    case ICM20948_ACCEL_FS_2G:   return ICM20948_ACCEL_SENS_2G;
    case ICM20948_ACCEL_FS_4G:   return ICM20948_ACCEL_SENS_4G;
    case ICM20948_ACCEL_FS_8G:   return ICM20948_ACCEL_SENS_8G;
    case ICM20948_ACCEL_FS_16G:  return ICM20948_ACCEL_SENS_16G;
    default:                     return ICM20948_ACCEL_SENS_2G;
    }
}

static float gyro_sensitivity(uint8_t gyro_fs)
{
    switch (gyro_fs) {
    case ICM20948_GYRO_FS_250DPS:  return ICM20948_GYRO_SENS_250DPS;
    case ICM20948_GYRO_FS_500DPS:  return ICM20948_GYRO_SENS_500DPS;
    case ICM20948_GYRO_FS_1000DPS: return ICM20948_GYRO_SENS_1000DPS;
    case ICM20948_GYRO_FS_2000DPS: return ICM20948_GYRO_SENS_2000DPS;
    default:                       return ICM20948_GYRO_SENS_250DPS;
    }
}

void icm20948_convert_accel(const icm20948_dev_t *dev,
                             const icm20948_sample_t *raw,
                             icm20948_accel_g_t *out)
{
    float sens = accel_sensitivity(dev->cfg.accel_fs);
    out->x = (float)raw->accel_x / sens;
    out->y = (float)raw->accel_y / sens;
    out->z = (float)raw->accel_z / sens;
}

void icm20948_convert_gyro(const icm20948_dev_t *dev,
                            const icm20948_sample_t *raw,
                            icm20948_gyro_dps_t *out)
{
    float sens = gyro_sensitivity(dev->cfg.gyro_fs);
    out->x = (float)raw->gyro_x / sens;
    out->y = (float)raw->gyro_y / sens;
    out->z = (float)raw->gyro_z / sens;
}

float icm20948_convert_temp(int16_t raw_temp)
{
    return ((float)raw_temp / ICM20948_TEMP_SENSITIVITY) + ICM20948_TEMP_OFFSET_DEGC;
}
