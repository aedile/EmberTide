/**
 * fiestamon_assets.h — FIESTAMON Asset Map (master header)
 *
 * Include this single header to get all sprite map data structures.
 *
 *   #include "fiestamon_assets.h"
 *
 * Sub-headers:
 *   fm_chars.h  — character archetypes, animation frames
 *   fm_items.h  — collectible items (keys, potions, shields, etc.)
 *   fm_tiles.h  — tile sets (Crypt, Dungeon, Hold, Land)
 *   fm_icons.h  — icon sheets (RPG, Weather, Controller, Map, Media)
 *   fm_fonts.h  — font glyph metrics + advance width tables
 *
 * All accessors return FmRect {x, y, w, h} for use as a blit source.
 * Asset PNGs live under assets/ relative to the project root.
 */
#pragma once

#include "fm_chars.h"
#include "fm_items.h"
#include "fm_tiles.h"
#include "fm_icons.h"
#include "fm_fonts.h"

/* ── Asset path constants ────────────────────────────────── */
#define FM_PATH_CHAR_FILL    "assets/Characters/32x32-Charset.png"
#define FM_PATH_CHAR_OUT     "assets/Characters/32x32-Charset-Outline.png"
#define FM_PATH_ITEMS1_FILL  "assets/Items/Items-24x24.png"
#define FM_PATH_ITEMS1_OUT   "assets/Items/Items-24x24-outline.png"
#define FM_PATH_ITEMS2_FILL  "assets/Items/Items2-24x24.png"
#define FM_PATH_ITEMS2_OUT   "assets/Items/Items2-24x24-outline.png"
#define FM_PATH_TILE_CRYPT   "assets/Tiles/Crypt-16x16.png"
#define FM_PATH_TILE_DUN     "assets/Tiles/Dungeon-16x16.png"
#define FM_PATH_TILE_HOLD    "assets/Tiles/Hold-16x16.png"
#define FM_PATH_TILE_LAND    "assets/Tiles/Land-16x16.png"
#define FM_PATH_ICON_RPG     "assets/Icons/Icons_RPG.png"
#define FM_PATH_ICON_WEATHER "assets/Icons/Icons_Weather.png"
#define FM_PATH_ICON_CTRL    "assets/Icons/Icons_Controller.png"
#define FM_PATH_ICON_MAP     "assets/Icons/Icons_Map_Markers.png"
#define FM_PATH_ICON_MEDIA   "assets/Icons/Icons_Media.png"
#define FM_PATH_FONT_REGS12  "assets/Fonts/FONT_REGS_12.png"
#define FM_PATH_FONT_REGS18  "assets/Fonts/FONT_REGS_18.png"
#define FM_PATH_FONT_REGS24  "assets/Fonts/FONT_REGS_24.png"
#define FM_PATH_FONT_SCR24   "assets/Fonts/FONT_SCRIPT_24.png"
#define FM_PATH_FONT_SCR36   "assets/Fonts/FONT_SCRIPT_36.png"
