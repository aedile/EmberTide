/**
 * test_p13_hal_sleep_bounds.c — Phase 13 Bound tests for hal_sleep interface.
 *
 * Rule 22: Written BEFORE feature tests and BEFORE implementation (BOUND RED).
 * Tests prove the system REJECTS / handles safely:
 *   - hal_sleep_enter before init     → HAL_SLEEP_ERR_INIT
 *   - Double deinit is safe           → no crash
 *   - Re-init after deinit is safe    → HAL_SLEEP_OK
 *   - timeout_sec == UINT32_MAX       → HAL_SLEEP_OK (no overflow check needed;
 *                                        timeout is passed directly to hardware)
 *   - HAL_SLEEP_OK == 0 (contract lock)
 */

#include "hal_sleep.h"
#include <stdint.h>
#include <stdio.h>

/* Forward-declare mock reset accessor. */
void mock_sleep_reset(void);

#define ASSERT_EQ(label, expected, actual)                              \
    do {                                                                \
        if ((int)(expected) != (int)(actual)) {                         \
            printf("FAIL [%s]: expected %d got %d\n",                  \
                   (label), (int)(expected), (int)(actual));            \
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
     * Contract lock: HAL_SLEEP_OK must be 0.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("sleep_ok_is_zero", 0, (int)HAL_SLEEP_OK);

    /* ------------------------------------------------------------------
     * enter before init must return ERR_INIT.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("enter_before_init",
              HAL_SLEEP_ERR_INIT,
              hal_sleep_enter(30u));

    /* ------------------------------------------------------------------
     * enter with timeout 0 before init also returns ERR_INIT.
     * (timeout_sec == 0 means wake-on-button-only — still needs init)
     * ------------------------------------------------------------------ */
    ASSERT_EQ("enter_zero_timeout_before_init",
              HAL_SLEEP_ERR_INIT,
              hal_sleep_enter(0u));

    /* ------------------------------------------------------------------
     * Init succeeds.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("init_ok", HAL_SLEEP_OK, hal_sleep_init());

    /* ------------------------------------------------------------------
     * UINT32_MAX timeout is accepted — no overflow guard required since
     * the value is a raw seconds count passed to hardware.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("max_timeout_accepted",
              HAL_SLEEP_OK,
              hal_sleep_enter(UINT32_MAX));

    /* ------------------------------------------------------------------
     * Double deinit is safe.
     * ------------------------------------------------------------------ */
    hal_sleep_deinit();
    hal_sleep_deinit();
    ASSERT_TRUE("double_deinit_safe", 1);

    /* ------------------------------------------------------------------
     * Re-init after deinit returns OK.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("reinit_after_deinit_ok", HAL_SLEEP_OK, hal_sleep_init());

    hal_sleep_deinit();
    return 0;
}
