/**
 * sfxr.c — Minimal sfxr-c PCM Generator Implementation (vendored)
 *
 * Simplified sfxr synthesizer for FiestaQuest retro SFX.
 * Generates 16-bit signed PCM samples from sfxr_params_t presets.
 *
 * Algorithm:
 *   1. Compute total sample count from total_ms (clamped to SFXR_MAX_DURATION_MS).
 *   2. For each sample i:
 *      a. Compute envelope gain via ADSR phases.
 *      b. Compute pitch with linear slide.
 *      c. Generate waveform sample at current phase.
 *      d. Apply gain, scale to int16_t range, write to buf.
 *
 * Float is used for all intermediate calculations.
 * No malloc. No global mutable state.
 *
 * Architecture boundary: MUST NOT include hal_*.h.
 */

#include "sfxr.h"
#include <stddef.h>

/* -------------------------------------------------------------------------
 * sfxr_generate
 * -------------------------------------------------------------------------
 */
size_t sfxr_generate(const sfxr_params_t *params,
                     int16_t             *buf,
                     size_t               buf_len)
{
    if (params == NULL || buf == NULL || buf_len == 0u) {
        return 0u;
    }

    /* Clamp total duration. */
    uint16_t ms = params->total_ms;
    if (ms > (uint16_t)SFXR_MAX_DURATION_MS) {
        ms = (uint16_t)SFXR_MAX_DURATION_MS;
    }
    if (ms == 0u) {
        ms = 1u; /* Minimum 1ms to avoid zero-sample outputs. */
    }

    /* Compute total sample count. */
    size_t total = ((size_t)SFXR_SAMPLE_RATE_HZ * (size_t)ms) / 1000u;
    if (total == 0u) { total = 1u; }
    if (total > buf_len) { total = buf_len; }

    /* Clamp envelope fractions to valid range. */
    float attack       = (params->attack       < 0.0f) ? 0.0f :
                         (params->attack       > 1.0f) ? 1.0f : params->attack;
    float decay        = (params->decay        < 0.0f) ? 0.0f :
                         (params->decay        > 1.0f) ? 1.0f : params->decay;
    float sustain_lvl  = (params->sustain_level < 0.0f) ? 0.0f :
                         (params->sustain_level > 1.0f) ? 1.0f : params->sustain_level;
    float sustain      = (params->sustain      < 0.0f) ? 0.0f :
                         (params->sustain      > 1.0f) ? 1.0f : params->sustain;
    float release      = (params->release      < 0.0f) ? 0.0f :
                         (params->release      > 1.0f) ? 1.0f : params->release;

    /* Normalise: scale envelope phases by total samples. */
    float n       = (float)total;
    float t_atk   = attack  * n;
    float t_dec   = decay   * n;
    float t_sus   = sustain * n;
    float t_rel   = release * n;

    /* Noise state (simple LCG — not used outside sfxr). */
    uint32_t noise_seed = 0x5F3759DFu;

    /* Phase accumulator for oscillator (0.0 to 1.0). */
    float phase = 0.0f;

    /* Slide: convert semitones/second to frequency multiplier per sample.
     * 1 semitone = 2^(1/12) ~= 1.0594631.
     * slide_per_sample = freq_slide / (12 * SFXR_SAMPLE_RATE_HZ)
     * We accumulate total semitones elapsed and compute the multiplier. */
    float slide_semitones_per_sample = params->freq_slide /
                                       ((float)SFXR_SAMPLE_RATE_HZ);
    float semitones_elapsed = 0.0f;

    float base_freq = (params->base_freq_hz > 0u)
                      ? (float)params->base_freq_hz
                      : 440.0f;

    for (size_t i = 0u; i < total; i++) {
        float fi = (float)i;

        /* --- Envelope --- */
        float env;
        if (fi < t_atk) {
            /* Attack: linear ramp 0 → 1. */
            env = (t_atk > 0.0f) ? (fi / t_atk) : 1.0f;
        } else if (fi < t_atk + t_dec) {
            /* Decay: linear ramp 1 → sustain_lvl. */
            float dt = t_dec > 0.0f ? (fi - t_atk) / t_dec : 1.0f;
            env = 1.0f - dt * (1.0f - sustain_lvl);
        } else if (fi < t_atk + t_dec + t_sus) {
            /* Sustain: hold. */
            env = sustain_lvl;
        } else {
            /* Release: linear ramp sustain_lvl → 0. */
            float rel_elapsed = fi - (t_atk + t_dec + t_sus);
            float dt = (t_rel > 0.0f) ? (rel_elapsed / t_rel) : 1.0f;
            float val = sustain_lvl * (1.0f - dt);
            env = (val < 0.0f) ? 0.0f : val;
        }

        /* --- Pitch slide --- */
        semitones_elapsed += slide_semitones_per_sample;
        /* freq = base_freq * 2^(semitones_elapsed/12) */
        /* Approximate 2^x using: 2^x ~ 1 + x*0.693147 for |x| < 1.
         * For larger slides use multi-step approximation. */
        float exponent = semitones_elapsed / 12.0f;
        /* Clamp exponent to reasonable range to avoid divergence. */
        if (exponent >  4.0f) { exponent =  4.0f; }
        if (exponent < -4.0f) { exponent = -4.0f; }
        /* pow2 approximation: 2^x = e^(x * ln2).
         * Use the series: e^y ~ 1 + y + y^2/2 + y^3/6 for |y| < 3. */
        float y     = exponent * 0.6931472f; /* x * ln(2) */
        float pow2  = 1.0f + y + (y*y)*0.5f + (y*y*y)*(1.0f/6.0f);
        /* Clamp to positive. */
        if (pow2 < 0.0625f) { pow2 = 0.0625f; }

        float freq = base_freq * pow2;

        /* --- Phase accumulator --- */
        float phase_inc = freq / (float)SFXR_SAMPLE_RATE_HZ;
        phase += phase_inc;
        /* Wrap phase to [0, 1). */
        if (phase >= 1.0f) { phase -= 1.0f; }

        /* --- Waveform --- */
        float sample;
        switch (params->wave) {
            case SFXR_WAVE_SQUARE:
                sample = (phase < 0.5f) ? 1.0f : -1.0f;
                break;

            case SFXR_WAVE_SAW:
                sample = 2.0f * phase - 1.0f;
                break;

            case SFXR_WAVE_SINE: {
                /* Bhaskara I sine approximation: good to ~0.2% over [0, pi].
                 * sin(x) ~ (16x(pi-x)) / (5pi^2 - 4x(pi-x)) for x in [0, pi].
                 * Map phase [0, 1] to x in [0, 2*pi]. */
                float x = phase; /* [0, 1) */
                /* Mirror for second half. */
                float sign;
                float xp;
                if (x < 0.5f) {
                    xp   = x * 2.0f;   /* [0, 1) */
                    sign = 1.0f;
                } else {
                    xp   = (x - 0.5f) * 2.0f; /* [0, 1) */
                    sign = -1.0f;
                }
                /* xp in [0, 1] maps to radians [0, pi]. */
                float pi_f = 3.14159265f;
                float rad  = xp * pi_f;
                float num  = 16.0f * rad * (pi_f - rad);
                float den  = 5.0f * pi_f * pi_f - 4.0f * rad * (pi_f - rad);
                sample     = sign * (num / den);
                break;
            }

            case SFXR_WAVE_NOISE:
            default: {
                /* LCG white noise — not seeded from combat PRNG. */
                noise_seed = noise_seed * 1664525u + 1013904223u;
                /* Map [0, UINT32_MAX] → [-1.0, 1.0]. */
                sample = ((float)(int32_t)noise_seed) / 2147483648.0f;
                break;
            }
        }

        /* Apply envelope gain. */
        float out = sample * env;

        /* Scale to int16_t range: multiply by 32767 * master_volume (0.75). */
        float scaled = out * 32767.0f * 0.75f;

        /* Clamp to int16_t. */
        if (scaled >  32767.0f) { scaled =  32767.0f; }
        if (scaled < -32768.0f) { scaled = -32768.0f; }

        buf[i] = (int16_t)scaled;
    }

    return total;
}
