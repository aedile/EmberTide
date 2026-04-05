/**
 * music.h — FiestaQuest Game Engine: MOD Music Playback API
 *
 * Wraps the micromod renderer with a simple play/stop/render interface.
 * Music state (current track, playback position) is maintained internally
 * in a static context — callers do not need to manage micromod_ctx_t directly.
 *
 * Architecture boundary:
 *   - Lives in components/game/include/ — game layer.
 *   - MUST NOT include hal_*.h headers.
 *   - The APPLICATION LAYER (app_main.c) calls fq_music_render() to obtain
 *     PCM samples, mixes them with SFX, and calls hal_audio_write_samples()
 *     to push the combined output to the HAL ring buffer.
 *   - This preserves the game -> main -> HAL dependency direction.
 *
 * Volume:
 *   music_vol [0, 255]. Scaling: (int32_t)sample * music_vol / 256.
 *   Volume 255 ≈ 99.6% of full scale — documented as max, not unity.
 *
 * PRNG isolation:
 *   This module uses tick_count (caller-provided entropy) for track
 *   selection — it NEVER touches the combat PRNG (Constitution Priority 0).
 *
 * Constitution Priority 0: No floating point. micromod uses integer-only math.
 */

#ifndef FIESTAQUEST_GAME_MUSIC_H
#define FIESTAQUEST_GAME_MUSIC_H

#include <stdint.h>
#include <stddef.h>
#include "types.h"

/* -------------------------------------------------------------------------
 * Return codes
 * -------------------------------------------------------------------------
 */
typedef enum {
    FQ_MUSIC_OK             = 0, /**< Success. */
    FQ_MUSIC_ERR_NULL       = 1, /**< NULL pointer argument. */
    FQ_MUSIC_ERR_FORMAT     = 2, /**< Invalid or truncated MOD data. */
    FQ_MUSIC_ERR_TOO_LARGE  = 3, /**< Data exceeds MAX_MOD_FILE_SIZE. */
    FQ_MUSIC_ERR_NOT_INIT   = 4, /**< fq_music_init() not called. */
    FQ_MUSIC_ERR_LOOP_GUARD = 5  /**< Pattern loop guard triggered. */
} fq_music_err_t;

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------
 */

/**
 * fq_music_init — Load a MOD file into the player.
 *
 * Parses the .mod buffer, validates the header, and prepares the player
 * for rendering. The caller retains ownership of mod_data — it must remain
 * valid until fq_music_stop() is called.
 *
 * Does NOT start playback — call fq_music_play() after init.
 *
 * @param mod_data Pointer to .mod file bytes. Must not be NULL.
 * @param len      Size of mod_data in bytes.
 * @return FQ_MUSIC_OK on success, error code on failure.
 */
fq_music_err_t fq_music_init(const uint8_t *mod_data, size_t len);

/**
 * fq_music_play — Start background playback.
 *
 * Sets the playing flag. Rendering begins on the next fq_music_render() call.
 * Idempotent: calling play() when already playing is a no-op (returns OK).
 *
 * @return FQ_MUSIC_OK on success, FQ_MUSIC_ERR_NOT_INIT if not initialised.
 */
fq_music_err_t fq_music_play(void);

/**
 * fq_music_stop — Stop playback and release the MOD data reference.
 *
 * Clears the playing flag and the internal mod_data pointer. After this call
 * fq_music_render() returns silence (zero-filled).
 * Safe to call when not playing (no-op).
 *
 * @return FQ_MUSIC_OK always.
 */
fq_music_err_t fq_music_stop(void);

/**
 * fq_music_render — Render next N PCM samples scaled by volume.
 *
 * If not playing or not initialised, fills buf with zeros (silence).
 * Applies volume scaling: output[i] = (micromod_sample * music_vol) / 256.
 * Uses int32 intermediate to prevent overflow before clamping to int16 range.
 *
 * This function is the sole producer of music PCM. It is called from the
 * main loop task ONLY (SPSC contract preserved).
 *
 * @param buf       Output buffer for int16_t PCM. Must not be NULL.
 * @param count     Number of samples to render.
 * @param music_vol Volume [0, 255].
 * @return FQ_MUSIC_OK on success.
 *         FQ_MUSIC_ERR_NULL if buf is NULL.
 *         FQ_MUSIC_ERR_LOOP_GUARD if the pattern loop guard triggered
 *           (output filled with zeros for this call).
 */
fq_music_err_t fq_music_render(int16_t *buf, size_t count, uint8_t music_vol);

/**
 * fq_music_is_playing — Returns 1 if music is currently playing, 0 otherwise.
 */
uint8_t fq_music_is_playing(void);

#endif /* FIESTAQUEST_GAME_MUSIC_H */
