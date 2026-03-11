# Gear Guardian — State Machine

## Top-Level Mode State Machine

```
         ┌──────────────────────────────────────────────────────┐
         │                    MODE_BOOT                         │
         │  (startup_run() executing; no events processed yet)  │
         └────────────────────────┬─────────────────────────────┘
                                  │ EVT_BOOT_COMPLETE
                    ┌─────────────┼─────────────┐
                    │             │             │
                    ▼             ▼             ▼
            MODE_DIAGNOSTIC  MODE_NORMAL   MODE_SAFE
              (diag tests)   (arm/alarm)   (terminal)
                    │
                    │ (TODO: exit via power cycle in V1)
```

**Mode transitions** are driven by `EVT_MODE_CHANGE` events posted to
`app_event_queue` and processed by `mode_manager_enter()` in the event
dispatcher thread.

**Fatal fault** during MODE_NORMAL → posts `EVT_MODE_CHANGE { MODE_SAFE }`.

---

## Normal Mode Sub-State Machine

```
 ┌─────────────────────────────────────────────────────────────────┐
 │                        MODE_NORMAL                              │
 │                                                                 │
 │  DISARMED ──[EVT_REED_CLOSE]──────────────► ARMING             │
 │     ▲                                          │                │
 │     │                          arming timer    │                │
 │     │                          fires           │                │
 │     │                    [EVT_COOLDOWN_EXPIRE  │                │
 │     │                      payload.raw[0]=1]   │                │
 │     │                                          ▼                │
 │     │     [EVT_REED_OPEN]              ARMED ◄─┘                │
 │     │◄───────────────────────────────────┤                      │
 │     │                                    │                      │
 │     │                       [EVT_REED_OPEN]                     │
 │     │                       [EVT_MOTION_DETECTED]               │
 │     │                                    │                      │
 │     │                                    ▼                      │
 │     │                                 ALARM                     │
 │     │                                    │                      │
 │     │            [EVT_ALARM_SILENCE]     │                      │
 │     │            [EVT_MODE_BUTTON_PRESS] │                      │
 │     │            [auto-timeout]          │                      │
 │     │                                    ▼                      │
 │     │                               COOLDOWN                    │
 │     │                                    │                      │
 │     │          [EVT_COOLDOWN_EXPIRE      │                      │
 │     │            payload.raw[0]=0]       │                      │
 │     └────────────────────────────────────┘                      │
 └─────────────────────────────────────────────────────────────────┘
```

---

## Transition Table

| Current State | Event | Next State | Side Effects |
|--------------|-------|------------|-------------|
| DISARMED | `EVT_REED_CLOSE` | ARMING | Start `s_arming_timer` (5s) |
| ARMING | `EVT_REED_OPEN` | DISARMED | Stop `s_arming_timer` |
| ARMING | `EVT_COOLDOWN_EXPIRE` (raw[0]=1) | ARMED | — |
| ARMED | `EVT_REED_OPEN` | ALARM | `alarm_service_trigger()`, start cooldown timer |
| ARMED | `EVT_MOTION_DETECTED` | ALARM | `alarm_service_trigger()`, start cooldown timer |
| ALARM | `EVT_ALARM_SILENCE` | COOLDOWN | `alarm_service_silence()`, restart cooldown timer |
| ALARM | `EVT_MODE_BUTTON_PRESS` | COOLDOWN | `alarm_service_silence()`, restart cooldown timer |
| ALARM | `EVT_COOLDOWN_EXPIRE` (raw[0]=0) | COOLDOWN | `alarm_service_silence()` (auto-timeout) |
| COOLDOWN | `EVT_COOLDOWN_EXPIRE` (raw[0]=0) | DISARMED | — |

Every transition also:
1. Calls `ui_service_request_update()` → OLED redraws
2. Calls `logger_service_post_str()` → SD log entry

---

## Timer Design

Two `K_TIMER_DEFINE` timers in `state_machine.c`:

| Timer | Duration | Posts | raw[0] |
|-------|----------|-------|--------|
| `s_arming_timer` | `APP_ARMING_DELAY_MS` (5s) | `EVT_COOLDOWN_EXPIRE` | 1 (= arming done) |
| `s_cooldown_timer` | `APP_ALARM_COOLDOWN_MS` (30s) | `EVT_COOLDOWN_EXPIRE` | 0 (= cooldown done) |

**raw[0]** is used to distinguish arming completion from cooldown completion
without creating a second event type.

Timer callbacks run in the Zephyr timer ISR.  They only call
`app_event_post_isr()` — no state modification in timer context.

---

## Diagnostic Mode

No sub-states.  `diag_mode_enter()` runs a sequential test routine:

1. Show header on OLED
2. Log firmware version and boot count
3. `icm20948_probe()` → display PASS/FAIL
4. `reed_switch_is_closed()` → display state
5. `buzzer_diag_pattern()`
6. `sdlog_diag_test_write()` → display PASS/FAIL
7. (Phase 5) Loop: stream live IMU data to OLED

---

## Safe Mode

Terminal state in V1.  No sub-states, no timers.

Entry sequence:
1. `oled_screen_safe(fault_flags)`
2. `logger_service_post_str("SAFE MODE: ...")`
3. `buzzer_sos_pattern()` (blocks ~6s)
4. Return to event loop (sits idle forever)

---

## TODO

- [ ] Phase 4: Hook sensor thread into this state machine
- [ ] Phase 5: Add continuous diagnostic IMU stream
- [ ] Phase 6: Add mode button as arm/disarm toggle in NORMAL mode
- [ ] Consider: add "LOCKED" sub-state with PIN entry for future versions
