/**
 * test_p16_hal_gpio_bounds.c — Phase 16 bound tests: GPIO HAL.
 *
 * BOUND RED tests (Rule 22): Prove the system REJECTS invalid inputs and
 * that the mock correctly handles ISR debounce-storm simulation.
 *
 * Bound conditions tested:
 *   B1 : hal_gpio_init(NULL) returns HAL_GPIO_ERR_NULL.
 *   B2 : hal_gpio_is_pressed() before init returns 0.
 *   B3 : hal_gpio_is_pressed(HAL_BTN_COUNT) returns 0 (sentinel OOB).
 *   B4 : hal_gpio_is_pressed(255) returns 0 (OOB).
 *   B5 : mock_gpio_simulate_press() before init is a safe no-op.
 *   B6 : Rapid fire of 100 presses all increment the press counter correctly.
 *   B7 : deinit() is safe before init (no crash, returns OK).
 *   B8 : Double init replaces the callback (second callback is used).
 *   B9 : mock_gpio_get_press_count(HAL_BTN_COUNT) returns 0 (OOB safe).
 */

#include "hal_gpio.h"
#include "mock_hal_gpio.h"
#include <stdint.h>
#include <stdio.h>

static int g_failures = 0;

#define ASSERT_EQ(label, expected, actual) do { \
    if ((int)(actual) != (int)(expected)) { \
        fprintf(stderr, "[FAIL] %s: expected %d, got %d\n", (label), (int)(expected), (int)(actual)); \
        g_failures++; \
    } \
} while (0)

/* Test callbacks — just track call count. */
static uint32_t s_cb1_count = 0u;
static uint32_t s_cb2_count = 0u;

static void callback1(hal_btn_id_t btn_id) { (void)btn_id; s_cb1_count++; }
static void callback2(hal_btn_id_t btn_id) { (void)btn_id; s_cb2_count++; }

int main(void)
{
    hal_gpio_err_t err;
    uint32_t i;

    /* -----------------------------------------------------------------------
     * B1: NULL callback returns ERR_NULL.
     * ----------------------------------------------------------------------- */
    mock_gpio_reset();
    err = hal_gpio_init((hal_btn_callback_t)0);
    ASSERT_EQ("B1 NULL callback", HAL_GPIO_ERR_NULL, err);

    /* -----------------------------------------------------------------------
     * B2: is_pressed before init returns 0.
     * ----------------------------------------------------------------------- */
    mock_gpio_reset();
    ASSERT_EQ("B2 is_pressed before init", 0u, hal_gpio_is_pressed(HAL_BTN_A));

    /* -----------------------------------------------------------------------
     * B3: is_pressed with sentinel value returns 0.
     * ----------------------------------------------------------------------- */
    mock_gpio_reset();
    hal_gpio_init(callback1);
    ASSERT_EQ("B3 OOB sentinel", 0u, hal_gpio_is_pressed(HAL_BTN_COUNT));

    /* -----------------------------------------------------------------------
     * B4: is_pressed with 255 (far OOB) returns 0.
     * ----------------------------------------------------------------------- */
    ASSERT_EQ("B4 OOB 255", 0u, hal_gpio_is_pressed((hal_btn_id_t)255u));

    /* -----------------------------------------------------------------------
     * B5: simulate_press before init is a safe no-op.
     * ----------------------------------------------------------------------- */
    mock_gpio_reset();
    s_cb1_count = 0u;
    mock_gpio_simulate_press(HAL_BTN_A);  /* must not crash or call callback */
    ASSERT_EQ("B5 simulate before init is no-op", 0u, s_cb1_count);
    ASSERT_EQ("B5 count still 0", 0u, mock_gpio_get_press_count(HAL_BTN_A));

    /* -----------------------------------------------------------------------
     * B6: 100 rapid presses all counted (no debounce in mock).
     * ----------------------------------------------------------------------- */
    mock_gpio_reset();
    s_cb1_count = 0u;
    hal_gpio_init(callback1);
    for (i = 0u; i < 100u; i++) {
        mock_gpio_simulate_press(HAL_BTN_A);
    }
    ASSERT_EQ("B6 100 presses counted",     100u, mock_gpio_get_press_count(HAL_BTN_A));
    ASSERT_EQ("B6 callback called 100 times", 100u, s_cb1_count);

    /* -----------------------------------------------------------------------
     * B7: deinit before init returns OK (no crash).
     * ----------------------------------------------------------------------- */
    mock_gpio_reset();
    err = hal_gpio_deinit();
    ASSERT_EQ("B7 deinit before init OK", HAL_GPIO_OK, err);

    /* -----------------------------------------------------------------------
     * B8: Double init — second callback replaces first.
     * ----------------------------------------------------------------------- */
    mock_gpio_reset();
    s_cb1_count = 0u;
    s_cb2_count = 0u;
    hal_gpio_init(callback1);
    hal_gpio_init(callback2);
    mock_gpio_simulate_press(HAL_BTN_B);
    ASSERT_EQ("B8 second callback used after reinit", 1u, s_cb2_count);
    ASSERT_EQ("B8 first callback not called after reinit", 0u, s_cb1_count);

    /* -----------------------------------------------------------------------
     * B9: get_press_count with OOB id returns 0.
     * ----------------------------------------------------------------------- */
    mock_gpio_reset();
    ASSERT_EQ("B9 count OOB safe",    0u, mock_gpio_get_press_count(HAL_BTN_COUNT));
    ASSERT_EQ("B9 count 255 safe",    0u, mock_gpio_get_press_count((hal_btn_id_t)255u));

    return g_failures;
}
