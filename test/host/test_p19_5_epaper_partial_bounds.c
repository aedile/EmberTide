/**
 * test_p19_5_epaper_partial_bounds.c — Phase 19.5 Bound Tests: Partial E-Paper Refresh
 *
 * Rule 22 (BOUND RED): These bound/negative tests are written BEFORE implementation
 * to prove the system REJECTS invalid inputs.
 *
 * Tests:
 *   1. test_flush_partial_before_init_returns_err_init
 *   2. test_flush_partial_null_buffer_returns_err_null
 *   3. test_flush_partial_wrong_size_returns_err_size
 *   4. test_flush_partial_guard_order_null_before_size_before_init
 *   5. test_flush_counter_at_uint32_max_no_ub
 *   6. test_flush_partial_error_does_not_increment_counter
 *   7. test_flush_partial_after_sleep_returns_err_init
 *
 * Uses the same ASSERT_EQ / ASSERT_TRUE local macros as other HAL bound tests
 * (no test_assert.h / PRIu32 dependency in HAL-only compilation units).
 */

#include "hal_epaper.h"
#include "mock_hal_epaper.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* Local assertion helpers (same pattern as test_p12_hal_epaper_bounds.c). */
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

/* A valid framebuffer for guard-order tests. */
static uint8_t s_valid_buf[HAL_EPAPER_FB_SIZE];

int main(void)
{
    hal_epaper_err_t err;

    /* ------------------------------------------------------------------
     * Test 1: flush_partial before init must return ERR_INIT.
     * ------------------------------------------------------------------ */
    mock_epaper_reset(); /* uninitialised */
    err = hal_epaper_flush_partial(s_valid_buf, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("flush_partial_before_init", HAL_EPAPER_ERR_INIT, err);

    /* ------------------------------------------------------------------
     * Test 2: NULL buffer must return ERR_NULL (guard order: NULL first).
     * ------------------------------------------------------------------ */
    mock_epaper_reset();
    hal_epaper_init();
    err = hal_epaper_flush_partial(NULL, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("flush_partial_null_buffer", HAL_EPAPER_ERR_NULL, err);

    /* ------------------------------------------------------------------
     * Test 3: Wrong size must return ERR_SIZE.
     * ------------------------------------------------------------------ */
    mock_epaper_reset();
    hal_epaper_init();
    err = hal_epaper_flush_partial(s_valid_buf, HAL_EPAPER_FB_SIZE - 1u);
    ASSERT_EQ("flush_partial_wrong_size_minus1", HAL_EPAPER_ERR_SIZE, err);

    err = hal_epaper_flush_partial(s_valid_buf, 0u);
    ASSERT_EQ("flush_partial_zero_size", HAL_EPAPER_ERR_SIZE, err);

    err = hal_epaper_flush_partial(s_valid_buf, HAL_EPAPER_FB_SIZE + 1u);
    ASSERT_EQ("flush_partial_wrong_size_plus1", HAL_EPAPER_ERR_SIZE, err);

    /* ------------------------------------------------------------------
     * Test 4: Guard order — NULL before size before init.
     *
     * NULL pointer must be detected even when size is wrong AND driver
     * is uninitialised. ERR_NULL must win over ERR_SIZE and ERR_INIT.
     * ------------------------------------------------------------------ */
    mock_epaper_reset(); /* uninitialised */

    /* NULL + wrong size + uninitialised → ERR_NULL */
    err = hal_epaper_flush_partial(NULL, HAL_EPAPER_FB_SIZE - 1u);
    ASSERT_EQ("guard_order_null_wins_over_size_and_init", HAL_EPAPER_ERR_NULL, err);

    /* Valid ptr + wrong size + uninitialised → ERR_SIZE (size before init) */
    err = hal_epaper_flush_partial(s_valid_buf, HAL_EPAPER_FB_SIZE - 1u);
    ASSERT_EQ("guard_order_size_wins_over_init", HAL_EPAPER_ERR_SIZE, err);

    /* Valid ptr + correct size + uninitialised → ERR_INIT */
    err = hal_epaper_flush_partial(s_valid_buf, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("guard_order_init_last", HAL_EPAPER_ERR_INIT, err);

    /* ------------------------------------------------------------------
     * Test 5: flush_counter at UINT32_MAX — modulo must not UB.
     *
     * Force the mock flush counter to UINT32_MAX - 1, then do two
     * partial flushes. The counter wraps (uint32_t is well-defined)
     * without crashing or hitting a division-by-zero.
     * ------------------------------------------------------------------ */
    mock_epaper_reset();
    hal_epaper_init();
    mock_epaper_set_partial_flush_count(UINT32_MAX - 1u);
    memset(s_valid_buf, 0xFFu, sizeof(s_valid_buf));
    err = hal_epaper_flush_partial(s_valid_buf, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("counter_near_uint32_max_first", HAL_EPAPER_OK, err);
    /* One more — wraps from UINT32_MAX to 0. */
    err = hal_epaper_flush_partial(s_valid_buf, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("counter_wraps_uint32_max", HAL_EPAPER_OK, err);

    /* ------------------------------------------------------------------
     * Test 6: flush_partial error does NOT increment the flush counter.
     * ------------------------------------------------------------------ */
    mock_epaper_reset();
    hal_epaper_init();
    uint32_t count_before = mock_epaper_get_partial_flush_count();

    /* ERR_NULL — counter must stay the same. */
    hal_epaper_flush_partial(NULL, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("error_no_increment_null",
              (int)count_before, (int)mock_epaper_get_partial_flush_count());

    /* ERR_SIZE — counter must stay the same. */
    hal_epaper_flush_partial(s_valid_buf, 0u);
    ASSERT_EQ("error_no_increment_size",
              (int)count_before, (int)mock_epaper_get_partial_flush_count());

    /* ------------------------------------------------------------------
     * Test 7: flush_partial after sleep must return ERR_INIT.
     *
     * hal_epaper_sleep() must clear s_initialized, so a subsequent
     * flush_partial call gets ERR_INIT.
     * ------------------------------------------------------------------ */
    mock_epaper_reset();
    hal_epaper_init();
    hal_epaper_sleep();
    err = hal_epaper_flush_partial(s_valid_buf, HAL_EPAPER_FB_SIZE);
    ASSERT_EQ("flush_partial_after_sleep", HAL_EPAPER_ERR_INIT, err);

    printf("test_p19_5_epaper_partial_bounds: PASS\n");
    return 0;
}
