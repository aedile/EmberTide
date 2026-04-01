/**
 * mock_hal_gpio.c — Host mock for hal_gpio button driver.
 *
 * Linked by test/host/ targets instead of components/hal/src/hal_gpio.c.
 * Simulates button press behaviour entirely in RAM:
 *   - Stores the registered callback pointer.
 *   - mock_gpio_simulate_press() calls the callback and increments the
 *     per-button press counter, mirroring ISR → callback flow on target.
 *   - mock_gpio_get_press_count() exposes the per-button counter for
 *     value assertions in test_p13_hal_gpio_feature.c.
 *   - Tracks a transient "pressed" flag per button so hal_gpio_is_pressed()
 *     returns 1 immediately after simulate_press() is called.  The flag is
 *     cleared on the next call to hal_gpio_is_pressed() (one-shot latch).
 *   - mock_gpio_reset() clears all state to power-on defaults.
 *
 * Design note: the 50 ms debounce gate present in the target ISR is NOT
 * simulated here — test code must be able to fire events at any rate and
 * assert exact counts.
 */

#include "hal_gpio.h"

/* -------------------------------------------------------------------------
 * Internal mock state.
 * -------------------------------------------------------------------------
 */
static hal_btn_callback_t s_mock_callback;
static uint8_t            s_mock_initialized;

/* Per-button cumulative press counters. */
static uint32_t s_press_count[HAL_BTN_COUNT];

/* Per-button transient pressed flag (cleared on first read). */
static uint8_t  s_pressed_latch[HAL_BTN_COUNT];

/* -------------------------------------------------------------------------
 * Public hal_gpio API — mock implementations.
 * -------------------------------------------------------------------------
 */

hal_gpio_err_t hal_gpio_init(hal_btn_callback_t callback)
{
    if (!callback) {
        return HAL_GPIO_ERR_NULL;
    }
    s_mock_callback    = callback;
    s_mock_initialized = 1u;
    return HAL_GPIO_OK;
}

hal_gpio_err_t hal_gpio_deinit(void)
{
    s_mock_callback    = (hal_btn_callback_t)0;
    s_mock_initialized = 0u;
    return HAL_GPIO_OK;
}

uint8_t hal_gpio_is_pressed(hal_btn_id_t btn_id)
{
    if (!s_mock_initialized || (uint32_t)btn_id >= (uint32_t)HAL_BTN_COUNT) {
        return 0u;
    }
    uint8_t result = s_pressed_latch[btn_id];
    s_pressed_latch[btn_id] = 0u; /* consume the latch */
    return result;
}

/* -------------------------------------------------------------------------
 * Test accessor functions — host-only, not declared in hal_gpio.h.
 * -------------------------------------------------------------------------
 */

/**
 * mock_gpio_simulate_press — Inject a button press event.
 *
 * B2 guard: returns immediately if the mock has not been initialised via
 * hal_gpio_init().  This mirrors the real driver's behaviour where no ISR
 * is registered until init runs.
 *
 * Sets the transient pressed latch, increments the press counter, and
 * calls the registered callback (if init was called).  Out-of-range
 * btn_id values are silently ignored.
 */
void mock_gpio_simulate_press(hal_btn_id_t btn_id)
{
    if (!s_mock_initialized) {
        return;
    }
    if ((uint32_t)btn_id >= (uint32_t)HAL_BTN_COUNT) {
        return;
    }
    s_pressed_latch[btn_id] = 1u;
    s_press_count[btn_id]++;
    if (s_mock_callback) {
        s_mock_callback(btn_id);
    }
}

/**
 * mock_gpio_get_press_count — Return cumulative press count for @p btn_id.
 *
 * Returns 0 for out-of-range btn_id values.
 */
uint32_t mock_gpio_get_press_count(hal_btn_id_t btn_id)
{
    if ((uint32_t)btn_id >= (uint32_t)HAL_BTN_COUNT) {
        return 0u;
    }
    return s_press_count[btn_id];
}

/**
 * mock_gpio_reset — Reset all mock state to power-on defaults.
 *
 * Clears callback, initialized flag, press counters, and pressed latches.
 * Call at the start of each test main() for a clean slate.
 */
void mock_gpio_reset(void)
{
    uint32_t i;
    s_mock_callback    = (hal_btn_callback_t)0;
    s_mock_initialized = 0u;
    for (i = 0u; i < (uint32_t)HAL_BTN_COUNT; i++) {
        s_press_count[i]   = 0u;
        s_pressed_latch[i] = 0u;
    }
}
