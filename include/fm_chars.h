/**
 * fm_chars.h — FIESTAMON Character Sprite Map
 *
 * Sheet:  Characters/32x32-Charset.png
 * Layout: 8 cols × 21 rows, each cell 32×32 px
 * Outline variant: Characters/32x32-Charset-Outline.png (same layout)
 *
 * Each ROW holds 8 animation frames (cols 0-7).
 * Adjacent even+odd rows form one full character archetype:
 *   even row = primary palette, odd row = alt/rear palette.
 */
#pragma once
#include <stdint.h>

/* ── Dimensions ─────────────────────────────────────────── */
#define FM_CHAR_W       32u   /* sprite width  (px) */
#define FM_CHAR_H       32u   /* sprite height (px) */
#define FM_CHAR_COLS    8u    /* frames per row */
#define FM_CHAR_ROWS    21u   /* total rows in sheet */

/* ── Animation frame semantics (col index) ──────────────── */
typedef enum {
    FM_FRAME_IDLE    = 0,
    FM_FRAME_WALK_1  = 1,
    FM_FRAME_WALK_2  = 2,
    FM_FRAME_WALK_3  = 3,
    FM_FRAME_WALK_4  = 4,
    FM_FRAME_WALK_5  = 5,
    FM_FRAME_WALK_6  = 6,
    FM_FRAME_WALK_7  = 7,
} FmAnimFrame;

/* ── Character archetypes ────────────────────────────────── */
/*
 * Each archetype occupies 2 consecutive rows (primary + alt).
 * sheet_row_primary = first row of the pair.
 *
 * Visual ID (pixel-confirmed):
 *   DARK_KNIGHT   – cloaked warrior, broadsword + shield
 *   BONE_KNIGHT   – skeletal armored fighter, dark cape
 *   SKULL_MAGE    – heavy robed figure, skull face, glowing eyes
 *   HORNED_DEMON  – large demonic brute, curved horns
 *   TROLL_BEAST   – massive horned troll, 4 horn nubs
 *   SLIME_GOBLIN  – short squat goblin/slug, round belly
 *   FAT_BOSS      – very wide boss, top hat / crown, frog grin
 *   VAMPIRE_ROGUE – tall cloaked vampire, upturned collar
 *   FLORAL_WITCH  – plant/flower-decorated, ornate crown
 *   SHADOW_HULK   – dark muscle creature, skull face (row 17)
 *   OGRE_BOSS     – wide grinning ogre, frames 4-7 spiked var.
 *   BEAR_SPIRIT   – bear-faced creature, natural animal body
 *   ARMORED_BEAR  – spiked-crown armored bear, very bulky (row 20)
 */
typedef enum {
    FM_CHAR_DARK_KNIGHT   =  0,  /* rows  0–1  */
    FM_CHAR_BONE_KNIGHT   =  1,  /* rows  2–3  */
    FM_CHAR_SKULL_MAGE    =  2,  /* rows  4–5  */
    FM_CHAR_HORNED_DEMON  =  3,  /* rows  6–7  */
    FM_CHAR_TROLL_BEAST   =  4,  /* rows  8–9  */
    FM_CHAR_SLIME_GOBLIN  =  5,  /* rows 10–11 */
    FM_CHAR_FAT_BOSS      =  6,  /* rows 12–13 */
    FM_CHAR_VAMPIRE_ROGUE =  7,  /* rows 14–15 */
    FM_CHAR_FLORAL_WITCH  =  8,  /* row  16    */
    FM_CHAR_SHADOW_HULK   =  9,  /* row  17    */
    FM_CHAR_OGRE_BOSS     = 10,  /* row  18    */
    FM_CHAR_BEAR_SPIRIT   = 11,  /* row  19    */
    FM_CHAR_ARMORED_BEAR  = 12,  /* row  20    */
    FM_CHAR_COUNT
} FmCharId;

/* ── Character definition table ─────────────────────────── */
typedef struct {
    const char* name;        /* human-readable archetype name */
    uint8_t     sheet_row;   /* primary row in the PNG         */
    uint8_t     has_alt_row; /* 1 = uses row+1 as alt palette  */
} FmCharDef;

static const FmCharDef FM_CHARS[FM_CHAR_COUNT] = {
    [FM_CHAR_DARK_KNIGHT  ] = { "DARK_KNIGHT",   0,  1 },
    [FM_CHAR_BONE_KNIGHT  ] = { "BONE_KNIGHT",   2,  1 },
    [FM_CHAR_SKULL_MAGE   ] = { "SKULL_MAGE",    4,  1 },
    [FM_CHAR_HORNED_DEMON ] = { "HORNED_DEMON",  6,  1 },
    [FM_CHAR_TROLL_BEAST  ] = { "TROLL_BEAST",   8,  1 },
    [FM_CHAR_SLIME_GOBLIN ] = { "SLIME_GOBLIN",  10, 1 },
    [FM_CHAR_FAT_BOSS     ] = { "FAT_BOSS",      12, 1 },
    [FM_CHAR_VAMPIRE_ROGUE] = { "VAMPIRE_ROGUE", 14, 1 },
    [FM_CHAR_FLORAL_WITCH ] = { "FLORAL_WITCH",  16, 0 },
    [FM_CHAR_SHADOW_HULK  ] = { "SHADOW_HULK",   17, 0 },
    [FM_CHAR_OGRE_BOSS    ] = { "OGRE_BOSS",     18, 0 },
    [FM_CHAR_BEAR_SPIRIT  ] = { "BEAR_SPIRIT",   19, 0 },
    [FM_CHAR_ARMORED_BEAR ] = { "ARMORED_BEAR",  20, 0 },
};

/* ── Sprite rectangle type ───────────────────────────────── */
typedef struct { uint16_t x, y, w, h; } FmRect;

/**
 * FM_CHAR_RECT — compute source rect for a character frame.
 *
 * @param char_id  FmCharId enum value
 * @param frame    FmAnimFrame (0-7)
 * @param use_alt  0 = primary row, 1 = alt row (if has_alt_row)
 * @return FmRect  {x, y, 32, 32} ready to blit
 */
static inline FmRect FM_CHAR_RECT(FmCharId char_id, FmAnimFrame frame, uint8_t use_alt)
{
    uint8_t row = FM_CHARS[char_id].sheet_row;
    if (use_alt && FM_CHARS[char_id].has_alt_row) row += 1;
    return (FmRect){
        .x = (uint16_t)(frame * FM_CHAR_W),
        .y = (uint16_t)(row   * FM_CHAR_H),
        .w = FM_CHAR_W,
        .h = FM_CHAR_H,
    };
}
