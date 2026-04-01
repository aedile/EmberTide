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
 */

#ifndef FIESTAQUEST_MOCK_HAL_FLASH_H
#define FIESTAQUEST_MOCK_HAL_FLASH_H

#include <stdint.h>

/** Reset all mock state to factory defaults. */
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

#endif /* FIESTAQUEST_MOCK_HAL_FLASH_H */
