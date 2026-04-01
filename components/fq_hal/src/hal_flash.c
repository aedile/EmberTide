/**
 * hal_flash.c — LittleFS Flash HAL: Waveshare ESP32-S3-ePaper-1.54 V2
 *
 * Target: ESP32-S3 internal flash via ESP-IDF v5.x + joltwallet/littlefs
 * component (fetched via idf_component.yml; header: esp_littlefs.h).
 * Compiled ONLY with idf.py build — NOT in host tests.
 * The host test suite links mock_hal_flash.c instead of this file.
 *
 * Partition label: "storage" (defined in partitions.csv).
 * Mount point: "/littlefs".
 *
 * Atomic write pattern:
 *   1. Write payload to "/littlefs/save.tmp".
 *   2. rename() tmp → "/littlefs/save.dat".
 *   If power is lost between steps, the old "save.dat" (if it exists) is
 *   intact. If lost during step 1, the orphaned "save.tmp" is silently
 *   discarded on the next read (read only opens "save.dat").
 *
 * Architecture constraint: This file is the BOTTOM layer.
 * It MUST NOT be included by game/, presentation/, or connectivity/.
 */

#include "hal_flash.h"

/* joltwallet/littlefs — provides esp_vfs_littlefs_conf_t and
 * esp_vfs_littlefs_register/unregister (same API as old ESP-IDF built-in,
 * but now lives in the component registry as joltwallet/littlefs). */
#include "esp_littlefs.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

static const char *TAG = "hal_flash";

/* -------------------------------------------------------------------------
 * Mount configuration.
 * -------------------------------------------------------------------------
 */
#define STORAGE_PARTITION_LABEL  "storage"
#define STORAGE_MOUNT_POINT      "/littlefs"
#define SAVE_FILE_PATH           STORAGE_MOUNT_POINT "/save.dat"
#define SAVE_TMP_PATH            STORAGE_MOUNT_POINT "/save.tmp"

/* -------------------------------------------------------------------------
 * Module state.
 * -------------------------------------------------------------------------
 */
static uint8_t s_mounted;  /* 0 = unmounted, 1 = mounted */

/* -------------------------------------------------------------------------
 * Public API.
 * -------------------------------------------------------------------------
 */

hal_flash_err_t hal_flash_init(void)
{
    if (s_mounted) {
        /* Idempotent reinit — already mounted. */
        return HAL_FLASH_OK;
    }

    esp_vfs_littlefs_conf_t conf = {
        .base_path              = STORAGE_MOUNT_POINT,
        .partition_label        = STORAGE_PARTITION_LABEL,
        .format_if_mount_failed = true,
        .dont_mount             = false
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS mount failed: %d", ret);
        return HAL_FLASH_ERR_MOUNT;
    }

    s_mounted = 1u;
    ESP_LOGI(TAG, "LittleFS mounted on %s", STORAGE_MOUNT_POINT);
    return HAL_FLASH_OK;
}

hal_flash_err_t hal_flash_read_save(uint8_t *buf, size_t buf_size,
                                     size_t *bytes_read)
{
    if (!buf) {
        if (bytes_read) { *bytes_read = 0u; }
        return HAL_FLASH_ERR_NULL;
    }
    if (buf_size == 0u) {
        if (bytes_read) { *bytes_read = 0u; }
        return HAL_FLASH_ERR_SIZE;
    }
    if (bytes_read) {
        *bytes_read = 0u;
    }

    FILE *f = fopen(SAVE_FILE_PATH, "rb");
    if (!f) {
        if (errno == ENOENT) {
            return HAL_FLASH_ERR_NOT_FOUND;
        }
        ESP_LOGE(TAG, "fopen read failed: errno=%d", errno);
        return HAL_FLASH_ERR_READ;
    }

    size_t n = fread(buf, 1u, buf_size, f);
    fclose(f);

    if (n == 0u) {
        ESP_LOGE(TAG, "fread returned 0");
        return HAL_FLASH_ERR_READ;
    }

    if (bytes_read) {
        *bytes_read = n;
    }
    return HAL_FLASH_OK;
}

hal_flash_err_t hal_flash_write_save(const uint8_t *buf, size_t size)
{
    if (!buf) {
        return HAL_FLASH_ERR_NULL;
    }
    if (size == 0u || size > HAL_FLASH_SAVE_MAX_SIZE) {
        return HAL_FLASH_ERR_SIZE;
    }

    /* Write to temp file first (atomic write pattern). */
    FILE *f = fopen(SAVE_TMP_PATH, "wb");
    if (!f) {
        ESP_LOGE(TAG, "fopen tmp failed: errno=%d", errno);
        return HAL_FLASH_ERR_WRITE;
    }

    size_t written = fwrite(buf, 1u, size, f);
    fclose(f);

    if (written != size) {
        ESP_LOGE(TAG, "fwrite incomplete: wrote %u of %u", (unsigned)written,
                 (unsigned)size);
        return HAL_FLASH_ERR_WRITE;
    }

    /* Atomic rename — if this fails, old save.dat is intact. */
    int rc = rename(SAVE_TMP_PATH, SAVE_FILE_PATH);
    if (rc != 0) {
        ESP_LOGE(TAG, "rename failed: errno=%d", errno);
        return HAL_FLASH_ERR_WRITE;
    }

    return HAL_FLASH_OK;
}

void hal_flash_deinit(void)
{
    if (!s_mounted) {
        return;
    }
    esp_vfs_littlefs_unregister(STORAGE_PARTITION_LABEL);
    s_mounted = 0u;
}
