/**
 * music.h — FiestaQuest Game Engine: MOD Music Playback API
 *
 * Wraps the micromod renderer with a simple play/stop/render interface.
 * All music state is stored in a caller-owned fq_music_ctx_t struct.
 * The caller allocates the context (e.g. as a static in app_main.c) and
 * passes it as the first argument to every API function.
 *
 * Architecture boundary:
 *   - Lives in components/game/include/ — game layer.
 *   - MUST NOT include hal_*.h headers.
 *   - The APPLICATION LAYER (app_main.c) allocates fq_music_ctx_t as a
 *     file-scope static (mutable statics are permitted in main/).
 *   - Callers pass a pointer to their fq_music_ctx_t on every call.
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
#include "../lib/micromod.h"

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
 * fq_music_ctx_t — Caller-owned music player context.
 *
 * Callers allocate this struct (e.g. as a file-scope static in app_main.c)
 * and pass a pointer to it on every fq_music_*() call. game/ layer contains
 * no mutable static state.
 *
 * Layout:
 *   mod_ctx      — micromod player state (see micromod.h)
 *   playing      — 1 = playing, 0 = stopped
 *   initialised  — 1 = micromod_init succeeded
 *   _pad[2]      — alignment padding
 * -------------------------------------------------------------------------
 */
typedef struct {
    micromod_ctx_t mod_ctx;      /**< micromod player state (caller-owned). */
    uint8_t        playing;      /**< 1 = playing, 0 = stopped. */
    uint8_t        initialised;  /**< 1 = fq_music_init succeeded. */
    uint8_t        _pad[2];      /**< Padding to 4-byte boundary. */
} fq_music_ctx_t;

/* Compile-time size pin — update if micromod_ctx_t layout changes. */
/* The 4 named payload bytes (playing, initialised, _pad[2]) plus
 * compiler trailing padding round to sizeof(micromod_ctx_t) + 8 bytes.
 * Pin this so a micromod_ctx_t layout change is caught at compile time. */
_Static_assert(sizeof(fq_music_ctx_t) ==
               sizeof(micromod_ctx_t) + 8u,
               "fq_music_ctx_t layout changed — update size assertion");

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------
 */

/**
 * fq_music_init — Load a MOD file into the player context.
 *
 * Parses the .mod buffer, validates the header, and prepares the player
 * for rendering. The caller retains ownership of mod_data — it must remain
 * valid until fq_music_stop() is called.
 *
 * Does NOT start playback — call fq_music_play() after init.
 *
 * @param ctx      Caller-owned music context. Must not be NULL.
 * @param mod_data Pointer to .mod file bytes. Must not be NULL.
 * @param len      Size of mod_data in bytes.
 * @return FQ_MUSIC_OK on success, error code on failure.
 */
fq_music_err_t fq_music_init(fq_music_ctx_t *ctx,
                              const uint8_t  *mod_data,
                              size_t          len);

/**
 * fq_music_play — Start background playback.
 *
 * Sets the playing flag. Rendering begins on the next fq_music_render() call.
 * Idempotent: calling play() when already playing is a no-op (returns OK).
 *
 * @param ctx  Caller-owned music context. Must not be NULL.
 * @return FQ_MUSIC_OK on success, FQ_MUSIC_ERR_NOT_INIT if not initialised.
 */
fq_music_err_t fq_music_play(fq_music_ctx_t *ctx);

/**
 * fq_music_stop — Stop playback and reset the context.
 *
 * Clears the playing and initialised flags and zeroes the mod context.
 * Safe to call when not playing (idempotent).
 *
 * @param ctx  Caller-owned music context. Must not be NULL.
 *             If NULL, this function is a safe no-op.
 * @return FQ_MUSIC_OK always.
 */
fq_music_err_t fq_music_stop(fq_music_ctx_t *ctx);

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
 * @param ctx       Caller-owned music context. Must not be NULL.
 * @param buf       Output buffer for int16_t PCM. Must not be NULL.
 * @param count     Number of samples to render.
 * @param music_vol Volume [0, 255].
 * @return FQ_MUSIC_OK on success.
 *         FQ_MUSIC_ERR_NULL if ctx or buf is NULL.
 *         FQ_MUSIC_ERR_LOOP_GUARD if the pattern loop guard triggered
 *           (output filled with zeros for this call).
 */
fq_music_err_t fq_music_render(fq_music_ctx_t *ctx,
                                int16_t        *buf,
                                size_t          count,
                                uint8_t         music_vol);

/**
 * fq_music_is_playing — Returns 1 if music is currently playing, 0 otherwise.
 *
 * @param ctx  Caller-owned music context. Returns 0 if NULL.
 */
uint8_t fq_music_is_playing(const fq_music_ctx_t *ctx);

#endif /* FIESTAQUEST_GAME_MUSIC_H */
