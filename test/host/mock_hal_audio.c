/**
 * mock_hal_audio.c — Host mock for hal_audio piezo driver.
 *
 * Linked by test/host/ targets instead of components/hal/src/hal_audio.c.
 * Simulates audio tone playback entirely in RAM:
 *   - Tracks initialised/uninitialised state.
 *   - Captures the last freq_hz and duration_ms passed to hal_audio_play().
 *   - Counts total successful hal_audio_play() calls.
 *   - mock_audio_reset() clears all state to power-on defaults.
 *
 * No timer or PWM simulation is performed — the mock is a pure recorder.
 * Overlapping calls (play before previous duration expires) increment the
 * play_count and overwrite last_freq / last_duration_ms, which is the
 * correct mock-level representation of the "resync" behaviour.
 */

#include "hal_audio.h"

/* -------------------------------------------------------------------------
 * Internal mock state.
 * -------------------------------------------------------------------------
 */
static uint8_t  s_mock_initialized;
static uint16_t s_mock_last_freq;
static uint16_t s_mock_last_duration_ms;
static uint32_t s_mock_play_count;

/* -------------------------------------------------------------------------
 * Public hal_audio API — mock implementations.
 * -------------------------------------------------------------------------
 */

hal_audio_err_t hal_audio_init(void)
{
    s_mock_initialized    = 1u;
    s_mock_last_freq      = 0u;
    s_mock_last_duration_ms = 0u;
    s_mock_play_count     = 0u;
    return HAL_AUDIO_OK;
}

hal_audio_err_t hal_audio_play(uint16_t freq_hz, uint16_t duration_ms)
{
    if (!s_mock_initialized) {
        return HAL_AUDIO_ERR_INIT;
    }
    if (freq_hz == 0u) {
        return HAL_AUDIO_ERR_INVALID_FREQ;
    }
    s_mock_last_freq        = freq_hz;
    s_mock_last_duration_ms = duration_ms;
    s_mock_play_count++;
    return HAL_AUDIO_OK;
}

hal_audio_err_t hal_audio_stop(void)
{
    /* Safe before init — always returns OK. */
    return HAL_AUDIO_OK;
}

void hal_audio_deinit(void)
{
    s_mock_initialized = 0u;
    /* Capture state intentionally preserved for post-deinit inspection. */
}

/* -------------------------------------------------------------------------
 * Test accessor functions — host-only, not declared in hal_audio.h.
 * -------------------------------------------------------------------------
 */

/** Returns the frequency (Hz) from the most recent successful play call. */
uint16_t mock_audio_get_last_freq(void)
{
    return s_mock_last_freq;
}

/** Returns the duration (ms) from the most recent successful play call. */
uint16_t mock_audio_get_last_duration_ms(void)
{
    return s_mock_last_duration_ms;
}

/** Returns the cumulative count of successful hal_audio_play() calls. */
uint32_t mock_audio_get_play_count(void)
{
    return s_mock_play_count;
}

/**
 * mock_audio_reset — Reset all mock state to power-on defaults.
 *
 * Clears initialized flag, last freq/duration, and play count.
 * Call at the start of each test main() for a clean slate.
 */
void mock_audio_reset(void)
{
    s_mock_initialized      = 0u;
    s_mock_last_freq        = 0u;
    s_mock_last_duration_ms = 0u;
    s_mock_play_count       = 0u;
}
