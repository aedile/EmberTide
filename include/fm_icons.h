/**
 * fm_icons.h — FIESTAMON Icon Sprite Map
 *
 * All icon sheets: 16×16 px sprites.
 * Offset formula: x = col * 16, y = row * 16.
 *
 * Sheets:
 *   Icons/Icons_RPG.png          ~10 cols × 14 rows
 *   Icons/Icons_Weather.png       5 cols ×  7 rows
 *   Icons/Icons_Controller.png   16 cols ×  8 rows
 *   Icons/Icons_Map_Markers.png  13 cols ×  7 rows (approx)
 *   Icons/Icons_Media.png        11 cols ×  8 rows (approx)
 *
 * Individual sprites also available in:
 *   Icons/Sprites/          — 16×16 padded
 *   Icons/Sprites_Cropped/  — tight-cropped variable size
 */
#pragma once
#include <stdint.h>
#include "fm_chars.h"   /* FmRect */

#define FM_ICON_W  16u
#define FM_ICON_H  16u

/* ── RPG icon sheet ───────────────────────────────────────
 * Rows 0–13, cols 0–9 (some rows shorter).
 * Use FM_ICON_RPG(row, col) for arbitrary access.
 * Named constants below cover the most-used icons.
 */
typedef enum {
    /* Row 0 — weapons */
    FM_RPG_SWORD       = 0x00,   /* r=0 c=0 */
    FM_RPG_SHIELD      = 0x01,
    FM_RPG_HAMMER      = 0x02,
    FM_RPG_HAMMERS_X   = 0x03,
    FM_RPG_SWORDS_X    = 0x04,
    FM_RPG_BOW         = 0x05,
    FM_RPG_BOOMERANG   = 0x06,
    FM_RPG_KEY         = 0x07,
    FM_RPG_SCEPTER     = 0x08,
    FM_RPG_BONE_WAND   = 0x09,
    /* Row 1 — blades */
    FM_RPG_SWORD_BROAD = 0x10,
    FM_RPG_SWORD_THICK = 0x11,
    FM_RPG_MACE_SPIKE  = 0x12,
    FM_RPG_MACE_STAR   = 0x13,
    FM_RPG_SPEAR       = 0x14,
    FM_RPG_SWORD_ORN   = 0x15,
    FM_RPG_PROJECTILE  = 0x16,
    /* Row 2 — magic */
    FM_RPG_DAGGER      = 0x20,
    FM_RPG_THROW_STAR  = 0x21,
    FM_RPG_WAND_FEATH  = 0x22,
    FM_RPG_NEEDLE      = 0x23,
    FM_RPG_WAND_MAGIC  = 0x24,
    FM_RPG_SKULL_STICK = 0x25,
    FM_RPG_SPELL_CIRC  = 0x26,
    FM_RPG_SNOWFLAKE   = 0x27,
    /* Row 3 — equipment/status */
    FM_RPG_HELMET      = 0x30,
    FM_RPG_WING        = 0x31,
    FM_RPG_ANGEL       = 0x32,
    FM_RPG_TOOLS_X     = 0x33,
    FM_RPG_PERSON      = 0x34,
    FM_RPG_ARMOR       = 0x35,
    FM_RPG_EYE         = 0x36,
    FM_RPG_AMULET      = 0x37,
    FM_RPG_CHAIN       = 0x38,
    FM_RPG_ANKH        = 0x39,
    /* Row 4 — misc utility */
    FM_RPG_LANTERN     = 0x40,
    FM_RPG_LIGHTNING   = 0x41,
    FM_RPG_CHEST_LOCK  = 0x42,
    /* Row 5 — status/emblems */
    FM_RPG_FLOWER_4    = 0x50,
    FM_RPG_SWIRL       = 0x51,
    FM_RPG_HEART_ARR   = 0x52,
    FM_RPG_HEART       = 0x53,
    FM_RPG_STAR        = 0x54,
    FM_RPG_STAR_BURST  = 0x55,
    FM_RPG_CLOVER      = 0x57,
    FM_RPG_FLOWER_5    = 0x58,
    FM_RPG_CROSS       = 0x59,
    /* Row 6 */
    FM_RPG_TARGET      = 0x60,
    FM_RPG_BULLSEYE    = 0x61,
    FM_RPG_HORSESHOE   = 0x62,
    FM_RPG_HEART_OUT   = 0x63,
    FM_RPG_STAR_OUT    = 0x64,
    FM_RPG_PAW         = 0x66,
    FM_RPG_CLOVER_4    = 0x67,
    FM_RPG_CRESCENT    = 0x6B,
    /* Row 10 — currency */
    FM_RPG_COIN_FACE   = 0xA0,
    FM_RPG_COIN_STRIPE = 0xA1,
    FM_RPG_COIN_DOT    = 0xA2,
    FM_RPG_SKULL_COIN  = 0xA4,
    /* Row 11 — creatures */
    FM_RPG_SKULL       = 0xB7,
    FM_RPG_SKULL_BONES = 0xB8,
    FM_RPG_SKULL_OUT   = 0xB9,
    /* Row 13 — economy */
    FM_RPG_MONEY_BILL  = 0xD0,
    FM_RPG_MONEY_BAG   = 0xD1,
    FM_RPG_COIN_PURSE  = 0xD2,
    FM_RPG_COIN_O      = 0xD3,
    FM_RPG_COIN_S      = 0xD4,
    FM_RPG_COINS_STACK = 0xD6,
    FM_RPG_BAG_S       = 0xD7,
} FmRpgIcon;

/* Decode packed enum: high nibble = row, low nibble = col */
static inline FmRect FM_RPG_RECT(FmRpgIcon id)
{
    uint8_t row = (uint8_t)((id >> 4) & 0xF);
    uint8_t col = (uint8_t)(id & 0xF);
    return (FmRect){
        .x = (uint16_t)(col * FM_ICON_W),
        .y = (uint16_t)(row * FM_ICON_H),
        .w = FM_ICON_W, .h = FM_ICON_H,
    };
}

/* ── Weather icon sheet ─────────────────────────────────── */
typedef enum {
    FM_WX_SUN         = 0x00,
    FM_WX_MOON        = 0x01,
    FM_WX_BIOHAZARD   = 0x02,
    FM_WX_NUCLEAR     = 0x03,
    FM_WX_CLOUD       = 0x04,
    FM_WX_RAIN_CLOUD  = 0x05,
    FM_WX_STORM       = 0x06,
    FM_WX_SUN_RING    = 0x10,
    FM_WX_MOON_ALT    = 0x11,
    FM_WX_SKULL       = 0x12,
    FM_WX_PLANET      = 0x13,
    FM_WX_CLOUD_PART  = 0x14,
    FM_WX_RAIN_DOTS   = 0x15,
    FM_WX_UMBRELLA    = 0x16,
    FM_WX_FLAME       = 0x20,
    FM_WX_WIND        = 0x21,
    FM_WX_WAVE        = 0x22,
    FM_WX_SNOWFLAKE   = 0x23,
    FM_WX_WATER_DROP  = 0x24,
    FM_WX_CANDLE      = 0x30,
    FM_WX_LEAF_WIND   = 0x31,
    FM_WX_MAPLE       = 0x32,
    FM_WX_LEAF        = 0x33,
    FM_WX_DROP_OUT    = 0x34,
    FM_WX_THERMO_E    = 0x40,
    FM_WX_THERMO_F    = 0x41,
    FM_WX_SNOWFLAKE6  = 0x50,
    FM_WX_CRYSTAL     = 0x51,
    FM_WX_DIAMOND     = 0x52,
    FM_WX_FLOWER_ICE  = 0x53,
} FmWeatherIcon;

static inline FmRect FM_WEATHER_RECT(FmWeatherIcon id)
{
    uint8_t row = (uint8_t)((id >> 4) & 0xF);
    uint8_t col = (uint8_t)(id & 0xF);
    return (FmRect){
        .x = (uint16_t)(col * FM_ICON_W),
        .y = (uint16_t)(row * FM_ICON_H),
        .w = FM_ICON_W, .h = FM_ICON_H,
    };
}

/* ── Controller icon sheet ──────────────────────────────── */
typedef enum {
    FM_CTRL_CIRCLE_EMPTY = 0x00,
    FM_CTRL_CIRCLE_FILL  = 0x01,
    FM_CTRL_CIRCLE_HALF  = 0x02,
    FM_CTRL_PACMAN       = 0x03,
    FM_CTRL_DPAD_FULL    = 0x14,
    FM_CTRL_RT           = 0x0A,
    FM_CTRL_LT           = 0x0B,
    FM_CTRL_R2           = 0x0C,
    FM_CTRL_L2           = 0x0D,
    FM_CTRL_LB           = 0x1A,
    FM_CTRL_L1           = 0x1D,
    FM_CTRL_GAMEPAD_NES  = 0x2A,
    FM_CTRL_GAMEPAD_SNES = 0x2B,
    FM_CTRL_JOYSTICK     = 0x2C,
    FM_CTRL_ARCADE       = 0x2D,
    FM_CTRL_SWITCH       = 0x2E,
    FM_CTRL_BTN_A        = 0x40,
    FM_CTRL_BTN_B        = 0x41,
    FM_CTRL_BTN_X        = 0x42,
    FM_CTRL_BTN_Y        = 0x43,
    FM_CTRL_PS_CROSS     = 0x60,
    FM_CTRL_PS_CIRCLE    = 0x61,
    FM_CTRL_PS_SQUARE    = 0x62,
    FM_CTRL_PS_TRIANGLE  = 0x63,
    FM_CTRL_BTN_START    = 0x70,
    FM_CTRL_BTN_SELECT   = 0x71,
    FM_CTRL_BTN_STOP     = 0x72,
    FM_CTRL_BTN_MENU     = 0x73,
} FmCtrlIcon;

static inline FmRect FM_CTRL_RECT(FmCtrlIcon id)
{
    uint8_t row = (uint8_t)((id >> 4) & 0xF);
    uint8_t col = (uint8_t)(id & 0xF);
    return (FmRect){
        .x = (uint16_t)(col * FM_ICON_W),
        .y = (uint16_t)(row * FM_ICON_H),
        .w = FM_ICON_W, .h = FM_ICON_H,
    };
}

/* ── Generic icon cell accessor (row, col) ──────────────── */
static inline FmRect FM_ICON_CELL(uint8_t row, uint8_t col)
{
    return (FmRect){
        .x = (uint16_t)(col * FM_ICON_W),
        .y = (uint16_t)(row * FM_ICON_H),
        .w = FM_ICON_W, .h = FM_ICON_H,
    };
}
