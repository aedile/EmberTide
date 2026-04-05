/**
 * mock_hal_flash.h — Public interface for the hal_flash host mock.
 *
 * Test files #include this header to access mock state inspectors and
 * failure injection APIs. Only compiled in the host test environment —
 * never on the target.
 *
 * Phase 16 additions:
 *   mock_flash_inject_write_error()  — Next write_save returns ERR_WRITE (one-shot).
 *   mock_flash_inject_mount_error()  — Next hal_flash_init returns ERR_MOUNT (one-shot).
 *
 * Phase 22 additions:
 *   mock_flash_store_file()          — Store a named file for hal_flash_read_file() tests.
 */

#ifndef FIESTAQUEST_MOCK_HAL_FLASH_H
#define FIESTAQUEST_MOCK_HAL_FLASH_H

#include <stdint.h>
#include <stddef.h>

/** Reset all mock state to factory defaults (including named file store). */
void mock_flash_reset(void);

/**
 * mock_flash_inject_write_error — Arm a one-shot write error.
 *
 * The next call to hal_flash_write_save() (after NULL/size guards pass)
 * will return HAL_FLASH_ERR_WRITE. The flag clears after one use.
 * The data stored before this error is preserved (atomic write contract).
 */
void mock_flash_inject_write_error(void);

/**
 * mock_flash_inject_mount_error — Arm a one-shot mount error.
 *
 * The next call to hal_flash_init() will return HAL_FLASH_ERR_MOUNT.
 * The flag clears after one use.
 */
void mock_flash_inject_mount_error(void);

/**
 * mock_flash_store_file — Phase 22: Store a named file in the mock.
 *
 * Injects a file at the given LittleFS path so that hal_flash_read_file()
 * can retrieve it. Up to MOCK_FILE_STORE_MAX (8) files may be stored
 * simultaneously. If the path already exists, it is overwritten.
 *
 * @param path  File path (e.g. "/littlefs/music/track1.mod").
 * @param data  File data bytes.
 * @param size  Number of bytes (clamped to MOCK_FILE_DATA_MAX = 2048).
 */
void mock_flash_store_file(const char *path, const uint8_t *data, size_t size);

#endif /* FIESTAQUEST_MOCK_HAL_FLASH_H */
