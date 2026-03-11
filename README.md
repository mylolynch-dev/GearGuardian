# Gear Guardian

A portable embedded anti-theft device for backpacks, camera bags, and gear
cases.  Runs on an **ESP32-S3 DevKitC-1** using **Zephyr RTOS**.

## Project Goals

This project demonstrates professional embedded systems engineering skills:

- **Zephyr RTOS** threading, message queues, semaphores, and timers
- **Custom device driver** for the ICM-20948 IMU written from the datasheet
  using only Zephyr I2C primitives — no vendor convenience library
- **Event-driven architecture** with a strongly typed event system and ISR-safe
  event posting
- **Boot/startup behavior** including boot metadata persistence, reset-cause
  detection, diagnostic mode, and safe mode
- **Modular, layered code** suitable for portfolio and recruiter review

## Hardware

| Component | Part |
|-----------|------|
| MCU | ESP32-S3 DevKitC-1 (N8R8) |
| IMU | SparkFun ICM-20948 9-DOF IMU breakout |
| Display | SPI SSD1306 0.96" OLED (128×64) |
| Storage | SPI microSD module |
| Tamper sensor | Magnetic reed switch (normally-closed) |
| Alarm output | Active buzzer module |
| Mode input | GPIO0 (BOOT button on DevKitC-1) |
| Power | 18650 + TP4056 charger (not in software scope for V1) |

## Operating Modes

| Mode | Entry Condition | Behavior |
|------|----------------|---------|
| **BOOT** | Power on | Startup sequence; hardware init |
| **NORMAL** | Default after boot | Arm/disarm/alarm cycle |
| **DIAGNOSTIC** | Mode button held at boot | Hardware self-test, live IMU display |
| **SAFE** | 3+ consecutive failed boots, or fatal fault | Minimal services, fault display |

### Normal Mode Sub-States

```
DISARMED → ARMING (5s) → ARMED
  ARMED → ALARM (reed open or motion) → COOLDOWN (30s) → DISARMED
```

## Repository Structure

```
gear-guardian/
  app/
    src/           Application source files (14 modules)
    include/       Central headers (events, modes, state, faults, config)
  drivers/
    icm20948/      Custom ICM-20948 driver (register-level, no vendor lib)
    reed_switch/   GPIO interrupt driver with debounce
    buzzer/        Active buzzer GPIO driver + patterns
    oled/          SSD1306 display driver + screen renderers
    sdlog/         FATFS over SPI for CSV event logging
  boards/
    esp32s3_devkitc.overlay  Devicetree overlay (pins, peripherals, NVS partition)
  docs/            Architecture, boot flow, state machine, wiring, bringup guide
  tests/           Host unit tests and on-target integration tests (Phase 6)
  CMakeLists.txt
  prj.conf
```

## Quick Start

### Prerequisites

- [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/getting_started/)
- `west` build tool
- ESP-IDF (for ESP32 flashing support in Zephyr)

### Build

```sh
# Set up Zephyr environment
source ~/zephyrproject/zephyr/zephyr-env.sh

# Build
cd gear-guardian
west build -b esp32s3_devkitc/esp32s3/procpu

# Flash
west flash

# Monitor serial output
west espressif monitor
```

### Before First Build

Review and update pin assignments in:

1. **`boards/esp32s3_devkitc.overlay`** — all GPIO, SPI, I2C pin numbers
2. **`app/include/app_config.h`** — tunable timing and threshold constants

Items marked `TODO:` in the overlay must be confirmed against your wiring.

## Development Phases

| Phase | Status | Goal |
|-------|--------|------|
| 1 | ✅ Done | Scaffold: headers, build system, docs |
| 2 | 🔲 Pending | GPIO drivers (reed, buzzer, mode button) |
| 3 | 🔲 Pending | ICM-20948 driver bring-up |
| 4 | 🔲 Pending | Normal mode state machine + event flow |
| 5 | 🔲 Pending | Diagnostic mode, safe mode, NVS metadata |
| 6 | 🔲 Pending | Watchdog, polish, threshold tuning |

## Key Design Decisions

### Custom ICM-20948 Driver

The IMU driver (`drivers/icm20948/`) is written entirely from the TDK
InvenSense ICM-20948 datasheet (DS-000189).  It handles:

- Register bank selection (REG_BANK_SEL 0x7F, 4 banks)
- WHO_AM_I probe (Bank 0 reg 0x00, expected 0xEA)
- Soft reset with polling for completion
- Accel and gyro full-scale and DLPF configuration (Bank 2)
- 14-byte burst read of accel + temp + gyro from Bank 0
- Engineering unit conversion helpers (g, deg/s, °C)

### Event System

All inter-thread communication uses a single Zephyr `k_msgq`.  The
`struct app_event` payload union is sized to 56 bytes and copied by value —
no heap allocation, no dynamic memory.

ISRs call only `k_msgq_put()`.  No mutex, no I2C, no `printk` in ISR context.

### Boot Metadata

`boot_metadata.c` provides a persistent boot counter, consecutive failure
counter, and fault history stored in NVS flash.  The Phase 1 implementation
is a RAM-only stub with the same API; the NVS upgrade in Phase 5 is a
drop-in replacement.

## Hardware Uncertainties

These items must be confirmed before flashing to real hardware:

| Item | Where to check |
|------|---------------|
| SSD1306 SPI binding string | `grep -r ssd1306 $ZEPHYR_BASE/dts/bindings` |
| SD card compatible string | `$ZEPHYR_BASE/dts/bindings/disk/` |
| Flash partition offsets | `west build` → inspect `build/zephyr/zephyr.dts` |
| GPIO bank for pins 32+ | GPIO38 → `gpio1 pin 6`, GPIO48 → `gpio1 pin 16` |
| ICM-20948 I2C address | 0x69 (AD0=high) or 0x68 (AD0=low) |
| `hwinfo` driver name on ESP32-S3 | `$ZEPHYR_BASE/drivers/hwinfo/Kconfig` |

## License

MIT License — see `LICENSE` file.

## Author

Portfolio project demonstrating Zephyr RTOS and embedded systems architecture.
