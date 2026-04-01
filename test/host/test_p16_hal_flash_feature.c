/**
 * test_p16_hal_flash_feature.c — Phase 16 feature tests: flash HAL.
 *
 * FEATURE tests (Rule 22 Phase B): Happy-path contracts for the flash HAL
 * mock.  These tests verify the full init→write→read→deinit lifecycle and
 * assert specific byte values to prove round-trip fidelity.
 *
 * Tests:
 *   F1 : init succeeds.
 *   F2 : read before any write returns ERR_NOT_FOUND (first-boot).
 *   F3 : write small payload returns OK.
 *   F4 : read after write returns OK and correct bytes_read.
 *   F5 : byte-level round-trip fidelity — specific bytes preserved.
 *   F6 : second write overwrites first (not append).
 *   F7 : data persists across deinit + reinit (flash persistence model).
 *   F8 : deinit is safe, reinit returns OK.
 *   F9 : write error injection — write fails but prior data is preserved.
 *   F10: mount error injection — init returns ERR_MOUNT (one-shot).
 *   F11: bytes_read is set to 0 when read returns ERR_NOT_FOUND.
 */

#include "hal_flash.h"
#include "mock_hal_flash.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int g_failures = 0;

#define ASSERT_EQ(label, expected, actual) do { \
    if ((int)(actual) != (int)(expected)) { \
        fprintf(stderr, "[FAIL] %s: expected %d, got %d\n", (label), (int)(expected), (int)(actual)); \
        g_failures++; \
    } \
} while (0)

int main(void)
{
    uint8_t write_buf[HAL_FLASH_SAVE_MAX_SIZE];
    uint8_t read_buf[HAL_FLASH_SAVE_MAX_SIZE];
    size_t  bytes_read;
    hal_flash_err_t err;
    uint32_t i;

    /* -----------------------------------------------------------------------
     * F1: init succeeds.
     * ----------------------------------------------------------------------- */
    mock_flash_reset();
    err = hal_flash_init();
    ASSERT_EQ("F1 init OK", HAL_FLASH_OK, err);

    /* -----------------------------------------------------------------------
     * F2: read before write — first boot returns ERR_NOT_FOUND.
     * ----------------------------------------------------------------------- */
    bytes_read = 99u;
    err = hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read);
    ASSERT_EQ("F2 not found on first boot", HAL_FLASH_ERR_NOT_FOUND, err);
    ASSERT_EQ("F11 bytes_read set 0 on NOT_FOUND", 0u, (uint32_t)bytes_read);

    /* -----------------------------------------------------------------------
     * F3: write small payload.
     * ----------------------------------------------------------------------- */
    write_buf[0] = 0xDE;
    write_buf[1] = 0xAD;
    write_buf[2] = 0xBE;
    write_buf[3] = 0xEF;
    err = hal_flash_write_save(write_buf, 4u);
    ASSERT_EQ("F3 write 4 bytes OK", HAL_FLASH_OK, err);

    /* -----------------------------------------------------------------------
     * F4 + F5: read returns correct count and bytes.
     * ----------------------------------------------------------------------- */
    memset(read_buf, 0, sizeof(read_buf));
    bytes_read = 0u;
    err = hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read);
    ASSERT_EQ("F4 read OK", HAL_FLASH_OK, err);
    ASSERT_EQ("F4 bytes_read == 4", 4u, (uint32_t)bytes_read);
    ASSERT_EQ("F5 byte[0] preserved", 0xDEu, read_buf[0]);
    ASSERT_EQ("F5 byte[1] preserved", 0xADu, read_buf[1]);
    ASSERT_EQ("F5 byte[2] preserved", 0xBEu, read_buf[2]);
    ASSERT_EQ("F5 byte[3] preserved", 0xEFu, read_buf[3]);

    /* -----------------------------------------------------------------------
     * F6: Second write overwrites first.
     * ----------------------------------------------------------------------- */
    write_buf[0] = 0x11;
    write_buf[1] = 0x22;
    hal_flash_write_save(write_buf, 2u);
    memset(read_buf, 0, sizeof(read_buf));
    bytes_read = 0u;
    hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read);
    ASSERT_EQ("F6 overwrite size 2",   2u,    (uint32_t)bytes_read);
    ASSERT_EQ("F6 overwrite byte[0]",  0x11u, read_buf[0]);
    ASSERT_EQ("F6 overwrite byte[1]",  0x22u, read_buf[1]);

    /* -----------------------------------------------------------------------
     * F7: Persistence across deinit + reinit.
     * ----------------------------------------------------------------------- */
    hal_flash_deinit();
    err = hal_flash_init();
    ASSERT_EQ("F7 reinit OK", HAL_FLASH_OK, err);
    memset(read_buf, 0, sizeof(read_buf));
    bytes_read = 0u;
    err = hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read);
    ASSERT_EQ("F7 data persists across reinit",  HAL_FLASH_OK, err);
    ASSERT_EQ("F7 bytes_read preserved",         2u, (uint32_t)bytes_read);
    ASSERT_EQ("F7 byte[0] persists",             0x11u, read_buf[0]);

    /* -----------------------------------------------------------------------
     * F8: deinit then reinit is idempotent.
     * ----------------------------------------------------------------------- */
    hal_flash_deinit();
    hal_flash_deinit();  /* double deinit safe */
    err = hal_flash_init();
    ASSERT_EQ("F8 reinit after double deinit OK", HAL_FLASH_OK, err);

    /* -----------------------------------------------------------------------
     * F9: Write error — prior data intact.
     * ----------------------------------------------------------------------- */
    mock_flash_reset();
    hal_flash_init();
    /* Write seed data. */
    for (i = 0u; i < 8u; i++) {
        write_buf[i] = (uint8_t)(0xA0u + i);
    }
    hal_flash_write_save(write_buf, 8u);
    /* Inject error on second write. */
    mock_flash_inject_write_error();
    memset(write_buf, 0xFF, sizeof(write_buf));
    err = hal_flash_write_save(write_buf, 8u);
    ASSERT_EQ("F9 injected write error", HAL_FLASH_ERR_WRITE, err);
    /* Confirm original data is still readable. */
    memset(read_buf, 0, sizeof(read_buf));
    bytes_read = 0u;
    hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read);
    ASSERT_EQ("F9 prior data bytes intact",   8u,    (uint32_t)bytes_read);
    ASSERT_EQ("F9 prior data byte[0] intact", 0xA0u, read_buf[0]);
    ASSERT_EQ("F9 prior data byte[7] intact", 0xA7u, read_buf[7]);

    /* -----------------------------------------------------------------------
     * F10: Mount error injection — one-shot.
     * ----------------------------------------------------------------------- */
    mock_flash_reset();
    mock_flash_inject_mount_error();
    err = hal_flash_init();
    ASSERT_EQ("F10 mount error injection", HAL_FLASH_ERR_MOUNT, err);
    /* Second init succeeds (one-shot). */
    err = hal_flash_init();
    ASSERT_EQ("F10 second init OK after one-shot", HAL_FLASH_OK, err);

    return g_failures;
}
