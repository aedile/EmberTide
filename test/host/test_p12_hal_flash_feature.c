/**
 * test_p12_hal_flash_feature.c — Phase 12 Feature tests for hal_flash.
 *
 * Rule 22: Written BEFORE implementation (FEATURE RED).
 * These tests define the happy-path and round-trip contract for the mock
 * HAL flash / LittleFS abstraction.
 *
 * Tested behaviours:
 *   - Write then read round-trip (exact byte match)
 *   - bytes_read populated correctly
 *   - Read before write → NOT_FOUND (first-boot scenario)
 *   - Double init is safe
 *   - Deinit + reinit + read → mock persists data across reinit
 *   - Write partial size (< MAX) round-trips correctly
 *   - Max-size write (HAL_FLASH_SAVE_MAX_SIZE exactly) round-trips
 *   - Overwrite: second write replaces first
 *   - NULL bytes_read out-param: read succeeds with prior written data
 */

#include "hal_flash.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define ASSERT_EQ(label, expected, actual)                          \
    do {                                                            \
        if ((size_t)(expected) != (size_t)(actual)) {              \
            printf("FAIL [%s]: expected %zu got %zu\n",            \
                   (label), (size_t)(expected), (size_t)(actual)); \
            return 1;                                               \
        }                                                           \
        printf("PASS [%s]\n", (label));                             \
    } while (0)

#define ASSERT_TRUE(label, cond)                                    \
    do {                                                            \
        if (!(cond)) {                                              \
            printf("FAIL [%s]: condition was false\n", (label));    \
            return 1;                                               \
        }                                                           \
        printf("PASS [%s]\n", (label));                             \
    } while (0)

int main(void)
{
    static uint8_t write_buf[HAL_FLASH_SAVE_MAX_SIZE];
    static uint8_t read_buf[HAL_FLASH_SAVE_MAX_SIZE];
    size_t bytes_read = 0u;

    /* ------------------------------------------------------------------ */
    /* 1. Fresh init, no prior write — first-boot NOT_FOUND.               */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("init_ok", HAL_FLASH_OK, hal_flash_init());
    ASSERT_EQ("read_before_write_not_found",
              HAL_FLASH_ERR_NOT_FOUND,
              hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read));
    ASSERT_EQ("bytes_read_zero_when_not_found", 0u, bytes_read);

    /* ------------------------------------------------------------------ */
    /* 2. Write a known pattern and read it back.                          */
    /* ------------------------------------------------------------------ */
    memset(write_buf, 0x00, sizeof(write_buf));
    write_buf[0]   = 0xDE;
    write_buf[1]   = 0xAD;
    write_buf[2]   = 0xBE;
    write_buf[3]   = 0xEF;
    write_buf[255] = 0x42;
    ASSERT_EQ("write_ok",
              HAL_FLASH_OK,
              hal_flash_write_save(write_buf, 256u));

    memset(read_buf, 0x00, sizeof(read_buf));
    bytes_read = 0u;
    ASSERT_EQ("read_ok",
              HAL_FLASH_OK,
              hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read));
    ASSERT_EQ("bytes_read_256", 256u, bytes_read);
    ASSERT_EQ("roundtrip_byte0",   0xDEu, (size_t)read_buf[0]);
    ASSERT_EQ("roundtrip_byte1",   0xADu, (size_t)read_buf[1]);
    ASSERT_EQ("roundtrip_byte2",   0xBEu, (size_t)read_buf[2]);
    ASSERT_EQ("roundtrip_byte3",   0xEFu, (size_t)read_buf[3]);
    ASSERT_EQ("roundtrip_byte255", 0x42u, (size_t)read_buf[255]);

    /* ------------------------------------------------------------------ */
    /* 3. NULL bytes_read out-param on successful read must not crash.     */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("read_null_bytes_out_ok",
              HAL_FLASH_OK,
              hal_flash_read_save(read_buf, sizeof(read_buf), NULL));
    printf("PASS [read_null_bytes_out_no_crash]\n");

    /* ------------------------------------------------------------------ */
    /* 4. Deinit + reinit + read — mock must persist data.                 */
    /* ------------------------------------------------------------------ */
    hal_flash_deinit();
    ASSERT_EQ("reinit_ok", HAL_FLASH_OK, hal_flash_init());
    memset(read_buf, 0xFF, sizeof(read_buf));
    bytes_read = 0u;
    ASSERT_EQ("read_after_reinit_ok",
              HAL_FLASH_OK,
              hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read));
    ASSERT_EQ("bytes_read_256_after_reinit", 256u, bytes_read);
    ASSERT_EQ("persisted_byte0",   0xDEu, (size_t)read_buf[0]);
    ASSERT_EQ("persisted_byte255", 0x42u, (size_t)read_buf[255]);

    /* ------------------------------------------------------------------ */
    /* 5. Max-size write round-trip.                                       */
    /* ------------------------------------------------------------------ */
    for (size_t i = 0u; i < HAL_FLASH_SAVE_MAX_SIZE; i++) {
        write_buf[i] = (uint8_t)(i & 0xFFu);
    }
    ASSERT_EQ("max_size_write_ok",
              HAL_FLASH_OK,
              hal_flash_write_save(write_buf, HAL_FLASH_SAVE_MAX_SIZE));

    memset(read_buf, 0x00, sizeof(read_buf));
    bytes_read = 0u;
    ASSERT_EQ("max_size_read_ok",
              HAL_FLASH_OK,
              hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read));
    ASSERT_EQ("max_size_bytes_read",
              (size_t)HAL_FLASH_SAVE_MAX_SIZE, bytes_read);
    ASSERT_EQ("max_size_last_byte",
              (size_t)((HAL_FLASH_SAVE_MAX_SIZE - 1u) & 0xFFu),
              (size_t)read_buf[HAL_FLASH_SAVE_MAX_SIZE - 1u]);

    /* ------------------------------------------------------------------ */
    /* 6. Overwrite: second write replaces the first.                      */
    /* ------------------------------------------------------------------ */
    memset(write_buf, 0x99, 10u);
    ASSERT_EQ("overwrite_ok",
              HAL_FLASH_OK,
              hal_flash_write_save(write_buf, 10u));

    memset(read_buf, 0x00, sizeof(read_buf));
    bytes_read = 0u;
    ASSERT_EQ("read_after_overwrite_ok",
              HAL_FLASH_OK,
              hal_flash_read_save(read_buf, sizeof(read_buf), &bytes_read));
    ASSERT_EQ("overwrite_bytes_read", 10u, bytes_read);
    ASSERT_EQ("overwrite_byte0", 0x99u, (size_t)read_buf[0]);

    /* ------------------------------------------------------------------ */
    /* 7. Double init is safe.                                             */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("double_init_ok", HAL_FLASH_OK, hal_flash_init());

    hal_flash_deinit();
    return 0;
}
