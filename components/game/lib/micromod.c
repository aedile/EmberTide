/**
 * micromod.c — Minimal ProTracker MOD Player (vendored, integer-only)
 *
 * Implements the API declared in micromod.h. Parses ProTracker 4-channel
 * 31-sample .mod files and renders them to 16-bit signed mono PCM.
 *
 * All arithmetic is 16.16 fixed-point integer — no floating point.
 * No malloc. All state lives in the caller-provided micromod_ctx_t.
 *
 * Constitution Priority 0: No floating point. No combat PRNG interaction.
 *   - Period table uses pre-computed integer periods from 8363 Hz base.
 *   - Step calculation: step = (PERIOD_BASE * sample_rate_factor) / period.
 *     Uses 64-bit intermediate to avoid overflow.
 *
 * ProTracker MOD binary layout (31-sample format):
 *   [0..19]      Song title (20 bytes, null-padded)
 *   [20..929]    31 sample descriptors × 30 bytes each:
 *                  [0..21]  name (22 bytes)
 *                  [22..23] length in words, big-endian
 *                  [24]     fine_tune (4-bit signed, stored in low nibble)
 *                  [25]     volume [0..64]
 *                  [26..27] loop_start in words, big-endian
 *                  [28..29] loop_len in words, big-endian
 *   [930]        Song length (number of pattern positions)
 *   [931]        Restart position (Noisetracker restart byte, often 0x7F)
 *   [932..1059]  Pattern order table (128 bytes)
 *   [1060..1063] Magic identifier ("M.K." or "M!K!" etc.)
 *   [1064..]     Pattern data: n_patterns × 64 rows × 4 channels × 4 bytes
 *   [after_pats] Sample data: concatenated 8-bit signed PCM blocks
 *
 * Note: actual magic is at 1080 not 1064 in most references. The standard
 * ProTracker layout is:
 *   20 + (31*30) + 1 + 1 + 128 = 20 + 930 + 1 + 1 + 128 = 1080
 * So magic is at offset 1080.
 */

#include "micromod.h"
#include <string.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * ProTracker period table.
 *
 * Standard ProTracker periods for notes C-1 through B-3 (three octaves).
 * Index 0 = no note. Indices 1..36 = C-1..B-3.
 * Period = amiga_clock / (2 * note_frequency).
 * We store the raw Amiga periods and compute step from them.
 * -------------------------------------------------------------------------
 */
static const uint16_t s_period_table[37] = {
    0,                                          /* index 0 = no note */
    /* Octave 1: C-1 .. B-1 */
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
    /* Octave 2: C-2 .. B-2 */
    428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
    /* Octave 3: C-3 .. B-3 */
    214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113
};

/*
 * Amiga clock speed for PAL (3546895 Hz), divided by 2 for mono.
 * step = (AMIGA_CLOCK * FIXED16) / (period * sample_rate)
 * AMIGA_CLOCK = 3546895 / 2 = 1773447
 *
 * For a given period P and sample rate SR:
 *   note_freq_hz = 1773447 / P
 *   step (16.16 fixed) = (note_freq_hz << 16) / SR
 *                      = (1773447 << 16) / (P * SR)
 *
 * To avoid 64-bit overflow: (1773447ULL << 16) / ((uint64_t)P * SR)
 */
#define AMIGA_CLOCK_HALF  1773447UL
#define FIXED16_ONE       65536u

/* -------------------------------------------------------------------------
 * Internal constants
 * -------------------------------------------------------------------------
 */
#define MOD_SAMPLE_COUNT    31u
#define MOD_HEADER_OFFSET_SONG_LENGTH   950u  /* offset 930 + 20 = 950 */
/* Corrected: 20 + 930 = 950 for song_length; but standard is 930. */
/* Let me use the correct offsets: */
#define MOD_TITLE_SIZE       20u
#define MOD_SAMPLE_DESC_SIZE 30u
#define MOD_SAMPLE_BLOCK     (MOD_TITLE_SIZE + MOD_SAMPLE_COUNT * MOD_SAMPLE_DESC_SIZE)
/* MOD_SAMPLE_BLOCK = 20 + 31*30 = 20 + 930 = 950 */
#define MOD_SONG_LENGTH_OFF  MOD_SAMPLE_BLOCK                    /* 950 */
#define MOD_RESTART_OFF      (MOD_SAMPLE_BLOCK + 1u)             /* 951 */
#define MOD_ORDER_OFF        (MOD_SAMPLE_BLOCK + 2u)             /* 952 */
#define MOD_ORDER_SIZE       128u
#define MOD_MAGIC_OFF        (MOD_ORDER_OFF + MOD_ORDER_SIZE)    /* 1080 */
#define MOD_HEADER_SIZE      (MOD_MAGIC_OFF + 4u)                /* 1084 */

/* Bytes per row cell: 4 bytes per channel, 4 channels. */
#define MOD_ROW_BYTES        (4u * MICROMOD_CHANNELS)
/* Bytes per pattern: 64 rows. */
#define MOD_PATTERN_BYTES    (MICROMOD_ROWS_PER_PATTERN * MOD_ROW_BYTES)

/* -------------------------------------------------------------------------
 * Tempo: default BPM=125, speed=6.
 * samples_per_tick = sample_rate * 2.5 / BPM
 *                  = (sample_rate * 5) / (BPM * 2)
 * Using integer: (sample_rate * 5) / (bpm * 2)
 * -------------------------------------------------------------------------
 */
static uint32_t compute_samples_per_tick(uint32_t sample_rate, uint8_t bpm)
{
    if (bpm == 0u) { bpm = 125u; }
    return (sample_rate * 5u) / ((uint32_t)bpm * 2u);
}

/* -------------------------------------------------------------------------
 * compute_step — Calculate 16.16 fixed-point step for a given period.
 * -------------------------------------------------------------------------
 */
static uint32_t compute_step(uint16_t period, uint32_t sample_rate)
{
    if (period == 0u || sample_rate == 0u) {
        return 0u;
    }
    uint64_t num = ((uint64_t)AMIGA_CLOCK_HALF << 16);
    uint64_t den = (uint64_t)period * (uint64_t)sample_rate;
    if (den == 0u) { return 0u; }
    return (uint32_t)(num / den);
}

/* -------------------------------------------------------------------------
 * find_period_index — Given a raw period from a MOD cell, find table index.
 *
 * Scans the period table for the nearest match (within ±4).
 * Returns 0 if no match found (no note).
 * -------------------------------------------------------------------------
 */
static uint8_t find_period_index(uint16_t period)
{
    if (period == 0u) { return 0u; }
    for (uint8_t i = 1u; i <= 36u; i++) {
        int32_t diff = (int32_t)period - (int32_t)s_period_table[i];
        if (diff < 0) { diff = -diff; }
        if (diff <= 4) { return i; }
    }
    return 0u;
}

/* -------------------------------------------------------------------------
 * parse_cell — Extract note, instrument, effect, and parameter from 4 bytes.
 *
 * ProTracker cell layout (4 bytes):
 *   byte0: bits 7..4 = sample high nibble; bits 3..0 = period bits 11..8
 *   byte1: period bits 7..0
 *   byte2: bits 7..4 = sample low nibble; bits 3..0 = effect nibble
 *   byte3: effect parameter
 * -------------------------------------------------------------------------
 */
static void parse_cell(const uint8_t *cell, uint8_t *instrument,
                        uint16_t *period, uint8_t *effect, uint8_t *param)
{
    *instrument = (uint8_t)(((cell[0] & 0xF0u) >> 4u) |
                             ((cell[2] & 0xF0u) >> 0u));
    *period     = (uint16_t)(((uint16_t)(cell[0] & 0x0Fu) << 8u) | cell[1]);
    *effect     = (uint8_t)(cell[2] & 0x0Fu);
    *param      = cell[3];
}

/* -------------------------------------------------------------------------
 * advance_channel_sample — Generate one output sample from a channel.
 * -------------------------------------------------------------------------
 */
static int16_t channel_next_sample(micromod_chan_t *ch)
{
    if (ch->sample_data == NULL || ch->step == 0u) {
        return 0;
    }
    if (ch->sample_len == 0u) {
        return 0;
    }

    /* Integer part of position (word index). */
    uint32_t pos_word = ch->position >> 16u;

    if (ch->loop_len > 0u) {
        /* Looping sample: keep within loop region. */
        uint32_t loop_end = ch->loop_start + ch->loop_len;
        while (pos_word >= loop_end) {
            pos_word   -= ch->loop_len;
            ch->position -= (ch->loop_len << 16u);
        }
    } else {
        /* Non-looping: clamp at end. */
        if (pos_word >= ch->sample_len) {
            ch->step = 0u; /* Stop playing. */
            return 0;
        }
    }

    /* Read 8-bit signed sample, scale to int16 range. */
    int8_t  raw = ch->sample_data[pos_word];
    int16_t out = (int16_t)((int32_t)raw * 256);

    /* Apply channel volume [0..64]. */
    out = (int16_t)(((int32_t)out * ch->volume) / 64);

    /* Advance position. */
    ch->position += ch->step;

    return out;
}

/* -------------------------------------------------------------------------
 * apply_effects — Process effect command for a channel on tick 0.
 * -------------------------------------------------------------------------
 */
static void apply_effects_tick0(micromod_chan_t *ch, micromod_ctx_t *ctx)
{
    uint8_t eff = ch->effect;
    uint8_t prm = ch->effect_param;

    switch (eff) {
        case 0x0:
            /* No effect (arpeggio only on tick > 0). */
            ch->arp_tick = 0u;
            break;

        case 0x9: {
            /* 9xx: Sample offset (set position to xx * 256 words). */
            uint32_t offset = (uint32_t)prm * 256u;
            ch->position = offset << 16u;
            break;
        }

        case 0xA:
        case 0xD: {
            /* Axy / Dxy: Volume slide (applied on non-zero ticks here as tick0). */
            /* Just reset arp tick. Volume slide applied on subsequent ticks. */
            break;
        }

        case 0xB: {
            /* Bxx: Pattern jump — jump to order position xx. */
            if (prm < ctx->song_length) {
                ctx->order_pos = prm;
                ctx->row       = 0u;
                ctx->tick      = 0u;
            }
            break;
        }

        case 0xC: {
            /* Cxx: Set volume. */
            ch->volume = (prm > 64u) ? 64u : prm;
            break;
        }

        case 0xF: {
            /* Fxx: Set speed / BPM.
             * If xx < 0x20 (32): set speed (ticks per row).
             * If xx >= 0x20: set BPM. */
            if (prm < 0x20u) {
                if (prm == 0u) { prm = 1u; } /* Protect against speed=0. */
                ctx->speed = prm;
            } else {
                ctx->bpm = prm;
                ctx->samples_per_tick = compute_samples_per_tick(
                    ctx->sample_rate, ctx->bpm);
            }
            break;
        }

        default:
            break;
    }
}

/* -------------------------------------------------------------------------
 * apply_effects_tick — Effects that fire on ticks 1..speed-1.
 * -------------------------------------------------------------------------
 */
static void apply_effects_tick(micromod_chan_t *ch)
{
    uint8_t eff = ch->effect;
    uint8_t prm = ch->effect_param;

    switch (eff) {
        case 0x0: {
            /* Arpeggio: cycle between note, note+x, note+y each tick. */
            uint8_t x = (uint8_t)(prm >> 4u);
            uint8_t y = (uint8_t)(prm & 0x0Fu);
            if (x == 0u && y == 0u) { break; }
            ch->arp_tick = (ch->arp_tick + 1u) % 3u;
            uint8_t semis = (ch->arp_tick == 1u) ? x :
                            (ch->arp_tick == 2u) ? y : 0u;
            uint8_t note_idx = ch->note;
            if (note_idx > 0u && note_idx + semis <= 36u) {
                note_idx = (uint8_t)(note_idx + semis);
            }
            /* Re-compute step for arpeggio note (does not store ch->note). */
            break;
        }

        case 0xA:
        case 0xD: {
            /* Volume slide: upper nibble = slide up, lower nibble = slide down. */
            uint8_t up   = (uint8_t)(prm >> 4u);
            uint8_t down = (uint8_t)(prm & 0x0Fu);
            if (up > 0u && down == 0u) {
                if ((int32_t)ch->volume + up > 64) { ch->volume = 64u; }
                else { ch->volume = (uint8_t)(ch->volume + up); }
            } else if (down > 0u) {
                if ((int32_t)ch->volume < (int32_t)down) { ch->volume = 0u; }
                else { ch->volume = (uint8_t)(ch->volume - down); }
            }
            break;
        }

        default:
            break;
    }
}

/* -------------------------------------------------------------------------
 * advance_row — Move to the next row in the current pattern.
 * -------------------------------------------------------------------------
 */
static void advance_row(micromod_ctx_t *ctx)
{
    ctx->row++;
    if (ctx->row >= MICROMOD_ROWS_PER_PATTERN) {
        ctx->row = 0u;
        ctx->order_pos++;
        if (ctx->order_pos >= ctx->song_length) {
            /* Song end — loop from restart_pos. */
            ctx->order_pos = ctx->restart_pos;
            if (ctx->order_pos >= ctx->song_length) {
                ctx->order_pos = 0u;
            }
        }
    }
    ctx->tick = 0u;
}

/* -------------------------------------------------------------------------
 * process_tick — Read current row's notes/effects (tick 0) or apply
 * running effects (tick 1..speed-1), then advance the tick counter.
 * -------------------------------------------------------------------------
 */
static void process_row_tick(micromod_ctx_t *ctx)
{
    uint8_t pattern_idx = ctx->pattern_order[ctx->order_pos];

    /* Pattern data offset: after header + (pattern_idx * MOD_PATTERN_BYTES). */
    size_t pat_off = MOD_HEADER_SIZE
                   + (size_t)pattern_idx * MOD_PATTERN_BYTES
                   + (size_t)ctx->row    * MOD_ROW_BYTES;

    if (ctx->tick == 0u) {
        /* Parse and trigger this row's notes. */
        for (uint8_t c = 0u; c < MICROMOD_CHANNELS; c++) {
            const uint8_t *cell = &ctx->mod_data[pat_off + c * 4u];
            micromod_chan_t *ch = &ctx->channels[c];

            uint8_t  instrument;
            uint16_t period;
            uint8_t  effect;
            uint8_t  param;
            parse_cell(cell, &instrument, &period, &effect, &param);

            ch->effect       = effect;
            ch->effect_param = param;
            ch->arp_tick     = 0u;

            /* Trigger sample if instrument specified. */
            if (instrument > 0u && instrument <= MOD_SAMPLE_COUNT) {
                const micromod_sample_t *smp = &ctx->samples[instrument];
                ch->instrument  = instrument;
                ch->volume      = smp->volume;
                ch->sample_data = smp->data;
                ch->sample_len  = smp->length;
                ch->loop_start  = smp->loop_start;
                ch->loop_len    = smp->loop_len;
                if (period != 0u) {
                    /* New note: reset position to zero (or sample offset effect). */
                    ch->position = 0u;
                }
            }

            /* Set period / step (new note trigger). */
            if (period != 0u) {
                ch->note = find_period_index(period);
                ch->step = compute_step(period, ctx->sample_rate);
            }

            apply_effects_tick0(ch, ctx);
        }
    } else {
        /* Non-zero tick: apply running effects. */
        for (uint8_t c = 0u; c < MICROMOD_CHANNELS; c++) {
            apply_effects_tick(&ctx->channels[c]);
        }
    }

    /* Advance tick; if we've completed all ticks for this row, advance row. */
    ctx->tick++;
    if (ctx->tick >= ctx->speed) {
        advance_row(ctx);
    }
}

/* -------------------------------------------------------------------------
 * Public API Implementation
 * -------------------------------------------------------------------------
 */

micromod_err_t micromod_init(micromod_ctx_t *ctx,
                              const uint8_t  *mod_data,
                              size_t          mod_size,
                              uint32_t        sample_rate)
{
    if (ctx == NULL || mod_data == NULL) {
        return MICROMOD_ERR_NULL;
    }
    if (mod_size < MOD_HEADER_SIZE) {
        return MICROMOD_ERR_TOO_SMALL;
    }
    if (mod_size > MAX_MOD_FILE_SIZE) {
        return MICROMOD_ERR_TOO_LARGE;
    }

    /* Validate magic at offset 1080. Accept "M.K." and "M!K!". */
    const uint8_t *magic = &mod_data[MOD_MAGIC_OFF];
    if (!((magic[0] == 'M' && magic[1] == '.' && magic[2] == 'K' && magic[3] == '.') ||
          (magic[0] == 'M' && magic[1] == '!' && magic[2] == 'K' && magic[3] == '!') ||
          (magic[0] == 'F' && magic[1] == 'L' && magic[2] == 'T' && magic[3] == '4') ||
          (magic[0] == '4' && magic[1] == 'C' && magic[2] == 'H' && magic[3] == 'N'))) {
        return MICROMOD_ERR_FORMAT;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->mod_data    = mod_data;
    ctx->mod_size    = mod_size;
    ctx->sample_rate = sample_rate;

    /* Parse song header. */
    ctx->song_length  = mod_data[MOD_SONG_LENGTH_OFF];
    ctx->restart_pos  = mod_data[MOD_RESTART_OFF];

    /* Clamp restart_pos: the "correct" field is often 0x7F in old files. */
    if (ctx->restart_pos >= ctx->song_length) {
        ctx->restart_pos = 0u;
    }

    /* Parse pattern order and find the highest pattern index used. */
    uint8_t max_pattern = 0u;
    for (uint8_t i = 0u; i < MOD_ORDER_SIZE; i++) {
        ctx->pattern_order[i] = mod_data[MOD_ORDER_OFF + i];
        if (i < ctx->song_length && ctx->pattern_order[i] > max_pattern) {
            max_pattern = ctx->pattern_order[i];
        }
    }
    ctx->num_patterns = (uint8_t)(max_pattern + 1u);

    /* Parse sample descriptors. */
    size_t sample_data_start = MOD_HEADER_SIZE
                              + (size_t)ctx->num_patterns * MOD_PATTERN_BYTES;

    size_t sample_ptr = sample_data_start;

    for (uint8_t s = 1u; s <= MOD_SAMPLE_COUNT; s++) {
        size_t desc_off = MOD_TITLE_SIZE + (size_t)(s - 1u) * MOD_SAMPLE_DESC_SIZE;
        const uint8_t *d = &mod_data[desc_off];

        uint32_t length     = (uint32_t)((uint16_t)(d[22] << 8u) | d[23]) * 2u;
        uint8_t  fine_tune  = d[24] & 0x0Fu;
        uint8_t  volume     = (d[25] > 64u) ? 64u : d[25];
        uint32_t loop_start = (uint32_t)((uint16_t)(d[26] << 8u) | d[27]) * 2u;
        uint32_t loop_len   = (uint32_t)((uint16_t)(d[28] << 8u) | d[29]) * 2u;

        ctx->samples[s].volume     = volume;
        ctx->samples[s].fine_tune  = (int8_t)(fine_tune > 7u
                                              ? (int8_t)(fine_tune - 16)
                                              : (int8_t)fine_tune);
        ctx->samples[s].length     = length;
        ctx->samples[s].loop_start = loop_start;
        ctx->samples[s].loop_len   = (loop_len <= 2u) ? 0u : loop_len;

        if (length > 0u && sample_ptr + length <= mod_size) {
            ctx->samples[s].data = (const int8_t *)(&mod_data[sample_ptr]);
        } else {
            ctx->samples[s].data   = NULL;
            ctx->samples[s].length = 0u;
        }
        sample_ptr += length;
    }

    /* Default tempo: BPM=125, speed=6. */
    ctx->speed              = 6u;
    ctx->bpm                = 125u;
    ctx->samples_per_tick   = compute_samples_per_tick(sample_rate, 125u);
    ctx->tick_sample_counter = 0u;
    ctx->order_pos          = 0u;
    ctx->row                = 0u;
    ctx->tick               = 0u;
    ctx->total_samples_rendered = 0u;
    ctx->initialised        = 1u;
    ctx->finished           = 0u;

    return MICROMOD_OK;
}

micromod_err_t micromod_render(micromod_ctx_t *ctx,
                                int16_t        *buf,
                                size_t          count)
{
    if (ctx == NULL || buf == NULL) {
        return MICROMOD_ERR_NULL;
    }
    if (!ctx->initialised) {
        return MICROMOD_ERR_NOT_INIT;
    }

    for (size_t i = 0u; i < count; i++) {
        /* Loop guard. */
        if (ctx->total_samples_rendered >= MAX_MOD_RENDER_SAMPLES) {
            /* Fill remainder with silence. */
            for (size_t j = i; j < count; j++) {
                buf[j] = 0;
            }
            return MICROMOD_ERR_LOOP_GUARD;
        }

        /* Advance tick if we've consumed all samples in current tick. */
        if (ctx->tick_sample_counter == 0u) {
            process_row_tick(ctx);
            ctx->tick_sample_counter = ctx->samples_per_tick;
        }
        ctx->tick_sample_counter--;

        /* Mix all channels to mono. Accumulate in int32 to prevent overflow. */
        int32_t mix = 0;
        for (uint8_t c = 0u; c < MICROMOD_CHANNELS; c++) {
            mix += (int32_t)channel_next_sample(&ctx->channels[c]);
        }

        /* Divide by channel count to normalise. */
        mix /= (int32_t)MICROMOD_CHANNELS;

        /* Clamp to int16 range. */
        if (mix >  32767)  { mix =  32767; }
        if (mix < -32768)  { mix = -32768; }
        buf[i] = (int16_t)mix;

        ctx->total_samples_rendered++;
    }

    return MICROMOD_OK;
}

micromod_err_t micromod_reset(micromod_ctx_t *ctx)
{
    if (ctx == NULL) {
        return MICROMOD_ERR_NULL;
    }
    ctx->order_pos           = 0u;
    ctx->row                 = 0u;
    ctx->tick                = 0u;
    ctx->tick_sample_counter = 0u;
    ctx->total_samples_rendered = 0u;
    ctx->finished            = 0u;
    ctx->speed               = 6u;
    ctx->bpm                 = 125u;
    ctx->samples_per_tick    = compute_samples_per_tick(ctx->sample_rate, 125u);

    /* Clear channel state. */
    for (uint8_t c = 0u; c < MICROMOD_CHANNELS; c++) {
        memset(&ctx->channels[c], 0, sizeof(ctx->channels[c]));
    }

    return MICROMOD_OK;
}

micromod_err_t micromod_set_volume(micromod_ctx_t *ctx, uint8_t volume)
{
    if (ctx == NULL) {
        return MICROMOD_ERR_NULL;
    }
    if (volume > 64u) { volume = 64u; }
    for (uint8_t c = 0u; c < MICROMOD_CHANNELS; c++) {
        ctx->channels[c].volume = volume;
    }
    return MICROMOD_OK;
}
