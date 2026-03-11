/**
 * @file boot_metadata.c
 * @brief Persistent boot metadata — NVS implementation.
 *
 * Phase 1 / bring-up note:
 *   This file provides a RAM-only stub when NVS is not yet configured.
 *   The Phase 5 implementation below uses Zephyr NVS over the flash
 *   "storage" partition defined in boards/esp32s3_devkitc.overlay.
 *
 * To switch from stub to real NVS:
 *   1. Ensure prj.conf has CONFIG_NVS=y, CONFIG_FLASH=y, CONFIG_FLASH_MAP=y
 *   2. Ensure the overlay has a storage_partition node with correct offsets
 *   3. Uncomment the NVS section below and remove the stub section
 *
 * TODO (Phase 5): Replace stub with full NVS implementation.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "boot_metadata.h"

LOG_MODULE_REGISTER(boot_metadata, LOG_LEVEL_INF);

/* ===========================================================================
 * Phase 1 stub — RAM-only, no persistence across power cycles
 *
 * All data is lost on reset.  This allows the rest of the system to compile
 * and run without a configured NVS partition.
 *
 * TODO (Phase 5): Replace with NVS implementation below.
 * =========================================================================== */

static boot_meta_t s_meta_ram;
static bool        s_initialized = false;

int boot_meta_init(void)
{
    if (!s_initialized) {
        boot_meta_reset(&s_meta_ram);
        s_initialized = true;
    }

    LOG_INF("boot_meta_init: RAM stub (no NVS persistence)");
    /* TODO (Phase 5): initialize NVS filesystem here.
     *
     * #include <zephyr/fs/nvs.h>
     * #include <zephyr/storage/flash_map.h>
     *
     * static struct nvs_fs s_nvs;
     * const struct flash_area *fa;
     * int rc = flash_area_open(FIXED_PARTITION_ID(storage_partition), &fa);
     * if (rc != 0) { return rc; }
     * s_nvs.sector_size  = flash_area_get_size(fa) / 8;
     * s_nvs.sector_count = 8;
     * s_nvs.offset       = FIXED_PARTITION_OFFSET(storage_partition);
     * s_nvs.flash_device = FIXED_PARTITION_DEVICE(storage_partition);
     * return nvs_mount(&s_nvs);
     */
    return 0;
}

int boot_meta_load(boot_meta_t *out)
{
    if (!out) {
        return -EINVAL;
    }

    *out = s_meta_ram;

    if (out->magic != BOOT_META_MAGIC) {
        boot_meta_reset(out);
    }

    /* TODO (Phase 5):
     * ssize_t rc = nvs_read(&s_nvs, BOOT_META_NVS_KEY, out, sizeof(*out));
     * if (rc < 0 || out->magic != BOOT_META_MAGIC) {
     *     boot_meta_reset(out);
     *     return boot_meta_save(out);
     * }
     * return 0;
     */

    return 0;
}

int boot_meta_save(const boot_meta_t *meta)
{
    if (!meta) {
        return -EINVAL;
    }

    s_meta_ram = *meta;

    /* TODO (Phase 5):
     * ssize_t rc = nvs_write(&s_nvs, BOOT_META_NVS_KEY, meta, sizeof(*meta));
     * return (rc < 0) ? (int)rc : 0;
     */

    return 0;
}

void boot_meta_reset(boot_meta_t *meta)
{
    if (!meta) {
        return;
    }
    memset(meta, 0, sizeof(*meta));
    meta->magic = BOOT_META_MAGIC;
}

int boot_meta_record_clean_boot(void)
{
    boot_meta_t meta;
    int rc = boot_meta_load(&meta);
    if (rc != 0) {
        return rc;
    }

    meta.consecutive_failures = 0;
    meta.last_fault_flags     = 0;

    LOG_INF("Clean boot recorded (count=%u)", meta.boot_count);
    return boot_meta_save(&meta);
}

int boot_meta_record_failed_boot(uint32_t faults)
{
    boot_meta_t meta;
    int rc = boot_meta_load(&meta);
    if (rc != 0) {
        return rc;
    }

    meta.consecutive_failures++;
    meta.last_fault_flags |= faults;

    LOG_WRN("Failed boot recorded (consecutive=%u, faults=0x%08X)",
            meta.consecutive_failures, meta.last_fault_flags);

    return boot_meta_save(&meta);
}
