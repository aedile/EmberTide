/**
 * test_text_feature.c — Phase 7: Font Renderer Feature Tests (FEATURE RED)
 *
 * Happy-path contract tests for fq_draw_text and fq_text_width.
 * Uses a minimal inline 8x8 monospace test font to prove the pipeline.
 * Uses TEST_ASSERT_EQUAL_UINT32 for byte/pixel comparisons.
 */

#include <inttypes.h>
#include "test_assert.h"
#include "fq_framebuffer.h"
#include "fq_text.h"
#include <string.h>
#include <stdint.h>

/* ── Minimal 8x8 monospace test font ──────────────────────────────────── */
/*
 * Covers ASCII 32..126 (95 glyphs).
 * Each glyph: 8 rows x 1 byte = 8 bytes total per glyph.
 * Advance widths: space=4, all printable chars=8.
 *
 * Glyph data is all-ones (0xFF per row) for every non-space character.
 */

#define TF_GLYPHS  95u
#define TF_H        8u

/* Full-black glyph: all 8 rows x 8 bits set. */
static const uint8_t g_solid_glyph[TF_H] = {
    0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu
};

/* Bitmap: 95 glyphs x 8 rows x 1 byte = 760 bytes. */
static uint8_t g_bitmap[TF_GLYPHS * TF_H];
static uint8_t g_widths[TF_GLYPHS];
static int8_t  g_offx[TF_GLYPHS];
static int8_t  g_offy[TF_GLYPHS];

static void init_font_tables(void)
{
    for (uint32_t i = 0; i < TF_GLYPHS; i++) {
        g_widths[i] = 8u;
        g_offx[i]   = 0;
        g_offy[i]   = 0;
        if (i == 0u) {
            /* Space: transparent, narrower. */
            for (uint32_t r = 0; r < TF_H; r++) {
                g_bitmap[i * TF_H + r] = 0x00u;
            }
            g_widths[i] = 4u;
        } else {
            for (uint32_t r = 0; r < TF_H; r++) {
                g_bitmap[i * TF_H + r] = g_solid_glyph[r];
            }
        }
    }
}

static fq_font_t make_font(void)
{
    fq_font_t f;
    f.bitmap       = g_bitmap;
    f.widths       = g_widths;
    f.offsets_x    = g_offx;
    f.offsets_y    = g_offy;
    f.glyph_h      = TF_H;
    f.glyph_max_w  = 8u;
    return f;
}

/* ── fq_text_width: single character ────────────────────────────────────── */
static void test_text_width_single_char(void)
{
    init_font_tables();
    fq_font_t f = make_font();

    /* 'A' (0x41 = glyph_index 33, width=8). */
    int16_t w = fq_text_width(&f, "A");
    TEST_ASSERT_EQUAL_INT(8, (int)w);
}

static void test_text_width_space(void)
{
    init_font_tables();
    fq_font_t f = make_font();
    int16_t w = fq_text_width(&f, " ");
    TEST_ASSERT_EQUAL_INT(4, (int)w);
}

/* ── fq_text_width: multi-character ─────────────────────────────────────── */
static void test_text_width_two_chars(void)
{
    init_font_tables();
    fq_font_t f = make_font();
    /* "AB": 8 + 8 = 16. */
    int16_t w = fq_text_width(&f, "AB");
    TEST_ASSERT_EQUAL_INT(16, (int)w);
}

static void test_text_width_with_space(void)
{
    init_font_tables();
    fq_font_t f = make_font();
    /* "A B": 8 + 4 + 8 = 20. */
    int16_t w = fq_text_width(&f, "A B");
    TEST_ASSERT_EQUAL_INT(20, (int)w);
}

/* ── fq_draw_text: cursor return value ───────────────────────────────────── */
static void test_draw_text_returns_cursor_after_single_char(void)
{
    init_font_tables();
    fq_font_t f = make_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    int16_t cursor = fq_draw_text(&fb, &f, 0, 0, "A");
    TEST_ASSERT_EQUAL_INT(8, (int)cursor);
}

static void test_draw_text_returns_cursor_after_three_chars(void)
{
    init_font_tables();
    fq_font_t f = make_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* "ABC": 3 x 8 = 24. */
    int16_t cursor = fq_draw_text(&fb, &f, 0, 0, "ABC");
    TEST_ASSERT_EQUAL_INT(24, (int)cursor);
}

/* ── fq_draw_text: pixels set correctly ─────────────────────────────────── */
static void test_draw_text_sets_pixels_for_solid_glyph(void)
{
    init_font_tables();
    fq_font_t f = make_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Draw 'A' at (0,0). All 8 rows x 8 pixels of glyph should be set. */
    fq_draw_text(&fb, &f, 0, 0, "A");

    for (int16_t row = 0; row < 8; row++) {
        TEST_ASSERT_EQUAL_UINT32(0xFFu,
                                 (uint32_t)fb.pixels[row * (int16_t)FQ_FB_STRIDE]);
    }
    /* Row 8 should be clear. */
    TEST_ASSERT_EQUAL_UINT32(0x00u,
                             (uint32_t)fb.pixels[8u * FQ_FB_STRIDE]);
}

/* ── fq_draw_text: space glyph is transparent ───────────────────────────── */
static void test_draw_text_space_leaves_framebuffer_clear(void)
{
    init_font_tables();
    fq_font_t f = make_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_draw_text(&fb, &f, 0, 0, " ");

    /* Space glyph has all-zero bitmap → OR-blit changes nothing. */
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

/* ── fq_draw_text: start X offset applied ───────────────────────────────── */
static void test_draw_text_x_offset_applied(void)
{
    init_font_tables();
    fq_font_t f = make_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Draw 'A' at (8, 0). Byte 1 of each row should be 0xFF. */
    fq_draw_text(&fb, &f, 8, 0, "A");

    for (int16_t row = 0; row < 8; row++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u,
                                 (uint32_t)fb.pixels[row * (int16_t)FQ_FB_STRIDE + 0]);
        TEST_ASSERT_EQUAL_UINT32(0xFFu,
                                 (uint32_t)fb.pixels[row * (int16_t)FQ_FB_STRIDE + 1]);
    }
}

/* ── fq_draw_text: consecutive chars placed without gap ─────────────────── */
static void test_two_chars_placed_consecutively(void)
{
    init_font_tables();
    fq_font_t f = make_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* "AB": 'A' at columns 0-7, 'B' at columns 8-15. */
    fq_draw_text(&fb, &f, 0, 0, "AB");

    /* Row 0: bytes 0 and 1 should be 0xFF, byte 2 clear. */
    TEST_ASSERT_EQUAL_UINT32(0xFFu, (uint32_t)fb.pixels[0]);
    TEST_ASSERT_EQUAL_UINT32(0xFFu, (uint32_t)fb.pixels[1]);
    TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[2]);
}

/* ── fq_draw_text: y offset applied ────────────────────────────────────── */
static void test_draw_text_y_offset_applied(void)
{
    init_font_tables();
    fq_font_t f = make_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Draw 'A' at (0, 10). Rows 10..17 of FB should start solid. */
    fq_draw_text(&fb, &f, 0, 10, "A");

    TEST_ASSERT_EQUAL_UINT32(0xFFu, (uint32_t)fb.pixels[10u * FQ_FB_STRIDE]);
    TEST_ASSERT_EQUAL_UINT32(0xFFu, (uint32_t)fb.pixels[17u * FQ_FB_STRIDE]);
    /* Row 9 must be clear. */
    TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[9u * FQ_FB_STRIDE]);
    /* Row 18 must be clear. */
    TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[18u * FQ_FB_STRIDE]);
}

/* ── main ──────────────────────────────────────────────────────────────── */
int main(void)
{
    test_text_width_single_char();
    test_text_width_space();
    test_text_width_two_chars();
    test_text_width_with_space();

    test_draw_text_returns_cursor_after_single_char();
    test_draw_text_returns_cursor_after_three_chars();
    test_draw_text_sets_pixels_for_solid_glyph();
    test_draw_text_space_leaves_framebuffer_clear();
    test_draw_text_x_offset_applied();
    test_two_chars_placed_consecutively();
    test_draw_text_y_offset_applied();

    printf("test_text_feature: ALL PASS\n");
    return 0;
}
