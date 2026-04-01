/**
 * test_sprite_bounds.c — Phase 7: Sprite Blitter Bound/Edge-Case Tests (BOUND RED)
 *
 * Rule 22: Boundary tests committed BEFORE any implementation.
 * Covers:
 *   N9:  4-edge clipping (negative x/y, past right/bottom)
 *   N10: Fully off-screen sprite → zero pixels written
 *   N11: NULL pointer on fb or sprite
 *   N12: All 8 x-alignment cases (X=0 through X=7, non-byte-aligned shifts)
 *   N13: Width not a multiple of 8 (partial last byte)
 *   N14: Large dimensions → clip, no overflow/OOB write
 *
 * Uses TEST_ASSERT_EQUAL_UINT32 for byte comparisons (avoids PRIu8).
 */

#include <inttypes.h>
#include "test_assert.h"
#include "fq_framebuffer.h"
#include "fq_sprite.h"
#include <string.h>
#include <stdint.h>

/* ── Minimal test sprites ──────────────────────────────────────────────── */

/* 8x1 sprite: all bits set (0xFF). Fully black 8-wide, 1-tall stripe. */
static const uint8_t s_8x1_full_data[] = { 0xFFu };
static const fq_sprite_t s_8x1_full = { s_8x1_full_data, 8u, 1u };

/* 8x8 sprite: all bits set (solid black square). */
static const uint8_t s_8x8_full_data[] = {
    0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu
};
static const fq_sprite_t s_8x8_full = { s_8x8_full_data, 8u, 8u };

/* 16x1 sprite: all bits set (16 pixels wide, 1 row). */
static const uint8_t s_16x1_full_data[] = { 0xFFu, 0xFFu };
static const fq_sprite_t s_16x1_full = { s_16x1_full_data, 16u, 1u };

/* 3x1 sprite: width not multiple of 8. Only top 3 bits of byte 0 = 0xE0. */
static const uint8_t s_3x1_data[] = { 0xE0u };
static const fq_sprite_t s_3x1 = { s_3x1_data, 3u, 1u };

/* Zero-dimension sprite (should exit immediately, no crash). */
static const uint8_t s_0x0_data[] = { 0x00u };
static const fq_sprite_t s_0w_sprite = { s_0x0_data, 0u, 8u };
static const fq_sprite_t s_0h_sprite = { s_0x0_data, 8u, 0u };

/* ── N11: NULL safety ──────────────────────────────────────────────────── */
static void test_null_fb_no_crash(void)
{
    fq_blit_sprite(NULL, 0, 0, &s_8x8_full);
    _TA_PASS("fq_blit_sprite(NULL fb) did not crash");
}

static void test_null_sprite_no_crash(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_blit_sprite(&fb, 0, 0, NULL);
    /* No bytes modified. */
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

static void test_null_sprite_data_no_crash(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_sprite_t bad_sprite;
    bad_sprite.data   = NULL;
    bad_sprite.width  = 8u;
    bad_sprite.height = 8u;
    fq_blit_sprite(&fb, 0, 0, &bad_sprite);
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

/* ── Zero-dimension sprites ────────────────────────────────────────────── */
static void test_zero_width_sprite_no_pixels(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_blit_sprite(&fb, 0, 0, &s_0w_sprite);
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

static void test_zero_height_sprite_no_pixels(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_blit_sprite(&fb, 0, 0, &s_0h_sprite);
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

/* ── N10: Fully off-screen sprites ─────────────────────────────────────── */
static void test_sprite_fully_left_of_screen(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    /* 8-wide sprite at x=-8: all pixels left of screen. */
    fq_blit_sprite(&fb, -8, 0, &s_8x1_full);
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

static void test_sprite_fully_above_screen(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_blit_sprite(&fb, 0, -8, &s_8x8_full);
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

static void test_sprite_fully_right_of_screen(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    /* x=200: just past right edge. */
    fq_blit_sprite(&fb, 200, 0, &s_8x1_full);
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

static void test_sprite_fully_below_screen(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_blit_sprite(&fb, 0, 200, &s_8x8_full);
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

/* ── N9: Partial left-edge clip (negative X) ───────────────────────────── */
static void test_sprite_partial_left_clip(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Place 8-wide all-set sprite at x=-4: only pixels 0..3 should appear.
     * Pixels 0..3 = bits 7..4 of byte 0 = 0xF0 (MSB first). */
    fq_blit_sprite(&fb, -4, 0, &s_8x1_full);

    TEST_ASSERT_EQUAL_UINT32(0xF0u, (uint32_t)fb.pixels[0]);
}

static void test_sprite_partial_top_clip(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* 8x8 full sprite at y=-4: only bottom 4 rows visible (sprite rows 4-7
     * map to FB rows 0-3). */
    fq_blit_sprite(&fb, 0, -4, &s_8x8_full);

    /* Row 0 of FB (y=0) should be set (= sprite row 4). */
    TEST_ASSERT_EQUAL_UINT32(0xFFu, (uint32_t)fb.pixels[0]);
    /* Row 3 of FB (y=3) should be set (= sprite row 7). */
    TEST_ASSERT_EQUAL_UINT32(0xFFu, (uint32_t)fb.pixels[3u * FQ_FB_STRIDE]);
    /* Row 4 of FB (y=4) is NOT drawn — sprite only has 8 rows. */
    TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[4u * FQ_FB_STRIDE]);
}

/* ── N9: Partial right-edge clip ───────────────────────────────────────── */
static void test_sprite_partial_right_clip(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* 32x1 sprite placed at x=192.
     * Only pixels 192..199 (8 pixels) visible.
     * 32 pixels / 8 = 4 bytes of sprite data, all 0xFF.
     */
    static const uint8_t wide_data[] = { 0xFFu, 0xFFu, 0xFFu, 0xFFu };
    static const fq_sprite_t wide_sprite = { wide_data, 32u, 1u };

    fq_blit_sprite(&fb, 192, 0, &wide_sprite);

    /* Byte 24 of row 0 = pixels 192..199 = all set. */
    TEST_ASSERT_EQUAL_UINT32(0xFFu, (uint32_t)fb.pixels[24]);
    /* No wrap-around to row 1. */
    for (uint32_t i = 25; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

/* ── N12: All 8 x-alignment cases ──────────────────────────────────────── */
/*
 * Place an 8x1 all-bits-set sprite at x=0..7 and verify the correct bit
 * pattern in bytes 0 and 1 of the framebuffer.
 *
 * x=0: byte[0]=0xFF, byte[1]=0x00
 * x=1: byte[0]=0x7F, byte[1]=0x80
 * x=2: byte[0]=0x3F, byte[1]=0xC0
 * ...
 * x=7: byte[0]=0x01, byte[1]=0xFE
 */
static void test_all_8_x_alignments(void)
{
    static const uint8_t expected_byte0[8] = {
        0xFFu, 0x7Fu, 0x3Fu, 0x1Fu, 0x0Fu, 0x07u, 0x03u, 0x01u
    };
    static const uint8_t expected_byte1[8] = {
        0x00u, 0x80u, 0xC0u, 0xE0u, 0xF0u, 0xF8u, 0xFCu, 0xFEu
    };

    for (int16_t x = 0; x < 8; x++) {
        fq_fb_t fb;
        fq_fb_clear(&fb);
        fq_blit_sprite(&fb, x, 0, &s_8x1_full);
        TEST_ASSERT_EQUAL_UINT32((uint32_t)expected_byte0[x],
                                 (uint32_t)fb.pixels[0]);
        TEST_ASSERT_EQUAL_UINT32((uint32_t)expected_byte1[x],
                                 (uint32_t)fb.pixels[1]);
    }
}

/* ── N13: Width not multiple of 8 ──────────────────────────────────────── */
static void test_width_not_multiple_of_8(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* 3x1 sprite data = 0xE0 (bits 7,6,5 set; bits 4..0 clear).
     * At x=0, y=0: only pixels 0,1,2 should be set.
     * Byte 0 of FB = bits 7..5 set = 0xE0. Bit 4..0 remain 0.
     */
    fq_blit_sprite(&fb, 0, 0, &s_3x1);
    TEST_ASSERT_EQUAL_UINT32(0xE0u, (uint32_t)fb.pixels[0]);
    /* No bleed into byte 1. */
    TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[1]);
}

/* ── N13: Width=9 (straddles byte boundary, not multiple of 8) ─────────── */
static void test_width_9_straddles_bytes(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* 9x1, all bits set: first byte=0xFF, second byte = 0x80 (1 bit). */
    static const uint8_t w9_data[] = { 0xFFu, 0x80u };
    static const fq_sprite_t w9_sprite = { w9_data, 9u, 1u };

    fq_blit_sprite(&fb, 0, 0, &w9_sprite);
    TEST_ASSERT_EQUAL_UINT32(0xFFu, (uint32_t)fb.pixels[0]);
    TEST_ASSERT_EQUAL_UINT32(0x80u, (uint32_t)fb.pixels[1]);
    TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[2]);
}

/* ── N14: Large sprite dims → clip without overflow ────────────────────── */
static void test_large_sprite_no_buffer_overflow(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* 16x1 all-set sprite at x=190 → only pixels 190..199 (10px) visible.
     * byte 23 = pixels 184..191 → bits 1,0 set = 0x03.
     * byte 24 = pixels 192..199 → all 8 bits = 0xFF.
     */
    fq_blit_sprite(&fb, 190, 0, &s_16x1_full);

    TEST_ASSERT_EQUAL_UINT32(0x03u, (uint32_t)fb.pixels[23]);
    TEST_ASSERT_EQUAL_UINT32(0xFFu, (uint32_t)fb.pixels[24]);
    /* Row 1 must be untouched. */
    for (uint32_t col = 0; col < FQ_FB_STRIDE; col++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[FQ_FB_STRIDE + col]);
    }
}

/* ── OR-blit transparency: bit=0 preserves existing pixel ─────────────── */
static void test_or_blit_transparency_preserves_background(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Set pixel (0,0) to black. */
    fq_fb_set_pixel(&fb, 0, 0, 1u);
    TEST_ASSERT_EQUAL_UINT32(0x80u, (uint32_t)fb.pixels[0]);

    /* Blit a 1x1 sprite with bit=0 (transparent). Background must be preserved. */
    static const uint8_t transparent_data[] = { 0x00u };
    static const fq_sprite_t transparent_1x1 = { transparent_data, 1u, 1u };

    fq_blit_sprite(&fb, 0, 0, &transparent_1x1);

    /* Pixel (0,0) must still be black (OR-blit: 0 | existing preserves). */
    TEST_ASSERT_EQUAL_UINT32(0x80u, (uint32_t)fb.pixels[0]);
}

/* ── main ──────────────────────────────────────────────────────────────── */
int main(void)
{
    test_null_fb_no_crash();
    test_null_sprite_no_crash();
    test_null_sprite_data_no_crash();
    test_zero_width_sprite_no_pixels();
    test_zero_height_sprite_no_pixels();

    test_sprite_fully_left_of_screen();
    test_sprite_fully_above_screen();
    test_sprite_fully_right_of_screen();
    test_sprite_fully_below_screen();

    test_sprite_partial_left_clip();
    test_sprite_partial_top_clip();
    test_sprite_partial_right_clip();

    test_all_8_x_alignments();

    test_width_not_multiple_of_8();
    test_width_9_straddles_bytes();

    test_large_sprite_no_buffer_overflow();
    test_or_blit_transparency_preserves_background();

    printf("test_sprite_bounds: ALL PASS\n");
    return 0;
}
