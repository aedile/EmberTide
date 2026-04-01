/**
 * hal_sleep.h — Hardware Abstraction Layer: Deep Sleep
 *
 * Target: ESP32-S3-PICO-1-N8R8 via ESP-IDF v5.x.
 * Wake sources:
 *   - RTC timer (if timeout_sec > 0)
 *   - External wake-up on GPIO 0 (HAL_BTN_A / BOOT, ext_wakeup_pin_1)
 *
 * Architecture constraint: hal/ is the BOTTOM layer. It MUST NOT be included
 * by game/, presentation/, or connectivity/.
 *
 * This header is host-compilable — no ESP-IDF types appear in the public API.
 * The target implementation calls esp_deep_sleep_start(). On the host the
 * mock (mock_hal_sleep.c) records the request without suspending the process.
 */

#ifndef FIESTAQUEST_HAL_SLEEP_H
#define FIESTAQUEST_HAL_SLEEP_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Return codes for hal_sleep operations.
 * -------------------------------------------------------------------------
 */
typedef enum {
    HAL_SLEEP_OK       = 0, /**< Operation completed successfully. */
    HAL_SLEEP_ERR_INIT = 1  /**< Driver not initialised. */
} hal_sleep_err_t;

/**
 * hal_sleep_init — Configure deep-sleep wake sources.
 *
 * Registers GPIO 0 as an ext0 RTC wake source (active-low) so the device
 * wakes on HAL_BTN_A press regardless of the timer setting.
 *
 * Safe to call multiple times (idempotent reinit).
 *
 * @return HAL_SLEEP_OK on success.
 */
hal_sleep_err_t hal_sleep_init(void);

/**
 * hal_sleep_enter — Flush state and enter deep sleep.
 *
 * On the target: saves any pending state, calls esp_deep_sleep_start().
 * This function does NOT return on the target — execution resumes from
 * reset after wake.
 *
 * On the host mock: records the request and returns immediately.
 *
 * @param timeout_sec  RTC timer wake timeout in seconds. Pass 0 to disable
 *                     the timer and wake on button press only.
 * @return HAL_SLEEP_OK       on success (host mock only — target never returns).
 *         HAL_SLEEP_ERR_INIT if hal_sleep_init() was not called.
 */
hal_sleep_err_t hal_sleep_enter(uint32_t timeout_sec);

/**
 * hal_sleep_deinit — Unregister wake sources and release RTC resources.
 *
 * Safe to call without a preceding init and safe to call multiple times.
 */
void hal_sleep_deinit(void);

#endif /* FIESTAQUEST_HAL_SLEEP_H */
