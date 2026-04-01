/**
 * hal_audio.c — Hardware Abstraction Layer: Audio / Piezo Buzzer (target stub)
 *
 * This translation unit is the TARGET implementation linked into the ESP-IDF
 * firmware image. It is NOT compiled on the host — mock_hal_audio.c is used
 * there instead.
 *
 * Target behaviour (ESP-IDF v5.x):
 *   - Enables Audio_PWR_PIN (GPIO 42) and PA_EN (GPIO 46) on init.
 *   - Configures one LEDC timer (LEDC_TIMER_0, 1 kHz base) and channel.
 *   - hal_audio_play() reconfigures the duty cycle for the requested freq_hz.
 *   - A one-shot esp_timer fires after duration_ms to call hal_audio_stop().
 *   - If a second play() arrives before the timer fires, the pending timer is
 *     cancelled (esp_timer_stop + esp_timer_start) before reconfiguring LEDC.
 *
 * Stub policy: all functions compile cleanly without ESP-IDF headers.
 */

#include "hal_audio.h"

static uint8_t s_initialized;

hal_audio_err_t hal_audio_init(void)
{
    s_initialized = 1u;
    /*
     * TODO (ESP-IDF wiring):
     *   gpio_set_direction(42, GPIO_MODE_OUTPUT); gpio_set_level(42, 1);
     *   gpio_set_direction(46, GPIO_MODE_OUTPUT); gpio_set_level(46, 1);
     *   ledc_timer_config_t / ledc_channel_config_t setup.
     */
    return HAL_AUDIO_OK;
}

hal_audio_err_t hal_audio_play(uint16_t freq_hz, uint16_t duration_ms)
{
    if (!s_initialized) {
        return HAL_AUDIO_ERR_INIT;
    }
    if (freq_hz == 0u) {
        return HAL_AUDIO_ERR_INVALID_FREQ;
    }
    (void)duration_ms;
    /*
     * TODO (ESP-IDF wiring):
     *   esp_timer_stop(s_stop_timer);   // cancel any pending stop
     *   ledc_set_freq() + ledc_set_duty() + ledc_update_duty();
     *   if (duration_ms > 0) esp_timer_start_once(s_stop_timer, duration_ms * 1000);
     */
    return HAL_AUDIO_OK;
}

hal_audio_err_t hal_audio_stop(void)
{
    /*
     * Safe before init — no-op if not initialised.
     * TODO (ESP-IDF wiring):
     *   esp_timer_stop(s_stop_timer);
     *   ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
     *   ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
     */
    return HAL_AUDIO_OK;
}

void hal_audio_deinit(void)
{
    s_initialized = 0u;
    /*
     * TODO (ESP-IDF wiring):
     *   ledc_stop(); gpio_set_level(46, 0); gpio_set_level(42, 0);
     */
}
