/**
 * @file icm20948_regs.h
 * @brief ICM-20948 register address definitions and bitfield constants.
 *
 * This file is included ONLY by icm20948.c — it is not part of the public
 * driver API and should not be included by application code.
 *
 * Register organization:
 *   The ICM-20948 has four register banks (0–3).  Bank selection is done by
 *   writing the bank number to REG_BANK_SEL (0x7F), which is available in
 *   all banks.  After reset the device is in Bank 0.
 *
 * Source: TDK InvenSense ICM-20948 datasheet, DS-000189 rev 1.3.
 *
 * Bank designations use the prefix:
 *   B0_  → Bank 0
 *   B1_  → Bank 1
 *   B2_  → Bank 2
 *   B3_  → Bank 3
 */

#ifndef ICM20948_REGS_H
#define ICM20948_REGS_H

/* ===========================================================================
 * Register bank select (present in all banks at address 0x7F)
 * =========================================================================== */

#define ICM20948_REG_BANK_SEL          0x7FU

#define ICM20948_BANK_0                (0U << 4)
#define ICM20948_BANK_1                (1U << 4)
#define ICM20948_BANK_2                (2U << 4)
#define ICM20948_BANK_3                (3U << 4)

/* ===========================================================================
 * Bank 0 registers
 * =========================================================================== */

#define B0_WHO_AM_I                    0x00U  /**< Device ID; should read 0xEA          */
#define B0_USER_CTRL                   0x03U  /**< Enable/disable features              */
#define B0_LP_CONFIG                   0x05U  /**< Low power configuration              */
#define B0_PWR_MGMT_1                  0x06U  /**< Power management 1                   */
#define B0_PWR_MGMT_2                  0x07U  /**< Power management 2                   */
#define B0_INT_PIN_CFG                 0x0FU  /**< Interrupt pin / bypass enable        */
#define B0_INT_ENABLE                  0x10U  /**< Interrupt enable                     */
#define B0_INT_ENABLE_1                0x11U  /**< Interrupt enable 1 (raw data ready)  */
#define B0_INT_ENABLE_2                0x12U  /**< Interrupt enable 2 (FIFO overflow)   */
#define B0_INT_ENABLE_3                0x13U  /**< Interrupt enable 3                   */
#define B0_I2C_MST_STATUS              0x17U  /**< I2C master status                    */
#define B0_INT_STATUS                  0x19U  /**< Interrupt status                     */
#define B0_INT_STATUS_1                0x1AU  /**< Interrupt status 1 (data ready)      */
#define B0_INT_STATUS_2                0x1BU  /**< Interrupt status 2                   */
#define B0_INT_STATUS_3                0x1CU  /**< Interrupt status 3                   */

/* Accel output registers (6 bytes: XH, XL, YH, YL, ZH, ZL) */
#define B0_ACCEL_XOUT_H                0x2DU
#define B0_ACCEL_XOUT_L                0x2EU
#define B0_ACCEL_YOUT_H                0x2FU
#define B0_ACCEL_YOUT_L                0x30U
#define B0_ACCEL_ZOUT_H                0x31U
#define B0_ACCEL_ZOUT_L                0x32U

/* Temperature output (2 bytes) */
#define B0_TEMP_OUT_H                  0x33U
#define B0_TEMP_OUT_L                  0x34U

/* Gyro output registers (6 bytes: XH, XL, YH, YL, ZH, ZL) */
#define B0_GYRO_XOUT_H                 0x35U
#define B0_GYRO_XOUT_L                 0x36U
#define B0_GYRO_YOUT_H                 0x37U
#define B0_GYRO_YOUT_L                 0x38U
#define B0_GYRO_ZOUT_H                 0x39U
#define B0_GYRO_ZOUT_L                 0x3AU

/* Burst-read starting address: reads 14 bytes = ax,ay,az (6) + temp (2) + gx,gy,gz (6) */
#define B0_BURST_READ_START            B0_ACCEL_XOUT_H
#define ICM20948_BURST_READ_LEN        14U

/* ===========================================================================
 * Bank 0 — PWR_MGMT_1 bitfields (register 0x06)
 * =========================================================================== */

#define PWR_MGMT_1_DEVICE_RESET        BIT(7) /**< Soft reset; self-clears when done  */
#define PWR_MGMT_1_SLEEP               BIT(6) /**< Sleep mode enable                   */
#define PWR_MGMT_1_LP_EN               BIT(5) /**< Low power mode enable               */
#define PWR_MGMT_1_TEMP_DIS            BIT(3) /**< Disable temperature sensor          */
#define PWR_MGMT_1_CLKSEL_AUTO         0x01U  /**< Auto-select clock source (recommended) */
#define PWR_MGMT_1_CLKSEL_INTERNAL     0x00U  /**< Internal 20 MHz oscillator          */

/* ===========================================================================
 * Bank 0 — USER_CTRL bitfields (register 0x03)
 * =========================================================================== */

#define USER_CTRL_DMP_EN               BIT(7)
#define USER_CTRL_FIFO_EN              BIT(6)
#define USER_CTRL_I2C_MST_EN           BIT(5)
#define USER_CTRL_I2C_IF_DIS           BIT(4)  /**< Disable I2C interface (SPI only mode) */
#define USER_CTRL_DMP_RST              BIT(3)
#define USER_CTRL_SRAM_RST             BIT(2)
#define USER_CTRL_I2C_MST_RST          BIT(1)

/* ===========================================================================
 * Bank 0 — INT_PIN_CFG bitfields (register 0x0F)
 * =========================================================================== */

#define INT_PIN_CFG_BYPASS_EN          BIT(1)  /**< Enable I2C master bypass */

/* ===========================================================================
 * Bank 2 registers — gyroscope configuration
 * =========================================================================== */

#define B2_GYRO_SMPLRT_DIV             0x00U  /**< Gyro sample rate divisor            */
#define B2_GYRO_CONFIG_1               0x01U  /**< Gyro full-scale, DLPF, ODR          */
#define B2_GYRO_CONFIG_2               0x02U  /**< Gyro averaging, self-test           */

/* GYRO_CONFIG_1 full-scale select (bits [2:1]) */
#define GYRO_FS_SEL_250DPS             (0U << 1)
#define GYRO_FS_SEL_500DPS             (1U << 1)
#define GYRO_FS_SEL_1000DPS            (2U << 1)
#define GYRO_FS_SEL_2000DPS            (3U << 1)

/* GYRO_CONFIG_1 DLPF enable (bit 0) */
#define GYRO_FCHOICE_DLPF_ENABLE       BIT(0)

/* ===========================================================================
 * Bank 2 registers — accelerometer configuration
 * =========================================================================== */

#define B2_ACCEL_SMPLRT_DIV_1          0x10U  /**< Accel sample rate divisor high byte */
#define B2_ACCEL_SMPLRT_DIV_2          0x11U  /**< Accel sample rate divisor low byte  */
#define B2_ACCEL_INTEL_CTRL            0x12U  /**< Accel intelligence control          */
#define B2_ACCEL_WOM_THR               0x13U  /**< Wake-on-motion threshold            */
#define B2_ACCEL_CONFIG                0x14U  /**< Accel full-scale, DLPF, ODR         */
#define B2_ACCEL_CONFIG_2              0x15U  /**< Accel averaging, self-test          */

/* ACCEL_CONFIG full-scale select (bits [2:1]) */
#define ACCEL_FS_SEL_2G                (0U << 1)
#define ACCEL_FS_SEL_4G                (1U << 1)
#define ACCEL_FS_SEL_8G                (2U << 1)
#define ACCEL_FS_SEL_16G               (3U << 1)

/* ACCEL_CONFIG DLPF enable (bit 0) */
#define ACCEL_FCHOICE_DLPF_ENABLE      BIT(0)

/* ===========================================================================
 * Sensor scaling constants (for engineering unit conversion)
 *
 * Sensitivity values from datasheet Table 1 (typical values).
 * Divide raw int16 by sensitivity to get physical units.
 * =========================================================================== */

/** Accel sensitivity at each FS range (LSB per g) */
#define ICM20948_ACCEL_SENS_2G         16384.0f
#define ICM20948_ACCEL_SENS_4G         8192.0f
#define ICM20948_ACCEL_SENS_8G         4096.0f
#define ICM20948_ACCEL_SENS_16G        2048.0f

/** Gyro sensitivity at each FS range (LSB per deg/s) */
#define ICM20948_GYRO_SENS_250DPS      131.0f
#define ICM20948_GYRO_SENS_500DPS      65.5f
#define ICM20948_GYRO_SENS_1000DPS     32.8f
#define ICM20948_GYRO_SENS_2000DPS     16.4f

/** Temperature: T_degC = (raw - 0) / 333.87 + 21.0  (datasheet formula) */
#define ICM20948_TEMP_SENSITIVITY      333.87f
#define ICM20948_TEMP_OFFSET_DEGC      21.0f

/* ===========================================================================
 * Expected WHO_AM_I response
 * =========================================================================== */

#define ICM20948_WHO_AM_I_RESPONSE     0xEAU

/* ===========================================================================
 * Reset timing
 * =========================================================================== */

/** Wait at least 1ms for the power supply to settle after reset. */
#define ICM20948_RESET_WAIT_MS         10U

/** Maximum time to poll for reset completion (PWR_MGMT_1 DEVICE_RESET to clear). */
#define ICM20948_RESET_TIMEOUT_MS      100U

#endif /* ICM20948_REGS_H */
