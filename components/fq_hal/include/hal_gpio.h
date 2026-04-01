/**
 * hal_gpio.h — Hardware Abstraction Layer: GPIO / Button Input
 *
 * Target: ESP32-S3-PICO-1-N8R8 via ESP-IDF v5.x.
 * Pin assignments (from docs/pin-definitions.md):
 *   HAL_BTN_A (BOOT) — GPIO_NUM_0  (also ext_wakeup_pin_1)
 *   HAL_BTN_B (PWR)  — GPIO_NUM_18
 *
 * Architecture constraint: hal/ is the BOTTOM layer. It MUST NOT be included
 * by game/, presentation/, or connectivity/. Upper layers reach hardware
 * exclusively through the platform/ services layer.
 *
 * This header is intentionally host-compilable — it contains no ESP-IDF types.
 * The target implementation (hal_gpio.c) uses ESP-IDF internally; the host
 * mock (mock_hal_gpio.c) replaces it entirely for unit tests.
 *
 * Interrupt storm mitigation note (spec-challenger requirement):
 *   The target implementation MUST check esp_timer_get_time() inside the ISR
 *   and enforce a 50 ms minimum inter-event gap before posting to the event
 *   queue. This prevents FreeRTOS queue saturation from a pin oscillating at
 *   high frequency. The host mock does NOT simulate this timing gate; it calls
 *   the callback unconditionally so tests can inspect exact invocation counts.
 */

#ifndef FIESTAQUEST_HAL_GPIO_H
#define FIESTAQUEST_HAL_GPIO_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Button identifiers.
 * HAL_BTN_COUNT is used as a sentinel for bounds checking — never pass it
 * to hal_gpio_is_pressed() or mock_gpio_simulate_press().
 * -------------------------------------------------------------------------
 */
typedef enum {
    HAL_BTN_A     = 0,  /**< BOOT button — GPIO 0 */
    HAL_BTN_B     = 1,  /**< PWR button  — GPIO 18 */
    HAL_BTN_COUNT = 2   /**< Sentinel — total number of physical buttons */
} hal_btn_id_t;

/* -------------------------------------------------------------------------
 * Return codes for hal_gpio operations.
 * -------------------------------------------------------------------------
 */
typedef enum {
    HAL_GPIO_OK       = 0, /**< Operation completed successfully. */
    HAL_GPIO_ERR_INIT = 1, /**< Driver not initialised. */
    HAL_GPIO_ERR_NULL = 2  /**< Caller passed a NULL callback pointer. */
} hal_gpio_err_t;

/* -------------------------------------------------------------------------
 * Callback type invoked on each debounced button event.
 *
 * On the target this is called from a task context (posted from ISR via
 * queue). On the host mock it is called directly from
 * mock_gpio_simulate_press().
 *
 * @param btn_id  Which button was pressed.
 * -------------------------------------------------------------------------
 */
typedef void (*hal_btn_callback_t)(hal_btn_id_t btn_id);

/**
 * hal_gpio_init — Initialise GPIO interrupt driver and register callback.
 *
 * Configures GPIO_INTR_NEGEDGE interrupts on BTN_A (GPIO 0) and BTN_B
 * (GPIO 18). The ISR posts to an internal queue; a dedicated FreeRTOS task
 * drains the queue and calls @p callback after applying a 50 ms software
 * debounce.
 *
 * Safe to call multiple times (re-init replaces the stored callback).
 *
 * @param callback  Function called on each valid button event. Must not be NULL.
 * @return HAL_GPIO_OK       on success.
 *         HAL_GPIO_ERR_NULL if callback is NULL.
 */
hal_gpio_err_t hal_gpio_init(hal_btn_callback_t callback);

/**
 * hal_gpio_deinit — Release GPIO resources and unregister ISR handlers.
 *
 * Safe to call without a preceding init (no-op in that case).
 * Safe to call multiple times.
 */
hal_gpio_err_t hal_gpio_deinit(void);

/**
 * hal_gpio_is_pressed — Poll the current logical press state of a button.
 *
 * Intended for host-side testing (mock sets the pressed flag on simulate).
 * On the target this reads the GPIO level directly (not debounced).
 *
 * Returns 0 if @p btn_id is out of range or if the driver is not initialised.
 *
 * @param btn_id  Button to query.
 * @return 1 if pressed (active low — GPIO reads 0), 0 otherwise.
 */
uint8_t hal_gpio_is_pressed(hal_btn_id_t btn_id);

#endif /* FIESTAQUEST_HAL_GPIO_H */
