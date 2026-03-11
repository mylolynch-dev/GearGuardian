/**
 * @file safe_mode.c
 * @brief Safe mode — minimal services, fault display, and distress signal.
 *
 * Safe mode is entered when:
 *   - consecutive_boot_failures >= APP_BOOT_FAIL_THRESHOLD, OR
 *   - A fatal init fault occurs (e.g. IMU not found)
 *
 * In safe mode:
 *   - Only OLED and buzzer are used (minimal hardware dependency)
 *   - The OLED shows the fault reason
 *   - The buzzer plays the SOS pattern once
 *   - No arming, no alarm monitoring, no sensor sampling
 *   - The system stays in safe mode until power-cycled
 *
 * This is a terminal mode in V1.  Future versions could add a re-init
 * attempt or a "manual override" button sequence to exit safe mode.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "safe_mode.h"
#include "app_faults.h"
#include "ui_service.h"
#include "logger_service.h"
#include "buzzer.h"
#include "oled.h"

LOG_MODULE_REGISTER(safe_mode, LOG_LEVEL_WRN);

void safe_mode_enter(uint32_t fault_flags)
{
    LOG_ERR("=== ENTERING SAFE MODE === fault_flags=0x%08X", fault_flags);

    /* Display safe mode screen with fault code */
    oled_screen_safe(fault_flags);

    /* Log the entry */
    char msg[48];
    snprintf(msg, sizeof(msg), "SAFE MODE: faults=0x%08X", fault_flags);
    logger_service_post_str(msg);

    /* Sound distress signal — this blocks ~6 seconds but that's acceptable
     * for a terminal fault condition */
    buzzer_sos_pattern();

    LOG_ERR("System in SAFE MODE — power cycle to recover");

    /* Safe mode is terminal: the event_dispatcher thread continues running
     * but no new alarm or arming events will be processed because the
     * state_machine ignores non-NORMAL-mode events. */
}

void safe_mode_exit(void)
{
    /* Safe mode is terminal in V1; this should never be called. */
    LOG_WRN("safe_mode_exit() called — unexpected in V1");
}
