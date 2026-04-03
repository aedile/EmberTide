/**
 * test_kerning_feature.c — Feature Tests: Kerning Fix + fq_draw_text_2x
 *
 * Happy-path contracts for the title screen kerning fix:
 *
 *   F1: fq_draw_text_2x renders pixels (non-zero framebuffer) for a visible glyph.
 *   F2: fq_draw_text_2x advances cursor by 2 * advance_width per character.
 *   F3: fq_draw_text_2x pixel at (x, y) AND (x+1, y) AND (x, y+1) AND (x+1, y+1)
 *       are all SET for a source glyph pixel at position (col, row) — 2x2 block.
 *   F4: Corrected FONT_SCRIPT_36 advance widths: "Ember" width >= 80px and
 *       <= 180px (wider than broken widths, fits on 200px line).
 *   F5: Corrected FONT_SCRIPT_36: "Tide" width >= 60px and <= 160px.
 *   F6: Corrected FONT_SCRIPT_36: "EmberTide" total width > 100px (no longer
 *       using the narrow broken advance widths of ~99px).
 *   F7: fq_draw_text_2x returns final cursor > start x after drawing "A".
 *   F8: fq_get_font_title() still returns non-NULL (no regression).
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
#include "asset_data.h"

/* ── Minimal test font with a visible 'A' glyph ────────────────────────── */

#define FTEST_FONT_GLYPHS  95u
#define FTEST_FONT_H        8u
#define FTEST_FONT_MAX_W    8u

/*
 * Glyph for 'A' (index = 'A'-' ' = 33):
 * Row 0: 0xFF = 11111111 — all 8 pixels set.
 * Rows 1-7: 0x00.
 *
 * When rendered at (x, y) via fq_draw_text_2x:
 *   - Source pixel (0,0) is SET → 2x2 block at fb (x+0,y+0)..(x+1,y+1) all SET.
 *   - Source pixel (7,0) is SET → 2x2 block at fb (x+14,y+0)..(x+15,y+1) all SET.
 */
static uint8_t g_ftest_bitmap[FTEST_FONT_GLYPHS * FTEST_FONT_H];
static uint8_t g_ftest_widths[FTEST_FONT_GLYPHS];
static int8_t  g_ftest_offsets_x[FTEST_FONT_GLYPHS];
static int8_t  g_ftest_offsets_y[FTEST_FONT_GLYPHS];

static void init_ftest_font(fq_font_t *font)
{
    uint32_t i;
    memset(g_ftest_bitmap,    0, sizeof(g_ftest_bitmap));
    memset(g_ftest_widths,    0, sizeof(g_ftest_widths));
    memset(g_ftest_offsets_x, 0, sizeof(g_ftest_offsets_x));
    memset(g_ftest_offsets_y, 0, sizeof(g_ftest_offsets_y));

    /* All glyphs advance by 8. */
    for (i = 0u; i < FTEST_FONT_GLYPHS; i++) {
        g_ftest_widths[i] = (uint8_t)FTEST_FONT_MAX_W;
    }

    /* 'A' (index 33): row 0 = 0xFF (all 8 bits set). */
    g_ftest_bitmap[33u * FTEST_FONT_H + 0u] = 0xFFu;

    font->bitmap      = g_ftest_bitmap;
    font->widths      = g_ftest_widths;
    font->offsets_x   = g_ftest_offsets_x;
    font->offsets_y   = g_ftest_offsets_y;
    font->glyph_h     = (uint8_t)FTEST_FONT_H;
    font->glyph_max_w = (uint8_t)FTEST_FONT_MAX_W;
}

int main(void)
{
    /* All declarations at top of scope (C99 strict). */
    static fq_fb_t      fb;
    fq_font_t           font;
    int16_t             result;
    uint32_t            found;
    int16_t             scan_x;
    int16_t             scan_y;
    const fq_font_t    *font_title;
    int16_t             w_ember;
    int16_t             w_tide;
    int16_t             w_total;
    int16_t             start_x;

    init_ftest_font(&font);

    /* -------------------------------------------------------------------
     * F1: Drawing "A" must produce non-zero pixels in the framebuffer.
     * ------------------------------------------------------------------- */
    fq_fb_clear(&fb);
    result = fq_draw_text_2x(&fb, &font, 0, 0, "A");
    TEST_ASSERT_TRUE(result > 0);

    /* Verify at least one pixel was set. */
    found = 0u;
    for (scan_y = 0; scan_y < (int16_t)FQ_FB_HEIGHT && found == 0u; scan_y++) {
        for (scan_x = 0; scan_x < (int16_t)FQ_FB_WIDTH; scan_x++) {
            if (fq_fb_get_pixel(&fb, scan_x, scan_y) != 0u) {
                found = 1u;
                break;
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1u, found);

    /* -------------------------------------------------------------------
     * F2: Cursor advances by 2 * advance_width per character.
     *     draw "A" from x=0, advance = 8 * 2 = 16. Final cursor = 16.
     * ------------------------------------------------------------------- */
    fq_fb_clear(&fb);
    result = fq_draw_text_2x(&fb, &font, 0, 0, "A");
    TEST_ASSERT_EQUAL_INT(16, (int)result);

    /* draw "AA" from x=0, final cursor = 32. */
    fq_fb_clear(&fb);
    result = fq_draw_text_2x(&fb, &font, 0, 0, "AA");
    TEST_ASSERT_EQUAL_INT(32, (int)result);

    /* -------------------------------------------------------------------
     * F3: 2x2 block test.
     *     'A' row 0 has bit 7 set → source pixel (col=0, row=0).
     *     Rendered at (x=10, y=5):
     *       2x2 block = pixels (10,5), (11,5), (10,6), (11,6) all SET.
     *     'A' row 0 has bit 0 set → source pixel (col=7, row=0).
     *       2x2 block = pixels (24,5), (25,5), (24,6), (25,6) all SET.
     * ------------------------------------------------------------------- */
    fq_fb_clear(&fb);
    fq_draw_text_2x(&fb, &font, 10, 5, "A");

    /* Top-left 2x2 block of first pixel (col=0, row=0). */
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 10, 5));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 11, 5));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 10, 6));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 11, 6));

    /* Last pixel in row 0 (col=7, row=0): rendered at x=10 + 7*2=24, y=5. */
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 24, 5));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 25, 5));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 24, 6));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 25, 6));

    /* Row 1 of 'A' is all-zero, so no 2x2 blocks at y=7. */
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, 10, 7));
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, 10, 8));

    /* -------------------------------------------------------------------
     * F4: Corrected FONT_SCRIPT_36 "Ember" must be >= 80px wide.
     *     With fixed advance (w+2, off_x=0), Ember = ~93px.
     *     Old broken widths: ~58px.
     * ------------------------------------------------------------------- */
    font_title = fq_get_font_title();
    TEST_ASSERT_TRUE(font_title != NULL);

    w_ember = fq_text_width(font_title, "Ember");
    TEST_ASSERT_TRUE(w_ember >= 80);
    TEST_ASSERT_TRUE(w_ember <= 180);

    /* -------------------------------------------------------------------
     * F5: Corrected FONT_SCRIPT_36 "Tide" must be >= 60px wide.
     *     With fixed advance, Tide = ~67px.
     * ------------------------------------------------------------------- */
    w_tide = fq_text_width(font_title, "Tide");
    TEST_ASSERT_TRUE(w_tide >= 60);
    TEST_ASSERT_TRUE(w_tide <= 160);

    /* -------------------------------------------------------------------
     * F6: "EmberTide" total must exceed 100px (was ~99px with broken advances).
     *     With fixed advance widths, EmberTide = ~160px.
     * ------------------------------------------------------------------- */
    w_total = fq_text_width(font_title, "EmberTide");
    TEST_ASSERT_TRUE(w_total > 100);

    /* -------------------------------------------------------------------
     * F7: fq_draw_text_2x returns final cursor > start x after drawing "A".
     * ------------------------------------------------------------------- */
    fq_fb_clear(&fb);
    start_x = 5;
    result = fq_draw_text_2x(&fb, &font, start_x, 10, "A");
    TEST_ASSERT_TRUE(result > start_x);

    /* -------------------------------------------------------------------
     * F8: fq_get_font_title() returns non-NULL (no regression).
     * ------------------------------------------------------------------- */
    TEST_ASSERT_TRUE(fq_get_font_title() != NULL);

    printf("test_kerning_feature: PASS\n");
    return 0;
}
