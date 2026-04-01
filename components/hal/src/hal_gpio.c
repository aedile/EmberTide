/**
 * hal_gpio.c — Hardware Abstraction Layer: GPIO / Button Input (target stub)
 *
 * This translation unit is the TARGET implementation linked into the ESP-IDF
 * firmware image. It is NOT compiled on the host — mock_hal_gpio.c is used
 * there instead.
 *
 * Target behaviour (ESP-IDF v5.x):
 *   - Configures GPIO_INTR_NEGEDGE on GPIO 0 (BTN_A) and GPIO 18 (BTN_B).
 *   - ISR checks esp_timer_get_time() and enforces a 50 ms debounce gate
 *     before posting to an internal FreeRTOS queue.
 *   - A dedicated task drains the queue and calls the registered callback.
 *
 * Stub policy: all functions compile cleanly without ESP-IDF headers so that
 * `idf.py build` succeeds as a cross-compilation smoke-test. When ESP-IDF
 * driver headers are available, replace the stub bodies with real calls.
 */

#include "hal_gpio.h"

/* Stored callback — set by hal_gpio_init(), cleared by hal_gpio_deinit(). */
static hal_btn_callback_t s_callback;

/* Pressed state polled by hal_gpio_is_pressed() (target reads GPIO level). */
static uint8_t s_initialized;

hal_gpio_err_t hal_gpio_init(hal_btn_callback_t callback)
{
    if (!callback) {
        return HAL_GPIO_ERR_NULL;
    }
    s_callback    = callback;
    s_initialized = 1u;
    /*
     * TODO (ESP-IDF wiring): configure GPIO 0 and GPIO 18 with
     *   gpio_config_t, gpio_install_isr_service(), gpio_isr_handler_add().
     * ISR must gate on esp_timer_get_time() diff >= 50000 us before
     * xQueueSendFromISR() to prevent interrupt storm starvation.
     */
    return HAL_GPIO_OK;
}

hal_gpio_err_t hal_gpio_deinit(void)
{
    s_callback    = (hal_btn_callback_t)0;
    s_initialized = 0u;
    /*
     * TODO (ESP-IDF wiring): gpio_isr_handler_remove(), gpio_uninstall_isr_service().
     */
    return HAL_GPIO_OK;
}

uint8_t hal_gpio_is_pressed(hal_btn_id_t btn_id)
{
    if (!s_initialized || (uint32_t)btn_id >= (uint32_t)HAL_BTN_COUNT) {
        return 0u;
    }
    /*
     * TODO (ESP-IDF wiring): return (uint8_t)!gpio_get_level(pin_for(btn_id));
     * Active-low: pressed == GPIO reads 0.
     */
    return 0u;
}
