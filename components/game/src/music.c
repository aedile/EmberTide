/**
 * music.c — FiestaQuest MOD Music Playback
 *
 * Wraps the micromod renderer with play/stop/render state management.
 * A single static micromod_ctx_t holds the player state. No malloc.
 *
 * Constitution Priority 0: No floating point. No combat PRNG.
 *
 * Architecture boundary: This module is game/ layer. It MUST NOT include
 * any hal_*.h headers. The application layer (app_main.c) is responsible
 * for pushing the rendered PCM to hal_audio_write_samples().
 */

#include "music.h"
#include "../lib/micromod.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Static player state.
 * -------------------------------------------------------------------------
 */
static micromod_ctx_t s_ctx;
static uint8_t        s_playing;    /* 1 = playing, 0 = stopped */
static uint8_t        s_initialised; /* 1 = micromod_init succeeded */


/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------
 */

fq_music_err_t fq_music_init(const uint8_t *mod_data, size_t len)
{
    if (mod_data == NULL) {
        return FQ_MUSIC_ERR_NULL;
    }
    if (len > MAX_MOD_FILE_SIZE) {
        return FQ_MUSIC_ERR_TOO_LARGE;
    }

    /* Stop any current playback first. */
    s_playing     = 0u;
    s_initialised = 0u;

    micromod_err_t err = micromod_init(&s_ctx, mod_data, len,
                                        (uint32_t)22050u);
    if (err == MICROMOD_ERR_NULL || err == MICROMOD_ERR_TOO_SMALL) {
        return FQ_MUSIC_ERR_FORMAT;
    }
    if (err == MICROMOD_ERR_TOO_LARGE) {
        return FQ_MUSIC_ERR_TOO_LARGE;
    }
    if (err == MICROMOD_ERR_FORMAT) {
        return FQ_MUSIC_ERR_FORMAT;
    }
    if (err != MICROMOD_OK) {
        return FQ_MUSIC_ERR_FORMAT;
    }

    s_initialised = 1u;
    return FQ_MUSIC_OK;
}

fq_music_err_t fq_music_play(void)
{
    if (!s_initialised) {
        return FQ_MUSIC_ERR_NOT_INIT;
    }
    s_playing = 1u;
    return FQ_MUSIC_OK;
}

fq_music_err_t fq_music_stop(void)
{
    s_playing     = 0u;
    s_initialised = 0u;
    memset(&s_ctx, 0, sizeof(s_ctx));
    return FQ_MUSIC_OK;
}

fq_music_err_t fq_music_render(int16_t *buf, size_t count, uint8_t music_vol)
{
    if (buf == NULL) {
        return FQ_MUSIC_ERR_NULL;
    }

    /* Not playing or not init: output silence. */
    if (!s_playing || !s_initialised) {
        memset(buf, 0, count * sizeof(int16_t));
        return FQ_MUSIC_OK;
    }

    /* Render raw PCM from micromod. */
    micromod_err_t merr = micromod_render(&s_ctx, buf, count);
    if (merr == MICROMOD_ERR_LOOP_GUARD) {
        /* Loop guard tripped: buf was filled with zeros by micromod_render. */
        return FQ_MUSIC_ERR_LOOP_GUARD;
    }

    /* Apply volume scaling: out = (sample * vol) / 256 using int32 intermediate. */
    for (size_t i = 0u; i < count; i++) {
        int32_t scaled = ((int32_t)buf[i] * (int32_t)music_vol) / 256;
        /* Clamp to int16 range. */
        if (scaled >  32767)  { scaled =  32767; }
        if (scaled < -32768)  { scaled = -32768; }
        buf[i] = (int16_t)scaled;
    }

    return FQ_MUSIC_OK;
}

uint8_t fq_music_is_playing(void)
{
    return s_playing;
}
