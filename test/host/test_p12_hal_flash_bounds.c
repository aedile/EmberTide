/**
 * test_p12_hal_flash_bounds.c — Phase 12 Bound tests for hal_flash interface.
 *
 * Rule 22: Written BEFORE feature tests and BEFORE implementation (BOUND RED).
 * Tests prove the system REJECTS:
 *   - NULL buf on read          → HAL_FLASH_ERR_NULL
 *   - NULL buf on write         → HAL_FLASH_ERR_NULL
 *   - Zero buf_size on read     → HAL_FLASH_ERR_SIZE
 *   - Zero size on write        → HAL_FLASH_ERR_SIZE
 *   - Oversized write           → HAL_FLASH_ERR_SIZE
 *   - Write size > HAL_FLASH_SAVE_MAX_SIZE → HAL_FLASH_ERR_SIZE
 *   - HAL_FLASH_SAVE_MAX_SIZE constant equals 512 (contract lock)
 *   - Read before write (first boot) → HAL_FLASH_ERR_NOT_FOUND
 *   - NULL bytes_read out-param is safe (no crash)
 */

#include "hal_flash.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define ASSERT_EQ(label, expected, actual)                          \
    do {                                                            \
        if ((expected) != (actual)) {                               \
            printf("FAIL [%s]: expected %d got %d\n",              \
                   (label), (int)(expected), (int)(actual));        \
            return 1;                                               \
        }                                                           \
        printf("PASS [%s]\n", (label));                             \
    } while (0)

int main(void)
{
    static uint8_t buf[HAL_FLASH_SAVE_MAX_SIZE + 1u];
    size_t bytes_read = 0u;

    /* ------------------------------------------------------------------ */
    /* Contract lock: HAL_FLASH_SAVE_MAX_SIZE must equal 512.              */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("SAVE_MAX_SIZE_equals_512", 512u, (unsigned)HAL_FLASH_SAVE_MAX_SIZE);

    /* Init so remaining checks exercise the parameter guards. */
    ASSERT_EQ("flash_init_ok", HAL_FLASH_OK, hal_flash_init());

    /* ------------------------------------------------------------------ */
    /* NULL pointer: read.                                                 */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("read_null_buf",
              HAL_FLASH_ERR_NULL,
              hal_flash_read_save(NULL, sizeof(buf), &bytes_read));

    /* ------------------------------------------------------------------ */
    /* NULL pointer: write.                                                */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("write_null_buf",
              HAL_FLASH_ERR_NULL,
              hal_flash_write_save(NULL, 10u));

    /* ------------------------------------------------------------------ */
    /* Zero size: read.                                                    */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("read_zero_size",
              HAL_FLASH_ERR_SIZE,
              hal_flash_read_save(buf, 0u, &bytes_read));

    /* ------------------------------------------------------------------ */
    /* Zero size: write.                                                   */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("write_zero_size",
              HAL_FLASH_ERR_SIZE,
              hal_flash_write_save(buf, 0u));

    /* ------------------------------------------------------------------ */
    /* Oversized write: HAL_FLASH_SAVE_MAX_SIZE + 1.                       */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("write_oversized",
              HAL_FLASH_ERR_SIZE,
              hal_flash_write_save(buf, HAL_FLASH_SAVE_MAX_SIZE + 1u));

    /* ------------------------------------------------------------------ */
    /* SIZE_MAX write.                                                     */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("write_size_max",
              HAL_FLASH_ERR_SIZE,
              hal_flash_write_save(buf, (size_t)-1));

    /* ------------------------------------------------------------------ */
    /* Read before any write (first-boot / empty storage).                 */
    /* After fresh init, no data exists — must return NOT_FOUND.           */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("read_before_write",
              HAL_FLASH_ERR_NOT_FOUND,
              hal_flash_read_save(buf, sizeof(buf), &bytes_read));

    /* ------------------------------------------------------------------ */
    /* NULL bytes_read out-param on a valid read attempt must not crash.   */
    /* (returns NOT_FOUND since still nothing written, but must not crash)  */
    /* ------------------------------------------------------------------ */
    hal_flash_err_t rc = hal_flash_read_save(buf, sizeof(buf), NULL);
    /* rc is either NOT_FOUND or OK — just must not segfault */
    (void)rc;
    printf("PASS [read_null_bytes_read_no_crash]\n");

    hal_flash_deinit();
    return 0;
}
