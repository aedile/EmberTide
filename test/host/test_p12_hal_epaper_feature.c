/**
 * test_p12_hal_epaper_feature.c — Phase 12 Feature tests for hal_epaper.
 *
 * Rule 22: Written BEFORE implementation (FEATURE RED).
 * These tests define the happy-path contract for the mock HAL e-paper driver.
 *
 * Tested behaviours:
 *   - Successful init resets flush count to 0
 *   - Successful flush returns OK and increments flush_count
 *   - Flushed pixels are captured in the mock buffer (value check)
 *   - Double init is safe (reinit resets state)
 *   - Sleep returns OK after init
 *   - Deinit followed by re-init is safe
 *   - Flush count increments monotonically over multiple flushes
 *   - All-ones buffer captured correctly
 */

#include "hal_epaper.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* Test accessor declarations (defined in mock_hal_epaper.c). */
const uint8_t *mock_epaper_get_buffer(void);
uint32_t       mock_epaper_get_flush_count(void);

#define ASSERT_EQ(label, expected, actual)                          \
    do {                                                            \
        if ((uint32_t)(expected) != (uint32_t)(actual)) {          \
            printf("FAIL [%s]: expected %u got %u\n",              \
                   (label), (unsigned)(expected), (unsigned)(actual)); \
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
    static uint8_t fb[HAL_EPAPER_FB_SIZE];

    /* ------------------------------------------------------------------ */
    /* 1. Init resets flush count to 0.                                    */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("init_ok", HAL_EPAPER_OK, hal_epaper_init());
    ASSERT_EQ("flush_count_after_init", 0u, mock_epaper_get_flush_count());

    /* ------------------------------------------------------------------ */
    /* 2. Flush all-zeros returns OK and increments counter.               */
    /* ------------------------------------------------------------------ */
    memset(fb, 0x00, sizeof(fb));
    ASSERT_EQ("flush_zeros_ok",
              HAL_EPAPER_OK,
              hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE));
    ASSERT_EQ("flush_count_after_first_flush", 1u, mock_epaper_get_flush_count());

    /* ------------------------------------------------------------------ */
    /* 3. Mock buffer captures the flushed pixels — value check.           */
    /* ------------------------------------------------------------------ */
    const uint8_t *captured = mock_epaper_get_buffer();
    ASSERT_TRUE("captured_ptr_nonnull", captured != NULL);
    ASSERT_EQ("captured_byte0_zeros", 0x00u, (uint32_t)captured[0]);
    ASSERT_EQ("captured_last_byte_zeros",
              0x00u, (uint32_t)captured[HAL_EPAPER_FB_SIZE - 1u]);

    /* ------------------------------------------------------------------ */
    /* 4. Flush all-ones captured correctly.                               */
    /* ------------------------------------------------------------------ */
    memset(fb, 0xFF, sizeof(fb));
    ASSERT_EQ("flush_ones_ok",
              HAL_EPAPER_OK,
              hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE));
    ASSERT_EQ("flush_count_after_second_flush", 2u, mock_epaper_get_flush_count());
    ASSERT_EQ("captured_byte0_ones", 0xFFu, (uint32_t)captured[0]);
    ASSERT_EQ("captured_last_byte_ones",
              0xFFu, (uint32_t)captured[HAL_EPAPER_FB_SIZE - 1u]);

    /* ------------------------------------------------------------------ */
    /* 5. Flush with known pattern at specific offset.                     */
    /* ------------------------------------------------------------------ */
    memset(fb, 0x00, sizeof(fb));
    fb[100] = 0xA5u;
    fb[4999] = 0x3Cu;
    ASSERT_EQ("flush_pattern_ok",
              HAL_EPAPER_OK,
              hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE));
    ASSERT_EQ("captured_byte100_pattern", 0xA5u, (uint32_t)captured[100]);
    ASSERT_EQ("captured_last_pattern",    0x3Cu, (uint32_t)captured[4999]);
    ASSERT_EQ("flush_count_3", 3u, mock_epaper_get_flush_count());

    /* ------------------------------------------------------------------ */
    /* 6. Sleep returns OK.                                                */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("sleep_ok", HAL_EPAPER_OK, hal_epaper_sleep());

    /* ------------------------------------------------------------------ */
    /* 7. Double init resets flush count.                                  */
    /* ------------------------------------------------------------------ */
    ASSERT_EQ("double_init_ok", HAL_EPAPER_OK, hal_epaper_init());
    ASSERT_EQ("flush_count_reset_on_double_init", 0u, mock_epaper_get_flush_count());

    /* ------------------------------------------------------------------ */
    /* 8. Deinit + reinit cycle is safe.                                   */
    /* ------------------------------------------------------------------ */
    hal_epaper_deinit();
    ASSERT_EQ("reinit_after_deinit_ok", HAL_EPAPER_OK, hal_epaper_init());
    ASSERT_EQ("flush_count_after_reinit", 0u, mock_epaper_get_flush_count());

    memset(fb, 0xCC, sizeof(fb));
    ASSERT_EQ("flush_after_reinit_ok",
              HAL_EPAPER_OK,
              hal_epaper_flush(fb, HAL_EPAPER_FB_SIZE));
    ASSERT_EQ("flush_count_1_after_reinit", 1u, mock_epaper_get_flush_count());
    ASSERT_EQ("captured_cc_pattern", 0xCCu, (uint32_t)captured[0]);

    hal_epaper_deinit();
    return 0;
}
