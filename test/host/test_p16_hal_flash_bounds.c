/**
 * test_p16_hal_flash_bounds.c — Phase 16 bound tests: flash HAL.
 *
 * BOUND RED tests (Rule 22): Prove the system REJECTS invalid inputs and
 * that the mock correctly supports write failure injection for atomic write
 * error simulation.
 *
 * Bound conditions tested:
 *   B1 : read_save with NULL buffer returns HAL_FLASH_ERR_NULL.
 *   B2 : read_save with buf_size == 0 returns HAL_FLASH_ERR_SIZE.
 *   B3 : write_save with NULL buffer returns HAL_FLASH_ERR_NULL.
 *   B4 : write_save with size == 0 returns HAL_FLASH_ERR_SIZE.
 *   B5 : write_save with size > HAL_FLASH_SAVE_MAX_SIZE returns ERR_SIZE.
 *   B6 : mock_flash_inject_write_error() makes write_save return ERR_WRITE.
 *   B7 : After injected write error fires once, subsequent write succeeds.
 *   B8 : mock_flash_inject_mount_error() makes init return ERR_MOUNT.
 *   B9 : Read after write-error leaves previous data intact (no corruption).
 *   B10: write_save with size == HAL_FLASH_SAVE_MAX_SIZE succeeds (boundary).
 */

#include "hal_flash.h"
#include "mock_hal_flash.h"
#include <stdint.h>
#include <string.h>

static int g_failures = 0;

#define ASSERT_EQ(actual, expected, label)               \
    do {                                                 \
        if ((actual) != (expected)) {                    \
            g_failures++;                                \
        }                                                \
    } while (0)

int main(void)
{
    uint8_t buf[HAL_FLASH_SAVE_MAX_SIZE];
    size_t  bytes_read;
    hal_flash_err_t err;

    /* -----------------------------------------------------------------------
     * B1: NULL buffer.
     * ----------------------------------------------------------------------- */
    mock_flash_reset();
    hal_flash_init();
    err = hal_flash_read_save(NULL, HAL_FLASH_SAVE_MAX_SIZE, &bytes_read);
    ASSERT_EQ(err, HAL_FLASH_ERR_NULL, "B1 read NULL buf");

    /* -----------------------------------------------------------------------
     * B2: buf_size == 0.
     * ----------------------------------------------------------------------- */
    mock_flash_reset();
    hal_flash_init();
    err = hal_flash_read_save(buf, 0u, &bytes_read);
    ASSERT_EQ(err, HAL_FLASH_ERR_SIZE, "B2 read buf_size 0");

    /* -----------------------------------------------------------------------
     * B3: write_save NULL buffer.
     * ----------------------------------------------------------------------- */
    mock_flash_reset();
    hal_flash_init();
    err = hal_flash_write_save(NULL, 4u);
    ASSERT_EQ(err, HAL_FLASH_ERR_NULL, "B3 write NULL buf");

    /* -----------------------------------------------------------------------
     * B4: write_save size == 0.
     * ----------------------------------------------------------------------- */
    mock_flash_reset();
    hal_flash_init();
    err = hal_flash_write_save(buf, 0u);
    ASSERT_EQ(err, HAL_FLASH_ERR_SIZE, "B4 write size 0");

    /* -----------------------------------------------------------------------
     * B5: write_save size exceeds maximum.
     * ----------------------------------------------------------------------- */
    mock_flash_reset();
    hal_flash_init();
    err = hal_flash_write_save(buf, HAL_FLASH_SAVE_MAX_SIZE + 1u);
    ASSERT_EQ(err, HAL_FLASH_ERR_SIZE, "B5 write size > MAX");

    /* -----------------------------------------------------------------------
     * B10: write_save size == HAL_FLASH_SAVE_MAX_SIZE succeeds (max boundary).
     * ----------------------------------------------------------------------- */
    mock_flash_reset();
    hal_flash_init();
    memset(buf, 0xAB, sizeof(buf));
    err = hal_flash_write_save(buf, HAL_FLASH_SAVE_MAX_SIZE);
    ASSERT_EQ(err, HAL_FLASH_OK, "B10 write MAX size succeeds");

    /* -----------------------------------------------------------------------
     * B6: Write error injection.
     * ----------------------------------------------------------------------- */
    mock_flash_reset();
    hal_flash_init();
    mock_flash_inject_write_error();
    err = hal_flash_write_save(buf, 4u);
    ASSERT_EQ(err, HAL_FLASH_ERR_WRITE, "B6 write error injection");

    /* -----------------------------------------------------------------------
     * B7: Write error is one-shot — subsequent write succeeds.
     * ----------------------------------------------------------------------- */
    err = hal_flash_write_save(buf, 4u);
    ASSERT_EQ(err, HAL_FLASH_OK, "B7 one-shot write error clears");

    /* -----------------------------------------------------------------------
     * B8: Mount error injection.
     * ----------------------------------------------------------------------- */
    mock_flash_reset();
    mock_flash_inject_mount_error();
    err = hal_flash_init();
    ASSERT_EQ(err, HAL_FLASH_ERR_MOUNT, "B8 mount error injection");

    /* -----------------------------------------------------------------------
     * B9: Data written before injected error is intact (no side effects).
     * Write good data, then inject error on next write, confirm first read OK.
     * ----------------------------------------------------------------------- */
    mock_flash_reset();
    hal_flash_init();
    buf[0] = 0x42u;
    buf[1] = 0x24u;
    err = hal_flash_write_save(buf, 2u);
    ASSERT_EQ(err, HAL_FLASH_OK, "B9 setup write OK");

    mock_flash_inject_write_error();
    memset(buf, 0, sizeof(buf));
    err = hal_flash_write_save(buf, 2u);   /* this write fails */
    ASSERT_EQ(err, HAL_FLASH_ERR_WRITE, "B9 injected error fires");

    memset(buf, 0, sizeof(buf));
    bytes_read = 0u;
    err = hal_flash_read_save(buf, sizeof(buf), &bytes_read);
    ASSERT_EQ(err, HAL_FLASH_OK, "B9 previous data intact after failed write");
    ASSERT_EQ(buf[0], 0x42u, "B9 data[0] preserved");
    ASSERT_EQ(buf[1], 0x24u, "B9 data[1] preserved");

    return g_failures;
}
