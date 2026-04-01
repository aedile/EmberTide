/**
 * test_sprite_feature.c — Phase 7: Sprite Blitter Feature Tests (FEATURE RED)
 *
 * Happy-path contract tests for fq_blit_sprite.
 */

#include "test_assert.h"
#include "fq_framebuffer.h"
#include "fq_sprite.h"
#include <string.h>
#include <stdint.h>

/* ── Basic 8x8 blit at byte-aligned position ───────────────────────────── */
static void test_8x8_blit_aligned(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    static const uint8_t data[8] = {
        0xFFu, 0xFFu, 0xFFu, 0xFFu,
        0xFFu, 0xFFu, 0xFFu, 0xFFu
    };
    static const fq_sprite_t sp = { data, 8u, 8u };

    fq_blit_sprite(&fb, 0, 0, &sp);

    /* All 8 rows × 8 pixels should be set (byte 0 of each row = 0xFF). */
    for (uint32_t row = 0; row < 8; row++) {
        TEST_ASSERT_EQUAL_UINT8(0xFFu, fb.pixels[row * FQ_FB_STRIDE]);
        /* Next byte in same row must be clear. */
        TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[row * FQ_FB_STRIDE + 1]);
    }
}

/* ── OR-blit: transparent bits leave existing pixels unchanged ─────────── */
static void test_or_blit_preserves_background(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Pre-set pixel (0,0). */
    fq_fb_set_pixel(&fb, 0, 0, 1u);

    /* Blit a sprite with only the second pixel set (0x40).
     * Pixel (0,0) must remain black (background). */
    static const uint8_t half_data[] = { 0x40u };
    static const fq_sprite_t half_sp = { half_data, 8u, 1u };

    fq_blit_sprite(&fb, 0, 0, &half_sp);

    TEST_ASSERT_EQUAL_UINT8(1u, fq_fb_get_pixel(&fb, 0, 0)); /* preserved */
    TEST_ASSERT_EQUAL_UINT8(1u, fq_fb_get_pixel(&fb, 1, 0)); /* sprite set */
    TEST_ASSERT_EQUAL_UINT8(0u, fq_fb_get_pixel(&fb, 2, 0)); /* untouched */
}

/* ── Checkerboard sprite pattern ────────────────────────────────────────── */
static void test_checkerboard_pattern(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* 8x1 sprite: alternating bits = 0xAA (10101010). */
    static const uint8_t check_data[] = { 0xAAu };
    static const fq_sprite_t check_sp = { check_data, 8u, 1u };

    fq_blit_sprite(&fb, 0, 0, &check_sp);

    /* Pixels 0,2,4,6 black; 1,3,5,7 white (0=MSB). */
    TEST_ASSERT_EQUAL_UINT8(1u, fq_fb_get_pixel(&fb, 0, 0)); /* bit 7 */
    TEST_ASSERT_EQUAL_UINT8(0u, fq_fb_get_pixel(&fb, 1, 0)); /* bit 6 */
    TEST_ASSERT_EQUAL_UINT8(1u, fq_fb_get_pixel(&fb, 2, 0)); /* bit 5 */
    TEST_ASSERT_EQUAL_UINT8(0u, fq_fb_get_pixel(&fb, 3, 0)); /* bit 4 */
}

/* ── Unaligned X (x=1): 8-wide sprite starting at bit offset 1 ─────────── */
static void test_unaligned_x1_single_row(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* All-set 8x1 at x=1: sprite bits spill into byte 0 and byte 1.
     * Expected: byte[0]=0x7F (bits 6..0), byte[1]=0x80 (bit 7). */
    static const uint8_t data[] = { 0xFFu };
    static const fq_sprite_t sp = { data, 8u, 1u };

    fq_blit_sprite(&fb, 1, 0, &sp);

    TEST_ASSERT_EQUAL_UINT8(0x7Fu, fb.pixels[0]);
    TEST_ASSERT_EQUAL_UINT8(0x80u, fb.pixels[1]);
}

/* ── Sprite at x=8 (second byte boundary) ──────────────────────────────── */
static void test_byte_aligned_x8(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    static const uint8_t data[] = { 0xFFu };
    static const fq_sprite_t sp = { data, 8u, 1u };

    fq_blit_sprite(&fb, 8, 0, &sp);

    TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[0]); /* byte 0 = pixels 0-7 */
    TEST_ASSERT_EQUAL_UINT8(0xFFu, fb.pixels[1]); /* byte 1 = pixels 8-15 */
    TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[2]);
}

/* ── 16x2 sprite at (0,0) ───────────────────────────────────────────────── */
static void test_16x2_two_row_blit(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    static const uint8_t data[] = {
        0xFFu, 0xFFu, /* row 0: all 16 bits */
        0xFFu, 0xFFu  /* row 1: all 16 bits */
    };
    static const fq_sprite_t sp = { data, 16u, 2u };

    fq_blit_sprite(&fb, 0, 0, &sp);

    /* Row 0: bytes 0,1 = 0xFF each. */
    TEST_ASSERT_EQUAL_UINT8(0xFFu, fb.pixels[0]);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, fb.pixels[1]);
    /* Row 1: bytes 25,26 = 0xFF each. */
    TEST_ASSERT_EQUAL_UINT8(0xFFu, fb.pixels[FQ_FB_STRIDE]);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, fb.pixels[FQ_FB_STRIDE + 1]);
    /* Byte 2 = clear. */
    TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[2]);
}

/* ── Bottom-right corner clip (partial bottom) ──────────────────────────── */
static void test_partial_bottom_clip(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* 8x8 sprite at y=197: only rows 0..2 visible (y=197,198,199). */
    static const uint8_t data[8] = {
        0xFFu, 0xFFu, 0xFFu, 0xFFu,
        0xFFu, 0xFFu, 0xFFu, 0xFFu
    };
    static const fq_sprite_t sp = { data, 8u, 8u };

    fq_blit_sprite(&fb, 0, 197, &sp);

    /* Rows 197, 198, 199 must be set. */
    TEST_ASSERT_EQUAL_UINT8(0xFFu, fb.pixels[197 * FQ_FB_STRIDE]);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, fb.pixels[198 * FQ_FB_STRIDE]);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, fb.pixels[199 * FQ_FB_STRIDE]);
    /* Rows 0..196 must be clear (sample row 0). */
    TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[0]);
}

/* ── main ──────────────────────────────────────────────────────────────── */
int main(void)
{
    test_8x8_blit_aligned();
    test_or_blit_preserves_background();
    test_checkerboard_pattern();
    test_unaligned_x1_single_row();
    test_byte_aligned_x8();
    test_16x2_two_row_blit();
    test_partial_bottom_clip();

    printf("test_sprite_feature: ALL PASS\n");
    return 0;
}
