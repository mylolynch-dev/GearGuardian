/**
 * @file normal_mode.c
 * @brief Normal operating mode — entry and exit.
 *
 * Entry: resets sub-state to DISARMED, enables sensor sampling, updates UI.
 * Exit:  stops timers, ensures alarm is silenced.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "normal_mode.h"
#include "state_machine.h"
#include "ui_service.h"
#include "logger_service.h"
#include "alarm_service.h"

LOG_MODULE_REGISTER(normal_mode, LOG_LEVEL_INF);

void normal_mode_enter(void)
{
    LOG_INF("Entering NORMAL mode");
    state_machine_set_substate(SUBSTATE_DISARMED);
    logger_service_post_str("NORMAL mode entered");
    ui_service_request_update();
}

void normal_mode_exit(void)
{
    LOG_INF("Exiting NORMAL mode");
    alarm_service_silence();
}
