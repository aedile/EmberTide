/**
 * mock_hal_gpio.h — Public interface for the hal_gpio host mock.
 *
 * Test files #include this header to access mock state inspectors and
 * button simulation APIs. Only compiled in the host test environment —
 * never on the target.
 */

#ifndef FIESTAQUEST_MOCK_HAL_GPIO_H
#define FIESTAQUEST_MOCK_HAL_GPIO_H

#include "hal_gpio.h"

/** Reset all mock state to power-on defaults. */
void mock_gpio_reset(void);

/**
 * mock_gpio_simulate_press — Inject a button press event.
 *
 * Sets the transient pressed latch, increments the press counter, and
 * calls the registered callback. Out-of-range btn_id values are silently
 * ignored. No-op if the mock has not been initialised via hal_gpio_init().
 */
void mock_gpio_simulate_press(hal_btn_id_t btn_id);

/**
 * mock_gpio_get_press_count — Return cumulative press count for @p btn_id.
 *
 * Returns 0 for out-of-range btn_id values.
 */
uint32_t mock_gpio_get_press_count(hal_btn_id_t btn_id);

#endif /* FIESTAQUEST_MOCK_HAL_GPIO_H */
