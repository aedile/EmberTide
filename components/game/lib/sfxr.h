/**
 * sfxr.h — Minimal sfxr-c PCM Generator (vendored)
 *
 * A simplified single-file sfxr synthesizer for FiestaQuest retro SFX.
 * Generates 16-bit signed mono PCM samples from a parameter preset struct.
 *
 * Architecture boundary:
 *   - Lives in components/game/lib/ (pure game logic, no HAL dependency).
 *   - This header MUST NOT include any hal_*.h headers.
 *   - Uses float for intermediate synthesis calculations ONLY.
 *     (float is sufficient precision for audio; double avoided because
 *      ESP32-S3 has no hardware FPU for double-precision operations.)
 *   - No malloc. No global state. Pure function — fully re-entrant.
 *
 * Constitution P0: sfxr is NOT part of the combat/PRNG engine. It lives in
 * game/lib/ because SFX presets are game logic constants, but it has no
 * influence on combat determinism.
 *
 * Usage:
 *   sfxr_params_t p = sfxr_preset_btn_press();
 *   int16_t buf[1024];
 *   size_t n = sfxr_generate(&p, buf, 1024);
 */

#ifndef FIESTAQUEST_GAME_LIB_SFXR_H
#define FIESTAQUEST_GAME_LIB_SFXR_H

#include <stdint.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Waveform types.
 * -------------------------------------------------------------------------
 */
typedef enum {
    SFXR_WAVE_SQUARE = 0,  /**< Classic 8-bit square wave. */
    SFXR_WAVE_SAW    = 1,  /**< Saw-tooth wave (bright, harsh). */
    SFXR_WAVE_SINE   = 2,  /**< Sine wave (smooth). */
    SFXR_WAVE_NOISE  = 3   /**< White noise (explosions, hits). */
} sfxr_wave_t;

/* -------------------------------------------------------------------------
 * sfxr_params_t — Full parameter preset for one SFX.
 *
 * Envelope: attack + decay + sustain_level + release (all in [0.0, 1.0]).
 *   - attack    : ramp-up time fraction
 *   - decay     : ramp-down from peak to sustain_level
 *   - sustain   : hold fraction of total duration at sustain_level
 *   - release   : ramp-down from sustain to silence
 *
 * Frequency: base_freq in Hz (integer), slide in semitones/second (float).
 *   - slide > 0: pitch rises over time (laser, power-up).
 *   - slide < 0: pitch falls over time (death, hit).
 *   - slide == 0: constant pitch.
 *
 * Duration: total_ms in milliseconds. Clamped to SFXR_MAX_DURATION_MS.
 *
 * Sample rate: always 22050 Hz (matches AUDIO_SAMPLE_RATE_HZ).
 * -------------------------------------------------------------------------
 */

/** Maximum SFX duration in milliseconds (500ms = 11025 samples at 22050Hz).
 *
 * WARNING: This ceiling EXCEEDS AUDIO_RING_BUF_SAMPLES (4096 samples).
 * Preset total_ms values must be kept <= 180ms (= ~3969 samples) to avoid
 * ring buffer overflow. SFXR_MAX_DURATION_MS is the engine clamp, not a
 * safe preset target.
 */
#define SFXR_MAX_DURATION_MS  500u

/** Sample rate (must match AUDIO_SAMPLE_RATE_HZ in hal_audio.h). */
#define SFXR_SAMPLE_RATE_HZ   22050u

typedef struct {
    sfxr_wave_t wave;          /**< Waveform type. */
    float       attack;        /**< Envelope attack  [0.0, 1.0]. */
    float       decay;         /**< Envelope decay   [0.0, 1.0]. */
    float       sustain_level; /**< Sustain amplitude [0.0, 1.0]. */
    float       sustain;       /**< Sustain hold fraction [0.0, 1.0]. */
    float       release;       /**< Envelope release [0.0, 1.0]. */
    uint16_t    base_freq_hz;  /**< Base frequency in Hz. */
    float       freq_slide;    /**< Pitch slide in semitones/second. */
    uint16_t    total_ms;      /**< Total duration in ms (clamped to SFXR_MAX_DURATION_MS). */
} sfxr_params_t;

/* Compile-time struct size guard — catches unexpected layout changes due to
 * field additions, reordering, or platform ABI divergence.
 * Layout: sfxr_wave_t(4) + attack(4) + decay(4) + sustain_level(4) +
 *         sustain(4) + release(4) + base_freq_hz(2) + [2 pad] +
 *         freq_slide(4) + total_ms(2) + [2 pad] = 36 bytes. */
_Static_assert(sizeof(sfxr_params_t) == 36u,
               "sfxr_params_t size changed — update preset table and this assert");

/* -------------------------------------------------------------------------
 * sfxr_generate — Generate PCM samples from a parameter preset.
 *
 * Fills buf[0..n-1] with 16-bit signed PCM samples at 22050 Hz.
 * Returns the number of samples actually written (min(n, computed_samples)).
 *
 * No malloc. No global state. Float intermediate calculations only.
 *
 * @param params  Preset parameters. Must not be NULL.
 * @param buf     Output buffer. Must not be NULL.
 * @param buf_len Maximum samples to write (buf must be >= buf_len * 2 bytes).
 * @return Number of samples written (0 if params or buf is NULL).
 * -------------------------------------------------------------------------
 */
size_t sfxr_generate(const sfxr_params_t *params,
                     int16_t             *buf,
                     size_t               buf_len);

#endif /* FIESTAQUEST_GAME_LIB_SFXR_H */
