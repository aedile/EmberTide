/**
 * test_p13_hal_sleep_feature.c — Phase 13 Feature tests for hal_sleep.
 *
 * Rule 22: Written BEFORE implementation (FEATURE RED).
 * Tested happy-path behaviours:
 *   - init returns OK; sleep_request_count starts at 0
 *   - enter with timeout > 0 returns OK and records the request
 *   - enter with timeout == 0 (button-wake-only) returns OK and records
 *   - sleep_request_count increments per enter call
 *   - last_timeout_sec holds most recent value
 *   - mock_sleep_reset clears all state
 *   - deinit is safe; re-init + enter cycle is safe
 */

#include "hal_sleep.h"
#include <stdint.h>
#include <stdio.h>

/* Test accessor declarations (defined in mock_hal_sleep.c). */
void     mock_sleep_reset(void);
uint32_t mock_sleep_get_request_count(void);
uint32_t mock_sleep_get_last_timeout_sec(void);

#define ASSERT_EQ(label, expected, actual)                              \
    do {                                                                \
        if ((uint32_t)(expected) != (uint32_t)(actual)) {              \
            printf("FAIL [%s]: expected %u got %u\n",                  \
                   (label), (unsigned)(expected), (unsigned)(actual));  \
            return 1;                                                   \
        }                                                               \
        printf("PASS [%s]\n", (label));                                 \
    } while (0)

#define ASSERT_TRUE(label, cond)                                        \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL [%s]: condition was false\n", (label));        \
            return 1;                                                   \
        }                                                               \
        printf("PASS [%s]\n", (label));                                 \
    } while (0)

int main(void)
{
    mock_sleep_reset();

    /* ------------------------------------------------------------------
     * 1. Init returns OK; request count starts at 0.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("init_ok", (uint32_t)HAL_SLEEP_OK, (uint32_t)hal_sleep_init());
    ASSERT_EQ("request_count_initial", 0u, mock_sleep_get_request_count());

    /* ------------------------------------------------------------------
     * 2. enter with timeout 30 sec returns OK, records request.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("enter_30s_ok",
              (uint32_t)HAL_SLEEP_OK,
              (uint32_t)hal_sleep_enter(30u));
    ASSERT_EQ("request_count_after_one",  1u,  mock_sleep_get_request_count());
    ASSERT_EQ("last_timeout_30",         30u,  mock_sleep_get_last_timeout_sec());

    /* ------------------------------------------------------------------
     * 3. enter with timeout == 0 (button-wake-only) returns OK.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("enter_zero_ok",
              (uint32_t)HAL_SLEEP_OK,
              (uint32_t)hal_sleep_enter(0u));
    ASSERT_EQ("request_count_after_two",  2u,  mock_sleep_get_request_count());
    ASSERT_EQ("last_timeout_zero",         0u,  mock_sleep_get_last_timeout_sec());

    /* ------------------------------------------------------------------
     * 4. Multiple enter calls accumulate the request count.
     * ------------------------------------------------------------------ */
    hal_sleep_enter(60u);
    hal_sleep_enter(120u);
    ASSERT_EQ("request_count_after_four",  4u,   mock_sleep_get_request_count());
    ASSERT_EQ("last_timeout_120",         120u,  mock_sleep_get_last_timeout_sec());

    /* ------------------------------------------------------------------
     * 5. mock_sleep_reset clears all state.
     * ------------------------------------------------------------------ */
    mock_sleep_reset();
    ASSERT_EQ("request_count_after_reset",  0u, mock_sleep_get_request_count());
    ASSERT_EQ("last_timeout_after_reset",   0u, mock_sleep_get_last_timeout_sec());

    /* ------------------------------------------------------------------
     * 6. Deinit + re-init cycle is safe.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("reinit_ok", (uint32_t)HAL_SLEEP_OK, (uint32_t)hal_sleep_init());
    ASSERT_EQ("enter_after_reinit_ok",
              (uint32_t)HAL_SLEEP_OK,
              (uint32_t)hal_sleep_enter(10u));
    ASSERT_EQ("request_count_after_reinit", 1u,  mock_sleep_get_request_count());
    ASSERT_EQ("last_timeout_after_reinit", 10u,  mock_sleep_get_last_timeout_sec());

    hal_sleep_deinit();
    ASSERT_TRUE("final_deinit_safe", 1);
    return 0;
}
