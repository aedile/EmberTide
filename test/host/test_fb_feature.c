/**
 * test_fb_feature.c — Phase 7: Framebuffer Feature Tests (FEATURE RED)
 *
 * Happy-path contract tests for fq_fb_t operations.
 * Uses TEST_ASSERT_EQUAL_UINT32 for byte comparisons.
 */

#include <inttypes.h>
#include "test_assert.h"
#include "fq_framebuffer.h"
#include <string.h>
#include <stdint.h>

/* ── fq_fb_clear ───────────────────────────────────────────────────────── */
static void test_clear_zeros_all_bytes(void)
{
    fq_fb_t fb;
    /* Poison the buffer first. */
    memset(fb.pixels, 0xFFu, sizeof(fb.pixels));

    fq_fb_clear(&fb);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

/* ── fq_fb_fill ────────────────────────────────────────────────────────── */
static void test_fill_black_sets_all_bytes_ff(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_fb_fill(&fb, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0xFFu, (uint32_t)fb.pixels[i]);
    }
}

static void test_fill_white_clears_all_bytes(void)
{
    fq_fb_t fb;
    memset(fb.pixels, 0xFFu, sizeof(fb.pixels));
    fq_fb_fill(&fb, 0u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

/* ── fq_fb_set_pixel / fq_fb_get_pixel — roundtrip ─────────────────────── */
static void test_set_get_pixel_black(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_set_pixel(&fb, 10, 5, 1u);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 10, 5));
    /* Neighbour pixel untouched. */
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, 11, 5));
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, 9, 5));
}

static void test_set_get_pixel_white_clears(void)
{
    fq_fb_t fb;
    fq_fb_fill(&fb, 1u);

    fq_fb_set_pixel(&fb, 10, 5, 0u);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, 10, 5));
    /* Neighbour still black. */
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 11, 5));
}

/* ── Corner pixels ─────────────────────────────────────────────────────── */
static void test_corner_pixels_all_four(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_set_pixel(&fb,   0,   0, 1u);
    fq_fb_set_pixel(&fb, 199,   0, 1u);
    fq_fb_set_pixel(&fb,   0, 199, 1u);
    fq_fb_set_pixel(&fb, 199, 199, 1u);

    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb,   0,   0));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 199,   0));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb,   0, 199));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 199, 199));
}

/* Last pixel (199,199) → byte index: 199*25 + 199/8 = 4975 + 24 = 4999,
 * bit: 7 - (199%8) = 7 - 7 = 0 → byte 4999 = 0x01. */
static void test_last_pixel_byte_layout(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_set_pixel(&fb, 199, 199, 1u);
    TEST_ASSERT_EQUAL_UINT32(0x01u, (uint32_t)fb.pixels[4999]);
}

/* ── fq_fb_draw_line — diagonal ─────────────────────────────────────────── */
static void test_diagonal_line_endpoints_set(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_draw_line(&fb, 0, 0, 10, 10, 1u);

    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 10, 10));
}

static void test_diagonal_line_midpoint_set(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* 0->10 diagonal — midpoint (5,5) must be set by Bresenham. */
    fq_fb_draw_line(&fb, 0, 0, 10, 10, 1u);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 5, 5));
}

/* ── fq_fb_draw_line — clipped line sets only in-bounds pixels ─────────── */
static void test_clipped_line_only_inbounds_pixels(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Line from (-5, 5) to (5, 5): only pixels 0..5 visible. */
    fq_fb_draw_line(&fb, -5, 5, 5, 5, 1u);

    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 0, 5));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 5, 5));
}

/* ── fq_fb_draw_rect ────────────────────────────────────────────────────── */
static void test_draw_rect_sets_all_four_sides(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Rect at (10,10) size 20x20. */
    fq_fb_draw_rect(&fb, 10, 10, 20, 20, 1u);

    /* Top-left corner. */
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 10, 10));
    /* Top-right corner. */
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 29, 10));
    /* Bottom-left corner. */
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 10, 29));
    /* Bottom-right corner. */
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 29, 29));
}

static void test_draw_rect_interior_not_filled(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_draw_rect(&fb, 10, 10, 20, 20, 1u);

    /* Interior pixel must remain white. */
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, 15, 15));
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, 11, 11));
}

/* ── fq_fb_fill_rect ────────────────────────────────────────────────────── */
static void test_fill_rect_sets_all_interior_pixels(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_fill_rect(&fb, 10, 10, 4, 4, 1u);

    for (int16_t y = 10; y < 14; y++) {
        for (int16_t x = 10; x < 14; x++) {
            TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, x, y));
        }
    }
}

static void test_fill_rect_does_not_bleed_outside(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_fill_rect(&fb, 10, 10, 4, 4, 1u);

    /* Row 9 (above) must be clear. */
    for (int16_t x = 9; x <= 15; x++) {
        TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, x, 9));
    }
    /* Row 14 (below) must be clear. */
    for (int16_t x = 9; x <= 15; x++) {
        TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, x, 14));
    }
    /* Column 9 (left) must be clear. */
    for (int16_t y = 9; y <= 15; y++) {
        TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, 9, y));
    }
    /* Column 14 (right) must be clear. */
    for (int16_t y = 9; y <= 15; y++) {
        TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, 14, y));
    }
}

/* ── fill_rect zero/negative dimensions ─────────────────────────────────── */
static void test_fill_rect_zero_width_no_pixels(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_fb_fill_rect(&fb, 10, 10, 0, 10, 1u);
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

static void test_fill_rect_zero_height_no_pixels(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_fb_fill_rect(&fb, 10, 10, 10, 0, 1u);
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

/* ── main ──────────────────────────────────────────────────────────────── */
int main(void)
{
    test_clear_zeros_all_bytes();
    test_fill_black_sets_all_bytes_ff();
    test_fill_white_clears_all_bytes();

    test_set_get_pixel_black();
    test_set_get_pixel_white_clears();
    test_corner_pixels_all_four();
    test_last_pixel_byte_layout();

    test_diagonal_line_endpoints_set();
    test_diagonal_line_midpoint_set();
    test_clipped_line_only_inbounds_pixels();

    test_draw_rect_sets_all_four_sides();
    test_draw_rect_interior_not_filled();

    test_fill_rect_sets_all_interior_pixels();
    test_fill_rect_does_not_bleed_outside();
    test_fill_rect_zero_width_no_pixels();
    test_fill_rect_zero_height_no_pixels();

    printf("test_fb_feature: ALL PASS\n");
    return 0;
}
