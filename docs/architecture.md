# Gear Guardian — Software Architecture

## Overview

Gear Guardian is a Zephyr RTOS firmware for an ESP32-S3 DevKitC-1 that
monitors a bag or equipment case for tampering.  The architecture emphasizes:

- **Modular separation**: hardware access, core services, and application logic
  are in distinct layers
- **Strongly typed events**: all inter-thread communication goes through a
  central message queue, not ad-hoc function calls
- **Minimal ISRs**: interrupt handlers post events and return; no heavy work
  in interrupt context
- **Professional boot behavior**: boot metadata, reset-cause detection, and
  explicit safe/diagnostic mode entry paths

---

## Layer Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                        APPLICATION LAYER                            │
│  startup.c  mode_manager.c  state_machine.c  normal/diag/safe_mode │
├─────────────────────────────────────────────────────────────────────┤
│                         SERVICE LAYER                               │
│  event_dispatcher.c  logger_service.c  ui_service.c  alarm_service │
│  motion_classifier.c  fault_manager.c  boot_metadata.c             │
├─────────────────────────────────────────────────────────────────────┤
│                          DRIVER LAYER                               │
│  icm20948/  reed_switch/  buzzer/  oled/  sdlog/                    │
├─────────────────────────────────────────────────────────────────────┤
│                       ZEPHYR RTOS + HAL                             │
│  I2C  SPI  GPIO  FATFS  NVS  hwinfo  k_msgq  k_timer  k_thread     │
├─────────────────────────────────────────────────────────────────────┤
│                          HARDWARE                                   │
│  ESP32-S3  ICM-20948  SSD1306  microSD  Reed switch  Buzzer         │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Thread Model

| Thread | Priority | Stack | Blocks on | Owns |
|--------|----------|-------|-----------|------|
| main / startup | 0 (main) | 2048 B | sequential (runs once) | boot sequence |
| event_dispatcher | 2 | 2048 B | `k_msgq_get(app_event_queue)` | state machine routing |
| sensor | 3 | 2048 B | `k_sleep` (100ms period) | ICM-20948 reads |
| logger | 5 | 2048 B | `k_msgq_get(logger_queue)` | SD/FATFS writes |
| ui | 6 | 1536 B | `k_sem_take(ui_update_sem)` | OLED display |
| alarm | 5 | 1024 B | `k_sem_take(alarm_trigger_sem)` | buzzer patterns |

**ISR discipline**: All ISRs call `k_msgq_put()` or `k_work_submit()` only.
No I2C, no mutex, no `printk` in interrupt context.

---

## Event Flow

```
Hardware ISR ──k_msgq_put──► app_event_queue ──► event_dispatcher
                                                        │
                  ┌─────────────────────────────────────┤
                  │                                     │
                  ▼                                     ▼
         state_machine_handle_event()        motion_classifier_feed()
                  │                                     │
                  ├── mode transition ──► mode_manager_enter()
                  ├── alarm trigger   ──► alarm_service_trigger()
                  ├── state change    ──► ui_service_request_update()
                  └── log entry       ──► logger_service_post_str()
```

---

## Module Dependency Graph

```
main.c
  └── startup.c
        ├── boot_metadata.c   (NVS persistence)
        ├── fault_manager.c   (fault accumulation)
        ├── state_machine.c   (shared state)
        ├── icm20948.c        (I2C driver)
        ├── reed_switch.c     (GPIO interrupt)
        ├── buzzer.c          (GPIO output)
        ├── oled.c            (SPI display)
        └── sdlog.c           (FATFS)

event_dispatcher.c (SYS_INIT)
  ├── state_machine.c
  ├── mode_manager.c
  │     ├── normal_mode.c
  │     ├── diag_mode.c
  │     └── safe_mode.c
  ├── motion_classifier.c
  ├── fault_manager.c
  ├── logger_service.c
  └── ui_service.c

logger_service.c (SYS_INIT)
  └── sdlog.c

ui_service.c (SYS_INIT)
  └── oled.c

alarm_service.c (SYS_INIT)
  └── buzzer.c
```

---

## Central Headers

| Header | Purpose |
|--------|---------|
| `app_config.h` | All compile-time constants and pin docs |
| `app_faults.h` | Fault ID enum, fatal mask, severity helpers |
| `app_modes.h` | Mode and substate enums, name string externs |
| `app_events.h` | Event type enum, payload union, `struct app_event`, queue extern |
| `app_state.h` | Shared `app_state_t` struct, mutex, inline accessors |
| `boot_metadata.h` | `boot_meta_t` struct, load/save/record API |

---

## Data Flow: IMU → Alarm

```
sensor_thread
  icm20948_read_sample()
        │
        └─► EVT_IMU_SAMPLE_READY → app_event_queue
                                          │
                               event_dispatcher
                                  motion_classifier_feed()
                                          │
                                  EVT_MOTION_DETECTED → app_event_queue
                                          │
                               event_dispatcher
                                  state_machine_handle_event()
                                  [ARMED → ALARM]
                                          │
                                  alarm_service_trigger()
                                  ui_service_request_update()
                                  logger_service_post_str()
```

---

## TODO / Known Limitations (Phase 1 Scaffold)

- [ ] Font rendering in `oled.c` is stubbed — Phase 2
- [ ] NVS persistence in `boot_metadata.c` is RAM-only stub — Phase 5
- [ ] Sensor thread not yet created (will be added in Phase 3/4)
- [ ] Exact SPI/I2C pin assignments need hardware confirmation
- [ ] SSD1306 SPI binding compatible string needs verification
