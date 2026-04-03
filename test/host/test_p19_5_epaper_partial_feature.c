/**
 * test_p19_5_epaper_partial_feature.c — Phase 19.5 Feature Tests: Partial E-Paper Refresh
 *
 * Rule 22 (FEATURE RED): Feature tests written after bound tests, before implementation.
 *
 * Tests:
 *   22. test_flush_partial_sends_partial_lut       — returns OK, counter increments
 *   23. test_flush_partial_full_refresh_at_interval — flushes 1-9 partial, 10 full, 11 partial
 *   24. test_flush_partial_double_call_no_corruption — two successive calls both succeed
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "test_assert.h"
#include "hal_epaper.h"
#include "mock_hal_epaper.h"

static uint8_t s_buf[HAL_EPAPER_FB_SIZE];

int main(void)
{
    /* ------------------------------------------------------------------
     * Test 22: flush_partial returns OK and increments partial counter.
     * ------------------------------------------------------------------ */
    mock_epaper_reset();
    hal_epaper_init();
    memset(s_buf, 0xAAu, sizeof(s_buf));

    hal_epaper_err_t err = hal_epaper_flush_partial(s_buf, HAL_EPAPER_FB_SIZE);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)HAL_EPAPER_OK, (uint32_t)err);

    /* Partial flush counter must be 1. */
    TEST_ASSERT_EQUAL_UINT32(1u, mock_epaper_get_partial_flush_count());

    /* The captured buffer must match what was passed. */
    const uint8_t *captured = mock_epaper_get_buffer();
    TEST_ASSERT_EQUAL_UINT32(0xAAu, (uint32_t)captured[0]);
    TEST_ASSERT_EQUAL_UINT32(0xAAu, (uint32_t)captured[HAL_EPAPER_FB_SIZE - 1u]);

    /* ------------------------------------------------------------------
     * Test 23: flush_partial does a FULL refresh every EPD_FULL_REFRESH_INTERVAL
     *          flushes. For interval=10: flushes 1-9 are partial, flush 10 is
     *          full, flush 11 is partial again.
     *
     * We detect "full" vs "partial" via mock_epaper_get_last_flush_was_full().
     * ------------------------------------------------------------------ */
    mock_epaper_reset();
    hal_epaper_init();
    memset(s_buf, 0x55u, sizeof(s_buf));

    /* First 9 flushes must be partial. */
    for (uint32_t i = 1u; i <= 9u; i++) {
        err = hal_epaper_flush_partial(s_buf, HAL_EPAPER_FB_SIZE);
        TEST_ASSERT_EQUAL_UINT32((uint32_t)HAL_EPAPER_OK, (uint32_t)err);
        TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)mock_epaper_get_last_flush_was_full());
    }
    TEST_ASSERT_EQUAL_UINT32(9u, mock_epaper_get_partial_flush_count());

    /* 10th flush must trigger a full refresh. */
    err = hal_epaper_flush_partial(s_buf, HAL_EPAPER_FB_SIZE);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)HAL_EPAPER_OK, (uint32_t)err);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)mock_epaper_get_last_flush_was_full());

    /* 11th flush must be partial again. */
    err = hal_epaper_flush_partial(s_buf, HAL_EPAPER_FB_SIZE);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)HAL_EPAPER_OK, (uint32_t)err);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)mock_epaper_get_last_flush_was_full());

    /* ------------------------------------------------------------------
     * Test 24: Two successive flush_partial calls both succeed, counter=2.
     * ------------------------------------------------------------------ */
    mock_epaper_reset();
    hal_epaper_init();
    memset(s_buf, 0x33u, sizeof(s_buf));

    err = hal_epaper_flush_partial(s_buf, HAL_EPAPER_FB_SIZE);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)HAL_EPAPER_OK, (uint32_t)err);

    memset(s_buf, 0xCCu, sizeof(s_buf));
    err = hal_epaper_flush_partial(s_buf, HAL_EPAPER_FB_SIZE);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)HAL_EPAPER_OK, (uint32_t)err);

    TEST_ASSERT_EQUAL_UINT32(2u, mock_epaper_get_partial_flush_count());

    /* Second buffer must be captured. */
    captured = mock_epaper_get_buffer();
    TEST_ASSERT_EQUAL_UINT32(0xCCu, (uint32_t)captured[0]);

    printf("test_p19_5_epaper_partial_feature: PASS\n");
    return 0;
}
