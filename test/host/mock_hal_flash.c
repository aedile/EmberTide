/**
 * mock_hal_flash.c — Host mock for hal_flash.
 *
 * Linked by test/host/ targets instead of components/hal/src/hal_flash.c.
 * Simulates LittleFS flash persistence entirely in RAM:
 *   - Holds one save slot: a static byte buffer + recorded size.
 *   - "No data" state (first-boot) represented by g_mock_flash_has_data == 0.
 *   - Data persists across hal_flash_deinit() + hal_flash_init() cycles,
 *     mirroring real flash behaviour (power-cycle does not erase data).
 *   - Reinit (double init) resets the mounted flag but NOT the data.
 *
 * Design note: the mock does NOT implement atomic-rename simulation.
 * That is a target implementation detail; the test contract only verifies
 * the read/write byte-level round-trip.
 */

#include "hal_flash.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal mock state.
 * -------------------------------------------------------------------------
 */
static uint8_t g_mock_flash_data[HAL_FLASH_SAVE_MAX_SIZE]; /* stored bytes   */
static size_t  g_mock_flash_data_size;                      /* bytes written  */
static uint8_t g_mock_flash_has_data;   /* 0 = no save file, 1 = data exists */
static uint8_t g_mock_flash_mounted;    /* 0 = unmounted, 1 = mounted        */

/* -------------------------------------------------------------------------
 * Public hal_flash API — mock implementations.
 * -------------------------------------------------------------------------
 */

hal_flash_err_t hal_flash_init(void)
{
    /* Re-mount is idempotent — does not erase persisted data. */
    g_mock_flash_mounted = 1u;
    return HAL_FLASH_OK;
}

hal_flash_err_t hal_flash_read_save(uint8_t *buf, size_t buf_size, size_t *bytes_read)
{
    /* NULL pointer guard. */
    if (!buf) {
        if (bytes_read) {
            *bytes_read = 0u;
        }
        return HAL_FLASH_ERR_NULL;
    }
    /* Size guard. */
    if (buf_size == 0u) {
        if (bytes_read) {
            *bytes_read = 0u;
        }
        return HAL_FLASH_ERR_SIZE;
    }
    /* First-boot: no data written yet. */
    if (!g_mock_flash_has_data) {
        if (bytes_read) {
            *bytes_read = 0u;
        }
        return HAL_FLASH_ERR_NOT_FOUND;
    }

    /* Copy min(stored_size, buf_size) bytes into caller's buffer. */
    size_t copy_len = g_mock_flash_data_size < buf_size
                      ? g_mock_flash_data_size
                      : buf_size;
    memcpy(buf, g_mock_flash_data, copy_len);
    if (bytes_read) {
        *bytes_read = copy_len;
    }
    return HAL_FLASH_OK;
}

hal_flash_err_t hal_flash_write_save(const uint8_t *buf, size_t size)
{
    /* NULL pointer guard. */
    if (!buf) {
        return HAL_FLASH_ERR_NULL;
    }
    /* Size guards. */
    if (size == 0u || size > HAL_FLASH_SAVE_MAX_SIZE) {
        return HAL_FLASH_ERR_SIZE;
    }

    memcpy(g_mock_flash_data, buf, size);
    g_mock_flash_data_size = size;
    g_mock_flash_has_data  = 1u;
    return HAL_FLASH_OK;
}

void hal_flash_deinit(void)
{
    g_mock_flash_mounted = 0u;
    /* Data is intentionally preserved — mirrors real flash persistence. */
}
