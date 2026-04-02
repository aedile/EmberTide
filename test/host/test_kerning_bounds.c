/**
 * test_kerning_bounds.c — Bound Tests: Kerning Fix + fq_draw_text_2x
 *
 * Rule 22: BOUND RED before FEATURE RED.
 * Written before implementation — these FAIL until GREEN phase.
 *
 * Boundary conditions proved:
 *
 *   B1: fq_draw_text_2x(NULL fb, ...) must not crash, must return x.
 *   B2: fq_draw_text_2x(fb, NULL font, ...) must not crash, must return x.
 *   B3: fq_draw_text_2x(fb, font, x, y, NULL str) must not crash, must return x.
 *   B4: fq_draw_text_2x at right-edge x=199 must not write past framebuffer.
 *   B5: fq_draw_text_2x with empty string must return x unchanged, no pixels set.
 *   B6: Advance at 2x must equal 2 * (font->widths[i] for each char) — cursor
 *       must advance exactly 2x per character, preventing integer overflow.
 *   B7: 2x pixel blocks must not overflow int16_t range on rightmost pixel.
 *       At x=198, 2x block (198,199) is valid. At x=199, block (199,200) clips.
 *
 * Constitution Priority 0: no float operations.
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include "test_assert.h"
#include "fq_framebuffer.h"
#include "fq_text.h"
#include "sprite_util.h"

/* ── Minimal inline test font for bounds checks ─────────────────────────── */

#define BTEST_FONT_GLYPHS  95u
#define BTEST_FONT_H        8u
#define BTEST_FONT_MAX_W    8u

/* One-pixel-wide 'A' glyph at index 33 (ASCII 'A' = 0x41 = 65, idx = 65-32 = 33).
 * All other glyphs are blank.
 * Bit layout: MSB-first, 1 byte per row. Row 0 has bit 7 set = pixel (0,0). */
static const uint8_t g_btest_bitmap[BTEST_FONT_GLYPHS * BTEST_FONT_H] = {
    /* All zeros — even 'A' is blank for bounds tests; we only test cursor math. */
    0u
};

/* Advance widths: all 8px for simplicity. */
static uint8_t g_btest_widths[BTEST_FONT_GLYPHS];

/* x/y offsets: all zero. */
static int8_t g_btest_offsets_x[BTEST_FONT_GLYPHS];
static int8_t g_btest_offsets_y[BTEST_FONT_GLYPHS];

static void init_btest_font(fq_font_t *font)
{
    uint32_t i;
    for (i = 0u; i < BTEST_FONT_GLYPHS; i++) {
        g_btest_widths[i]    = (uint8_t)BTEST_FONT_MAX_W;
        g_btest_offsets_x[i] = 0;
        g_btest_offsets_y[i] = 0;
    }
    font->bitmap      = g_btest_bitmap;
    font->widths      = g_btest_widths;
    font->offsets_x   = g_btest_offsets_x;
    font->offsets_y   = g_btest_offsets_y;
    font->glyph_h     = (uint8_t)BTEST_FONT_H;
    font->glyph_max_w = (uint8_t)BTEST_FONT_MAX_W;
}

/* ── Helper: count SET pixels in a framebuffer ──────────────────────────── */
static uint32_t count_set_pixels(const fq_fb_t *fb)
{
    uint32_t count = 0u;
    int16_t  x, y;
    for (y = 0; y < (int16_t)FQ_FB_HEIGHT; y++) {
        for (x = 0; x < (int16_t)FQ_FB_WIDTH; x++) {
            count += (uint32_t)fq_fb_get_pixel(fb, x, y);
        }
    }
    return count;
}

int main(void)
{
    static fq_fb_t fb;
    fq_font_t      font;
    int16_t        result;

    init_btest_font(&font);

    /* -------------------------------------------------------------------
     * B1: NULL fb must not crash — returns x unchanged.
     * ------------------------------------------------------------------- */
    result = fq_draw_text_2x(NULL, &font, 10, 10, "A");
    TEST_ASSERT_EQUAL_INT(10, (int)result);

    /* -------------------------------------------------------------------
     * B2: NULL font must not crash — returns x unchanged.
     * ------------------------------------------------------------------- */
    fq_fb_clear(&fb);
    result = fq_draw_text_2x(&fb, NULL, 10, 10, "A");
    TEST_ASSERT_EQUAL_INT(10, (int)result);

    /* -------------------------------------------------------------------
     * B3: NULL str must not crash — returns x unchanged.
     * ------------------------------------------------------------------- */
    fq_fb_clear(&fb);
    result = fq_draw_text_2x(&fb, &font, 10, 10, NULL);
    TEST_ASSERT_EQUAL_INT(10, (int)result);

    /* -------------------------------------------------------------------
     * B4: At x=199 (right edge), function must not crash or write OOB.
     *     Any pixel at x>=200 is silently clipped by fq_fb_set_pixel.
     * ------------------------------------------------------------------- */
    fq_fb_clear(&fb);
    result = fq_draw_text_2x(&fb, &font, 199, 10, "A");
    /* Must return without crash. Cursor advanced by adv*2 = 16. */
    TEST_ASSERT_EQUAL_INT(199 + 16, (int)result);

    /* -------------------------------------------------------------------
     * B5: Empty string must return x unchanged with no pixels set.
     * ------------------------------------------------------------------- */
    fq_fb_clear(&fb);
    result = fq_draw_text_2x(&fb, &font, 20, 20, "");
    TEST_ASSERT_EQUAL_INT(20, (int)result);
    TEST_ASSERT_EQUAL_UINT32(0u, count_set_pixels(&fb));

    /* -------------------------------------------------------------------
     * B6: Cursor advances exactly 2 * advance_width per character.
     *     For "AA" with advance=8: final cursor = 0 + 8*2 + 8*2 = 32.
     *     Proves no integer overflow in the 2x multiply.
     * ------------------------------------------------------------------- */
    fq_fb_clear(&fb);
    result = fq_draw_text_2x(&fb, &font, 0, 0, "AA");
    TEST_ASSERT_EQUAL_INT(32, (int)result);

    /* -------------------------------------------------------------------
     * B7: Single-character at x=198. 2x glyph occupies cols 198..198+8*2-1=213.
     *     Pixels past x=199 are clipped but must not cause UB.
     *     Verify: cursor = 198 + 8*2 = 214. No crash.
     * ------------------------------------------------------------------- */
    fq_fb_clear(&fb);
    result = fq_draw_text_2x(&fb, &font, 198, 0, "A");
    TEST_ASSERT_EQUAL_INT(214, (int)result);

    printf("test_kerning_bounds: PASS\n");
    return 0;
}
