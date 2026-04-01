/**
 * test_p17_asset_bounds.c — Phase 17 Asset Pipeline: Bound/Guard Tests
 *
 * Rule 22 (BOUND RED): Negative and boundary tests that prove the system
 * REJECTS invalid inputs and that structural invariants of the generated
 * asset headers hold.
 *
 * Tests:
 *   B01 — sprite_char array size == 128 bytes (32*32/8 = 4 bytes/row * 32 rows)
 *   B02 — sprite table entry dimensions (width=32, height=32)
 *   B03 — sprite table count == 168
 *   B04 — font bitmap size (95 glyphs * 30 rows * 2 bytes/row = 5700 bytes)
 *   B05 — space glyph all zeros (first 60 bytes of BITMAP are 0x00)
 *   B06 — fq_blit_sprite(NULL fb) does not crash
 *   B07 — fq_blit_sprite(NULL sprite) does not crash, fb unchanged
 *   B08 — fq_blit_sprite fully off-screen (fb unchanged)
 *   B09 — fq_draw_text cursor advance for 'A' == width['A'] == 11
 *   B10 — fq_text_width(NULL font, "A") returns 0
 *   B11 — fq_text_width(font, NULL) returns 0
 *   B12 — font descriptor fields match expected constants
 *
 * Architecture: presentation layer only -- no hal headers, no game headers.
 * Constitution Priority 0: no float arithmetic.
 */

#include <inttypes.h>
#include "test_assert.h"
#include "fq_framebuffer.h"
#include "fq_sprite.h"
#include "fq_text.h"

/* Generated asset headers (Phase 17 output) */
#include "sprites/sprite_chars.h"
#include "fonts/font_regs_12.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ── Constants derived from the generator spec ──────────────────────────── */

/* 32x32 sprite: ceil(32/8) = 4 bytes/row, 32 rows → 128 bytes per bitmap. */
#define SPRITE_CHAR_BYTES      128u
#define SPRITE_CHAR_WIDTH      32u
#define SPRITE_CHAR_HEIGHT     32u
#define SPRITE_CHAR_COUNT      168u

/*
 * Font: glyph_max_w=11, glyph_h=30, stride=ceil(11/8)=2 bytes/row.
 * Total bitmap = 95 * 30 * 2 = 5700 bytes.
 */
#define FONT_GLYPH_H           30u
#define FONT_GLYPH_MAX_W       11u
#define FONT_GLYPH_COUNT       95u
#define FONT_BITMAP_BYTES      5700u

/*
 * 'A' is ASCII 0x41.  Glyph index = 0x41 - 0x20 = 33.
 * FONT_REGS_12_WIDTHS[33] == 11U (verified against generated header).
 */
#define GLYPH_IDX_A            33u
#define GLYPH_ADVANCE_A        11

/* ── B01: sprite_char array size == 128 bytes ─────────────────────────── */
static void test_sprite_char_array_size(void)
{
    /*
     * sizeof() on the first bitmap array in sprite_chars.h must equal 128.
     * This validates the bit-packing: ceil(32/8)=4 bytes/row * 32 rows.
     */
    uint32_t sz = (uint32_t)sizeof(sprite_char_r00_c00);
    TEST_ASSERT_EQUAL_UINT32(SPRITE_CHAR_BYTES, sz);
}

/* ── B02: sprite table entry dimensions (width=32, height=32) ─────────── */
static void test_sprite_table_entry_dimensions(void)
{
    TEST_ASSERT_EQUAL_UINT32(SPRITE_CHAR_WIDTH,  (uint32_t)SPRITE_CHAR_TABLE[0].width);
    TEST_ASSERT_EQUAL_UINT32(SPRITE_CHAR_HEIGHT, (uint32_t)SPRITE_CHAR_TABLE[0].height);
    /* Also check last entry. */
    TEST_ASSERT_EQUAL_UINT32(SPRITE_CHAR_WIDTH,
                             (uint32_t)SPRITE_CHAR_TABLE[SPRITE_CHAR_COUNT - 1u].width);
    TEST_ASSERT_EQUAL_UINT32(SPRITE_CHAR_HEIGHT,
                             (uint32_t)SPRITE_CHAR_TABLE[SPRITE_CHAR_COUNT - 1u].height);
}

/* ── B03: sprite table count == 168 ──────────────────────────────────── */
static void test_sprite_table_count(void)
{
    uint32_t count = (uint32_t)(sizeof(SPRITE_CHAR_TABLE) / sizeof(SPRITE_CHAR_TABLE[0]));
    TEST_ASSERT_EQUAL_UINT32(SPRITE_CHAR_COUNT, count);
}

/* ── B04: font bitmap size == 5700 bytes ─────────────────────────────── */
static void test_font_bitmap_size(void)
{
    uint32_t sz = (uint32_t)sizeof(FONT_REGS_12_glyph_BITMAP);
    TEST_ASSERT_EQUAL_UINT32(FONT_BITMAP_BYTES, sz);
}

/* ── B05: space glyph all zeros (first 60 bytes) ─────────────────────── */
static void test_space_glyph_all_zeros(void)
{
    /*
     * Space is glyph index 0 (ASCII 0x20 - 0x20).
     * It starts at BITMAP offset 0 and occupies glyph_h * stride = 30 * 2 = 60 bytes.
     * All bytes must be 0x00 (space has no visible pixels).
     */
    uint32_t stride  = 2u;    /* ceil(11/8) = 2 */
    uint32_t rows    = FONT_GLYPH_H;
    uint32_t nbytes  = rows * stride;   /* 60 */

    for (uint32_t i = 0; i < nbytes; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)FONT_REGS_12_glyph_BITMAP[i]);
    }
}

/* ── B06: blit with NULL fb — no crash ───────────────────────────────── */
static void test_blit_null_fb_no_crash(void)
{
    fq_blit_sprite(NULL, 0, 0, &SPRITE_CHAR_TABLE[0]);
    _TA_PASS("fq_blit_sprite(NULL fb, table[0]) did not crash");
}

/* ── B07: blit with NULL sprite — no crash, fb unchanged ─────────────── */
static void test_blit_null_sprite_no_crash(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_blit_sprite(&fb, 0, 0, NULL);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

/* ── B08: blit fully off-screen — fb unchanged ───────────────────────── */
static void test_blit_fully_offscreen_fb_unchanged(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Place a 32x32 sprite entirely past the right and bottom edges. */
    fq_blit_sprite(&fb, 200, 200, &SPRITE_CHAR_TABLE[0]);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

/* ── B09: draw_text cursor advance for "A" ───────────────────────────── */
static void test_draw_text_cursor_advance_A(void)
{
    fq_fb_t  fb;
    fq_fb_clear(&fb);

    int16_t x_start = 0;
    int16_t x_end   = fq_draw_text(&fb, &FONT_REGS_12_FONT, x_start, 0, "A");

    /*
     * After rendering 'A' the cursor must have advanced by exactly the
     * advance width of 'A'.  FONT_REGS_12_WIDTHS[33] == 11.
     */
    TEST_ASSERT_EQUAL_INT(GLYPH_ADVANCE_A, (int)(x_end - x_start));
}

/* ── B10: text_width with NULL font returns 0 ────────────────────────── */
static void test_text_width_null_font_returns_0(void)
{
    int16_t w = fq_text_width(NULL, "A");
    TEST_ASSERT_EQUAL_INT(0, (int)w);
}

/* ── B11: text_width with NULL string returns 0 ─────────────────────── */
static void test_text_width_null_string_returns_0(void)
{
    int16_t w = fq_text_width(&FONT_REGS_12_FONT, NULL);
    TEST_ASSERT_EQUAL_INT(0, (int)w);
}

/* ── B12: font descriptor fields match expected constants ─────────────── */
static void test_font_descriptor_fields_match(void)
{
    TEST_ASSERT_EQUAL_UINT32(FONT_GLYPH_H,     (uint32_t)FONT_REGS_12_FONT.glyph_h);
    TEST_ASSERT_EQUAL_UINT32(FONT_GLYPH_MAX_W, (uint32_t)FONT_REGS_12_FONT.glyph_max_w);
    TEST_ASSERT_NOT_NULL(FONT_REGS_12_FONT.bitmap);
    TEST_ASSERT_NOT_NULL(FONT_REGS_12_FONT.widths);
    TEST_ASSERT_NOT_NULL(FONT_REGS_12_FONT.offsets_x);
    TEST_ASSERT_NOT_NULL(FONT_REGS_12_FONT.offsets_y);
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void)
{
    test_sprite_char_array_size();
    test_sprite_table_entry_dimensions();
    test_sprite_table_count();
    test_font_bitmap_size();
    test_space_glyph_all_zeros();
    test_blit_null_fb_no_crash();
    test_blit_null_sprite_no_crash();
    test_blit_fully_offscreen_fb_unchanged();
    test_draw_text_cursor_advance_A();
    test_text_width_null_font_returns_0();
    test_text_width_null_string_returns_0();
    test_font_descriptor_fields_match();

    printf("test_p17_asset_bounds: ALL PASS\n");
    return 0;
}
