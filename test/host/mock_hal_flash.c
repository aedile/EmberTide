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
 * Phase 16 additions:
 *   - mock_flash_inject_write_error(): arm a one-shot HAL_FLASH_ERR_WRITE.
 *     Data stored before the failed write is NOT corrupted (atomic write).
 *   - mock_flash_inject_mount_error(): arm a one-shot HAL_FLASH_ERR_MOUNT.
 *
 * Phase 22 additions:
 *   - hal_flash_read_file(): read an arbitrary named file from the mock.
 *   - mock_flash_store_file(): inject a named file into the mock for tests.
 *   - Up to MOCK_FILE_STORE_MAX named files stored simultaneously.
 *
 * Design note: the mock does NOT implement atomic-rename simulation.
 * That is a target implementation detail; the test contract only verifies
 * the read/write byte-level round-trip and error-path data integrity.
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

/* One-shot failure injection flags (Phase 16). */
static uint8_t g_mock_inject_write_error; /* 1 = fire ERR_WRITE next write */
static uint8_t g_mock_inject_mount_error; /* 1 = fire ERR_MOUNT next init  */

/* -------------------------------------------------------------------------
 * Phase 22: Named file store (for hal_flash_read_file mock).
 * -------------------------------------------------------------------------
 */
#define MOCK_FILE_STORE_MAX      8u    /**< Max simultaneous files stored. */
#define MOCK_FILE_PATH_MAX       128u  /**< Max path length (bytes, incl NUL). */
#define MOCK_FILE_DATA_MAX       2048u /**< Max bytes per stored file. */

typedef struct {
    char    path[MOCK_FILE_PATH_MAX];
    uint8_t data[MOCK_FILE_DATA_MAX];
    size_t  size;
    uint8_t valid;
} mock_file_entry_t;

static mock_file_entry_t g_mock_files[MOCK_FILE_STORE_MAX];

/* -------------------------------------------------------------------------
 * Public hal_flash API — mock implementations.
 * -------------------------------------------------------------------------
 */

hal_flash_err_t hal_flash_init(void)
{
    /* Phase 16: one-shot mount failure injection. */
    if (g_mock_inject_mount_error) {
        g_mock_inject_mount_error = 0u;
        return HAL_FLASH_ERR_MOUNT;
    }
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

    /* Phase 16: one-shot write failure injection.
     * Fires AFTER all guards pass but BEFORE writing — data is preserved.
     * This models the real atomic temp-file pattern: if the rename fails, the
     * original save.dat is untouched. */
    if (g_mock_inject_write_error) {
        g_mock_inject_write_error = 0u;
        return HAL_FLASH_ERR_WRITE;
    }

    memcpy(g_mock_flash_data, buf, size);
    g_mock_flash_data_size = size;
    g_mock_flash_has_data  = 1u;
    return HAL_FLASH_OK;
}

/**
 * hal_flash_read_file — Phase 22: Read a named file from the mock store.
 *
 * Looks up the path in g_mock_files. Returns ERR_NOT_FOUND if no entry
 * with that path was stored via mock_flash_store_file().
 */
hal_flash_err_t hal_flash_read_file(const char *path, uint8_t *buf,
                                     size_t buf_size, size_t *bytes_read)
{
    if (bytes_read) { *bytes_read = 0u; }

    if (path == NULL || buf == NULL) {
        return HAL_FLASH_ERR_NULL;
    }
    if (buf_size == 0u) {
        return HAL_FLASH_ERR_SIZE;
    }

    /* Search for matching path. */
    for (uint8_t i = 0u; i < MOCK_FILE_STORE_MAX; i++) {
        if (!g_mock_files[i].valid) {
            continue;
        }
        if (strncmp(g_mock_files[i].path, path, MOCK_FILE_PATH_MAX) == 0) {
            /* Found — copy data into caller's buffer. */
            size_t copy_len = g_mock_files[i].size < buf_size
                              ? g_mock_files[i].size
                              : buf_size;
            memcpy(buf, g_mock_files[i].data, copy_len);
            if (bytes_read) {
                *bytes_read = copy_len;
            }
            return HAL_FLASH_OK;
        }
    }

    /* Path not found. */
    return HAL_FLASH_ERR_NOT_FOUND;
}

void hal_flash_deinit(void)
{
    g_mock_flash_mounted = 0u;
    /* Data is intentionally preserved — mirrors real flash persistence. */
}

/* -------------------------------------------------------------------------
 * Test accessor functions — host-only, not declared in hal_flash.h.
 * -------------------------------------------------------------------------
 */

/**
 * mock_flash_reset — Reset all mock state to factory defaults.
 *
 * Clears the stored data buffer, resets data size and has_data flag,
 * marks the mock as unmounted, clears all injection flags, and clears
 * the named file store.
 * Call at the start of each test main() to ensure a clean slate.
 */
void mock_flash_reset(void)
{
    memset(g_mock_flash_data, 0, sizeof(g_mock_flash_data));
    g_mock_flash_data_size    = 0u;
    g_mock_flash_has_data     = 0u;
    g_mock_flash_mounted      = 0u;
    g_mock_inject_write_error = 0u;
    g_mock_inject_mount_error = 0u;
    memset(g_mock_files, 0, sizeof(g_mock_files));
}

/**
 * mock_flash_inject_write_error — Arm a one-shot write error.
 *
 * The next hal_flash_write_save() that passes all NULL/size guards will
 * return HAL_FLASH_ERR_WRITE. The flag is cleared after one firing.
 * Previously stored data is NOT modified (atomic write contract preserved).
 */
void mock_flash_inject_write_error(void)
{
    g_mock_inject_write_error = 1u;
}

/**
 * mock_flash_inject_mount_error — Arm a one-shot mount error.
 *
 * The next hal_flash_init() will return HAL_FLASH_ERR_MOUNT.
 * The flag is cleared after one firing.
 */
void mock_flash_inject_mount_error(void)
{
    g_mock_inject_mount_error = 1u;
}

/**
 * mock_flash_store_file — Phase 22: Store a named file in the mock.
 *
 * Places up to MOCK_FILE_DATA_MAX bytes from data into a named slot.
 * If the path already exists, it is overwritten. If all MOCK_FILE_STORE_MAX
 * slots are full and the path is not already present, the store silently
 * drops the entry (test design should not exceed the limit).
 *
 * @param path  File path string (e.g. "/littlefs/music/track1.mod"). Truncated
 *              to MOCK_FILE_PATH_MAX-1 characters.
 * @param data  File data bytes. Must not be NULL.
 * @param size  Number of bytes. Clamped to MOCK_FILE_DATA_MAX.
 */
void mock_flash_store_file(const char *path, const uint8_t *data, size_t size)
{
    if (!path || !data) { return; }

    /* Look for existing entry or free slot. */
    int8_t free_slot = -1;
    for (uint8_t i = 0u; i < MOCK_FILE_STORE_MAX; i++) {
        if (g_mock_files[i].valid &&
            strncmp(g_mock_files[i].path, path, MOCK_FILE_PATH_MAX) == 0) {
            /* Overwrite existing. */
            free_slot = (int8_t)i;
            break;
        }
        if (!g_mock_files[i].valid && free_slot < 0) {
            free_slot = (int8_t)i;
        }
    }

    if (free_slot < 0) { return; } /* No space — silently drop. */

    mock_file_entry_t *e = &g_mock_files[(uint8_t)free_slot];
    memset(e, 0, sizeof(*e));

    strncpy(e->path, path, MOCK_FILE_PATH_MAX - 1u);
    e->path[MOCK_FILE_PATH_MAX - 1u] = '\0';

    if (size > MOCK_FILE_DATA_MAX) { size = MOCK_FILE_DATA_MAX; }
    memcpy(e->data, data, size);
    e->size  = size;
    e->valid = 1u;
}
