/**
 * test_p13_hal_gpio_feature.c — Phase 13 Feature tests for hal_gpio button API.
 *
 * Rule 22: Written BEFORE implementation (FEATURE RED).
 * Tested happy-path behaviours:
 *   - init with valid callback returns OK
 *   - simulate press A fires callback with HAL_BTN_A
 *   - simulate press B fires callback with HAL_BTN_B
 *   - press count per button increments correctly
 *   - mock_gpio_reset() clears all state
 *   - hal_gpio_is_pressed() reflects simulated press state
 *   - deinit after init is safe
 *   - callback invoked exact number of times (value assertion)
 */

#include "hal_gpio.h"
#include <stdint.h>
#include <stdio.h>

/* Test accessor declarations (defined in mock_hal_gpio.c). */
void          mock_gpio_reset(void);
void          mock_gpio_simulate_press(hal_btn_id_t btn_id);
uint32_t      mock_gpio_get_press_count(hal_btn_id_t btn_id);

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

/* Callback tracking state — file-scope so the callback can write it. */
static hal_btn_id_t g_last_callback_btn = (hal_btn_id_t)255u;
static uint32_t     g_callback_fire_count = 0u;

static void test_callback(hal_btn_id_t btn_id)
{
    g_last_callback_btn   = btn_id;
    g_callback_fire_count++;
}

int main(void)
{
    mock_gpio_reset();
    g_last_callback_btn   = (hal_btn_id_t)255u;
    g_callback_fire_count = 0u;

    /* ------------------------------------------------------------------
     * 1. Init with valid callback returns OK.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("init_ok", (uint32_t)HAL_GPIO_OK,
              (uint32_t)hal_gpio_init(test_callback));

    /* ------------------------------------------------------------------
     * 2. No press yet — press count for both buttons is 0.
     * ------------------------------------------------------------------ */
    ASSERT_EQ("press_count_a_initial", 0u, mock_gpio_get_press_count(HAL_BTN_A));
    ASSERT_EQ("press_count_b_initial", 0u, mock_gpio_get_press_count(HAL_BTN_B));

    /* ------------------------------------------------------------------
     * 3. Simulate press A — callback fires with HAL_BTN_A.
     * ------------------------------------------------------------------ */
    mock_gpio_simulate_press(HAL_BTN_A);
    ASSERT_EQ("callback_fires_on_press_a",    1u, g_callback_fire_count);
    ASSERT_EQ("callback_btn_id_is_a",
              (uint32_t)HAL_BTN_A,
              (uint32_t)g_last_callback_btn);
    ASSERT_EQ("press_count_a_after_one",      1u, mock_gpio_get_press_count(HAL_BTN_A));
    ASSERT_EQ("press_count_b_unchanged_0",    0u, mock_gpio_get_press_count(HAL_BTN_B));

    /* ------------------------------------------------------------------
     * 4. Simulate press B — callback fires with HAL_BTN_B.
     * ------------------------------------------------------------------ */
    mock_gpio_simulate_press(HAL_BTN_B);
    ASSERT_EQ("callback_total_fires_2",       2u, g_callback_fire_count);
    ASSERT_EQ("callback_btn_id_is_b",
              (uint32_t)HAL_BTN_B,
              (uint32_t)g_last_callback_btn);
    ASSERT_EQ("press_count_b_after_one",      1u, mock_gpio_get_press_count(HAL_BTN_B));
    ASSERT_EQ("press_count_a_unchanged_1",    1u, mock_gpio_get_press_count(HAL_BTN_A));

    /* ------------------------------------------------------------------
     * 5. Simulate multiple A presses — count accumulates.
     * ------------------------------------------------------------------ */
    mock_gpio_simulate_press(HAL_BTN_A);
    mock_gpio_simulate_press(HAL_BTN_A);
    ASSERT_EQ("press_count_a_three",          3u, mock_gpio_get_press_count(HAL_BTN_A));
    ASSERT_EQ("callback_total_fires_4",       4u, g_callback_fire_count);

    /* ------------------------------------------------------------------
     * 6. hal_gpio_is_pressed reflects simulated press.
     *    mock_gpio_simulate_press sets the transient pressed flag.
     * ------------------------------------------------------------------ */
    mock_gpio_simulate_press(HAL_BTN_A);
    ASSERT_EQ("is_pressed_a_true",            1u, (uint32_t)hal_gpio_is_pressed(HAL_BTN_A));

    /* ------------------------------------------------------------------
     * 7. mock_gpio_reset clears all state (press counts → 0).
     * ------------------------------------------------------------------ */
    mock_gpio_reset();
    ASSERT_EQ("press_count_a_after_reset",    0u, mock_gpio_get_press_count(HAL_BTN_A));
    ASSERT_EQ("press_count_b_after_reset",    0u, mock_gpio_get_press_count(HAL_BTN_B));
    ASSERT_EQ("is_pressed_a_after_reset",     0u, (uint32_t)hal_gpio_is_pressed(HAL_BTN_A));

    /* ------------------------------------------------------------------
     * 8. Re-init after reset; deinit is safe.
     * ------------------------------------------------------------------ */
    g_callback_fire_count = 0u;
    ASSERT_EQ("reinit_ok", (uint32_t)HAL_GPIO_OK,
              (uint32_t)hal_gpio_init(test_callback));
    mock_gpio_simulate_press(HAL_BTN_B);
    ASSERT_EQ("callback_after_reinit",        1u, g_callback_fire_count);

    hal_gpio_deinit();
    ASSERT_TRUE("deinit_safe", 1);

    return 0;
}
