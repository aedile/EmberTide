/**
 * test_fb_bounds.c — Phase 7: Framebuffer Bound/Edge-Case Tests (BOUND RED)
 *
 * Rule 22: These boundary tests MUST be written and committed BEFORE any
 * implementation. They prove the system rejects:
 *   - Out-of-bounds pixel coordinates (negative AND >= 200)
 *   - NULL pointer inputs (no segfault)
 *   - Integer underflow from signed negative coordinates
 *   - Degenerate line inputs (same point, all-OOB coords)
 *   - MSB bit-order fidelity (pixel 0,0 == bit 7 of byte 0)
 *
 * Tests here are HOST-only; no hal_*.h included.
 */

#include "test_assert.h"
#include "fq_framebuffer.h"
#include <string.h>
#include <stdint.h>

/* ── N3: MSB bit-order ─────────────────────────────────────────────────────
 * Pixel (0,0) must map to bit 7 of byte 0 (MSB first).
 * Pixel (7,0) must map to bit 0 of byte 0.
 * Pixel (8,0) must map to bit 7 of byte 1.
 */
static void test_msb_bit_order_pixel_0_0(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Set only pixel (0,0). */
    fq_fb_set_pixel(&fb, 0, 0, 1u);

    /* Byte 0 should have bit 7 set = 0x80. All others zero. */
    TEST_ASSERT_EQUAL_UINT8(0x80u, fb.pixels[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[1]);
}

static void test_msb_bit_order_pixel_7_0(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Pixel (7,0) = bit 0 of byte 0 = 0x01. */
    fq_fb_set_pixel(&fb, 7, 0, 1u);
    TEST_ASSERT_EQUAL_UINT8(0x01u, fb.pixels[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[1]);
}

static void test_msb_bit_order_pixel_8_0(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Pixel (8,0) = bit 7 of byte 1. */
    fq_fb_set_pixel(&fb, 8, 0, 1u);
    TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[0]);
    TEST_ASSERT_EQUAL_UINT8(0x80u, fb.pixels[1]);
}

/* ── N1: Negative coordinate → drop silently ───────────────────────────── */
static void test_negative_x_drops_pixel(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Must not corrupt any byte, must not crash. */
    fq_fb_set_pixel(&fb, -1, 0, 1u);
    fq_fb_set_pixel(&fb, -5, 100, 1u);

    /* All bytes remain 0x00. */
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[i]);
    }
}

static void test_negative_y_drops_pixel(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_set_pixel(&fb, 0, -1, 1u);
    fq_fb_set_pixel(&fb, 100, -100, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[i]);
    }
}

/* ── Signed underflow: INT16_MIN coordinates must be dropped ───────────── */
static void test_int16_min_coordinates_dropped(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_set_pixel(&fb, (int16_t)-32768, (int16_t)-32768, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[i]);
    }
}

/* ── OOB positive coordinates ──────────────────────────────────────────── */
static void test_x_equals_200_drops_pixel(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_set_pixel(&fb, 200, 0, 1u);
    fq_fb_set_pixel(&fb, 255, 0, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[i]);
    }
}

static void test_y_equals_200_drops_pixel(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_set_pixel(&fb, 0, 200, 1u);
    fq_fb_set_pixel(&fb, 0, 255, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[i]);
    }
}

/* ── N8: NULL pointer handling ─────────────────────────────────────────── */
static void test_null_fb_set_pixel_no_crash(void)
{
    /* Must not crash/segfault — silently returns. */
    fq_fb_set_pixel(NULL, 0, 0, 1u);
    _TA_PASS("fq_fb_set_pixel(NULL,...) did not crash");
}

static void test_null_fb_clear_no_crash(void)
{
    fq_fb_clear(NULL);
    _TA_PASS("fq_fb_clear(NULL) did not crash");
}

static void test_null_fb_fill_no_crash(void)
{
    fq_fb_fill(NULL, 1u);
    _TA_PASS("fq_fb_fill(NULL,...) did not crash");
}

static void test_null_fb_get_pixel_returns_zero(void)
{
    uint8_t val = fq_fb_get_pixel(NULL, 0, 0);
    TEST_ASSERT_EQUAL_UINT8(0u, val);
}

static void test_null_fb_draw_line_no_crash(void)
{
    fq_fb_draw_line(NULL, 0, 0, 10, 10, 1u);
    _TA_PASS("fq_fb_draw_line(NULL,...) did not crash");
}

static void test_null_fb_draw_rect_no_crash(void)
{
    fq_fb_draw_rect(NULL, 0, 0, 10, 10, 1u);
    _TA_PASS("fq_fb_draw_rect(NULL,...) did not crash");
}

static void test_null_fb_fill_rect_no_crash(void)
{
    fq_fb_fill_rect(NULL, 0, 0, 10, 10, 1u);
    _TA_PASS("fq_fb_fill_rect(NULL,...) did not crash");
}

/* ── N4: Vertical line ─────────────────────────────────────────────────── */
static void test_vertical_line_terminates(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    /* x0==x1 must not loop forever. */
    fq_fb_draw_line(&fb, 5, 0, 5, 199, 1u);
    /* Pixel (5, 0) should be set. */
    TEST_ASSERT_EQUAL_UINT8(1u, fq_fb_get_pixel(&fb, 5, 0));
    TEST_ASSERT_EQUAL_UINT8(1u, fq_fb_get_pixel(&fb, 5, 199));
}

/* ── N5: Horizontal line ───────────────────────────────────────────────── */
static void test_horizontal_line_terminates(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_fb_draw_line(&fb, 0, 10, 199, 10, 1u);
    TEST_ASSERT_EQUAL_UINT8(1u, fq_fb_get_pixel(&fb, 0, 10));
    TEST_ASSERT_EQUAL_UINT8(1u, fq_fb_get_pixel(&fb, 199, 10));
}

/* ── N6: Single-point line (x0==x1, y0==y1) ───────────────────────────── */
static void test_single_point_line_terminates(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_fb_draw_line(&fb, 50, 50, 50, 50, 1u);
    TEST_ASSERT_EQUAL_UINT8(1u, fq_fb_get_pixel(&fb, 50, 50));
}

/* ── N7: Reversed line (x1<x0 or y1<y0) ───────────────────────────────── */
static void test_reversed_line_draws_same_pixels(void)
{
    fq_fb_t fb1, fb2;
    fq_fb_clear(&fb1);
    fq_fb_clear(&fb2);

    fq_fb_draw_line(&fb1, 10, 20, 50, 60, 1u);
    fq_fb_draw_line(&fb2, 50, 60, 10, 20, 1u);

    /* Both framebuffers should produce the same pixel pattern. */
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8(fb1.pixels[i], fb2.pixels[i]);
    }
}

/* ── All-OOB line must not write any pixel ─────────────────────────────── */
static void test_fully_oob_line_no_pixels_written(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* All coordinates outside [0,199]. */
    fq_fb_draw_line(&fb, 300, 300, 500, 500, 1u);
    fq_fb_draw_line(&fb, -10, -10, -50, -50, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[i]);
    }
}

/* ── get_pixel: OOB returns 0 ──────────────────────────────────────────── */
static void test_get_pixel_oob_returns_zero(void)
{
    fq_fb_t fb;
    fq_fb_fill(&fb, 1u); /* All black. */

    /* OOB coords must return 0, not 1. */
    TEST_ASSERT_EQUAL_UINT8(0u, fq_fb_get_pixel(&fb, -1, 0));
    TEST_ASSERT_EQUAL_UINT8(0u, fq_fb_get_pixel(&fb, 200, 0));
    TEST_ASSERT_EQUAL_UINT8(0u, fq_fb_get_pixel(&fb, 0, -1));
    TEST_ASSERT_EQUAL_UINT8(0u, fq_fb_get_pixel(&fb, 0, 200));
}

/* ── Static size assertion (compile-time) ──────────────────────────────── */
_Static_assert(sizeof(fq_fb_t) == 5000u,
               "fq_fb_t must be exactly 5000 bytes (200x200/8)");
_Static_assert(FQ_FB_SIZE == 5000u,
               "FQ_FB_SIZE must be 5000");
_Static_assert(FQ_FB_STRIDE == 25u,
               "FQ_FB_STRIDE must be 25");
_Static_assert(FQ_FB_WIDTH == 200u,
               "FQ_FB_WIDTH must be 200");
_Static_assert(FQ_FB_HEIGHT == 200u,
               "FQ_FB_HEIGHT must be 200");

/* ── main ──────────────────────────────────────────────────────────────── */
int main(void)
{
    /* MSB bit-order */
    test_msb_bit_order_pixel_0_0();
    test_msb_bit_order_pixel_7_0();
    test_msb_bit_order_pixel_8_0();

    /* OOB negative */
    test_negative_x_drops_pixel();
    test_negative_y_drops_pixel();
    test_int16_min_coordinates_dropped();

    /* OOB positive */
    test_x_equals_200_drops_pixel();
    test_y_equals_200_drops_pixel();

    /* NULL safety */
    test_null_fb_set_pixel_no_crash();
    test_null_fb_clear_no_crash();
    test_null_fb_fill_no_crash();
    test_null_fb_get_pixel_returns_zero();
    test_null_fb_draw_line_no_crash();
    test_null_fb_draw_rect_no_crash();
    test_null_fb_fill_rect_no_crash();

    /* Line edge cases */
    test_vertical_line_terminates();
    test_horizontal_line_terminates();
    test_single_point_line_terminates();
    test_reversed_line_draws_same_pixels();
    test_fully_oob_line_no_pixels_written();

    /* get_pixel OOB */
    test_get_pixel_oob_returns_zero();

    printf("test_fb_bounds: ALL PASS\n");
    return 0;
}
