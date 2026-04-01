/**
 * hal_flash.c — LittleFS Flash HAL: Target Stub
 *
 * This file compiles ONLY with idf.py build (target = ESP32-S3).
 * It requires ESP-IDF esp_vfs_littlefs.h which is NOT available on
 * the host.  The host test suite links mock_hal_flash.c from test/host/
 * instead of this file.
 *
 * Phase 12 status: STUB — returns OK/NOT_FOUND for all operations.
 * Real implementation deferred until LittleFS partition bring-up.
 *
 * Real implementation plan:
 *   hal_flash_init  : esp_vfs_littlefs_register() with format_if_mount_failed=true,
 *                     partition_label="littlefs".  On ESP_FAIL returns ERR_MOUNT.
 *   hal_flash_read_save : fopen("/storage/save.dat","rb"), fread, fclose.
 *                          Returns ERR_NOT_FOUND if errno==ENOENT.
 *   hal_flash_write_save: fopen("/storage/save.tmp","wb"), fwrite, fclose,
 *                          rename("/storage/save.tmp","/storage/save.dat").
 *                          Atomic rename prevents half-written corruption.
 *   hal_flash_deinit: esp_vfs_littlefs_unregister("littlefs").
 */

#include "hal_flash.h"

/* -------------------------------------------------------------------------
 * Phase 12 target stubs.
 * -------------------------------------------------------------------------
 */

hal_flash_err_t hal_flash_init(void)
{
    return HAL_FLASH_OK;
}

hal_flash_err_t hal_flash_read_save(uint8_t *buf, size_t buf_size, size_t *bytes_read)
{
    if (!buf) {
        return HAL_FLASH_ERR_NULL;
    }
    if (buf_size == 0u) {
        return HAL_FLASH_ERR_SIZE;
    }
    if (bytes_read) {
        *bytes_read = 0u;
    }
    /* Stub: no filesystem — always first-boot. */
    return HAL_FLASH_ERR_NOT_FOUND;
}

hal_flash_err_t hal_flash_write_save(const uint8_t *buf, size_t size)
{
    if (!buf) {
        return HAL_FLASH_ERR_NULL;
    }
    if (size == 0u || size > HAL_FLASH_SAVE_MAX_SIZE) {
        return HAL_FLASH_ERR_SIZE;
    }
    return HAL_FLASH_OK;
}

void hal_flash_deinit(void)
{
    /* No resources to release in stub. */
}
