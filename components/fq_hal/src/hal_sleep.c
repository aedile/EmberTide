/**
 * hal_sleep.c — Hardware Abstraction Layer: Deep Sleep (target stub)
 *
 * This translation unit is the TARGET implementation linked into the ESP-IDF
 * firmware image. It is NOT compiled on the host — mock_hal_sleep.c is used
 * there instead.
 *
 * Target behaviour (ESP-IDF v5.x):
 *   - hal_sleep_init() configures GPIO 0 as an ext0 RTC wake source.
 *   - hal_sleep_enter() optionally sets esp_sleep_enable_timer_wakeup(),
 *     then calls esp_deep_sleep_start() — execution does NOT return.
 *   - On wake the chip boots from reset; caller is responsible for
 *     identifying the wake reason via esp_sleep_get_wakeup_cause().
 *
 * Stub policy: all functions compile cleanly without ESP-IDF headers.
 */

#include "hal_sleep.h"

static uint8_t s_initialized;

hal_sleep_err_t hal_sleep_init(void)
{
    s_initialized = 1u;
    /*
     * TODO (ESP-IDF wiring):
     *   esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);  // active-low
     */
    return HAL_SLEEP_OK;
}

hal_sleep_err_t hal_sleep_enter(uint32_t timeout_sec)
{
    if (!s_initialized) {
        return HAL_SLEEP_ERR_INIT;
    }
    (void)timeout_sec;
    /*
     * TODO (ESP-IDF wiring):
     *   if (timeout_sec > 0) {
     *       esp_sleep_enable_timer_wakeup((uint64_t)timeout_sec * 1000000ULL);
     *   }
     *   esp_deep_sleep_start();  // does not return on target
     */
    return HAL_SLEEP_OK;
}

void hal_sleep_deinit(void)
{
    s_initialized = 0u;
    /*
     * TODO (ESP-IDF wiring):
     *   esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
     */
}
