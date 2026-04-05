/**
 * hal_flash.h — Hardware Abstraction Layer: LittleFS Flash Storage
 *
 * Target: ESP32-S3-PICO-1-N8R8 internal flash via ESP-IDF v5.x.
 * Uses esp_vfs_littlefs to mount the "littlefs" partition defined
 * in partitions.csv and exposes a simple binary-blob persistence API.
 *
 * Architecture constraint: This header is the BOTTOM layer.
 * It MUST NOT be included by game/, presentation/, or connectivity/.
 * Upper layers access persistence exclusively through the platform/
 * storage.h service which calls hal_flash internally.
 *
 * No ESP-IDF types appear in this public API, keeping the header
 * host-compilable so that mock implementations can be linked in test/host/.
 *
 * Write atomicity: The real target implementation writes to a temporary
 * file ("save.tmp") then renames to "save.dat" to prevent half-written
 * state after a power loss mid-write.
 *
 * Phase 22: Added hal_flash_read_file() for loading .mod music files
 * from the LittleFS partition into caller-provided buffers (SPIRAM).
 */

#ifndef FIESTAQUEST_HAL_FLASH_H
#define FIESTAQUEST_HAL_FLASH_H

#include <stdint.h>
#include <stddef.h>

/** Maximum save payload size in bytes. */
#define HAL_FLASH_SAVE_MAX_SIZE  512u

/**
 * hal_flash_err_t — Return codes for all hal_flash operations.
 *
 * HAL_FLASH_OK            — Operation completed successfully.
 * HAL_FLASH_ERR_MOUNT     — LittleFS partition failed to mount/format.
 * HAL_FLASH_ERR_NOT_FOUND — Save file does not exist (first boot).
 * HAL_FLASH_ERR_WRITE     — fwrite / rename failed.
 * HAL_FLASH_ERR_READ      — fread failed after file was found.
 * HAL_FLASH_ERR_NULL      — Caller passed a NULL pointer.
 * HAL_FLASH_ERR_SIZE      — size is 0 or exceeds HAL_FLASH_SAVE_MAX_SIZE.
 */
typedef enum {
    HAL_FLASH_OK            = 0,
    HAL_FLASH_ERR_MOUNT     = 1,
    HAL_FLASH_ERR_NOT_FOUND = 2,
    HAL_FLASH_ERR_WRITE     = 3,
    HAL_FLASH_ERR_READ      = 4,
    HAL_FLASH_ERR_NULL      = 5,
    HAL_FLASH_ERR_SIZE      = 6
} hal_flash_err_t;

/**
 * hal_flash_init — Mount the LittleFS partition and prepare the storage API.
 *
 * On a factory-fresh chip the partition is unformatted; this function
 * instructs LittleFS to format it automatically (format_if_mount_failed=true).
 * After formatting, no save file exists and hal_flash_read_save will return
 * HAL_FLASH_ERR_NOT_FOUND — the caller must treat this as a first-boot state.
 *
 * Safe to call multiple times (idempotent reinit).
 *
 * @return HAL_FLASH_OK on success, HAL_FLASH_ERR_MOUNT on persistent failure.
 */
hal_flash_err_t hal_flash_init(void);

/**
 * hal_flash_read_save — Read the binary save payload from flash.
 *
 * @param buf        Output buffer. Must be at least buf_size bytes.
 * @param buf_size   Size of buf. Must be > 0.
 * @param bytes_read If non-NULL, set to the number of bytes actually read.
 *                   Set to 0 on any error.
 * @return HAL_FLASH_OK            on success.
 *         HAL_FLASH_ERR_NULL      if buf is NULL.
 *         HAL_FLASH_ERR_SIZE      if buf_size is 0.
 *         HAL_FLASH_ERR_NOT_FOUND if no save file exists (first boot).
 *         HAL_FLASH_ERR_READ      on I/O failure.
 */
hal_flash_err_t hal_flash_read_save(uint8_t *buf, size_t buf_size, size_t *bytes_read);

/**
 * hal_flash_write_save — Write the binary save payload to flash.
 *
 * Atomically writes via a temp file + rename pattern to guard against
 * power-loss corruption during the ~50 ms write window.
 *
 * @param buf   Source buffer. Must not be NULL.
 * @param size  Number of bytes to write. Must be in [1, HAL_FLASH_SAVE_MAX_SIZE].
 * @return HAL_FLASH_OK        on success.
 *         HAL_FLASH_ERR_NULL  if buf is NULL.
 *         HAL_FLASH_ERR_SIZE  if size is 0 or > HAL_FLASH_SAVE_MAX_SIZE.
 *         HAL_FLASH_ERR_WRITE on I/O failure.
 */
hal_flash_err_t hal_flash_write_save(const uint8_t *buf, size_t size);

/**
 * hal_flash_read_file — Read an arbitrary file from the LittleFS partition.
 *
 * Phase 22: Supports loading .mod music files (and other assets) from
 * LittleFS into caller-provided buffers (e.g. SPIRAM on the target).
 *
 * @param path       Absolute LittleFS path (e.g. "/littlefs/music/track1.mod").
 *                   Must not be NULL.
 * @param buf        Output buffer. Must not be NULL.
 * @param buf_size   Size of buf in bytes. Must be > 0.
 * @param bytes_read If non-NULL, set to the number of bytes actually read.
 *                   Set to 0 on any error.
 * @return HAL_FLASH_OK            on success.
 *         HAL_FLASH_ERR_NULL      if path or buf is NULL.
 *         HAL_FLASH_ERR_SIZE      if buf_size is 0.
 *         HAL_FLASH_ERR_NOT_FOUND if the file does not exist.
 *         HAL_FLASH_ERR_READ      on I/O failure.
 */
hal_flash_err_t hal_flash_read_file(const char *path, uint8_t *buf,
                                     size_t buf_size, size_t *bytes_read);

/**
 * hal_flash_deinit — Unmount the LittleFS partition and release resources.
 *
 * Safe to call without a preceding init (no-op in that case).
 */
void hal_flash_deinit(void);

#endif /* FIESTAQUEST_HAL_FLASH_H */
