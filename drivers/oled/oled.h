/**
 * @file oled.h
 * @brief SSD1306 OLED display driver and screen rendering API.
 *
 * This module wraps the Zephyr display subsystem API (CONFIG_DISPLAY=y,
 * CONFIG_SSD1306=y).  If the Zephyr SSD1306 driver is not available for
 * the ESP32-S3 SPI binding, the implementation falls back to raw SPI
 * transactions with a minimal 128×64 framebuffer.
 *
 * All display output goes through this module.  No other file should call
 * Zephyr display or SPI APIs for the OLED.
 *
 * Screen rendering functions:
 *   The high-level oled_screen_*() functions are the intended interface for
 *   ui_service.c.  They draw a complete screen in one call, overwriting any
 *   previous content.
 *
 * Hardware note:
 *   The SSD1306 SPI node is defined in the devicetree overlay with the
 *   label "SSD1306".  DC and RESET GPIOs are also set in the overlay.
 *
 * TODO: verify that CONFIG_SSD1306=y compiles for ESP32-S3 with SPI binding.
 *       If not, set CONFIG_DISPLAY=n and implement raw SPI path in oled.c.
 */

#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include "app_modes.h"
#include "app_faults.h"

/* ===========================================================================
 * Lifecycle
 * =========================================================================== */

/**
 * @brief Initialize the OLED display.
 *
 * Powers on, sets default contrast, clears framebuffer.
 *
 * @return 0 on success, negative errno (mapped as FAULT_OLED_INIT).
 */
int oled_init(void);

/**
 * @brief Clear the display (all pixels off).
 */
void oled_clear(void);

/* ===========================================================================
 * Primitive drawing (used internally by screen functions)
 * =========================================================================== */

/**
 * @brief Draw a null-terminated ASCII string at pixel position (x, y).
 *
 * Uses a minimal 5×7 bitmap font baked into oled.c.
 * Clips text that extends beyond the display boundary.
 *
 * @param x     Horizontal pixel offset (0 = left edge).
 * @param y     Vertical pixel offset (0 = top edge).
 * @param text  Null-terminated string.
 */
void oled_draw_text(uint8_t x, uint8_t y, const char *text);

/**
 * @brief Draw a horizontal line across the full display width.
 *
 * @param y  Vertical pixel row.
 */
void oled_draw_hline(uint8_t y);

/**
 * @brief Push the local framebuffer to the display over SPI.
 *
 * Call after all drawing commands are complete to make changes visible.
 * The high-level screen functions call this internally.
 */
void oled_flush(void);

/* ===========================================================================
 * High-level screen renderers
 *
 * Each function draws a complete screen for the named state and calls
 * oled_flush() before returning.  ui_service.c calls these.
 * =========================================================================== */

/** Display: "GEAR GUARDIAN" splash + firmware version. */
void oled_screen_boot(void);

/** Display: "DIAGNOSTIC MODE" header + pass/fail table. */
void oled_screen_diagnostic(void);

/**
 * @brief Display: "SAFE MODE" header + fault code string.
 *
 * @param fault_flags  Bitmask of active app_fault_id_t values.
 */
void oled_screen_safe(uint32_t fault_flags);

/** Display: "DISARMED" status screen with boot count. */
void oled_screen_disarmed(uint32_t boot_count);

/**
 * @brief Display: "ARMING..." countdown screen.
 *
 * @param remaining_ms  Milliseconds remaining in arming countdown.
 */
void oled_screen_arming(uint32_t remaining_ms);

/** Display: "ARMED" status screen. */
void oled_screen_armed(void);

/** Display: "ALARM!" screen with flashing indicator. */
void oled_screen_alarm(void);

/**
 * @brief Display: diagnostic IMU live data screen.
 *
 * @param ax, ay, az  Raw accel counts.
 * @param gx, gy, gz  Raw gyro counts.
 */
void oled_screen_imu_data(int16_t ax, int16_t ay, int16_t az,
                           int16_t gx, int16_t gy, int16_t gz);

#endif /* OLED_H */
