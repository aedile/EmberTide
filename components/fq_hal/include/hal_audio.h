/**
 * hal_audio.h — Hardware Abstraction Layer: Audio / I2S + ES8311 Codec
 *
 * Target: ESP32-S3-PICO-1-N8R8 via ESP-IDF v5.x.
 * Hardware: ES8311 codec driven by I2S (MCLK=GPIO14, BCLK=GPIO15,
 *   WS=GPIO38, DOUT=GPIO45, DIN=GPIO16). I2C address 0x18 (ASEL=LOW).
 *   Audio power rail: GPIO42 (Audio_PWR_PIN, active-LOW).
 *   Speaker amplifier enable: GPIO46 (PA_EN, managed by codec_board).
 *
 * Architecture constraint: hal/ is the BOTTOM layer. It MUST NOT be included
 * by game/, presentation/, or connectivity/.
 *
 * This header is host-compilable — no ESP-IDF types appear in the public API.
 * The target implementation uses codec_board + esp_codec_dev + I2S driver.
 * The host mock (mock_hal_audio.c) captures calls for testing.
 *
 * Audio format: 16-bit signed mono PCM at 22050 Hz.
 * Ring buffer: AUDIO_RING_BUF_SAMPLES samples of internal SRAM (DMA-safe).
 * The drain task expands mono → stereo before writing to the codec.
 *
 * Phase 21: LEDC/PWM replaced by I2S + ES8311.
 *   hal_audio_play()         generates a square wave into the ring buffer.
 *   hal_audio_write_samples() accepts raw PCM (from sfxr-c / micromod) into ring.
 *   Duration clamped to AUDIO_MAX_TONE_MS to prevent ring buffer overflow.
 *
 * Phase 23: Ring buffer enlarged to 44100 samples (2 seconds at 22050 Hz).
 *   hal_audio_get_ring_count() accessor added — used by prefill_audio() to
 *   determine when the ring is full enough before blocking e-paper flushes.
 *   Real ES8311 codec wiring via codec_board component behind CONFIG_BSP_AUDIO.
 *   Audio init moved BEFORE e-paper init (GDMA channel allocation order matters).
 *
 * Error code ABI: values are pinned — new codes MUST be appended after existing.
 *   HAL_AUDIO_OK              = 0
 *   HAL_AUDIO_ERR_INIT        = 1  (unchanged from Phase 13)
 *   HAL_AUDIO_ERR_INVALID_FREQ = 2 (unchanged from Phase 13)
 *   HAL_AUDIO_ERR_NULL        = 3  (Phase 21 addition)
 *   HAL_AUDIO_ERR_OVERFLOW    = 4  (Phase 21 addition)
 */

#ifndef FIESTAQUEST_HAL_AUDIO_H
#define FIESTAQUEST_HAL_AUDIO_H

#include <stdint.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Audio constants.
 * -------------------------------------------------------------------------
 */

/** Sample rate: 16-bit signed mono PCM at 22050 Hz. */
#define AUDIO_SAMPLE_RATE_HZ    22050u

/**
 * Ring buffer size in int16_t samples.
 *
 * Phase 23: Enlarged from 4096 to 44100 (2 seconds at 22050 Hz).
 * Larger buffer allows prefilling before blocking e-paper flushes (~1.8 s).
 * Static internal SRAM allocation: 44100 * 2 bytes = ~86 KB.
 * This fits in the S3's 512 KB internal SRAM with headroom to spare.
 */
#define AUDIO_RING_BUF_SAMPLES  44100u

/**
 * Maximum tone duration in milliseconds.
 *
 * Clamp applied by hal_audio_play() to prevent a single tone from consuming
 * more ring buffer space than is available in one call.
 */
#define AUDIO_MAX_TONE_MS       2000u

/* -------------------------------------------------------------------------
 * Return codes for hal_audio operations.
 *
 * IMPORTANT: Numeric values MUST NOT change. New codes append only.
 * -------------------------------------------------------------------------
 */
typedef enum {
    HAL_AUDIO_OK               = 0, /**< Operation completed successfully. */
    HAL_AUDIO_ERR_INIT         = 1, /**< Driver not initialised. */
    HAL_AUDIO_ERR_INVALID_FREQ = 2, /**< freq_hz == 0 is invalid. */
    HAL_AUDIO_ERR_NULL         = 3, /**< NULL pointer argument. */
    HAL_AUDIO_ERR_OVERFLOW     = 4  /**< Ring buffer overflow (samples dropped). */
} hal_audio_err_t;

/* Compile-time ABI pinning — existing values must never be reordered. */
_Static_assert((int)HAL_AUDIO_OK               == 0, "HAL_AUDIO_OK ABI broken");
_Static_assert((int)HAL_AUDIO_ERR_INIT         == 1, "HAL_AUDIO_ERR_INIT ABI broken");
_Static_assert((int)HAL_AUDIO_ERR_INVALID_FREQ == 2, "HAL_AUDIO_ERR_INVALID_FREQ ABI broken");
_Static_assert((int)HAL_AUDIO_ERR_NULL         == 3, "HAL_AUDIO_ERR_NULL ABI broken");
_Static_assert((int)HAL_AUDIO_ERR_OVERFLOW     == 4, "HAL_AUDIO_ERR_OVERFLOW ABI broken");

/**
 * hal_audio_init — Initialise the I2S peripheral, ES8311 codec via
 * codec_board, and audio power rail.
 *
 * Phase 23: Uses factory codec_board component for proven hardware init.
 * Must be called BEFORE hal_epaper_init() — GDMA channel order matters.
 *
 * Safe to call multiple times (idempotent reinit).
 *
 * @return HAL_AUDIO_OK on success, HAL_AUDIO_ERR_INIT on failure.
 */
hal_audio_err_t hal_audio_init(void);

/**
 * hal_audio_play — Generate a square-wave tone at the given frequency.
 *
 * Synthesizes a square-wave burst of (clamped) duration into the ring buffer
 * asynchronously. Duration is clamped to AUDIO_MAX_TONE_MS.
 *
 * @param freq_hz     Frequency in Hz. Must be > 0; returns
 *                    HAL_AUDIO_ERR_INVALID_FREQ otherwise.
 * @param duration_ms Duration in milliseconds. Clamped to AUDIO_MAX_TONE_MS.
 * @return HAL_AUDIO_OK               on success.
 *         HAL_AUDIO_ERR_INIT         if hal_audio_init() was not called.
 *         HAL_AUDIO_ERR_INVALID_FREQ if freq_hz == 0.
 */
hal_audio_err_t hal_audio_play(uint16_t freq_hz, uint16_t duration_ms);

/**
 * hal_audio_write_samples — Write raw 16-bit PCM samples into the ring buffer.
 *
 * Non-blocking. If the ring buffer does not have enough space for all samples,
 * the NEWEST samples are dropped (ring buffer overflow) and
 * HAL_AUDIO_ERR_OVERFLOW is returned. The audio task plays samples in order.
 *
 * @param buf   Pointer to int16_t sample buffer. Must not be NULL.
 * @param count Number of samples to write from buf.
 * @return HAL_AUDIO_OK               if all samples were queued.
 *         HAL_AUDIO_ERR_INIT         if hal_audio_init() was not called.
 *         HAL_AUDIO_ERR_NULL         if buf is NULL.
 *         HAL_AUDIO_ERR_OVERFLOW     if ring buffer was full (samples dropped).
 */
hal_audio_err_t hal_audio_write_samples(const int16_t *buf, size_t count);

/**
 * hal_audio_get_ring_count — Return the current number of samples queued in
 * the ring buffer.
 *
 * Thread-safe read of the volatile ring fill counter. Returns 0 if the driver
 * has not been initialised. Used by prefill_audio() to determine how full the
 * ring is before blocking the main loop for e-paper flushes.
 *
 * Phase 23 addition.
 *
 * @return Number of samples currently in the ring buffer [0..AUDIO_RING_BUF_SAMPLES].
 */
uint32_t hal_audio_get_ring_count(void);

/**
 * hal_audio_stop — Stop any currently playing tone immediately.
 *
 * Flushes the ring buffer. Safe to call when no tone is playing, and safe
 * before init (no-op).
 *
 * @return HAL_AUDIO_OK always.
 */
hal_audio_err_t hal_audio_stop(void);

/**
 * hal_audio_deinit — Release I2S, codec, and GPIO resources.
 *
 * Signals the audio task to stop and waits for termination. Powers down
 * Audio_PWR_PIN. Safe to call without a preceding init and safe to call
 * multiple times.
 */
void hal_audio_deinit(void);

#endif /* FIESTAQUEST_HAL_AUDIO_H */
