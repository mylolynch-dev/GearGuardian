/**
 * @file sdlog.c
 * @brief microSD card logging — FATFS over SPI.
 *
 * Uses Zephyr's file system API (zephyr/fs/fs.h) with the ELM FAT driver.
 * The FATFS volume is mounted at "/SD:" by convention.
 *
 * Log file format: CSV with header row
 *   timestamp_ms,mode,substate,event_type,fault_flags,summary
 *
 * This module is called ONLY from logger_service.c (the dedicated logger
 * thread).  It is not thread-safe.
 *
 * Disabled at compile time when CONFIG_FILE_SYSTEM is not set (i.e. before
 * the SD card hardware is connected).  All public functions return -ENOTSUP
 * in that case so the rest of the application compiles and runs normally.
 *
 * TODO (Phase 2 bring-up):
 *   Re-enable CONFIG_FILE_SYSTEM, CONFIG_FAT_FILESYSTEM_ELM, and
 *   CONFIG_DISK_DRIVER_SDMMC in prj.conf once the SD card is wired.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>

#include "sdlog.h"
#include "app_config.h"

LOG_MODULE_REGISTER(sdlog, LOG_LEVEL_INF);

#ifdef CONFIG_FILE_SYSTEM

#include <zephyr/fs/fs.h>
#include <ff.h>   /* ELM FatFs types for static FATFS struct */

/* ---------------------------------------------------------------------------
 * FATFS mount
 * --------------------------------------------------------------------------- */
static FATFS s_fat_fs;

static struct fs_mount_t s_mount = {
    .type       = FS_FATFS,
    .fs_data    = &s_fat_fs,
    .mnt_point  = "/SD:",
};

static bool s_mounted   = false;
static bool s_file_open = false;

/* CSV header written on first open */
#define CSV_HEADER "timestamp_ms,mode,substate,event_type,fault_flags,summary\n"

static struct fs_file_t s_log_file;

/* ===========================================================================
 * Lifecycle
 * =========================================================================== */

int sdlog_init(void)
{
    int rc = fs_mount(&s_mount);
    if (rc != 0) {
        LOG_ERR("fs_mount failed: %d (SD present? FAT32 formatted?)", rc);
        return rc;
    }
    s_mounted = true;
    LOG_INF("SD card mounted at %s", s_mount.mnt_point);

    fs_file_t_init(&s_log_file);
    rc = fs_open(&s_log_file, APP_LOG_FILENAME,
                 FS_O_WRITE | FS_O_CREATE | FS_O_APPEND);
    if (rc != 0) {
        LOG_ERR("fs_open failed for %s: %d", APP_LOG_FILENAME, rc);
        return rc;
    }
    s_file_open = true;

    struct fs_dirent dirent;
    if (fs_stat(APP_LOG_FILENAME, &dirent) == 0 && dirent.size == 0) {
        fs_write(&s_log_file, CSV_HEADER, strlen(CSV_HEADER));
    }

    LOG_INF("Log file open: %s", APP_LOG_FILENAME);
    return 0;
}

int sdlog_write(const log_record_t *rec)
{
    if (!s_file_open || !rec) {
        return -ENODEV;
    }

    char line[128];
    int len = snprintf(line, sizeof(line),
                       "%lld,%u,%u,%u,0x%08X,%s\n",
                       (long long)rec->timestamp_ms,
                       (unsigned)rec->mode,
                       (unsigned)rec->substate,
                       (unsigned)rec->event_type,
                       rec->fault_flags,
                       rec->summary);

    if (len <= 0 || len >= (int)sizeof(line)) {
        len = sizeof(line) - 1;
        line[len] = '\0';
    }

    ssize_t written = fs_write(&s_log_file, line, (size_t)len);
    if (written < 0) {
        LOG_ERR("fs_write failed: %d", (int)written);
        return (int)written;
    }
    return 0;
}

int sdlog_flush(void)
{
    if (!s_file_open) {
        return -ENODEV;
    }
    int rc = fs_sync(&s_log_file);
    if (rc != 0) {
        LOG_ERR("fs_sync failed: %d", rc);
    }
    return rc;
}

void sdlog_close(void)
{
    if (s_file_open) {
        fs_sync(&s_log_file);
        fs_close(&s_log_file);
        s_file_open = false;
    }
    if (s_mounted) {
        fs_unmount(&s_mount);
        s_mounted = false;
    }
    LOG_INF("SD log closed");
}

int sdlog_diag_test_write(void)
{
    if (!s_file_open) {
        return -ENODEV;
    }
    char line[64];
    snprintf(line, sizeof(line), "DIAG_TEST,%lld\n", (long long)k_uptime_get());
    ssize_t written = fs_write(&s_log_file, line, strlen(line));
    if (written < 0) {
        return (int)written;
    }
    return fs_sync(&s_log_file);
}

#else /* CONFIG_FILE_SYSTEM not set */

int  sdlog_init(void)               { LOG_INF("SD logging disabled (no filesystem)"); return -ENOTSUP; }
int  sdlog_write(const log_record_t *rec) { ARG_UNUSED(rec); return -ENOTSUP; }
int  sdlog_flush(void)              { return -ENOTSUP; }
void sdlog_close(void)              { }
int  sdlog_diag_test_write(void)    { return -ENOTSUP; }

#endif /* CONFIG_FILE_SYSTEM */
