# Gear Guardian — Hardware Bring-Up Plan

## Phase 1: Build System Only (No Hardware Needed)

**Goal**: `west build -b esp32s3_devkitc/esp32s3/procpu` succeeds with zero errors.

### Steps
1. Install Zephyr SDK and `west`:
   ```
   pip install west
   west init ~/zephyrproject
   cd ~/zephyrproject && west update
   west sdk install
   ```
2. Set `ZEPHYR_BASE`:
   ```
   source ~/zephyrproject/zephyr/zephyr-env.sh
   ```
3. Build from the project root:
   ```
   cd ~/path/to/gear-guardian
   west build -b esp32s3_devkitc/esp32s3/procpu
   ```
4. Inspect the build output:
   - Check `build/zephyr/zephyr.dts` for correct SPI/I2C/GPIO node resolution
   - Verify flash partition offsets in the DTS match your partition table

### Expected Issues at This Stage
- `CONFIG_SSD1306=y` may not compile for SPI on ESP32-S3 → set to `n` and stub `oled.c`
- `CONFIG_DISK_DRIVER_SDMMC` symbol name may differ → check `$ZEPHYR_BASE/subsys/disk/Kconfig`
- Flash partition offsets may conflict with ESP-IDF partition table

---

## Phase 2: GPIO Drivers (Reed Switch, Buzzer, Mode Button)

**Goal**: Basic GPIO test binary runs on hardware.

### Test Procedure
1. Wire only: ESP32-S3 + buzzer + reed switch + mode button
2. Comment out ICM-20948, OLED, and SD init in `startup.c`
3. Build and flash:
   ```
   west flash
   west espressif monitor
   ```
4. Observe over UART (115200 baud):
   - `[startup] Mode button held: false/true`
   - Buzzer chirps 2× at boot (diag pattern)
   - Reed switch state changes printed on `printk` when magnet moved

### Pass Criteria
- [ ] Buzzer sounds when GPIO5 driven high
- [ ] Reed switch ISR fires on both open and close
- [ ] Mode button reads correctly (GPIO0 = DevKitC BOOT button)

---

## Phase 3: ICM-20948 IMU Driver

**Goal**: WHO_AM_I = 0xEA, live accel/gyro values print over UART.

### Test Procedure
1. Wire: ESP32-S3 I2C0 (GPIO8=SDA, GPIO9=SCL) → ICM-20948 breakout
2. Verify 3.3V and GND connections
3. Verify AD0 pin is connected to 3.3V (address = 0x69)
4. Build and flash with a minimal test `main()`:
   ```c
   const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));
   icm20948_dev_t imu = { .i2c_dev = i2c, .i2c_addr = 0x69 };
   int rc = icm20948_init(&imu, NULL);
   printk("init: %d\n", rc);
   icm20948_sample_t s;
   while(1) {
       icm20948_read_sample(&imu, &s);
       printk("ax=%d ay=%d az=%d\n", s.accel_x, s.accel_y, s.accel_z);
       k_sleep(K_MSEC(500));
   }
   ```

### Pass Criteria
- [ ] `icm20948_init()` returns 0 (not -EIO or -ENODEV)
- [ ] WHO_AM_I log line shows `0xEA`
- [ ] Accel values change when board is tilted
- [ ] With board flat, one axis reads approximately ±16384 (1g at ±2g FS)

### Troubleshooting
- `-EIO` on first I2C transaction → check wiring, pull-ups, power
- `WHO_AM_I = 0x00` → I2C NACKs (wrong address or wiring issue)
- `WHO_AM_I = 0xFF` → SDA stuck high (missing pull-up)
- Check I2C bus frequency: start at 100 kHz if 400 kHz fails

---

## Phase 4: OLED Display

**Goal**: Status screen renders correctly on the SSD1306.

### Test Procedure
1. Wire SPI2 to OLED (MOSI=GPIO11, SCLK=GPIO12, CS=GPIO10, DC=GPIO13, RST=GPIO14)
2. Verify `CONFIG_DISPLAY=y` and `CONFIG_SSD1306=y` build successfully
3. Call `oled_screen_boot()` at startup
4. Verify "GEAR GUARDIAN" text appears on OLED

### If Zephyr SSD1306 SPI Driver Unavailable
Implement raw SPI initialization in `oled.c`:
- SSD1306 init sequence: 0xAE (off), 0x20 0x00 (horizontal addressing), etc.
- See SSD1306 datasheet §8.1 Application Example

---

## Phase 5: microSD Card

**Goal**: CSV log file created and written on boot.

### Test Procedure
1. Format SD card as FAT32 (not exFAT)
2. Wire SPI3 (MOSI=GPIO35, MISO=GPIO37, SCLK=GPIO36, CS=GPIO38)
3. Flash firmware, check UART for `SD card mounted at /SD:`
4. Remove SD card and verify `gg_log.csv` was created with header row

---

## Phase 6: Full Integration

**Goal**: Complete alarm cycle works end-to-end.

### Test Procedure
1. Power on with all peripherals connected
2. Verify boot sequence over UART: BOOT → NORMAL → DISARMED on OLED
3. Close the magnet on the reed switch → ARMING countdown on OLED
4. Wait 5 seconds → ARMED state on OLED
5. Remove the magnet → ALARM: buzzer sounds, OLED shows ALARM screen
6. Press mode button → COOLDOWN, buzzer silences
7. Wait 30 seconds → DISARMED
8. Remove SD card → verify log entries for each state transition
9. Hold mode button at boot → verify DIAGNOSTIC MODE entry and test sequence
10. Edit `prj.conf`: simulate 3 failed boots (set consecutive_failures manually) → verify SAFE MODE

---

## Common Issues Reference

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `west build` fails with "device not found" | Missing DT node or bad compatible | Check overlay vs Zephyr version bindings |
| I2C -EIO | Wiring issue or no pull-ups | Verify SDA/SCL with scope |
| SD mount fails | Card not FAT32 or SPI wiring | Re-format card, check MISO pull-up |
| OLED blank | Wrong CS/DC/RST pins | Verify overlay matches your module |
| Buzzer silent | GPIO wrong or current limited | Check with multimeter, add transistor |
| WHO_AM_I ≠ 0xEA | Wrong I2C address (AD0 pin) | Check 0x68 vs 0x69 |
