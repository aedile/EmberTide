/**
 * hal_audio.h — Hardware Abstraction Layer: Audio / Piezo Buzzer
 *
 * Target: ESP32-S3-PICO-1-N8R8 via ESP-IDF v5.x.
 * Hardware: ES8311 codec driven by I2S (MCLK=GPIO14, SCLK=GPIO15,
 *   LRCK=GPIO16, DIN=GPIO38). Audio power rail: GPIO42 (Audio_PWR_PIN).
 *   Speaker amplifier enable: GPIO46 (PA_EN).
 *
 * Architecture constraint: hal/ is the BOTTOM layer. It MUST NOT be included
 * by game/, presentation/, or connectivity/.
 *
 * This header is host-compilable — no ESP-IDF types appear in the public API.
 * The target implementation uses the LEDC (PWM) peripheral to generate
 * square waves. The host mock (mock_hal_audio.c) captures calls for testing.
 *
 * Overlapping tone policy (spec-challenger requirement):
 *   If hal_audio_play() is called while a tone is already playing, the
 *   implementation MUST cancel the pending stop timer and reconfigure the
 *   LEDC channel before starting the new timer. This prevents multiple
 *   hardware timers from fighting over the LEDC configuration.
 */

#ifndef FIESTAQUEST_HAL_AUDIO_H
#define FIESTAQUEST_HAL_AUDIO_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Return codes for hal_audio operations.
 * -------------------------------------------------------------------------
 */
typedef enum {
    HAL_AUDIO_OK               = 0, /**< Operation completed successfully. */
    HAL_AUDIO_ERR_INIT         = 1, /**< Driver not initialised. */
    HAL_AUDIO_ERR_INVALID_FREQ = 2  /**< freq_hz == 0 is invalid. */
} hal_audio_err_t;

/**
 * hal_audio_init — Initialise the LEDC peripheral and audio power rail.
 *
 * Enables Audio_PWR_PIN (GPIO 42) and PA_EN (GPIO 46), configures one
 * LEDC timer and channel for square-wave output.
 *
 * Safe to call multiple times (idempotent reinit).
 *
 * @return HAL_AUDIO_OK on success.
 */
hal_audio_err_t hal_audio_init(void);

/**
 * hal_audio_play — Start a square-wave tone at the given frequency.
 *
 * Non-blocking: configures the LEDC duty cycle and starts a one-shot
 * esp_timer to call hal_audio_stop() after @p duration_ms milliseconds.
 * If a tone is already playing, the pending timer is cancelled and the
 * new frequency overrides it.
 *
 * @param freq_hz     Frequency in Hz. Must be > 0; returns
 *                    HAL_AUDIO_ERR_INVALID_FREQ otherwise.
 * @param duration_ms Duration in milliseconds. 0 means play indefinitely
 *                    until hal_audio_stop() is called explicitly.
 * @return HAL_AUDIO_OK               on success.
 *         HAL_AUDIO_ERR_INIT         if hal_audio_init() was not called.
 *         HAL_AUDIO_ERR_INVALID_FREQ if freq_hz == 0.
 */
hal_audio_err_t hal_audio_play(uint16_t freq_hz, uint16_t duration_ms);

/**
 * hal_audio_stop — Stop any currently playing tone immediately.
 *
 * Cancels the pending stop timer and sets the LEDC duty cycle to 0.
 * Safe to call when no tone is playing, and safe before init (no-op).
 *
 * @return HAL_AUDIO_OK always.
 */
hal_audio_err_t hal_audio_stop(void);

/**
 * hal_audio_deinit — Release LEDC and GPIO resources.
 *
 * Powers down Audio_PWR_PIN and PA_EN. Safe to call without a preceding
 * init and safe to call multiple times.
 */
void hal_audio_deinit(void);

#endif /* FIESTAQUEST_HAL_AUDIO_H */
