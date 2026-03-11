/**
 * @file sdlog.h
 * @brief microSD card logging driver — FATFS read/write abstraction.
 *
 * This module owns the SD card hardware and the FATFS filesystem handle.
 * Only logger_service.c should call these functions.  All other modules
 * post log entries to the logger_service message queue.
 *
 * The log file is CSV with the header row:
 *   timestamp_ms,mode,substate,event_type,fault_flags,summary
 *
 * Hardware note:
 *   The SD card SPI node is defined in the devicetree overlay.
 *   The FATFS mount point is defined by APP_LOG_FILENAME in app_config.h.
 *
 * TODO: verify CONFIG_DISK_DRIVER_SDMMC compatible string for ESP32-S3 SPI.
 */

#ifndef SDLOG_H
#define SDLOG_H

#include <stdint.h>
#include "app_modes.h"

/* ===========================================================================
 * Log record struct
 *
 * This is the message passed over the logger_service queue.
 * Keep it small enough for the k_msgq buffer (logger_service defines depth).
 * =========================================================================== */

/**
 * @brief One structured log record to be written to the SD card.
 */
typedef struct log_record {
    int64_t  timestamp_ms;    /**< k_uptime_get() at time of event          */
    uint8_t  mode;            /**< app_mode_t cast to uint8                  */
    uint8_t  substate;        /**< app_normal_substate_t cast to uint8       */
    uint8_t  event_type;      /**< app_event_type_t cast to uint8            */
    uint32_t fault_flags;     /**< Active app_fault_id_t bitmask             */
    char     summary[48];     /**< Human-readable description, null-terminated */
} log_record_t;

/* ===========================================================================
 * Lifecycle
 * =========================================================================== */

/**
 * @brief Mount the SD card FATFS and open (or create) the log file.
 *
 * If the log file already exists, new records are appended.
 * If the SD card is absent or mount fails, returns negative errno
 * (caller should post FAULT_SD_MOUNT).
 *
 * @return 0 on success, negative errno on failure.
 */
int sdlog_init(void);

/**
 * @brief Write one log record as a CSV row.
 *
 * Formats the record fields into a line and calls fs_write().
 * Does not flush unless sdlog_flush() is called explicitly.
 *
 * @param rec  Pointer to the record to write.
 * @return 0 on success, negative errno on fs_write failure.
 */
int sdlog_write(const log_record_t *rec);

/**
 * @brief Flush the FATFS write buffer to the SD card.
 *
 * Called periodically by logger_service to reduce data loss on
 * unexpected power loss.
 *
 * @return 0 on success, negative errno on failure.
 */
int sdlog_flush(void);

/**
 * @brief Close the log file and unmount the SD card gracefully.
 *
 * Called during controlled shutdown or before entering safe mode.
 */
void sdlog_close(void);

/**
 * @brief Write a single diagnostic test entry.
 *
 * Used in diagnostic mode to verify SD card write access.
 * Writes "DIAG_TEST,<timestamp_ms>" to the log.
 *
 * @return 0 on success, negative errno on failure.
 */
int sdlog_diag_test_write(void);

#endif /* SDLOG_H */
