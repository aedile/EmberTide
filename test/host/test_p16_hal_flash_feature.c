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
#include <string.h>

static int g_failures = 0;

#define ASSERT_EQ(actual, expected, label)               \
    do {                                                 \
        if ((int32_t)(actual) != (int32_t)(expected)) {  \
            g_failures++;                                \
        }                                                \
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
    ASSERT_EQ(err, HAL_FLASH_OK, "F1 init OK");

    /* -----------------------------------------------------------------------
     * F2: read before write — first boot returns ERR_NOT_FOUND.
     * ----------------------------------------------------------------------- */
    bytes_read = 99u;
    err = hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read);
    ASSERT_EQ(err, HAL_FLASH_ERR_NOT_FOUND, "F2 not found on first boot");
    ASSERT_EQ(bytes_read, 0u, "F11 bytes_read set 0 on NOT_FOUND");

    /* -----------------------------------------------------------------------
     * F3: write small payload.
     * ----------------------------------------------------------------------- */
    write_buf[0] = 0xDE;
    write_buf[1] = 0xAD;
    write_buf[2] = 0xBE;
    write_buf[3] = 0xEF;
    err = hal_flash_write_save(write_buf, 4u);
    ASSERT_EQ(err, HAL_FLASH_OK, "F3 write 4 bytes OK");

    /* -----------------------------------------------------------------------
     * F4 + F5: read returns correct count and bytes.
     * ----------------------------------------------------------------------- */
    memset(read_buf, 0, sizeof(read_buf));
    bytes_read = 0u;
    err = hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read);
    ASSERT_EQ(err, HAL_FLASH_OK, "F4 read OK");
    ASSERT_EQ(bytes_read, 4u, "F4 bytes_read == 4");
    ASSERT_EQ(read_buf[0], 0xDEu, "F5 byte[0] preserved");
    ASSERT_EQ(read_buf[1], 0xADu, "F5 byte[1] preserved");
    ASSERT_EQ(read_buf[2], 0xBEu, "F5 byte[2] preserved");
    ASSERT_EQ(read_buf[3], 0xEFu, "F5 byte[3] preserved");

    /* -----------------------------------------------------------------------
     * F6: Second write overwrites first.
     * ----------------------------------------------------------------------- */
    write_buf[0] = 0x11;
    write_buf[1] = 0x22;
    hal_flash_write_save(write_buf, 2u);
    memset(read_buf, 0, sizeof(read_buf));
    bytes_read = 0u;
    hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read);
    ASSERT_EQ(bytes_read, 2u, "F6 overwrite size 2");
    ASSERT_EQ(read_buf[0], 0x11u, "F6 overwrite byte[0]");
    ASSERT_EQ(read_buf[1], 0x22u, "F6 overwrite byte[1]");

    /* -----------------------------------------------------------------------
     * F7: Persistence across deinit + reinit.
     * ----------------------------------------------------------------------- */
    hal_flash_deinit();
    err = hal_flash_init();
    ASSERT_EQ(err, HAL_FLASH_OK, "F7 reinit OK");
    memset(read_buf, 0, sizeof(read_buf));
    bytes_read = 0u;
    err = hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read);
    ASSERT_EQ(err, HAL_FLASH_OK, "F7 data persists across reinit");
    ASSERT_EQ(bytes_read, 2u, "F7 bytes_read preserved");
    ASSERT_EQ(read_buf[0], 0x11u, "F7 byte[0] persists");

    /* -----------------------------------------------------------------------
     * F8: deinit then reinit is idempotent.
     * ----------------------------------------------------------------------- */
    hal_flash_deinit();
    hal_flash_deinit();  /* double deinit safe */
    err = hal_flash_init();
    ASSERT_EQ(err, HAL_FLASH_OK, "F8 reinit after double deinit OK");

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
    ASSERT_EQ(err, HAL_FLASH_ERR_WRITE, "F9 injected write error");
    /* Confirm original data is still readable. */
    memset(read_buf, 0, sizeof(read_buf));
    bytes_read = 0u;
    hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read);
    ASSERT_EQ(bytes_read, 8u, "F9 prior data bytes intact");
    ASSERT_EQ(read_buf[0], 0xA0u, "F9 prior data byte[0] intact");
    ASSERT_EQ(read_buf[7], 0xA7u, "F9 prior data byte[7] intact");

    /* -----------------------------------------------------------------------
     * F10: Mount error injection — one-shot.
     * ----------------------------------------------------------------------- */
    mock_flash_reset();
    mock_flash_inject_mount_error();
    err = hal_flash_init();
    ASSERT_EQ(err, HAL_FLASH_ERR_MOUNT, "F10 mount error injection");
    /* Second init succeeds (one-shot). */
    err = hal_flash_init();
    ASSERT_EQ(err, HAL_FLASH_OK, "F10 second init OK after one-shot");

    return g_failures;
}
