/**
 * fm_items.h — FIESTAMON Item Sprite Map
 *
 * Sheet 1: Items/Items-24x24.png  (+ outline variant)
 *   Layout: 4 cols × 4 rows, 24×24 px each
 *   Total : 16 sprites
 *
 * Sheet 2: Items/Items2-24x24.png (+ outline variant)
 *   Layout: 4 cols × 2 rows, 24×24 px each
 *   Total : 8 sprites
 */
#pragma once
#include <stdint.h>
#include "fm_chars.h"   /* FmRect */

#define FM_ITEM_W  24u
#define FM_ITEM_H  24u

/* ── Sheet 1 item IDs ────────────────────────────────────── */
/*
 * Row 0: Keys     – 4 variants (ornate/square/rounded/chunky bow)
 * Row 1: Potions  – 4 vial sizes (sm/md/lg/lg-narrow)
 * Row 2: Shields  – 4 heraldic variants
 * Row 3: Daggers  – 4 tilt variants ~45°
 */
typedef enum {
    /* Keys — row 0 */
    FM_ITEM_KEY_ORNATE   = 0,   /* col 0 */
    FM_ITEM_KEY_SQUARE   = 1,   /* col 1 */
    FM_ITEM_KEY_ROUNDED  = 2,   /* col 2 */
    FM_ITEM_KEY_CHUNKY   = 3,   /* col 3 */
    /* Potions — row 1 */
    FM_ITEM_POTION_SM    = 4,
    FM_ITEM_POTION_MD    = 5,
    FM_ITEM_POTION_LG    = 6,
    FM_ITEM_POTION_XL    = 7,
    /* Shields — row 2 */
    FM_ITEM_SHIELD_SOLID = 8,
    FM_ITEM_SHIELD_OUT   = 9,
    FM_ITEM_SHIELD_WHITE = 10,
    FM_ITEM_SHIELD_THIN  = 11,
    /* Daggers — row 3 */
    FM_ITEM_DAGGER_A     = 12,
    FM_ITEM_DAGGER_B     = 13,
    FM_ITEM_DAGGER_C     = 14,
    FM_ITEM_DAGGER_D     = 15,
    FM_ITEM1_COUNT       = 16,
} FmItem1Id;

/* ── Sheet 2 item IDs ────────────────────────────────────── */
/*
 * Row 0: Flasks  – 4 round-belly potion variants
 * Row 1: Chests  – 4 states (closed / open variants)
 */
typedef enum {
    FM_ITEM_FLASK_DARK   = 0,
    FM_ITEM_FLASK_OUT    = 1,
    FM_ITEM_FLASK_LIGHT  = 2,
    FM_ITEM_FLASK_DARK2  = 3,
    FM_ITEM_CHEST_CLOSED = 4,
    FM_ITEM_CHEST_OPEN_A = 5,
    FM_ITEM_CHEST_OPEN_B = 6,
    FM_ITEM_CHEST_OPEN_C = 7,
    FM_ITEM2_COUNT       = 8,
} FmItem2Id;

/* ── Accessor macros ─────────────────────────────────────── */

/**
 * FM_ITEM1_RECT — source rect for Items-24x24.png
 * id: FmItem1Id (0-15)
 */
static inline FmRect FM_ITEM1_RECT(FmItem1Id id)
{
    uint8_t row = (uint8_t)(id / 4);
    uint8_t col = (uint8_t)(id % 4);
    return (FmRect){
        .x = (uint16_t)(col * FM_ITEM_W),
        .y = (uint16_t)(row * FM_ITEM_H),
        .w = FM_ITEM_W,
        .h = FM_ITEM_H,
    };
}

/**
 * FM_ITEM2_RECT — source rect for Items2-24x24.png
 * id: FmItem2Id (0-7)
 */
static inline FmRect FM_ITEM2_RECT(FmItem2Id id)
{
    uint8_t row = (uint8_t)(id / 4);
    uint8_t col = (uint8_t)(id % 4);
    return (FmRect){
        .x = (uint16_t)(col * FM_ITEM_W),
        .y = (uint16_t)(row * FM_ITEM_H),
        .w = FM_ITEM_W,
        .h = FM_ITEM_H,
    };
}
