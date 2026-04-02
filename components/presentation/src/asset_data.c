/**
 * asset_data.c — FiestaQuest Presentation Layer: Asset Accessor Implementation
 *
 * SINGLE TRANSLATION UNIT that includes the generated sprite and font headers.
 * No other .c file in the presentation component may include these headers.
 *
 * This file is the ONE owner of sprite and font RODATA in the presentation
 * component.  Screen renderers access assets only via the accessor functions
 * declared in asset_data.h.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 */

#include "asset_data.h"
#include <stddef.h>  /* NULL */

/*
 * Include generated asset headers here — and ONLY here.
 *
 * Each header defines static const arrays.  One TU = one copy in RODATA.
 * Warning guard: the headers themselves carry a comment explaining the rule.
 */
#include "sprites/sprite_chars.h"
#include "sprites/sprite_items.h"
#include "fonts/font_regs_12.h"
#include "fonts/font_script_24.h"
#include "fonts/font_script_36.h"

/* ── Sprite table sizes (derived from generated data) ─────────────────────── */

/** Total entries in the character sprite table (21 chars × 8 frames). */
#define ASSET_CHAR_TABLE_SIZE  \
    ((uint16_t)(sizeof(SPRITE_CHAR_TABLE) / sizeof(SPRITE_CHAR_TABLE[0])))

/** Total entries in the item sprite table (4×4 grid = 16). */
#define ASSET_ITEM_TABLE_SIZE  \
    ((uint16_t)(sizeof(SPRITE_ITEM_TABLE) / sizeof(SPRITE_ITEM_TABLE[0])))

/* ── fq_get_font_small ────────────────────────────────────────────────────── */

const fq_font_t *fq_get_font_small(void)
{
    return &FONT_REGS_12_FONT;
}

/* ── fq_get_font_title ────────────────────────────────────────────────────── */

/**
 * fq_get_font_title — returns the 36px Jacquard script font.
 *
 * Upgraded from FONT_SCRIPT_24 to FONT_SCRIPT_36 for better legibility
 * on the 200×200 e-paper display.  "EmberTide" at 36px exceeds 180px so
 * callers must split into two lines: "Ember" and "Tide".
 */
const fq_font_t *fq_get_font_title(void)
{
    return &FONT_SCRIPT_36_FONT;
}

/* ── fq_get_char_sprite ───────────────────────────────────────────────────── */

const fq_sprite_t *fq_get_char_sprite(uint8_t char_id, uint8_t frame)
{
    /*
     * Compute flat index.  Use uint16_t intermediates to avoid uint8_t
     * overflow when char_id is large (e.g. 200 * 8 = 1600 would wrap).
     */
    uint16_t idx = (uint16_t)((uint16_t)char_id * 8u + (uint16_t)frame);
    if (idx >= ASSET_CHAR_TABLE_SIZE) {
        return NULL;
    }
    return &SPRITE_CHAR_TABLE[idx];
}

/* ── fq_get_item_sprite ───────────────────────────────────────────────────── */

const fq_sprite_t *fq_get_item_sprite(uint8_t item_id)
{
    if ((uint16_t)item_id >= ASSET_ITEM_TABLE_SIZE) {
        return NULL;
    }
    return &SPRITE_ITEM_TABLE[item_id];
}
