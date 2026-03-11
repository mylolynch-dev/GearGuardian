/**
 * @file logger_service.h
 * @brief SD card logging service — public interface for posting log records.
 *
 * Only logger_service.c (the logger thread) calls sdlog_write().
 * All other code posts records via logger_service_post() or
 * logger_service_post_str().
 */

#ifndef LOGGER_SERVICE_H
#define LOGGER_SERVICE_H

#include <stdint.h>
#include "sdlog.h"

/**
 * @brief Enqueue a full log_record_t for the logger thread to write.
 *
 * Non-blocking; drops the record if the queue is full.
 *
 * @param rec  Record to enqueue (copied by value).
 * @return 0 on success, -ENOMSG if queue is full.
 */
int logger_service_post(const log_record_t *rec);

/**
 * @brief Convenience: enqueue a log record from a plain text string.
 *
 * Timestamps, mode, and substate are filled in automatically from
 * current app state.
 *
 * @param summary  Short human-readable description (max 47 chars + null).
 */
void logger_service_post_str(const char *summary);

#endif /* LOGGER_SERVICE_H */
