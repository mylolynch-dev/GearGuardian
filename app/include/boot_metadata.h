/**
 * @file boot_metadata.h
 * @brief Persistent boot metadata: boot count, failure tracking, and fault history.
 *
 * The boot_meta struct is stored in NVS (non-volatile storage) across power
 * cycles.  It lets the system detect boot loops, audit fault history, and
 * steer into SAFE MODE when consecutive failures exceed the threshold.
 *
 * Phase 1 / bring-up note:
 *   boot_metadata.c provides a RAM-only stub implementation that satisfies
 *   the full API without requiring NVS to be configured.  Replace the stub
 *   with real NVS calls in Phase 5 once the flash partition map is confirmed.
 *
 * Usage:
 *   1. Call boot_meta_init() early in startup_run(), before any other
 *      boot_meta_* calls.
 *   2. Call boot_meta_load() to populate your local struct.
 *   3. On clean boot completion, call boot_meta_record_clean_boot().
 *   4. On fault/reset before completion, call boot_meta_record_failed_boot().
 */

#ifndef BOOT_METADATA_H
#define BOOT_METADATA_H

#include <stdint.h>
#include <stdbool.h>
#include "app_faults.h"

/* ---------------------------------------------------------------------------
 * Magic value
 *
 * Written into every saved boot_meta struct.  On load, if magic does not
 * match, the struct is treated as uninitialized and reset to defaults.
 * --------------------------------------------------------------------------- */

/** Spells "gear" in ASCII bytes, packed into a uint32. */
#define BOOT_META_MAGIC  0x67656172U

/* ---------------------------------------------------------------------------
 * NVS key IDs
 * --------------------------------------------------------------------------- */

#define BOOT_META_NVS_KEY  1U  /**< NVS ID for the boot_meta struct */

/* ---------------------------------------------------------------------------
 * Boot metadata struct
 * --------------------------------------------------------------------------- */

/**
 * @brief Persistent boot metadata stored in NVS.
 *
 * Keep this struct small (< 32 bytes) — NVS writes are wear-leveled but
 * still have a finite endurance, and we write on every boot.
 */
typedef struct boot_meta {
    uint32_t magic;                /**< Must equal BOOT_META_MAGIC          */
    uint32_t boot_count;           /**< Total lifetime boot count           */
    uint32_t consecutive_failures; /**< Consecutive boots that did not reach
                                        boot_meta_record_clean_boot()       */
    uint32_t last_fault_flags;     /**< Bitmask of app_fault_id_t from last
                                        failed boot (0 if last was clean)   */
} boot_meta_t;

/* ---------------------------------------------------------------------------
 * Lifecycle API
 * --------------------------------------------------------------------------- */

/**
 * @brief Initialize the NVS filesystem used by boot metadata.
 *
 * Must be called before any other boot_meta_* function.
 * In the Phase 1 stub this is a no-op.
 *
 * @return 0 on success, negative errno on NVS init failure.
 *         Failure here should be treated as FAULT_NVS_INIT.
 */
int boot_meta_init(void);

/**
 * @brief Load boot metadata from NVS into the provided struct.
 *
 * If no valid data exists in NVS (first boot or corrupt), the struct is
 * zero-initialized and saved so subsequent loads succeed.
 *
 * @param[out] out  Destination struct; must not be NULL.
 * @return 0 on success, negative errno on NVS read failure.
 */
int boot_meta_load(boot_meta_t *out);

/**
 * @brief Save boot metadata to NVS.
 *
 * @param[in] meta  Source struct; must not be NULL.
 * @return 0 on success, negative errno on NVS write failure.
 */
int boot_meta_save(const boot_meta_t *meta);

/**
 * @brief Zero-initialize a boot_meta struct and set the magic value.
 *
 * Convenience for first-boot initialization.
 *
 * @param[out] meta  Struct to reset.
 */
void boot_meta_reset(boot_meta_t *meta);

/* ---------------------------------------------------------------------------
 * Convenience update calls (load → modify → save internally)
 * --------------------------------------------------------------------------- */

/**
 * @brief Record a successful boot completion.
 *
 * Increments boot_count, resets consecutive_failures to 0, clears
 * last_fault_flags, and saves to NVS.
 *
 * Call this from startup_run() after all drivers are initialized
 * successfully and the system enters NORMAL or DIAGNOSTIC mode.
 *
 * @return 0 on success, negative errno on NVS save failure.
 */
int boot_meta_record_clean_boot(void);

/**
 * @brief Record a failed boot attempt.
 *
 * Increments consecutive_failures, OR-combines fault_flags into
 * last_fault_flags, and saves to NVS.
 *
 * Call this from startup_run() before returning if a fatal driver
 * init fails or if the watchdog-reset cause is detected.
 *
 * @param faults  Bitmask of app_fault_id_t values to record.
 * @return 0 on success, negative errno on NVS save failure.
 */
int boot_meta_record_failed_boot(uint32_t faults);

#endif /* BOOT_METADATA_H */
