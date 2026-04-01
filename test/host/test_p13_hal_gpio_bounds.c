/**
 * test_p13_hal_gpio_bounds.c — Phase 13 Bound tests for hal_gpio button API.
 *
 * Rule 22: Written BEFORE feature tests and BEFORE implementation (BOUND RED).
 * Tests prove the system REJECTS:
 *   - NULL callback at init          → HAL_GPIO_ERR_NULL
 *   - Invalid btn_id (>= HAL_BTN_COUNT) to hal_gpio_is_pressed → returns 0 (safe)
 *   - Press query before init        → returns 0 (safe, not a crash)
 *   - HAL_BTN_COUNT value locked to 2 (contract)
 *   - Double deinit is safe (no crash)
 *   - Init with valid callback after deinit (re-init is safe)
 */

#include "hal_gpio.h"
#include <stdint.h>
#include <stdio.h>

/* Forward-declare mock reset accessor. */
void mock_gpio_reset(void);

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

/* Dummy callback used only to satisfy init signature. */
static void dummy_callback(hal_btn_id_t btn_id)
{
    (void)btn_id;
}

int main(void)
{
    mock_gpio_reset();

    /* ------------------------------------------------------------------
     * Contract lock: HAL_BTN_COUNT must equal 2.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("btn_count_is_2", 2, (int)HAL_BTN_COUNT);

    /* ------------------------------------------------------------------
     * NULL callback rejected at init.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("null_callback_rejected",
              HAL_GPIO_ERR_NULL,
              hal_gpio_init(NULL));

    /* ------------------------------------------------------------------
     * Press query before any init returns 0 (safe, not a crash).
     * ------------------------------------------------------------------ */
    ASSERT_EQ("is_pressed_before_init_btn_a",
              0,
              (int)hal_gpio_is_pressed(HAL_BTN_A));
    ASSERT_EQ("is_pressed_before_init_btn_b",
              0,
              (int)hal_gpio_is_pressed(HAL_BTN_B));

    /* ------------------------------------------------------------------
     * Invalid btn_id (HAL_BTN_COUNT itself, out-of-range) returns 0.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("is_pressed_invalid_id_count",
              0,
              (int)hal_gpio_is_pressed(HAL_BTN_COUNT));

    /* ------------------------------------------------------------------
     * Invalid btn_id (255, clearly out-of-range) returns 0.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("is_pressed_invalid_id_255",
              0,
              (int)hal_gpio_is_pressed((hal_btn_id_t)255u));

    /* ------------------------------------------------------------------
     * Double deinit is safe — no crash, no undefined behaviour.
     * ------------------------------------------------------------------ */
    hal_gpio_deinit();
    hal_gpio_deinit(); /* second call must not crash */
    ASSERT_TRUE("double_deinit_safe", 1);

    /* ------------------------------------------------------------------
     * Re-init after deinit with valid callback returns OK.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("reinit_after_deinit_ok",
              HAL_GPIO_OK,
              hal_gpio_init(dummy_callback));

    hal_gpio_deinit();
    return 0;
}
