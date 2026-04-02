/**
 * asset_data.h — FiestaQuest Presentation Layer: Asset Accessor API
 *
 * Exposes the compiled-in sprite and font assets through accessor functions,
 * allowing all screen renderers to call fq_get_font_small() and
 * fq_get_char_sprite() without each including the large generated headers
 * directly.
 *
 * Architecture note:
 *   The generated headers (sprite_chars.h, sprite_items.h, font_regs_12.h)
 *   contain `static const` arrays.  Including them in more than one translation
 *   unit per link target silently duplicates RODATA.  By confining the includes
 *   to the single TU asset_data.c we pay for the data exactly once.
 *
 * All functions return const pointers — callers must never cast away const.
 *
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 */

#ifndef FIESTAQUEST_PRESENTATION_ASSET_DATA_H
#define FIESTAQUEST_PRESENTATION_ASSET_DATA_H

#include "fq_sprite.h"
#include "fq_text.h"
#include <stdint.h>

/* ── Font accessors ───────────────────────────────────────────────────────── */

/**
 * fq_get_font_small — Return a pointer to the FONT_REGS_12 font descriptor.
 *
 * This is the primary display font for all screen renderers.
 * The returned pointer is always non-NULL (points to a static const struct).
 *
 * @return  Pointer to the FONT_REGS_12_FONT fq_font_t descriptor.
 */
const fq_font_t *fq_get_font_small(void);

/**
 * fq_get_font_title — Return a pointer to the FONT_SCRIPT_24 font descriptor.
 *
 * Jacquard 12 script font at 24 px, used for decorative title text.
 * Glyphs are 30 px tall in 30×30 cells with variable advance widths.
 * The returned pointer is always non-NULL (points to a static const struct).
 *
 * @return  Pointer to the FONT_SCRIPT_24_FONT fq_font_t descriptor.
 */
const fq_font_t *fq_get_font_title(void);

/* ── Sprite accessors ─────────────────────────────────────────────────────── */

/**
 * fq_get_char_sprite — Look up a character sprite by class ID and frame.
 *
 * The sprite table has 168 entries arranged as 21 characters × 8 frames.
 * index = char_id * 8 + frame.
 *
 * Bounds checking: returns NULL if index >= 168 (prevents OOB access).
 *
 * @param char_id  Character class index (0-20). Values ≥ 21 return NULL.
 * @param frame    Animation frame (0-7). Ignored for OOB char_id.
 * @return         Pointer to the fq_sprite_t, or NULL if OOB.
 */
const fq_sprite_t *fq_get_char_sprite(uint8_t char_id, uint8_t frame);

/**
 * fq_get_item_sprite — Look up an item sprite by item ID.
 *
 * The item table has 16 entries (4 cols × 4 rows of 24×24 sprites).
 *
 * Bounds checking: returns NULL if item_id >= 16.
 *
 * @param item_id  Item index (0-15). Values ≥ 16 return NULL.
 * @return         Pointer to the fq_sprite_t, or NULL if OOB.
 */
const fq_sprite_t *fq_get_item_sprite(uint8_t item_id);

#endif /* FIESTAQUEST_PRESENTATION_ASSET_DATA_H */
