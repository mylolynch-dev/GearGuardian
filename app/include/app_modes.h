/**
 * @file app_modes.h
 * @brief Top-level operating mode and normal-mode sub-state enumerations.
 *
 * Boot flow:
 *   MODE_BOOT → (mode button held) → MODE_DIAGNOSTIC
 *   MODE_BOOT → (boot failure threshold exceeded) → MODE_SAFE
 *   MODE_BOOT → (default) → MODE_NORMAL
 *
 * Normal mode sub-states:
 *   SUBSTATE_DISARMED → SUBSTATE_ARMING → SUBSTATE_ARMED → SUBSTATE_ALARM
 *                                                                  ↓
 *                       SUBSTATE_DISARMED ← SUBSTATE_COOLDOWN ←──┘
 *
 * Mode and substate strings are declared extern here and defined in
 * mode_manager.c for display and logging use.
 */

#ifndef APP_MODES_H
#define APP_MODES_H

/* ---------------------------------------------------------------------------
 * Top-level operating modes
 * --------------------------------------------------------------------------- */

typedef enum app_mode {
    MODE_BOOT       = 0, /**< Startup in progress; no alarm behavior active  */
    MODE_NORMAL     = 1, /**< Normal arm/disarm/alarm operation               */
    MODE_DIAGNOSTIC = 2, /**< Hardware self-test and status display           */
    MODE_SAFE       = 3, /**< Minimal services; fatal fault recovery          */

    MODE_COUNT           /**< Sentinel — keep last                            */
} app_mode_t;

/* ---------------------------------------------------------------------------
 * Normal-mode sub-states
 * (Only meaningful when current_mode == MODE_NORMAL)
 * --------------------------------------------------------------------------- */

typedef enum app_normal_substate {
    SUBSTATE_DISARMED = 0, /**< System idle; no alarm monitoring              */
    SUBSTATE_ARMING   = 1, /**< Arming countdown; abort window open           */
    SUBSTATE_ARMED    = 2, /**< Actively monitoring reed switch and IMU       */
    SUBSTATE_ALARM    = 3, /**< Alarm triggered; buzzer active                */
    SUBSTATE_COOLDOWN = 4, /**< Alarm silenced; lockout period before re-arm  */

    SUBSTATE_COUNT         /**< Sentinel — keep last                          */
} app_normal_substate_t;

/* ---------------------------------------------------------------------------
 * Human-readable name strings (defined in mode_manager.c)
 * --------------------------------------------------------------------------- */

/** Mode name strings indexed by app_mode_t. */
extern const char *const app_mode_names[MODE_COUNT];

/** Substate name strings indexed by app_normal_substate_t. */
extern const char *const app_substate_names[SUBSTATE_COUNT];

#endif /* APP_MODES_H */
