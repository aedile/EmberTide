/**
 * sfx.h — FiestaQuest Game Engine: Sound Effects API
 *
 * Defines the fq_sfx_id_t enum (10 presets) and the fq_sfx_play() function
 * that generates PCM samples from sfxr-c presets into a caller-provided buffer.
 *
 * Architecture boundary:
 *   - sfx.h lives in game/include/ — game layer.
 *   - sfxr.h/sfxr.c live in game/lib/ — vendored pure PCM generator.
 *   - This header MUST NOT include any hal_*.h headers.
 *   - fq_sfx_play() returns game_err_t, writes PCM into caller's buffer.
 *   - The APPLICATION LAYER (app_main.c) calls fq_sfx_play(), then calls
 *     hal_audio_write_samples() to push the PCM to the HAL ring buffer.
 *     This preserves the game→main→HAL dependency direction.
 *
 * Quiet mode:
 *   The sfx_enabled flag lives in fq_app_ctx_t (application layer).
 *   The caller checks the flag before calling fq_sfx_play(). This module
 *   has no quiet-mode awareness — single-responsibility preserved.
 *
 * SFX duration constraint:
 *   At 22050 Hz, AUDIO_RING_BUF_SAMPLES == 4096 samples.
 *   Max safe duration = 4096 / 22050 * 1000 ~= 185ms.
 *   ALL presets have total_ms <= 180ms to stay safely within the ring buffer
 *   capacity. SFXR_MAX_DURATION_MS (500ms = 11025 samples) is the sfxr engine
 *   ceiling and EXCEEDS the ring buffer — it must NOT be used as a preset
 *   duration. Preset durations are individually bounded below 185ms.
 */

#ifndef FIESTAQUEST_GAME_SFX_H
#define FIESTAQUEST_GAME_SFX_H

#include <stdint.h>
#include <stddef.h>
#include "types.h"

/* -------------------------------------------------------------------------
 * fq_sfx_id_t — Sound effect identifiers.
 *
 * Values are pinned — new IDs append before SFX_COUNT.
 * -------------------------------------------------------------------------
 */
typedef enum {
    SFX_BTN_PRESS      = 0,  /**< Short click: any button press. */
    SFX_BTN_BACK       = 1,  /**< Soft click: back/cancel navigation. */
    SFX_MENU_NAVIGATE  = 2,  /**< Blip: cycling through menu items. */
    SFX_COMBAT_HIT     = 3,  /**< Impact thud: successful hit. */
    SFX_COMBAT_MISS    = 4,  /**< Whoosh: missed attack. */
    SFX_COMBAT_CRIT    = 5,  /**< Rising strike: critical hit. */
    SFX_LEVEL_UP       = 6,  /**< Ascending arpeggio: level gained. */
    SFX_ITEM_EQUIP     = 7,  /**< Equip chime: item equipped. */
    SFX_DEATH          = 8,  /**< Descending tone: character died. */
    SFX_REBIRTH        = 9,  /**< Phoenix chord: rebirth. */
    SFX_COUNT          = 10  /**< Sentinel — number of valid SFX IDs. */
} fq_sfx_id_t;

/**
 * fq_sfx_play — Generate PCM samples for a given SFX preset.
 *
 * Looks up the static preset for @p id, calls sfxr_generate() into @p buf,
 * and writes the sample count into @p samples_out.
 *
 * The caller (application layer) is responsible for passing the resulting
 * samples to hal_audio_write_samples().
 *
 * @param id          SFX identifier. Must be < SFX_COUNT.
 * @param buf         Output buffer for int16_t PCM samples. Must not be NULL.
 * @param buf_len     Maximum samples to write (capacity of buf).
 * @param samples_out Written with the number of samples generated. Must not be NULL.
 *
 * @return GAME_OK on success.
 *         GAME_ERR_INVALID   if id >= SFX_COUNT.
 *         GAME_ERR_NULL_PTR  if buf or samples_out is NULL.
 */
game_err_t fq_sfx_play(fq_sfx_id_t  id,
                        int16_t     *buf,
                        size_t       buf_len,
                        size_t      *samples_out);

#endif /* FIESTAQUEST_GAME_SFX_H */
