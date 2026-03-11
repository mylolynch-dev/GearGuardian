/**
 * @file oled.c
 * @brief SSD1306 OLED driver — Zephyr display API wrapper.
 *
 * This implementation wraps the Zephyr display subsystem.  If CONFIG_DISPLAY=y
 * and CONFIG_SSD1306=y build successfully for the ESP32-S3 SPI binding, this
 * module uses display_write() to push pixel data.
 *
 * Font: A minimal 5×7 ASCII bitmap font is baked in for text rendering.
 * Only printable ASCII (0x20–0x7E) is supported.
 *
 * TODO (Phase 2 bring-up):
 *   If the Zephyr SSD1306 SPI driver is not available for ESP32-S3, replace
 *   the display_write() calls with raw SPI transactions.  The SSD1306 SPI
 *   initialization sequence is documented in the datasheet §8.1.
 *
 * TODO (Phase 5): Add proper font support and icon rendering for the alarm
 *   screen.  Consider using LVGL if RAM allows (ESP32-S3 has 512 KB SRAM).
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>

/* Zephyr display API — only available when CONFIG_DISPLAY=y */
#ifdef CONFIG_DISPLAY
#include <zephyr/drivers/display.h>
#endif

#include "oled.h"
#include "app_config.h"
#include "app_faults.h"

LOG_MODULE_REGISTER(oled, LOG_LEVEL_INF);

/* ---------------------------------------------------------------------------
 * Display device
 *
 * DT_NODELABEL(ssd1306) matches the "ssd1306@0" node in the overlay.
 * TODO: verify the label matches exactly — check generated zephyr.dts.
 * --------------------------------------------------------------------------- */
static const struct device *s_display_dev;

/* ---------------------------------------------------------------------------
 * Framebuffer
 *
 * SSD1306 128×64 monochrome = 128×64 / 8 = 1024 bytes.
 * We maintain a local framebuffer and push it to the device with oled_flush().
 * --------------------------------------------------------------------------- */
#define FB_SIZE  (APP_OLED_WIDTH * APP_OLED_HEIGHT / 8)
static uint8_t s_framebuffer[FB_SIZE];

/* ---------------------------------------------------------------------------
 * Minimal 5×7 ASCII font (characters 0x20–0x7E)
 *
 * Each character is 5 bytes wide.  Bit 0 of each byte is the topmost pixel.
 * Source: public domain 5×7 font widely used in embedded displays.
 * --------------------------------------------------------------------------- */
/* TODO (Phase 2): create oled_font5x7.h with 5x7 ASCII bitmap data.
 * Font rendering in oled_draw_text() is stubbed until then. */

/* ===========================================================================
 * Lifecycle
 * =========================================================================== */

int oled_init(void)
{
#ifdef CONFIG_DISPLAY
    s_display_dev = DEVICE_DT_GET(DT_NODELABEL(ssd1306_dev));

    if (!device_is_ready(s_display_dev)) {
        LOG_ERR("SSD1306 display device not ready");
        s_display_dev = NULL;
        return -ENODEV;
    }

    int rc = display_set_pixel_format(s_display_dev, PIXEL_FORMAT_MONO01);
    if (rc != 0) {
        LOG_WRN("display_set_pixel_format returned %d — continuing", rc);
    }

    display_blanking_off(s_display_dev);
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    LOG_INF("OLED initialized (%dx%d)", APP_OLED_WIDTH, APP_OLED_HEIGHT);
#else
    s_display_dev = NULL;
    LOG_INF("OLED disabled (CONFIG_DISPLAY not set) — text will log only");
#endif
    return 0;
}

void oled_clear(void)
{
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    oled_flush();
}

/* ===========================================================================
 * Primitives
 * =========================================================================== */

void oled_draw_text(uint8_t x, uint8_t y, const char *text)
{
    if (!text || !s_display_dev) {
        return;
    }

    /* TODO (Phase 2): Implement 5×7 bitmap font rendering into s_framebuffer.
     *
     * For each character c in text:
     *   Look up font5x7[c - 0x20] (5 column bytes)
     *   For each column byte, iterate bits and set pixels in s_framebuffer
     *   Advance x by 6 pixels (5 + 1 spacing)
     *
     * For now, log the text via printk as a bring-up placeholder.
     */
    LOG_INF("OLED text [%d,%d]: %s", x, y, text);
}

void oled_draw_hline(uint8_t y)
{
    if (y >= APP_OLED_HEIGHT || !s_display_dev) {
        return;
    }

    /* TODO (Phase 2): Set all pixels in row y of s_framebuffer */
    uint8_t page  = y / 8;
    uint8_t bit   = BIT(y % 8);

    for (int col = 0; col < APP_OLED_WIDTH; col++) {
        s_framebuffer[page * APP_OLED_WIDTH + col] |= bit;
    }
}

void oled_flush(void)
{
#ifdef CONFIG_DISPLAY
    if (!s_display_dev) {
        return;
    }

    struct display_buffer_descriptor desc = {
        .buf_size = sizeof(s_framebuffer),
        .width    = APP_OLED_WIDTH,
        .height   = APP_OLED_HEIGHT,
        .pitch    = APP_OLED_WIDTH,
    };

    int rc = display_write(s_display_dev, 0, 0, &desc, s_framebuffer);
    if (rc != 0) {
        LOG_ERR("display_write failed: %d", rc);
    }
#endif
}

/* ===========================================================================
 * High-level screen renderers
 * =========================================================================== */

void oled_screen_boot(void)
{
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    oled_draw_text(10, 16, "GEAR GUARDIAN");
    oled_draw_text(20, 32, APP_FW_VERSION_STR);
    oled_draw_hline(26);
    oled_flush();
}

void oled_screen_diagnostic(void)
{
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    oled_draw_text(0, 0, "DIAGNOSTIC MODE");
    oled_draw_hline(9);
    oled_draw_text(0, 12, "FW: " APP_FW_VERSION_STR);
    oled_flush();
}

void oled_screen_safe(uint32_t fault_flags)
{
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    oled_draw_text(15, 0, "!!! SAFE MODE !!!");
    oled_draw_hline(9);

    char fault_str[20];
    snprintf(fault_str, sizeof(fault_str), "FAULT:0x%08X", fault_flags);
    oled_draw_text(0, 16, fault_str);
    oled_draw_text(0, 40, "Power cycle to reset");
    oled_flush();
}

void oled_screen_disarmed(uint32_t boot_count)
{
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    oled_draw_text(25, 8, "GEAR GUARDIAN");
    oled_draw_hline(18);
    oled_draw_text(30, 28, "DISARMED");

    char boot_str[16];
    snprintf(boot_str, sizeof(boot_str), "Boot #%u", boot_count);
    oled_draw_text(34, 50, boot_str);
    oled_flush();
}

void oled_screen_arming(uint32_t remaining_ms)
{
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    oled_draw_text(30, 8, "ARMING...");
    oled_draw_hline(18);

    char timer_str[16];
    snprintf(timer_str, sizeof(timer_str), "%u s", remaining_ms / 1000);
    oled_draw_text(50, 32, timer_str);
    oled_draw_text(10, 50, "Close bag to arm");
    oled_flush();
}

void oled_screen_armed(void)
{
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    oled_draw_text(35, 8, "ARMED");
    oled_draw_hline(18);
    oled_draw_text(10, 32, "Monitoring active");
    oled_flush();
}

void oled_screen_alarm(void)
{
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    /* Fill top bar for visual impact */
    for (int y = 0; y < 12; y++) {
        oled_draw_hline(y);
    }
    oled_draw_text(30, 2, "!! ALARM !!");
    oled_draw_text(5, 24, "Tamper detected!");
    oled_draw_text(5, 40, "Press button: silence");
    oled_flush();
}

void oled_screen_imu_data(int16_t ax, int16_t ay, int16_t az,
                           int16_t gx, int16_t gy, int16_t gz)
{
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    oled_draw_text(0, 0, "IMU LIVE DATA");
    oled_draw_hline(9);

    char buf[22];
    snprintf(buf, sizeof(buf), "A:%6d%6d%6d", ax, ay, az);
    oled_draw_text(0, 12, buf);

    snprintf(buf, sizeof(buf), "G:%6d%6d%6d", gx, gy, gz);
    oled_draw_text(0, 24, buf);

    oled_flush();
}
