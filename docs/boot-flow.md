# Gear Guardian — Boot Flow

## Overview

`startup_run()` in `app/src/startup.c` executes the complete boot sequence
on the main Zephyr thread.  It determines which operating mode to enter,
initializes all drivers, records the boot result in NVS, and then posts
`EVT_BOOT_COMPLETE` to hand off to the event-driven thread model.

---

## Boot Flow Diagram

```
main()
  │
  └─► startup_run()
          │
          ├─ 1. boot_meta_init()
          │       └─ Open NVS partition "storage"
          │          FAULT_NVS_INIT → non-fatal, continue with RAM stub
          │
          ├─ 2. boot_meta_load()
          │       └─ Read persistent boot_meta struct
          │          First boot: reset to defaults and save
          │
          ├─ 3. Increment boot_count, save
          │
          ├─ 4. hwinfo_get_reset_cause()
          │       └─ RESET_WATCHDOG detected?
          │            YES → boot_meta_record_failed_boot(FAULT_WATCHDOG)
          │            NO  → continue
          │
          ├─ 5. startup_mode_button_held()?
          │       YES ──────────────────────────────► MODE_DIAGNOSTIC
          │       NO  → continue
          │
          ├─ 6. consecutive_failures >= APP_BOOT_FAIL_THRESHOLD (3)?
          │       YES ──────────────────────────────► MODE_SAFE
          │       NO  → MODE_NORMAL (tentative)
          │
          ├─ 7. Driver initialization (in order):
          │       a. reed_switch_init()  → non-fatal
          │       b. buzzer_init()       → non-fatal
          │       c. oled_init()         → FAULT_OLED_INIT, non-fatal
          │       d. icm20948_init()     → FAULT_IMU_INIT, FATAL
          │       e. sdlog_init()        → FAULT_SD_MOUNT, non-fatal
          │
          ├─ 8. Any FATAL faults AND mode was NORMAL?
          │       YES → override to MODE_SAFE
          │
          ├─ 9. Record boot result in NVS:
          │       faults != 0 → boot_meta_record_failed_boot(faults)
          │       faults == 0 → boot_meta_record_clean_boot()
          │
          ├─ 10. post EVT_BOOT_COMPLETE { next_mode, fault_flags }
          │
          └─ 11. Loop forever: k_sleep(K_SECONDS(60))
                  [RTOS threads handle everything from here]
```

---

## Mode Selection Logic

```
                     ┌─────────────────────────────┐
                     │  startup_run() called        │
                     └──────────────┬──────────────┘
                                    │
                          ┌─────────▼──────────┐
                          │ Mode button held?  │
                          └────┬──────────┬────┘
                            YES│          │NO
                               ▼          │
                     ┌──────────────┐     │
                     │ DIAGNOSTIC   │     ▼
                     │    MODE      │  ┌─────────────────────┐
                     └──────────────┘  │ consecutive_failures│
                                       │ >= threshold?       │
                                       └────┬───────────┬────┘
                                          YES│           │NO
                                             ▼           │
                                   ┌──────────────┐      ▼
                                   │  SAFE MODE   │  ┌──────────────┐
                                   │              │  │ NORMAL MODE  │
                                   └──────────────┘  │  (tentative) │
                                                      └──────┬───────┘
                                                             │
                                                    Driver init faults?
                                                    Any FATAL?
                                                        │
                                                  YES───┘ → SAFE MODE
                                                  NO      → NORMAL MODE
```

---

## Boot Metadata NVS Layout

```
NVS key 1: struct boot_meta (20 bytes)
  ├── magic              : 0x67656172 ("gear")
  ├── boot_count         : uint32_t
  ├── consecutive_failures: uint32_t
  └── last_fault_flags   : uint32_t (bitmask of app_fault_id_t)
```

**Phase 1**: `boot_metadata.c` uses a RAM-only stub.  Values reset to zero
on every power cycle.  The interface is identical so Phase 5 NVS upgrade is
a drop-in replacement.

---

## Reset Cause Codes (Zephyr hwinfo)

| Bit | Meaning |
|-----|---------|
| RESET_PIN | External reset pin asserted |
| RESET_SOFTWARE | Software-triggered reset |
| RESET_BROWNOUT | Supply voltage brownout |
| RESET_WATCHDOG | Watchdog timer expired |
| RESET_DEBUG | Debug interface reset |
| RESET_SECURITY | Security violation |
| RESET_LOW_POWER_WAKE | Wake from low-power sleep |
| RESET_CPU_LOCKUP | CPU lockup / hardfault |
| RESET_POR | Power-on reset |

`RESET_WATCHDOG` is the only cause that triggers `boot_meta_record_failed_boot()`.

---

## Safe Mode Entry Conditions

1. `consecutive_failures >= APP_BOOT_FAIL_THRESHOLD` (default: 3)
2. Any fatal fault occurs during driver init (`FAULT_IMU_INIT` or `FAULT_BOOT_LOOP`)
3. A future runtime EVT_FAULT with `is_fatal=true` posted during operation

---

## TODO

- [ ] Phase 5: Implement real NVS reads/writes in `boot_metadata.c`
- [ ] Phase 6: Add watchdog init in `startup_run()` and feed in service threads
- [ ] Phase 6: Display reset cause string on OLED in diagnostic mode
- [ ] Verify `hwinfo_esp32s3` driver is available for ESP32-S3 in your Zephyr
