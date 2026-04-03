/**
 * test_p19_5_epaper_partial_feature.c — Phase 19.5 Feature Tests: Partial E-Paper Refresh
 *
 * Rule 22 (FEATURE RED): Feature tests written after bound tests, before implementation.
 *
 * Tests:
 *   22. test_flush_partial_sends_partial_lut       — returns OK, counter increments
 *   23. test_flush_partial_full_refresh_at_interval — flushes 1-9 partial, 10 full, 11 partial
 *   24. test_flush_partial_double_call_no_corruption — two successive calls both succeed
 *
 * Uses local ASSERT macros (same pattern as other HAL tests — no PRIu32 dependency).
 */

#include "hal_epaper.h"
#include "mock_hal_epaper.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define ASSERT_EQ(label, expected, actual)                          \
    do {                                                            \
        if ((uint32_t)(expected) != (uint32_t)(actual)) {          \
            printf("FAIL [%s]: expected %d got %d\n",              \
                   (label), (int)(expected), (int)(actual));        \
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

static uint8_t s_buf[HAL_EPAPER_FB_SIZE];

int main(void)
{
    hal_epaper_err_t err;

    /* ------------------------------------------------------------------
     * Test 22: flush_partial returns OK and increments partial counter.
     * ------------------------------------------------------------------ */
    mock_epaper_reset();
    hal_epaper_init();
    memset(s_buf, 0xAAu, sizeof(s_buf));

    err = hal_epaper_flush_partial(s_buf, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("flush_partial_ok", HAL_EPAPER_OK, err);

    /* Partial flush counter must be 1. */
    ASSERT_EQ("partial_count_after_1_flush", 1u, mock_epaper_get_partial_flush_count());

    /* The captured buffer must match what was passed. */
    const uint8_t *captured = mock_epaper_get_buffer();
    ASSERT_EQ("buffer_first_byte", 0xAAu, (uint32_t)captured[0]);
    ASSERT_EQ("buffer_last_byte",  0xAAu, (uint32_t)captured[HAL_EPAPER_FB_SIZE - 1u]);

    /* ------------------------------------------------------------------
     * Test 23: flush_partial does a FULL refresh every EPD_FULL_REFRESH_INTERVAL
     *          flushes (default: 10). Flushes 1-9 are partial, flush 10 is
     *          full, flush 11 is partial again.
     * ------------------------------------------------------------------ */
    mock_epaper_reset();
    hal_epaper_init();
    memset(s_buf, 0x55u, sizeof(s_buf));

    /* First 9 flushes must NOT trigger a full refresh. */
    for (uint32_t i = 1u; i <= 9u; i++) {
        err = hal_epaper_flush_partial(s_buf, HAL_EPAPER_FB_SIZE);
        ASSERT_EQ("partial_9_flushes_ok", HAL_EPAPER_OK, err);
        ASSERT_EQ("partial_9_not_full", 0u,
                  (uint32_t)mock_epaper_get_last_flush_was_full());
    }
    ASSERT_EQ("partial_count_9", 9u, mock_epaper_get_partial_flush_count());

    /* 10th flush must trigger a full refresh. */
    err = hal_epaper_flush_partial(s_buf, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("10th_flush_ok", HAL_EPAPER_OK, err);
    ASSERT_EQ("10th_flush_was_full", 1u,
              (uint32_t)mock_epaper_get_last_flush_was_full());

    /* 11th flush must be partial again. */
    err = hal_epaper_flush_partial(s_buf, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("11th_flush_ok", HAL_EPAPER_OK, err);
    ASSERT_EQ("11th_flush_not_full", 0u,
              (uint32_t)mock_epaper_get_last_flush_was_full());

    /* ------------------------------------------------------------------
     * Test 24: Two successive flush_partial calls both succeed, counter=2.
     * Second call's buffer is captured correctly.
     * ------------------------------------------------------------------ */
    mock_epaper_reset();
    hal_epaper_init();
    memset(s_buf, 0x33u, sizeof(s_buf));

    err = hal_epaper_flush_partial(s_buf, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("double_call_first_ok", HAL_EPAPER_OK, err);

    memset(s_buf, 0xCCu, sizeof(s_buf));
    err = hal_epaper_flush_partial(s_buf, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("double_call_second_ok", HAL_EPAPER_OK, err);

    ASSERT_EQ("double_call_count", 2u, mock_epaper_get_partial_flush_count());

    /* Second buffer must be captured. */
    captured = mock_epaper_get_buffer();
    ASSERT_EQ("double_call_buffer_updated", 0xCCu, (uint32_t)captured[0]);

    printf("test_p19_5_epaper_partial_feature: PASS\n");
    return 0;
}
