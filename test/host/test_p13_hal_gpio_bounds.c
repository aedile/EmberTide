/**
 * test_p13_hal_gpio_bounds.c — Phase 13 Bound tests for hal_gpio button API.
 *
 * Rule 22: Written BEFORE feature tests and BEFORE implementation (BOUND RED).
 * Tests prove the system REJECTS:
 *   - NULL callback at init              → HAL_GPIO_ERR_NULL
 *   - Invalid btn_id (>= HAL_BTN_COUNT) to hal_gpio_is_pressed → returns 0 (safe)
 *   - Press query before init            → returns 0 (safe, not a crash)
 *   - HAL_BTN_COUNT value locked to 2    → contract assertion
 *   - Double deinit is safe              → no crash
 *   - Init with valid callback after deinit (re-init is safe)
 *   - deinit return value is OK          → A1
 *   - simulate_press before init         → press count stays 0 (B2)
 *   - simulate_press with invalid btn_id → press counts stay 0 (A2)
 *   - is_pressed after deinit (not reset)→ returns 0 (A3)
 *   - Enum value contract locks          → A4
 */

#include "hal_gpio.h"
#include <stdint.h>
#include <stdio.h>

/* Forward-declare mock accessors. */
void     mock_gpio_reset(void);
void     mock_gpio_simulate_press(hal_btn_id_t btn_id);
uint32_t mock_gpio_get_press_count(hal_btn_id_t btn_id);

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
     * A4: Enum value contract locks — numeric values must never change.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("gpio_ok_is_zero",       0, (int)HAL_GPIO_OK);
    ASSERT_EQ("gpio_err_init_is_one",  1, (int)HAL_GPIO_ERR_INIT);
    ASSERT_EQ("gpio_err_null_is_two",  2, (int)HAL_GPIO_ERR_NULL);

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
     * B2: simulate_press before init must not increment the press count.
     * The mock must guard on s_mock_initialized before doing any work.
     * ------------------------------------------------------------------ */
    mock_gpio_reset();
    mock_gpio_simulate_press(HAL_BTN_A);
    ASSERT_EQ("simulate_before_init_no_count",
              0u,
              mock_gpio_get_press_count(HAL_BTN_A));

    /* ------------------------------------------------------------------
     * A2: simulate_press with out-of-range btn_id must not increment any
     * press count.  Init first so the init-guard does not interfere.
     * ------------------------------------------------------------------ */
    mock_gpio_reset();
    hal_gpio_init(dummy_callback);
    mock_gpio_simulate_press(HAL_BTN_COUNT);   /* sentinel — invalid */
    mock_gpio_simulate_press((hal_btn_id_t)255u); /* far out-of-range */
    ASSERT_EQ("simulate_invalid_id_count_no_press",
              0u,
              mock_gpio_get_press_count(HAL_BTN_A));
    ASSERT_EQ("simulate_invalid_id_255_no_press",
              0u,
              mock_gpio_get_press_count(HAL_BTN_B));
    hal_gpio_deinit();

    /* ------------------------------------------------------------------
     * A1: deinit return value must be HAL_GPIO_OK.
     * Double deinit must also return HAL_GPIO_OK.
     * ------------------------------------------------------------------ */
    mock_gpio_reset();
    hal_gpio_init(dummy_callback);
    ASSERT_EQ("deinit_returns_ok",    HAL_GPIO_OK, hal_gpio_deinit());
    ASSERT_EQ("double_deinit_ok",     HAL_GPIO_OK, hal_gpio_deinit());

    /* ------------------------------------------------------------------
     * Double deinit is safe — no crash, no undefined behaviour.
     * (Redundant safety check for non-return-value callers.)
     * ------------------------------------------------------------------ */
    ASSERT_TRUE("double_deinit_safe", 1);

    /* ------------------------------------------------------------------
     * A3: is_pressed after deinit (not reset) must return 0.
     * Ensures deinit clears the initialized flag so the pressed latch
     * cannot be read even if simulate was called before deinit.
     * ------------------------------------------------------------------ */
    mock_gpio_reset();
    hal_gpio_init(dummy_callback);
    mock_gpio_simulate_press(HAL_BTN_A);
    hal_gpio_deinit(); /* deinit — NOT reset; latch data remains in RAM */
    ASSERT_EQ("is_pressed_after_deinit_zero",
              0,
              (int)hal_gpio_is_pressed(HAL_BTN_A));

    /* ------------------------------------------------------------------
     * Re-init after deinit with valid callback returns OK.
     * ------------------------------------------------------------------ */
    mock_gpio_reset();
    ASSERT_EQ("reinit_after_deinit_ok",
              HAL_GPIO_OK,
              hal_gpio_init(dummy_callback));

    hal_gpio_deinit();
    return 0;
}
