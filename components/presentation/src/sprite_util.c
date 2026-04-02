/**
 * sprite_util.c — FiestaQuest Presentation Layer: Sprite & Text Utilities
 *
 * Implements 2x-scaled sprite blitting, 2x-scaled text rendering,
 * inverted-text rendering, and the header/footer bar helpers shared by all
 * screen renderers.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 */

#include "sprite_util.h"
#include "fq_framebuffer.h"
#include "fq_sprite.h"
#include "fq_text.h"
#include <stdint.h>
#include <stddef.h>

/* ── fq_blit_sprite_2x ───────────────────────────────────────────────────── */

void fq_blit_sprite_2x(fq_fb_t *fb,
                       int16_t x,
                       int16_t y,
                       const fq_sprite_t *spr)
{
    if (fb == NULL || spr == NULL || spr->data == NULL) { return; }
    if (spr->width == 0u || spr->height == 0u)          { return; }

    uint16_t stride = (uint16_t)(((uint32_t)spr->width + 7u) / 8u);

    for (uint16_t row = 0u; row < spr->height; row++) {
        for (uint16_t col = 0u; col < spr->width; col++) {
            uint16_t byte_idx = (uint16_t)((uint32_t)row * stride + col / 8u);
            uint8_t  bit      = (uint8_t)((spr->data[byte_idx] >> (7u - (col % 8u))) & 1u);
            if (bit != 0u) {
                int16_t dx = (int16_t)(x + (int16_t)(col * 2u));
                int16_t dy = (int16_t)(y + (int16_t)(row * 2u));
                fq_fb_set_pixel(fb, dx,           dy,           1u);
                fq_fb_set_pixel(fb, (int16_t)(dx + 1), dy,           1u);
                fq_fb_set_pixel(fb, dx,           (int16_t)(dy + 1), 1u);
                fq_fb_set_pixel(fb, (int16_t)(dx + 1), (int16_t)(dy + 1), 1u);
            }
        }
    }
}

/* ── fq_draw_text_2x ─────────────────────────────────────────────────────── */
/*
 * Renders each glyph at 2x scale: each source pixel becomes a 2x2 block on
 * the framebuffer. The cursor advances by advance_width * 2 after each glyph.
 *
 * Implementation notes:
 *   - glyph bitmap indexing is identical to fq_draw_text (g * glyph_h * row_bytes).
 *   - For each source (col, row) where the bit is SET, write 2x2 block at
 *     (blit_x + col*2, blit_y + row*2) on the framebuffer.
 *   - fq_fb_set_pixel handles all clipping — no explicit bounds check needed here.
 *   - The cursor is int16_t; multiplying uint8_t advance_width by 2 never
 *     overflows: max advance = 255, 255*2 = 510 < INT16_MAX (32767).
 */
int16_t fq_draw_text_2x(fq_fb_t         *fb,
                        const fq_font_t *font,
                        int16_t          x,
                        int16_t          y,
                        const char      *str)
{
    if (fb == NULL || font == NULL || str == NULL) { return x; }

    uint32_t    row_bytes = ((uint32_t)font->glyph_max_w + 7u) / 8u;
    int16_t     cursor    = x;
    const char *p         = str;

    while (*p != '\0') {
        unsigned char ch = (unsigned char)*p;

        /* Skip non-printable characters. */
        if (ch < 0x20u || ch > 0x7Eu) {
            p++;
            continue;
        }

        uint8_t glyph_idx = (uint8_t)(ch - 0x20u);
        uint8_t adv_width = font->widths[glyph_idx];
        int8_t  off_x     = font->offsets_x[glyph_idx];
        int8_t  off_y     = font->offsets_y[glyph_idx];

        uint32_t glyph_offset = (uint32_t)glyph_idx
                                 * (uint32_t)font->glyph_h
                                 * row_bytes;

        /* Glyph render origin: apply font offsets once (not scaled). */
        int16_t blit_x = (int16_t)(cursor + (int16_t)off_x);
        int16_t blit_y = (int16_t)(y      + (int16_t)off_y);

        /* Blit each glyph pixel as a 2x2 block. */
        for (uint8_t gy = 0u; gy < font->glyph_h; gy++) {
            for (uint8_t gx = 0u; gx < font->glyph_max_w; gx++) {
                uint32_t bidx = glyph_offset
                                + (uint32_t)gy * row_bytes
                                + gx / 8u;
                uint8_t gbit  = (uint8_t)((font->bitmap[bidx] >> (7u - (gx % 8u))) & 1u);
                if (gbit != 0u) {
                    int16_t px = (int16_t)(blit_x + (int16_t)((uint16_t)gx * 2u));
                    int16_t py = (int16_t)(blit_y + (int16_t)((uint16_t)gy * 2u));
                    fq_fb_set_pixel(fb, px,                    py,                    1u);
                    fq_fb_set_pixel(fb, (int16_t)(px + 1),    py,                    1u);
                    fq_fb_set_pixel(fb, px,                    (int16_t)(py + 1),    1u);
                    fq_fb_set_pixel(fb, (int16_t)(px + 1),    (int16_t)(py + 1),    1u);
                }
            }
        }

        /* Advance cursor by 2 * advance_width. */
        cursor = (int16_t)(cursor + (int16_t)((uint16_t)adv_width * 2u));

        p++;
    }

    return cursor;
}

/* ── fq_draw_text_inverted ───────────────────────────────────────────────── */

int16_t fq_draw_text_inverted(fq_fb_t         *fb,
                               const fq_font_t *font,
                               int16_t          x,
                               int16_t          y,
                               const char      *str)
{
    if (fb == NULL || font == NULL || str == NULL) { return x; }

    uint32_t row_bytes = ((uint32_t)font->glyph_max_w + 7u) / 8u;
    int16_t  cursor    = x;
    const char *p      = str;

    while (*p != '\0') {
        unsigned char ch = (unsigned char)*p;

        if (ch < 0x20u || ch > 0x7Eu) {
            p++;
            continue;
        }

        if (cursor >= (int16_t)FQ_FB_WIDTH) { break; }

        uint8_t glyph_idx = (uint8_t)(ch - 0x20u);
        uint8_t adv_width = font->widths[glyph_idx];
        int8_t  off_x     = font->offsets_x[glyph_idx];
        int8_t  off_y     = font->offsets_y[glyph_idx];

        uint32_t glyph_offset = (uint32_t)glyph_idx
                                 * (uint32_t)font->glyph_h
                                 * row_bytes;

        /* Draw each glyph pixel using CLEAR instead of SET. */
        int16_t blit_x = (int16_t)(cursor + (int16_t)off_x);
        int16_t blit_y = (int16_t)(y      + (int16_t)off_y);

        for (uint8_t gy = 0u; gy < font->glyph_h; gy++) {
            for (uint8_t gx = 0u; gx < font->glyph_max_w; gx++) {
                uint32_t bidx     = glyph_offset
                                    + (uint32_t)gy * row_bytes
                                    + gx / 8u;
                uint8_t  gbit     = (uint8_t)((font->bitmap[bidx] >> (7u - (gx % 8u))) & 1u);
                if (gbit != 0u) {
                    fq_fb_set_pixel(fb,
                                    (int16_t)(blit_x + (int16_t)gx),
                                    (int16_t)(blit_y + (int16_t)gy),
                                    0u);  /* CLEAR = white on black */
                }
            }
        }

        cursor = (int16_t)(cursor + (int16_t)adv_width);
        p++;
    }

    return cursor;
}

/* ── fq_draw_header_bar ──────────────────────────────────────────────────── */

void fq_draw_header_bar(fq_fb_t         *fb,
                        const fq_font_t *font,
                        int16_t          y,
                        int16_t          h,
                        const char      *text)
{
    if (fb == NULL) { return; }
    /* Fill the bar black. */
    fq_fb_fill_rect(fb, 0, y, (int16_t)FQ_FB_WIDTH, h, 1u);
    /* Draw white text at a 2px vertical margin inside the bar. */
    if (font != NULL && text != NULL) {
        fq_draw_text_inverted(fb, font, 5, (int16_t)(y + 2), text);
    }
}

/* ── fq_draw_header_bar2 ─────────────────────────────────────────────────── */

void fq_draw_header_bar2(fq_fb_t         *fb,
                         const fq_font_t *font,
                         int16_t          y,
                         int16_t          h,
                         const char      *left_txt,
                         const char      *right_txt)
{
    if (fb == NULL) { return; }
    /* Fill the bar black. */
    fq_fb_fill_rect(fb, 0, y, (int16_t)FQ_FB_WIDTH, h, 1u);
    if (font == NULL) { return; }
    /* Left-aligned white text. */
    if (left_txt != NULL) {
        fq_draw_text_inverted(fb, font, 5, (int16_t)(y + 2), left_txt);
    }
    /* Right-aligned white text: measure width, then draw from right margin. */
    if (right_txt != NULL) {
        int16_t tw = fq_text_width(font, right_txt);
        int16_t rx = (int16_t)((int16_t)FQ_FB_WIDTH - tw - 5);
        if (rx < 5) { rx = 5; }
        fq_draw_text_inverted(fb, font, rx, (int16_t)(y + 2), right_txt);
    }
}
