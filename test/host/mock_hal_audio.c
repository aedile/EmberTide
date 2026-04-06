/**
 * mock_hal_audio.c — Host mock for hal_audio I2S + ES8311 driver.
 *
 * Linked by test/host/ targets instead of components/fq_hal/src/hal_audio.c.
 * Simulates I2S audio output entirely in RAM:
 *   - Tracks initialised/uninitialised state.
 *   - Captures the last freq_hz and duration_ms passed to hal_audio_play()
 *     (with duration clamped to AUDIO_MAX_TONE_MS, as the real driver does).
 *   - Counts total successful hal_audio_play() calls.
 *   - Captures written samples in a static ring-capture buffer.
 *   - Tracks total samples written (cumulative).
 *   - Tracks overflow_flag: set when ring capacity is exceeded.
 *   - mock_audio_reset() clears all state to power-on defaults.
 *
 * No I2S or DMA simulation is performed — the mock is a pure recorder.
 *
 * Phase 21: added hal_audio_write_samples() and extended mock accessors for
 * sample capture, overflow tracking.
 * Phase 23: added hal_audio_get_ring_count() — returns current ring fill level,
 * enabling prefill_audio() tests without real hardware.
 */

#include "hal_audio.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal mock state.
 * -------------------------------------------------------------------------
 */
static uint8_t  s_mock_initialized;
static uint16_t s_mock_last_freq;
static uint16_t s_mock_last_duration_ms;
static uint32_t s_mock_play_count;

/** Captured PCM samples (most recent writes, capped at MOCK_AUDIO_CAPTURE_MAX). */
#define MOCK_AUDIO_CAPTURE_MAX  8192u
static int16_t  s_mock_capture_buf[MOCK_AUDIO_CAPTURE_MAX];

/** Total samples written across all hal_audio_write_samples() calls. */
static uint32_t s_mock_samples_written;

/** 1 if any write_samples call has overflowed the mock ring capacity. */
static uint32_t s_mock_overflow_flag;

/** Running "ring fill" counter — how many samples are currently queued. */
static uint32_t s_mock_ring_fill;

/* -------------------------------------------------------------------------
 * Public hal_audio API — mock implementations.
 * -------------------------------------------------------------------------
 */

hal_audio_err_t hal_audio_init(void)
{
    s_mock_initialized      = 1u;
    s_mock_last_freq        = 0u;
    s_mock_last_duration_ms = 0u;
    s_mock_play_count       = 0u;
    s_mock_samples_written  = 0u;
    s_mock_overflow_flag    = 0u;
    s_mock_ring_fill        = 0u;
    memset(s_mock_capture_buf, 0, sizeof(s_mock_capture_buf));
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

    /* Clamp duration to AUDIO_MAX_TONE_MS as the real driver does. */
    uint16_t clamped = duration_ms;
    if (clamped > (uint16_t)AUDIO_MAX_TONE_MS) {
        clamped = (uint16_t)AUDIO_MAX_TONE_MS;
    }

    s_mock_last_freq        = freq_hz;
    s_mock_last_duration_ms = clamped;
    s_mock_play_count++;
    return HAL_AUDIO_OK;
}

hal_audio_err_t hal_audio_write_samples(const int16_t *buf, size_t count)
{
    if (!s_mock_initialized) {
        return HAL_AUDIO_ERR_INIT;
    }
    if (buf == NULL) {
        return HAL_AUDIO_ERR_NULL;
    }

    hal_audio_err_t result = HAL_AUDIO_OK;

    /* Check if writing 'count' samples would overflow the mock ring.
     * The mock ring capacity mirrors AUDIO_RING_BUF_SAMPLES. */
    uint32_t available = AUDIO_RING_BUF_SAMPLES - s_mock_ring_fill;
    if (count > (size_t)available) {
        /* Overflow: record the flag and only write what fits. */
        s_mock_overflow_flag = 1u;
        result = HAL_AUDIO_ERR_OVERFLOW;
        count  = (size_t)available;   /* Newest samples dropped; write what fits. */
    }

    /* Copy up to MOCK_AUDIO_CAPTURE_MAX samples for test inspection. */
    if (count > 0u) {
        size_t capture_avail = MOCK_AUDIO_CAPTURE_MAX
                               - (s_mock_samples_written % MOCK_AUDIO_CAPTURE_MAX);
        size_t copy_count    = (count < capture_avail) ? count : capture_avail;
        size_t offset        = s_mock_samples_written % MOCK_AUDIO_CAPTURE_MAX;
        memcpy(&s_mock_capture_buf[offset], buf,
               copy_count * sizeof(int16_t));

        s_mock_samples_written += (uint32_t)count;
        s_mock_ring_fill       += (uint32_t)count;
    }

    return result;
}

/**
 * hal_audio_get_ring_count — Return the current ring buffer fill level.
 *
 * Phase 23 addition. Returns 0 before init (s_mock_ring_fill is 0 after
 * mock_audio_reset). Returns the running fill counter after init and writes.
 * Does not drain — the mock has no consumer task.
 */
uint32_t hal_audio_get_ring_count(void)
{
    return s_mock_ring_fill;
}

hal_audio_err_t hal_audio_stop(void)
{
    /* Safe before init — always returns OK. Flush mock ring. */
    s_mock_ring_fill = 0u;
    return HAL_AUDIO_OK;
}

void hal_audio_deinit(void)
{
    s_mock_initialized = 0u;
    s_mock_ring_fill   = 0u;
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

/** Returns the duration (ms) from the most recent successful play call
 *  (already clamped to AUDIO_MAX_TONE_MS). */
uint16_t mock_audio_get_last_duration_ms(void)
{
    return s_mock_last_duration_ms;
}

/** Returns the cumulative count of successful hal_audio_play() calls. */
uint32_t mock_audio_get_play_count(void)
{
    return s_mock_play_count;
}

/** Returns the cumulative number of int16_t samples written via
 *  hal_audio_write_samples() since the last mock_audio_reset(). */
uint32_t mock_audio_get_samples_written(void)
{
    return s_mock_samples_written;
}

/**
 * mock_audio_get_capture_buf — Returns a pointer to the capture buffer.
 *
 * The capture buffer holds the most recently written samples (up to
 * MOCK_AUDIO_CAPTURE_MAX). Use mock_audio_get_samples_written() to
 * determine how many samples are valid.
 */
const int16_t *mock_audio_get_capture_buf(void)
{
    return s_mock_capture_buf;
}

/**
 * mock_audio_get_overflow_flag — Returns 1 if any write_samples call
 * has produced a ring buffer overflow since the last mock_audio_reset().
 */
uint32_t mock_audio_get_overflow_flag(void)
{
    return s_mock_overflow_flag;
}

/**
 * mock_audio_reset — Reset all mock state to power-on defaults.
 *
 * Clears initialized flag, last freq/duration, play count, sample capture
 * buffer, samples_written counter, overflow_flag, and ring fill counter.
 * Call at the start of each test main() for a clean slate.
 */
void mock_audio_reset(void)
{
    s_mock_initialized      = 0u;
    s_mock_last_freq        = 0u;
    s_mock_last_duration_ms = 0u;
    s_mock_play_count       = 0u;
    s_mock_samples_written  = 0u;
    s_mock_overflow_flag    = 0u;
    s_mock_ring_fill        = 0u;
    memset(s_mock_capture_buf, 0, sizeof(s_mock_capture_buf));
}
