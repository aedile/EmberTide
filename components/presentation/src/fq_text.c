/**
 * fq_text.c — FiestaQuest Presentation Layer: Variable-Width Font Renderer
 *
 * Renders ASCII 0x20-0x7E text onto a framebuffer using an fq_font_t.
 *
 * Rendering model:
 *   1. For each character in the string:
 *      a. If char is outside [0x20, 0x7E], skip it (no cursor advance).
 *      b. If cursor position >= FQ_FB_WIDTH, stop (clipping at right edge).
 *      c. Compute glyph_index = ch - 0x20 (0..94).
 *      d. Look up advance width, offset_x, offset_y.
 *      e. Compute bitmap offset in the font's packed data.
 *      f. OR-blit via fq_blit_sprite at (cursor + offset_x, y + offset_y).
 *      g. Advance cursor by advance_width.
 *
 * Bitmap layout:
 *   Each glyph occupies glyph_h rows of ceil(glyph_max_w / 8) bytes.
 *   Glyph g's data starts at: g * glyph_h * row_bytes.
 *
 * No floating-point. No malloc. Pure function.
 * HOST-COMPILABLE — no hal_*.h.
 */

#include "fq_text.h"
#include "fq_sprite.h"
#include "fq_framebuffer.h"
#include <stdint.h>
#include <stddef.h>

/* ── ASCII range constants ────────────────────────────────────────────── */

#define FQ_TEXT_ASCII_MIN  0x20u   /* space (inclusive) */
#define FQ_TEXT_ASCII_MAX  0x7Eu   /* tilde (inclusive) */

/* ── fq_text_width ────────────────────────────────────────────────────── */

int16_t fq_text_width(const fq_font_t *font, const char *str)
{
    if (font == NULL || str == NULL) { return (int16_t)0; }

    int16_t width = 0;
    const char *p = str;

    while (*p != '\0') {
        unsigned char ch = (unsigned char)*p;
        if (ch >= FQ_TEXT_ASCII_MIN && ch <= FQ_TEXT_ASCII_MAX) {
            uint8_t glyph_idx = (uint8_t)(ch - FQ_TEXT_ASCII_MIN);
            width = (int16_t)(width + (int16_t)font->widths[glyph_idx]);
        }
        p++;
    }

    return width;
}

/* ── fq_draw_text ─────────────────────────────────────────────────────── */

int16_t fq_draw_text(fq_fb_t         *fb,
                     const fq_font_t *font,
                     int16_t          x,
                     int16_t          y,
                     const char      *str)
{
    if (fb == NULL || font == NULL || str == NULL) { return x; }

    /* Bytes per glyph row in the packed bitmap. */
    uint32_t row_bytes = ((uint32_t)font->glyph_max_w + 7u) / 8u;

    int16_t cursor = x;
    const char *p  = str;

    while (*p != '\0') {
        unsigned char ch = (unsigned char)*p;

        /* Skip non-printable characters. */
        if (ch < FQ_TEXT_ASCII_MIN || ch > FQ_TEXT_ASCII_MAX) {
            p++;
            continue;
        }

        /* Stop rendering at right edge. */
        if (cursor >= (int16_t)FQ_FB_WIDTH) { break; }

        uint8_t glyph_idx   = (uint8_t)(ch - FQ_TEXT_ASCII_MIN);
        uint8_t adv_width   = font->widths[glyph_idx];
        int8_t  off_x       = font->offsets_x[glyph_idx];
        int8_t  off_y       = font->offsets_y[glyph_idx];

        /* Bitmap pointer for this glyph. */
        uint32_t glyph_offset = (uint32_t)glyph_idx
                                 * (uint32_t)font->glyph_h
                                 * row_bytes;

        /* Build sprite descriptor (points into the font's static data). */
        fq_sprite_t glyph_sprite;
        glyph_sprite.data   = font->bitmap + glyph_offset;
        glyph_sprite.width  = font->glyph_max_w;
        glyph_sprite.height = font->glyph_h;

        /* Blit the glyph. Clipping is handled by fq_blit_sprite. */
        fq_blit_sprite(fb,
                       (int16_t)(cursor + (int16_t)off_x),
                       (int16_t)(y      + (int16_t)off_y),
                       &glyph_sprite);

        /* Advance cursor. */
        cursor = (int16_t)(cursor + (int16_t)adv_width);

        p++;
    }

    return cursor;
}
