/**
 * fq_text.h — FiestaQuest Presentation Layer: Variable-Width Font Renderer
 *
 * Renders ASCII text (0x20 to 0x7E inclusive) onto a framebuffer using a
 * caller-supplied fq_font_t descriptor.
 *
 * Characters outside the printable ASCII range (< 0x20 or > 0x7E) are
 * silently skipped — they do not advance the cursor and write no pixels.
 *
 * Rendering stops when the next glyph's start position would be past x=199
 * (i.e., the cursor has reached the right edge of the display).
 *
 * Cursor type is int16_t (not uint8_t) so it can correctly represent values
 * above 200 for fq_text_width measurements and multi-glyph strings.
 *
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 */

#ifndef FIESTAQUEST_PRESENTATION_FQ_TEXT_H
#define FIESTAQUEST_PRESENTATION_FQ_TEXT_H

#include "fq_framebuffer.h"
#include <stdint.h>

/* ── Font descriptor ──────────────────────────────────────────────────── */

/**
 * fq_font_t — Describes a 1-bit packed variable-width bitmap font.
 *
 * Covers ASCII 0x20 (space) through 0x7E ('~'), 95 glyphs total.
 * Glyph index = character_code - 0x20.
 *
 * Glyph bitmap layout:
 *   - Row-major, MSB first.
 *   - Each row occupies ceil(glyph_max_w / 8) bytes.
 *   - Glyph g starts at offset: g * glyph_h * ceil(glyph_max_w / 8) bytes.
 *
 * @field bitmap       Packed 1-bit glyph bitmaps (95 × glyph_h rows).
 * @field widths       Advance width per glyph in pixels (95 entries).
 * @field offsets_x    X render offset per glyph (signed, for spacing).
 * @field offsets_y    Y render offset per glyph (signed, for descenders).
 * @field glyph_h      Height of each glyph in pixels.
 * @field glyph_max_w  Max width of any glyph in pixels (determines row bytes).
 */
typedef struct {
    const uint8_t *bitmap;      /**< Packed 1-bit glyph bitmaps.         */
    const uint8_t *widths;      /**< Advance width per glyph (95 entries).*/
    const int8_t  *offsets_x;   /**< X offset per glyph.                 */
    const int8_t  *offsets_y;   /**< Y offset per glyph (for descenders).*/
    uint8_t        glyph_h;     /**< Height of each glyph in pixels.     */
    uint8_t        glyph_max_w; /**< Max glyph width (sets row byte size).*/
} fq_font_t;

/* ── Text rendering API ───────────────────────────────────────────────── */

/**
 * fq_draw_text — Render a string onto a framebuffer.
 *
 * Each printable ASCII character (0x20-0x7E) is OR-blitted via fq_blit_sprite
 * at the current cursor position. The cursor advances by the glyph's advance
 * width after each character.
 *
 * Rendering stops when cursor >= FQ_FB_WIDTH (200). Characters that start
 * at x >= 200 are not drawn; the cursor may exceed 200 after the last drawn
 * glyph's advance width is applied.
 *
 * Non-printable characters are silently skipped (cursor not advanced).
 * NULL fb, NULL font, or NULL str: returns x unchanged without crashing.
 *
 * @param fb    Target framebuffer. NULL-safe.
 * @param font  Font descriptor. NULL-safe.
 * @param x     Starting X cursor position.
 * @param y     Starting Y cursor position.
 * @param str   Null-terminated string. NULL-safe.
 * @return      Final cursor X position after rendering all characters.
 */
int16_t fq_draw_text(fq_fb_t         *fb,
                     const fq_font_t *font,
                     int16_t          x,
                     int16_t          y,
                     const char      *str);

/**
 * fq_text_width — Measure the pixel width of a string without rendering.
 *
 * Sums advance widths for all printable ASCII characters in the string.
 * Non-printable characters contribute zero width.
 * No clipping is applied — the returned value may exceed FQ_FB_WIDTH.
 *
 * NULL font or NULL str: returns 0.
 *
 * @param font  Font descriptor. NULL-safe.
 * @param str   Null-terminated string. NULL-safe.
 * @return      Total pixel width as int16_t. May exceed 200 for long strings.
 */
int16_t fq_text_width(const fq_font_t *font, const char *str);

#endif /* FIESTAQUEST_PRESENTATION_FQ_TEXT_H */
