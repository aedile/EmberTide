/**
 * micromod.h — Minimal ProTracker MOD Player (vendored, integer-only)
 *
 * A simplified MOD tracker renderer for FiestaQuest. Parses ProTracker
 * .mod files (4-channel, 31-sample format) and renders them to 16-bit
 * signed mono PCM at a caller-specified sample rate.
 *
 * Design constraints:
 *   - No malloc. Operates entirely on caller-provided buffers.
 *   - No floating point. All math uses 16.16 fixed-point.
 *   - No global state. All state is in micromod_ctx_t.
 *   - Re-entrant: multiple contexts can coexist.
 *   - The caller owns the .mod data buffer for the lifetime of the context.
 *   - Pattern loop guard: render aborts after MAX_MOD_RENDER_SAMPLES.
 *
 * Architecture boundary:
 *   - Lives in components/game/lib/ alongside sfxr.h.
 *   - MUST NOT include hal_*.h headers.
 *   - MUST NOT include any game/ module headers (pure standalone library).
 *
 * ProTracker 4-channel .mod format assumptions:
 *   - Magic at offset 1080: "M.K." or "M!K!" (31-sample MOD).
 *   - 20-byte title, 31 sample descriptors (30 bytes each), 1 byte song length,
 *     1 byte restart position, 128-byte pattern order, 4-byte magic, patterns.
 *   - Samples follow the pattern data as raw 8-bit signed PCM.
 *
 * Effects supported (minimal set for gameplay music):
 *   0xy — Arpeggio
 *   Axy — Volume slide
 *   Bxx — Pattern jump
 *   Cxx — Pattern break
 *   Dxy — Volume slide (alias)
 *   9xx — Sample offset
 *   No effect (0x00 with no data) — note+sample trigger only
 *
 * Constitution Priority 0: No floating point. No combat PRNG interaction.
 */

#ifndef FIESTAQUEST_GAME_LIB_MICROMOD_H
#define FIESTAQUEST_GAME_LIB_MICROMOD_H

#include <stdint.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------
 */

/** Maximum .mod file size accepted by micromod_init() (512 KiB). */
#define MAX_MOD_FILE_SIZE        (524288u)

/**
 * Pattern loop guard: maximum total samples that can be rendered per init.
 * 22050 * 600 = ~10 minutes at 22050 Hz. Malformed infinitely-looping MODs
 * are capped here.
 */
#define MAX_MOD_RENDER_SAMPLES   (22050u * 600u)

/** Number of channels in a ProTracker 4-channel MOD. */
#define MICROMOD_CHANNELS        4u

/** ProTracker .mod pattern rows per pattern. */
#define MICROMOD_ROWS_PER_PATTERN 64u

/* -------------------------------------------------------------------------
 * Error codes
 * -------------------------------------------------------------------------
 */
typedef enum {
    MICROMOD_OK              = 0, /**< Success. */
    MICROMOD_ERR_NULL        = 1, /**< NULL pointer argument. */
    MICROMOD_ERR_TOO_SMALL   = 2, /**< Buffer too small to be a valid MOD. */
    MICROMOD_ERR_TOO_LARGE   = 3, /**< Data exceeds MAX_MOD_FILE_SIZE. */
    MICROMOD_ERR_FORMAT      = 4, /**< Not a 31-sample ProTracker MOD. */
    MICROMOD_ERR_NOT_INIT    = 5, /**< Context not initialised; call micromod_init first. */
    MICROMOD_ERR_LOOP_GUARD  = 6  /**< MAX_MOD_RENDER_SAMPLES reached. */
} micromod_err_t;

/* -------------------------------------------------------------------------
 * Sample descriptor (parsed from MOD header).
 * -------------------------------------------------------------------------
 */
typedef struct {
    const int8_t *data;       /**< Pointer into .mod buffer at sample start. */
    uint32_t      length;     /**< Sample length in words (raw bytes / 2). */
    uint32_t      loop_start; /**< Loop start in words. */
    uint32_t      loop_len;   /**< Loop length in words. 0 = no loop. */
    uint8_t       volume;     /**< Default volume [0, 64]. */
    int8_t        fine_tune;  /**< Fine tune [-8, 7] (4-bit signed). */
} micromod_sample_t;

/* -------------------------------------------------------------------------
 * Channel state (one per 4-channel voice).
 * -------------------------------------------------------------------------
 */
typedef struct {
    const int8_t *sample_data;   /**< Pointer to current sample data. */
    uint32_t      sample_len;    /**< Length of current sample in words. */
    uint32_t      loop_start;    /**< Loop start in words. */
    uint32_t      loop_len;      /**< Loop length in words. 0 = no loop. */

    uint32_t      position;      /**< Fixed-point 16.16 position within sample. */
    uint32_t      step;          /**< Fixed-point 16.16 step per output sample. */

    uint8_t       volume;        /**< Current channel volume [0, 64]. */
    uint8_t       note;          /**< Current note (1-indexed period table index). */
    uint8_t       instrument;    /**< Current instrument index [1, 31]. */

    /* Effect state */
    uint8_t       effect;        /**< Effect nibble (high nibble of effect byte). */
    uint8_t       effect_param;  /**< Effect parameter byte. */
    uint8_t       arp_tick;      /**< Arpeggio sub-tick counter. */
} micromod_chan_t;

/* -------------------------------------------------------------------------
 * micromod_ctx_t — Complete player state.
 *
 * All state fits in this struct. No dynamic allocation.
 * -------------------------------------------------------------------------
 */
typedef struct {
    /* Pointer to caller-owned .mod data (NOT owned by this struct). */
    const uint8_t *mod_data;
    size_t         mod_size;

    /* Parsed header fields */
    uint8_t        song_length;             /**< Number of patterns in song. */
    uint8_t        restart_pos;             /**< Restart position in pattern order. */
    uint8_t        pattern_order[128];      /**< Pattern order table. */
    uint8_t        num_patterns;            /**< Highest pattern index + 1. */

    /* Sample descriptors (31 samples, indices 1-31) */
    micromod_sample_t samples[32];          /**< Index 0 unused; 1-31 are valid. */

    /* Playback state */
    micromod_chan_t   channels[MICROMOD_CHANNELS];
    uint32_t          sample_rate;          /**< Output sample rate in Hz. */
    uint32_t          samples_per_tick;     /**< Samples between tick advances. */
    uint32_t          tick_sample_counter;  /**< Samples rendered in current tick. */

    uint8_t           order_pos;            /**< Current position in pattern_order. */
    uint8_t           row;                  /**< Current row within pattern [0,63]. */
    uint8_t           tick;                 /**< Current tick within row [0, speed-1]. */
    uint8_t           speed;               /**< Ticks per row (default 6). */
    uint8_t           bpm;                 /**< Beats per minute (default 125). */

    /* Loop guard */
    uint32_t          total_samples_rendered; /**< Cumulative rendered samples. */

    uint8_t           initialised;           /**< 1 after successful micromod_init(). */
    uint8_t           finished;             /**< 1 after song end (before loop). */
} micromod_ctx_t;

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------
 */

/**
 * micromod_init — Parse a .mod file buffer and prepare the context for playback.
 *
 * The caller retains ownership of mod_data; it must remain valid for the
 * lifetime of ctx.
 *
 * @param ctx         Player context to initialise. Must not be NULL.
 * @param mod_data    Pointer to .mod file bytes. Must not be NULL.
 * @param mod_size    Size of mod_data in bytes.
 * @param sample_rate Output sample rate in Hz (e.g. 22050).
 * @return MICROMOD_OK on success, error code on failure.
 */
micromod_err_t micromod_init(micromod_ctx_t *ctx,
                              const uint8_t  *mod_data,
                              size_t          mod_size,
                              uint32_t        sample_rate);

/**
 * micromod_render — Render up to 'count' samples into buf.
 *
 * Advances playback state. The song loops when it reaches the end of the
 * pattern order (restart at restart_pos). Rendering is capped by
 * MAX_MOD_RENDER_SAMPLES (pattern loop guard).
 *
 * @param ctx    Initialised player context. Must not be NULL.
 * @param buf    Output buffer for int16_t PCM samples. Must not be NULL.
 * @param count  Number of samples to render.
 * @return MICROMOD_OK on success.
 *         MICROMOD_ERR_NOT_INIT if ctx was not initialised.
 *         MICROMOD_ERR_NULL if ctx or buf is NULL.
 *         MICROMOD_ERR_LOOP_GUARD if MAX_MOD_RENDER_SAMPLES exceeded.
 */
micromod_err_t micromod_render(micromod_ctx_t *ctx,
                                int16_t        *buf,
                                size_t          count);

/**
 * micromod_reset — Reset playback position to the beginning without
 * re-parsing the .mod data. Clears the loop guard counter.
 *
 * @param ctx Initialised player context. Must not be NULL.
 * @return MICROMOD_OK on success, MICROMOD_ERR_NULL if ctx is NULL.
 */
micromod_err_t micromod_reset(micromod_ctx_t *ctx);

/**
 * micromod_set_volume — Set master playback volume.
 *
 * @param ctx    Initialised player context. Must not be NULL.
 * @param volume Volume [0, 64]. Values above 64 are clamped to 64.
 * @return MICROMOD_OK on success, MICROMOD_ERR_NULL if ctx is NULL.
 */
micromod_err_t micromod_set_volume(micromod_ctx_t *ctx, uint8_t volume);

#endif /* FIESTAQUEST_GAME_LIB_MICROMOD_H */
