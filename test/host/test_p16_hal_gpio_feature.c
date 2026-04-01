/**
 * test_p16_hal_gpio_feature.c — Phase 16 feature tests: GPIO HAL.
 *
 * FEATURE tests (Rule 22 Phase B): Happy-path contracts for the GPIO HAL
 * mock. These tests verify the full init→press→deinit lifecycle and assert
 * specific callback-invocation counts to prove correct ISR callback routing.
 *
 * Debounce note: The host mock calls the registered callback unconditionally
 * on every simulated press (no 50 ms timing gate). This is by design — the
 * mock allows tests to inspect exact invocation counts. The target
 * implementation enforces the 50 ms gate inside the ISR handler via
 * esp_timer_get_time(). Tests F4 and F5 document this contract boundary: on
 * the mock, 10 presses in a burst yield exactly 10 counted events; the target
 * would yield ≤10 (limited by the debounce window).
 *
 * Tests:
 *   F1 : init with valid callback returns HAL_GPIO_OK.
 *   F2 : simulate press BTN_A fires callback with BTN_A; press count == 1.
 *   F3 : simulate press BTN_B increments BTN_B counter independently.
 *   F4 : rapid burst of 10 BTN_A presses yields press count == 10 (mock, no debounce).
 *   F5 : rapid burst of 10 BTN_B presses independent of BTN_A count.
 *   F6 : deinit + reinit cycle restores full operation.
 *   F7 : double init replaces callback cleanly (old callback not called after reinit).
 */

#include "hal_gpio.h"
#include "mock_hal_gpio.h"
#include <stdint.h>
#include <stdio.h>

static int g_failures = 0;

/*
 * ASSERT_EQ — accumulate failures rather than early-exit so all tests always
 * run and the full failure count is visible in the ctest output.
 */
#define ASSERT_EQ(label, expected, actual) do { \
    if ((int)(actual) != (int)(expected)) { \
        fprintf(stderr, "[FAIL] %s: expected %d, got %d\n", (label), (int)(expected), (int)(actual)); \
        g_failures++; \
    } \
} while (0)

/* Callback tracking — file-scope so callbacks can write to them. */
static hal_btn_id_t s_last_btn_cb1  = (hal_btn_id_t)255u;
static uint32_t     s_fire_count_cb1 = 0u;

static hal_btn_id_t s_last_btn_cb2  = (hal_btn_id_t)255u;
static uint32_t     s_fire_count_cb2 = 0u;

static void callback1(hal_btn_id_t btn_id)
{
    s_last_btn_cb1 = btn_id;
    s_fire_count_cb1++;
}

static void callback2(hal_btn_id_t btn_id)
{
    s_last_btn_cb2 = btn_id;
    s_fire_count_cb2++;
}

int main(void)
{
    hal_gpio_err_t err;
    uint32_t       i;

    /* -----------------------------------------------------------------------
     * F1: init with valid callback returns HAL_GPIO_OK.
     * ----------------------------------------------------------------------- */
    mock_gpio_reset();
    s_fire_count_cb1 = 0u;
    s_last_btn_cb1   = (hal_btn_id_t)255u;

    err = hal_gpio_init(callback1);
    ASSERT_EQ("F1 init OK", HAL_GPIO_OK, err);

    /* -----------------------------------------------------------------------
     * F2: Simulate press BTN_A — callback fires with HAL_BTN_A,
     *     press count for BTN_A == 1, BTN_B press count remains 0.
     * ----------------------------------------------------------------------- */
    mock_gpio_simulate_press(HAL_BTN_A);
    ASSERT_EQ("F2 callback fired once",      1u,           s_fire_count_cb1);
    ASSERT_EQ("F2 callback btn is A",        (int)HAL_BTN_A, (int)s_last_btn_cb1);
    ASSERT_EQ("F2 press_count_A == 1",       1u, mock_gpio_get_press_count(HAL_BTN_A));
    ASSERT_EQ("F2 press_count_B still 0",    0u, mock_gpio_get_press_count(HAL_BTN_B));

    /* -----------------------------------------------------------------------
     * F3: Simulate press BTN_B — BTN_B counter increments independently.
     * ----------------------------------------------------------------------- */
    mock_gpio_simulate_press(HAL_BTN_B);
    ASSERT_EQ("F3 callback total fires 2",   2u,           s_fire_count_cb1);
    ASSERT_EQ("F3 callback btn is B",        (int)HAL_BTN_B, (int)s_last_btn_cb1);
    ASSERT_EQ("F3 press_count_B == 1",       1u, mock_gpio_get_press_count(HAL_BTN_B));
    ASSERT_EQ("F3 press_count_A unchanged",  1u, mock_gpio_get_press_count(HAL_BTN_A));

    /* -----------------------------------------------------------------------
     * F4: Rapid burst of 10 BTN_A presses.
     *
     * Mock contract: no debounce gate — all 10 presses are counted and each
     * fires the callback. On the target the ISR 50 ms gate would limit the
     * effective event rate; the mock provides a deterministic count instead.
     * ----------------------------------------------------------------------- */
    mock_gpio_reset();
    s_fire_count_cb1 = 0u;
    hal_gpio_init(callback1);

    for (i = 0u; i < 10u; i++) {
        mock_gpio_simulate_press(HAL_BTN_A);
    }
    ASSERT_EQ("F4 burst_10_A count == 10",       10u, mock_gpio_get_press_count(HAL_BTN_A));
    ASSERT_EQ("F4 burst_10_A callback fires 10", 10u, s_fire_count_cb1);
    /* BTN_B must be unaffected by BTN_A burst. */
    ASSERT_EQ("F4 BTN_B unaffected by A burst",  0u,  mock_gpio_get_press_count(HAL_BTN_B));

    /* -----------------------------------------------------------------------
     * F5: Rapid burst of 10 BTN_B presses — independent of BTN_A count.
     * ----------------------------------------------------------------------- */
    for (i = 0u; i < 10u; i++) {
        mock_gpio_simulate_press(HAL_BTN_B);
    }
    ASSERT_EQ("F5 burst_10_B count == 10",      10u, mock_gpio_get_press_count(HAL_BTN_B));
    ASSERT_EQ("F5 BTN_A count unchanged at 10", 10u, mock_gpio_get_press_count(HAL_BTN_A));
    ASSERT_EQ("F5 total callback fires 20",     20u, s_fire_count_cb1);

    /* -----------------------------------------------------------------------
     * F6: Deinit + reinit cycle restores full operation.
     * ----------------------------------------------------------------------- */
    err = hal_gpio_deinit();
    ASSERT_EQ("F6 deinit OK", HAL_GPIO_OK, err);

    s_fire_count_cb1 = 0u;
    err = hal_gpio_init(callback1);
    ASSERT_EQ("F6 reinit OK", HAL_GPIO_OK, err);
    /* After reinit the mock press counters are preserved by deinit/reinit
     * (mock_gpio_reset was NOT called here — only deinit+init). We just
     * verify the callback fires correctly after reinit. */
    mock_gpio_simulate_press(HAL_BTN_A);
    ASSERT_EQ("F6 callback fires after reinit", 1u, s_fire_count_cb1);

    /* -----------------------------------------------------------------------
     * F7: Double init replaces callback cleanly.
     *     After reinit with callback2, pressing a button must call only
     *     callback2 — callback1 must not be called.
     * ----------------------------------------------------------------------- */
    mock_gpio_reset();
    s_fire_count_cb1 = 0u;
    s_fire_count_cb2 = 0u;
    s_last_btn_cb2   = (hal_btn_id_t)255u;

    hal_gpio_init(callback1);
    hal_gpio_init(callback2);  /* replaces callback1 */

    mock_gpio_simulate_press(HAL_BTN_A);
    ASSERT_EQ("F7 cb2 fires after double-init",  1u,           s_fire_count_cb2);
    ASSERT_EQ("F7 cb1 NOT called after double-init", 0u,       s_fire_count_cb1);
    ASSERT_EQ("F7 cb2 receives BTN_A",           (int)HAL_BTN_A, (int)s_last_btn_cb2);

    mock_gpio_simulate_press(HAL_BTN_B);
    ASSERT_EQ("F7 cb2 fires for BTN_B too",      2u, s_fire_count_cb2);
    ASSERT_EQ("F7 cb1 still not called",         0u, s_fire_count_cb1);

    return g_failures;
}
