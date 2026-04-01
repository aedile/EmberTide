/**
 * fm_tiles.h — FIESTAMON Tile Sprite Map
 *
 * All tile sheets: 16×16 px sprites, white-on-black PNG.
 * Invert colors when rendering to e-paper (white→white, black→black).
 *
 * Sheet metadata:
 *   CRYPT   – Crypt-16x16.png    – 8×3  (24 tiles)
 *   DUNGEON – Dungeon-16x16.png  – 8×4  (32 tiles)
 *   HOLD    – Hold-16x16.png     – 8×3  (24 tiles)
 *   LAND    – Land-16x16.png     – 8×4  (32 tiles)
 */
#pragma once
#include <stdint.h>
#include "fm_chars.h"   /* FmRect */

#define FM_TILE_W  16u
#define FM_TILE_H  16u

/* ── Tileset selector ────────────────────────────────────── */
typedef enum {
    FM_TILESET_CRYPT   = 0,
    FM_TILESET_DUNGEON = 1,
    FM_TILESET_HOLD    = 2,
    FM_TILESET_LAND    = 3,
    FM_TILESET_COUNT
} FmTileSet;

/* ── Crypt tileset (8×3) ─────────────────────────────────── */
/*
 * Row 0: H/V wall segments, T-junctions, cross junction
 * Row 1: Diagonal corners, angled rubble, entrance arch frags
 * Row 2: Star/dot floor accents, diamond patterns, rubble, col bases
 */
typedef enum {
    FM_CRYPT_WALL_H      = 0,   /* row0 col0 */
    FM_CRYPT_WALL_V      = 1,
    FM_CRYPT_WALL_TL     = 2,
    FM_CRYPT_WALL_TR     = 3,
    FM_CRYPT_JUNCTION_T  = 4,
    FM_CRYPT_JUNCTION_X  = 5,
    FM_CRYPT_WALL_BL     = 6,
    FM_CRYPT_WALL_BR     = 7,
    FM_CRYPT_DIAG_CORNER = 8,   /* row1 col0 */
    FM_CRYPT_RUBBLE_A    = 9,
    FM_CRYPT_RUBBLE_B    = 10,
    FM_CRYPT_ARCH_L      = 11,
    FM_CRYPT_ARCH_R      = 12,
    FM_CRYPT_RUBBLE_C    = 13,
    FM_CRYPT_RUBBLE_D    = 14,
    FM_CRYPT_RUBBLE_E    = 15,
    FM_CRYPT_FLOOR_DOT   = 16,  /* row2 col0 */
    FM_CRYPT_FLOOR_STAR  = 17,
    FM_CRYPT_FLOOR_DIA   = 18,
    FM_CRYPT_RUBBLE_F    = 19,
    FM_CRYPT_COL_BASE    = 20,
    FM_CRYPT_FLOOR_PLAIN = 21,
    FM_CRYPT_ACCENT_A    = 22,
    FM_CRYPT_ACCENT_B    = 23,
    FM_CRYPT_COUNT       = 24,
} FmCryptTile;

/* ── Dungeon tileset (8×4) ───────────────────────────────── */
/*
 * Row 0: Solid fill, outlined cell, large round-corner rooms
 * Row 1: Cross-wall junctions with circular nodes, half-walls
 * Row 2: Arch/gate segments, floor transitions
 * Row 3: Chevron/arrow floor markings, decorative diamond tiles
 */
typedef enum {
    FM_DUN_SOLID         = 0,
    FM_DUN_CELL_OUT      = 1,
    FM_DUN_ROUND_A       = 2,
    FM_DUN_ROUND_B       = 3,
    FM_DUN_ROUND_C       = 4,
    FM_DUN_ROUND_D       = 5,
    FM_DUN_ROUND_E       = 6,
    FM_DUN_ROUND_F       = 7,
    FM_DUN_JUNCTION_NODE = 8,
    FM_DUN_HALF_WALL_H   = 9,
    FM_DUN_HALF_WALL_V   = 10,
    FM_DUN_NODE_CROSS    = 11,
    FM_DUN_NODE_T        = 12,
    FM_DUN_NODE_CORNER   = 13,
    FM_DUN_FLOOR_TRANS   = 14,
    FM_DUN_PIT_EDGE      = 15,
    FM_DUN_ARCH_TOP      = 16,
    FM_DUN_GATE_L        = 17,
    FM_DUN_GATE_R        = 18,
    FM_DUN_FLOOR_A       = 19,
    FM_DUN_FLOOR_B       = 20,
    FM_DUN_TRANS_EDGE    = 21,
    FM_DUN_DOOR_H        = 22,
    FM_DUN_DOOR_V        = 23,
    FM_DUN_CHEVRON_U     = 24,
    FM_DUN_CHEVRON_R     = 25,
    FM_DUN_CHEVRON_D     = 26,
    FM_DUN_CHEVRON_L     = 27,
    FM_DUN_DIAMOND_A     = 28,
    FM_DUN_DIAMOND_B     = 29,
    FM_DUN_DIAMOND_C     = 30,
    FM_DUN_DIAMOND_D     = 31,
    FM_DUN_COUNT         = 32,
} FmDungeonTile;

/* ── Hold tileset (8×3) ──────────────────────────────────── */
/*
 * Row 0: Solid fill, brick outline, triangular ceiling/floor transitions
 * Row 1: Angular arch tops, sawtooth floor, triangular corners
 * Row 2: Horizontal band walls, crosshatch, palm/leaf motif accent
 */
typedef enum {
    FM_HOLD_SOLID        = 0,
    FM_HOLD_BRICK_OUT    = 1,
    FM_HOLD_TRI_CEIL_L   = 2,
    FM_HOLD_TRI_CEIL_R   = 3,
    FM_HOLD_TRI_FLOOR_L  = 4,
    FM_HOLD_TRI_FLOOR_R  = 5,
    FM_HOLD_PLANK_A      = 6,
    FM_HOLD_PLANK_B      = 7,
    FM_HOLD_ARCH_TOP_L   = 8,
    FM_HOLD_ARCH_TOP_R   = 9,
    FM_HOLD_SAWTOOTH_T   = 10,
    FM_HOLD_SAWTOOTH_B   = 11,
    FM_HOLD_TRI_CORNER_A = 12,
    FM_HOLD_TRI_CORNER_B = 13,
    FM_HOLD_TRI_CORNER_C = 14,
    FM_HOLD_TRI_CORNER_D = 15,
    FM_HOLD_BAND_H       = 16,
    FM_HOLD_BAND_V       = 17,
    FM_HOLD_CROSSHATCH   = 18,
    FM_HOLD_PALM_LEAF    = 19,
    FM_HOLD_ACCENT_A     = 20,
    FM_HOLD_ACCENT_B     = 21,
    FM_HOLD_ACCENT_C     = 22,
    FM_HOLD_ACCENT_D     = 23,
    FM_HOLD_COUNT        = 24,
} FmHoldTile;

/* ── Land tileset (8×4) ──────────────────────────────────── */
/*
 * Row 0: Flowing wave/grass fills (light→dark)
 * Row 1: Bow-tie/hourglass connectors, edge transitions
 * Row 2: Chevron directionals, fish-scale water
 * Row 3: Central motifs (flower, crest, diamond), dot scatter
 */
typedef enum {
    FM_LAND_GRASS_LT     = 0,
    FM_LAND_GRASS_MD     = 1,
    FM_LAND_GRASS_DK     = 2,
    FM_LAND_WAVE_A       = 3,
    FM_LAND_WAVE_B       = 4,
    FM_LAND_WAVE_C       = 5,
    FM_LAND_WAVE_D       = 6,
    FM_LAND_WAVE_E       = 7,
    FM_LAND_BOWTIE_H     = 8,
    FM_LAND_BOWTIE_V     = 9,
    FM_LAND_EDGE_TL      = 10,
    FM_LAND_EDGE_TR      = 11,
    FM_LAND_EDGE_BL      = 12,
    FM_LAND_EDGE_BR      = 13,
    FM_LAND_EDGE_T       = 14,
    FM_LAND_EDGE_B       = 15,
    FM_LAND_CHEVRON_U    = 16,
    FM_LAND_CHEVRON_R    = 17,
    FM_LAND_CHEVRON_D    = 18,
    FM_LAND_CHEVRON_L    = 19,
    FM_LAND_WATER_SCALE  = 20,
    FM_LAND_WATER_A      = 21,
    FM_LAND_WATER_B      = 22,
    FM_LAND_WATER_EDGE   = 23,
    FM_LAND_FLOWER       = 24,
    FM_LAND_CREST        = 25,
    FM_LAND_DIAMOND      = 26,
    FM_LAND_DOT_A        = 27,
    FM_LAND_DOT_B        = 28,
    FM_LAND_DOT_C        = 29,
    FM_LAND_ACCENT_A     = 30,
    FM_LAND_ACCENT_B     = 31,
    FM_LAND_COUNT        = 32,
} FmLandTile;

/* ── Accessor ────────────────────────────────────────────── */

/**
 * FM_TILE_RECT — compute source rect for any tile.
 *
 * @param tile_index  flat index within the chosen sheet (e.g. FmCryptTile)
 * @param cols        number of columns in that sheet (always 8)
 */
static inline FmRect FM_TILE_RECT(uint8_t tile_index, uint8_t cols)
{
    uint8_t row = tile_index / cols;
    uint8_t col = tile_index % cols;
    return (FmRect){
        .x = (uint16_t)(col * FM_TILE_W),
        .y = (uint16_t)(row * FM_TILE_H),
        .w = FM_TILE_W,
        .h = FM_TILE_H,
    };
}

/* Convenience wrappers */
#define FM_CRYPT_RECT(id)   FM_TILE_RECT((uint8_t)(id), 8u)
#define FM_DUN_RECT(id)     FM_TILE_RECT((uint8_t)(id), 8u)
#define FM_HOLD_RECT(id)    FM_TILE_RECT((uint8_t)(id), 8u)
#define FM_LAND_RECT(id)    FM_TILE_RECT((uint8_t)(id), 8u)
