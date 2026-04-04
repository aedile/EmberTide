/**
 * sfx.c — FiestaQuest SFX Preset Table + Generation Glue
 *
 * Implements fq_sfx_play(): looks up a static const sfxr_params_t preset
 * for each fq_sfx_id_t and calls sfxr_generate() into the caller's buffer.
 *
 * Architecture boundary:
 *   - This file is game layer. MUST NOT include hal_*.h.
 *   - sfxr.h is a pure PCM generator — also no hal dependency.
 *   - Preset table is static const — zero runtime allocation.
 *
 * Preset design goals (retro 8-bit feel):
 *   SFX_BTN_PRESS     — Short square pop (short attack, fast decay).
 *   SFX_BTN_BACK      — Softer square click (lower freq, slightly longer).
 *   SFX_MENU_NAVIGATE — Tiny blip (high freq square, very short).
 *   SFX_COMBAT_HIT    — Noise burst with short decay (impact).
 *   SFX_COMBAT_MISS   — Falling saw (freq slides down).
 *   SFX_COMBAT_CRIT   — Rising square (freq slides up sharply).
 *   SFX_LEVEL_UP      — Ascending square (two-note implied by slow rise).
 *   SFX_ITEM_EQUIP    — Mid sine chime.
 *   SFX_DEATH         — Low noise with long descending slide.
 *   SFX_REBIRTH       — Rising sine burst (ethereal).
 */

#include "sfx.h"
#include "../lib/sfxr.h"

/* -------------------------------------------------------------------------
 * Static preset table — zero runtime allocation.
 *
 * Envelope fractions (attack + decay + sustain + release) do not need to
 * sum to exactly 1.0. Phases beyond the total sample count are silent.
 * -------------------------------------------------------------------------
 */
static const sfxr_params_t k_sfx_presets[SFX_COUNT] = {

    /* SFX_BTN_PRESS (0) — short square pop */
    [SFX_BTN_PRESS] = {
        .wave          = SFXR_WAVE_SQUARE,
        .attack        = 0.0f,
        .decay         = 0.2f,
        .sustain_level = 0.6f,
        .sustain       = 0.05f,
        .release       = 0.15f,
        .base_freq_hz  = 880u,
        .freq_slide    = 0.0f,
        .total_ms      = 60u
    },

    /* SFX_BTN_BACK (1) — softer lower click */
    [SFX_BTN_BACK] = {
        .wave          = SFXR_WAVE_SQUARE,
        .attack        = 0.0f,
        .decay         = 0.15f,
        .sustain_level = 0.4f,
        .sustain       = 0.05f,
        .release       = 0.2f,
        .base_freq_hz  = 440u,
        .freq_slide    = -2.0f,
        .total_ms      = 80u
    },

    /* SFX_MENU_NAVIGATE (2) — tiny blip */
    [SFX_MENU_NAVIGATE] = {
        .wave          = SFXR_WAVE_SQUARE,
        .attack        = 0.0f,
        .decay         = 0.3f,
        .sustain_level = 0.5f,
        .sustain       = 0.0f,
        .release       = 0.1f,
        .base_freq_hz  = 1320u,
        .freq_slide    = 0.0f,
        .total_ms      = 40u
    },

    /* SFX_COMBAT_HIT (3) — noise burst impact */
    [SFX_COMBAT_HIT] = {
        .wave          = SFXR_WAVE_NOISE,
        .attack        = 0.0f,
        .decay         = 0.4f,
        .sustain_level = 0.2f,
        .sustain       = 0.1f,
        .release       = 0.2f,
        .base_freq_hz  = 200u,
        .freq_slide    = -8.0f,
        .total_ms      = 120u
    },

    /* SFX_COMBAT_MISS (4) — falling saw whoosh */
    [SFX_COMBAT_MISS] = {
        .wave          = SFXR_WAVE_SAW,
        .attack        = 0.0f,
        .decay         = 0.1f,
        .sustain_level = 0.7f,
        .sustain       = 0.4f,
        .release       = 0.3f,
        .base_freq_hz  = 660u,
        .freq_slide    = -12.0f,
        .total_ms      = 150u
    },

    /* SFX_COMBAT_CRIT (5) — rising square strike */
    [SFX_COMBAT_CRIT] = {
        .wave          = SFXR_WAVE_SQUARE,
        .attack        = 0.0f,
        .decay         = 0.1f,
        .sustain_level = 0.8f,
        .sustain       = 0.2f,
        .release       = 0.3f,
        .base_freq_hz  = 440u,
        .freq_slide    = 18.0f,
        .total_ms      = 180u
    },

    /* SFX_LEVEL_UP (6) — ascending square arpeggio feel */
    [SFX_LEVEL_UP] = {
        .wave          = SFXR_WAVE_SQUARE,
        .attack        = 0.05f,
        .decay         = 0.2f,
        .sustain_level = 0.7f,
        .sustain       = 0.4f,
        .release       = 0.2f,
        .base_freq_hz  = 330u,
        .freq_slide    = 24.0f,
        .total_ms      = 400u
    },

    /* SFX_ITEM_EQUIP (7) — mid sine chime */
    [SFX_ITEM_EQUIP] = {
        .wave          = SFXR_WAVE_SINE,
        .attack        = 0.1f,
        .decay         = 0.2f,
        .sustain_level = 0.6f,
        .sustain       = 0.3f,
        .release       = 0.3f,
        .base_freq_hz  = 660u,
        .freq_slide    = 6.0f,
        .total_ms      = 200u
    },

    /* SFX_DEATH (8) — low noise descending */
    [SFX_DEATH] = {
        .wave          = SFXR_WAVE_NOISE,
        .attack        = 0.05f,
        .decay         = 0.3f,
        .sustain_level = 0.5f,
        .sustain       = 0.3f,
        .release       = 0.5f,
        .base_freq_hz  = 110u,
        .freq_slide    = -6.0f,
        .total_ms      = 500u
    },

    /* SFX_REBIRTH (9) — rising sine burst */
    [SFX_REBIRTH] = {
        .wave          = SFXR_WAVE_SINE,
        .attack        = 0.15f,
        .decay         = 0.1f,
        .sustain_level = 0.9f,
        .sustain       = 0.4f,
        .release       = 0.35f,
        .base_freq_hz  = 220u,
        .freq_slide    = 30.0f,
        .total_ms      = 500u
    }
};

/* -------------------------------------------------------------------------
 * fq_sfx_play
 * -------------------------------------------------------------------------
 */
game_err_t fq_sfx_play(fq_sfx_id_t  id,
                        int16_t     *buf,
                        size_t       buf_len,
                        size_t      *samples_out)
{
    if (id >= SFX_COUNT) {
        return GAME_ERR_INVALID;
    }
    if (buf == NULL || samples_out == NULL) {
        return GAME_ERR_NULL_PTR;
    }

    *samples_out = sfxr_generate(&k_sfx_presets[id], buf, buf_len);
    return GAME_OK;
}
