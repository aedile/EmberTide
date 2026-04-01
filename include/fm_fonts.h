/**
 * fm_fonts.h — FIESTAMON Font Glyph Map
 *
 * All font sheets: 570×150 px, 19 cols × 5 rows, 30×30 px per cell.
 * Covers ASCII 0x20 (space) → 0x7E (~), 95 glyphs total.
 *
 * Glyphs: white on transparent background.
 * For e-paper: transparent = white pixel, white glyph = black pixel.
 *
 * Advance widths are computed from the rightmost non-transparent
 * pixel column + 1 px inter-glyph gap.
 */
#pragma once
#include <stdint.h>
#include "fm_chars.h"   /* FmRect */

/* ── Sheet constants ─────────────────────────────────────── */
#define FM_FONT_CELL_W   30u   /* cell width  in the PNG */
#define FM_FONT_CELL_H   30u   /* cell height in the PNG */
#define FM_FONT_COLS     19u
#define FM_FONT_ROWS      5u
#define FM_FONT_GLYPHS   95u   /* 0x20..0x7E */
#define FM_FONT_ASCII0   0x20u /* first encoded character */

/* ── Font selector ───────────────────────────────────────── */
typedef enum {
    FM_FONT_REGS_12   = 0,   /* Press Start 2P  @ 12 px */
    FM_FONT_REGS_18   = 1,   /* Press Start 2P  @ 18 px */
    FM_FONT_REGS_24   = 2,   /* Press Start 2P  @ 24 px */
    FM_FONT_SCRIPT_24 = 3,   /* Jacquard 12     @ 24 px */
    FM_FONT_SCRIPT_36 = 4,   /* Jacquard 12     @ 36 px */
    FM_FONT_COUNT
} FmFontId;

/* ── Per-glyph advance width tables ─────────────────────────
 * Index = char_code - 0x20  (0 = space, 94 = '~')
 * FONT_REGS_* are effectively fixed-width except 'j' and space.
 * FONT_SCRIPT_* are fully variable.
 */
static const uint8_t FM_FONT_ADVANCE[FM_FONT_COUNT][FM_FONT_GLYPHS] = {

/* ── FONT_REGS_12 — Press Start 2P @ 12 px ─────────────── */
[FM_FONT_REGS_12] = {
/*       sp !  "  #  $  %  &  '  (  )  *  +  ,  -  .  /  */
         15,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
/*   0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?  @  A */
     20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
/*   B  C  D  E  F  G  H  I  J  K  L  M  N  O  P  Q  R  S */
     20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
/*   T  U  V  W  X  Y  Z  [  \  ]  ^  _  `  a  b  c  d  e */
     20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,
/*   f  g  h  i  j  k  l  m  n  o  p  q  r  s  t  u  v  w */
     20,20,20,20,19,20,20,20,20,20,20,20,20,20,20,20,20,20,
/*   x  y  z  {  |  }  ~  */
     20,20,20,20,20,20,20,
},

/* ── FONT_REGS_18 — Press Start 2P @ 18 px ─────────────── */
[FM_FONT_REGS_18] = {
     15,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
     23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
     23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
     23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,
     23,23,23,23,20,23,23,23,23,23,23,23,23,23,23,23,23,23,
     23,23,23,23,23,23,23,
},

/* ── FONT_REGS_24 — Press Start 2P @ 24 px ─────────────── */
[FM_FONT_REGS_24] = {
     15,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
     25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
     25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
     25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
     25,25,25,25,22,25,25,25,25,25,25,25,25,25,25,25,25,25,
     25,25,25,25,25,25,25,
},

/* ── FONT_SCRIPT_24 — Jacquard 12 @ 24 px ──────────────── */
[FM_FONT_SCRIPT_24] = {
/*sp  !   "   #   $   %   &   '   (   )   *   +   ,   -   .   / */
  15, 12, 17, 22, 20, 22, 22, 12, 16, 16, 18, 20, 12, 16, 12, 18,
/* 0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ?   @   A */
  20, 18, 20, 19, 20, 19, 19, 20, 20, 19, 12, 12, 18, 18, 18, 18, 24, 24,
/* B   C   D   E   F   G   H   I   J   K   L   M   N   O   P   Q   R   S */
  24, 23, 22, 23, 24, 23, 24, 23, 23, 24, 23, 24, 24, 24, 23, 24, 24, 23,
/* T   U   V   W   X   Y   Z   [   \   ]   ^   _   `   a   b   c   d   e */
  25, 24, 25, 24, 24, 24, 23, 16, 16, 16, 18, 18, 14, 21, 21, 20, 20, 20,
/* f   g   h   i   j   k   l   m   n   o   p   q   r   s   t   u   v   w */
  20, 21, 21, 19, 19, 21, 19, 24, 21, 21, 21, 21, 20, 20, 19, 22, 21, 24,
/* x   y   z   {   |   }   ~  */
  21, 21, 21, 16, 12, 16, 20,
},

/* ── FONT_SCRIPT_36 — Jacquard 12 @ 36 px ──────────────── */
[FM_FONT_SCRIPT_36] = {
/*sp  !   "   #   $   %   &   '   (   )   *   +   ,   -   .   / */
  15, 14, 20, 26, 22, 26, 26, 14, 20, 20, 22, 24, 14, 18, 14, 22,
/* 0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ?   @   A */
  22, 19, 22, 20, 23, 20, 20, 23, 22, 20, 14, 14, 22, 22, 22, 22, 28, 28,
/* B   C   D   E   F   G   H   I   J   K   L   M   N   O   P   Q   R   S */
  28, 26, 25, 26, 28, 27, 28, 26, 26, 29, 27, 29, 28, 28, 27, 29, 29, 27,
/* T   U   V   W   X   Y   Z   [   \   ]   ^   _   `   a   b   c   d   e */
  30, 29, 30, 29, 28, 28, 26, 18, 18, 18, 22, 22, 16, 23, 23, 22, 23, 23,
/* f   g   h   i   j   k   l   m   n   o   p   q   r   s   t   u   v   w */
  22, 23, 23, 20, 20, 23, 20, 28, 24, 23, 24, 23, 23, 23, 20, 25, 24, 28,
/* x   y   z   {   |   }   ~  */
  24, 24, 23, 18, 14, 18, 24,
},

}; /* end FM_FONT_ADVANCE */

/* ── Glyph rect accessor ─────────────────────────────────── */

/**
 * FM_GLYPH_RECT — source rect for one glyph in the given font sheet.
 *
 * @param font   FmFontId
 * @param ch     ASCII character (0x20..0x7E)
 * @return FmRect {x, y, advance_width, FM_FONT_CELL_H}
 *
 * Blit the full cell height (30 px); the advance width tells you
 * how far to advance the cursor for the next character.
 */
static inline FmRect FM_GLYPH_RECT(FmFontId font, char ch)
{
    uint8_t idx = (uint8_t)((uint8_t)ch - FM_FONT_ASCII0);
    if (idx >= FM_FONT_GLYPHS) idx = 0; /* default to space */
    uint8_t col = idx % FM_FONT_COLS;
    uint8_t row = idx / FM_FONT_COLS;
    return (FmRect){
        .x = (uint16_t)(col * FM_FONT_CELL_W),
        .y = (uint16_t)(row * FM_FONT_CELL_H),
        .w = FM_FONT_ADVANCE[font][idx],
        .h = FM_FONT_CELL_H,
    };
}

/**
 * FM_TEXT_WIDTH — compute total pixel width of a string.
 *
 * @param font   FmFontId
 * @param str    null-terminated ASCII string
 * @return total advance width in pixels
 */
static inline uint16_t FM_TEXT_WIDTH(FmFontId font, const char* str)
{
    uint16_t w = 0;
    for (; *str; ++str) {
        uint8_t idx = (uint8_t)((uint8_t)*str - FM_FONT_ASCII0);
        if (idx < FM_FONT_GLYPHS) w += FM_FONT_ADVANCE[font][idx];
    }
    return w;
}
