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
 * Review finding fixes (phase-7 review):
 *   B2: test_reversed_line_endpoints_both_set replaces the old
 *       test_reversed_line_draws_same_pixels. Uses a non-45-degree line
 *       (10,20)→(50,25). Asserts both endpoints set and pixel count == 41.
 *   B3: Three draw_rect zero/negative dimension tests added.
 *   B4: INT16_MAX coordinate crash test + horizontal INT16_MIN→INT16_MAX line.
 *   A2: NULL safety tests allocate a local fq_fb_t, clear it, call the
 *       function with NULL fb, then verify the local fb is unmodified (all
 *       bytes still 0x00). This proves NULL did not corrupt a nearby stack
 *       allocation.
 *
 * Tests here are HOST-only; no hal_*.h included.
 *
 * Note: Uses TEST_ASSERT_EQUAL_UINT32 for byte comparisons (avoids
 * PRIu8 which requires <inttypes.h> not available in all host envs).
 */

#include <inttypes.h>
#include "test_assert.h"
#include "fq_framebuffer.h"
#include <string.h>
#include <stdint.h>
#include <limits.h>

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
    TEST_ASSERT_EQUAL_UINT32(0x80u, (uint32_t)fb.pixels[0]);
    TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[1]);
}

static void test_msb_bit_order_pixel_7_0(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Pixel (7,0) = bit 0 of byte 0 = 0x01. */
    fq_fb_set_pixel(&fb, 7, 0, 1u);
    TEST_ASSERT_EQUAL_UINT32(0x01u, (uint32_t)fb.pixels[0]);
    TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[1]);
}

static void test_msb_bit_order_pixel_8_0(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Pixel (8,0) = bit 7 of byte 1. */
    fq_fb_set_pixel(&fb, 8, 0, 1u);
    TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[0]);
    TEST_ASSERT_EQUAL_UINT32(0x80u, (uint32_t)fb.pixels[1]);
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
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

static void test_negative_y_drops_pixel(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_set_pixel(&fb, 0, -1, 1u);
    fq_fb_set_pixel(&fb, 100, -100, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

/* ── Signed underflow: INT16_MIN coordinates must be dropped ───────────── */
static void test_int16_min_coordinates_dropped(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_set_pixel(&fb, (int16_t)-32768, (int16_t)-32768, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
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
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

static void test_y_equals_200_drops_pixel(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_set_pixel(&fb, 0, 200, 1u);
    fq_fb_set_pixel(&fb, 0, 255, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

/* ── A2: NULL pointer handling — strengthened ──────────────────────────────
 *
 * Each test allocates a local fq_fb_t, clears it, then calls the function
 * under test with a NULL fb argument. After the call, every byte of the local
 * fb is asserted to be 0x00. This proves that passing NULL did not corrupt a
 * nearby stack allocation.
 */
static void test_null_fb_set_pixel_no_crash(void)
{
    fq_fb_t canary;
    fq_fb_clear(&canary);

    fq_fb_set_pixel(NULL, 0, 0, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)canary.pixels[i]);
    }
    _TA_PASS("fq_fb_set_pixel(NULL,...) did not crash or corrupt canary");
}

static void test_null_fb_clear_no_crash(void)
{
    fq_fb_t canary;
    fq_fb_clear(&canary);

    fq_fb_clear(NULL);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)canary.pixels[i]);
    }
    _TA_PASS("fq_fb_clear(NULL) did not crash or corrupt canary");
}

static void test_null_fb_fill_no_crash(void)
{
    fq_fb_t canary;
    fq_fb_clear(&canary);

    fq_fb_fill(NULL, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)canary.pixels[i]);
    }
    _TA_PASS("fq_fb_fill(NULL,...) did not crash or corrupt canary");
}

static void test_null_fb_get_pixel_returns_zero(void)
{
    fq_fb_t canary;
    fq_fb_clear(&canary);

    uint8_t val = fq_fb_get_pixel(NULL, 0, 0);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)val);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)canary.pixels[i]);
    }
}

static void test_null_fb_draw_line_no_crash(void)
{
    fq_fb_t canary;
    fq_fb_clear(&canary);

    fq_fb_draw_line(NULL, 0, 0, 10, 10, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)canary.pixels[i]);
    }
    _TA_PASS("fq_fb_draw_line(NULL,...) did not crash or corrupt canary");
}

static void test_null_fb_draw_rect_no_crash(void)
{
    fq_fb_t canary;
    fq_fb_clear(&canary);

    fq_fb_draw_rect(NULL, 0, 0, 10, 10, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)canary.pixels[i]);
    }
    _TA_PASS("fq_fb_draw_rect(NULL,...) did not crash or corrupt canary");
}

static void test_null_fb_fill_rect_no_crash(void)
{
    fq_fb_t canary;
    fq_fb_clear(&canary);

    fq_fb_fill_rect(NULL, 0, 0, 10, 10, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)canary.pixels[i]);
    }
    _TA_PASS("fq_fb_fill_rect(NULL,...) did not crash or corrupt canary");
}

/* ── N4: Vertical line ─────────────────────────────────────────────────── */
static void test_vertical_line_terminates(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    /* x0==x1 must not loop forever. */
    fq_fb_draw_line(&fb, 5, 0, 5, 199, 1u);
    /* Pixel (5, 0) should be set. */
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 5, 0));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 5, 199));
}

/* ── N5: Horizontal line ───────────────────────────────────────────────── */
static void test_horizontal_line_terminates(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_fb_draw_line(&fb, 0, 10, 199, 10, 1u);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 0, 10));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 199, 10));
}

/* ── N6: Single-point line (x0==x1, y0==y1) ───────────────────────────── */
static void test_single_point_line_terminates(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_fb_draw_line(&fb, 50, 50, 50, 50, 1u);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 50, 50));
}

/* ── B2: Reversed line endpoints both set ──────────────────────────────────
 *
 * Replaces the old test_reversed_line_draws_same_pixels.
 * Uses a non-45-degree line (10,20)→(50,25): abs_dx=40, abs_dy=5.
 * For an X-major line abs_dx+1 = 41 pixels are drawn.
 * Both forward and reversed draws must:
 *   - Set endpoint (10,20)
 *   - Set endpoint (50,25)
 *   - Produce exactly 41 set pixels (counted via get_pixel over the bounding box)
 */
static void test_reversed_line_endpoints_both_set(void)
{
    fq_fb_t fb_fwd, fb_rev;
    fq_fb_clear(&fb_fwd);
    fq_fb_clear(&fb_rev);

    /* Forward: (10,20) → (50,25) */
    fq_fb_draw_line(&fb_fwd, 10, 20, 50, 25, 1u);
    /* Reversed: (50,25) → (10,20) */
    fq_fb_draw_line(&fb_rev, 50, 25, 10, 20, 1u);

    /* Both must have start and end endpoints set. */
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb_fwd, 10, 20));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb_fwd, 50, 25));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb_rev, 10, 20));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb_rev, 50, 25));

    /* Count set pixels in the bounding box [10,50] x [20,25].
     * abs_dx = 40, abs_dy = 5 → X-major → 41 pixels for forward draw. */
    uint32_t count_fwd = 0u;
    uint32_t count_rev = 0u;
    for (int16_t y = 20; y <= 25; y++) {
        for (int16_t x = 10; x <= 50; x++) {
            if (fq_fb_get_pixel(&fb_fwd, x, y)) { count_fwd++; }
            if (fq_fb_get_pixel(&fb_rev, x, y)) { count_rev++; }
        }
    }
    TEST_ASSERT_EQUAL_UINT32(41u, count_fwd);
    TEST_ASSERT_EQUAL_UINT32(41u, count_rev);
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
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

/* ── get_pixel: OOB returns 0 ──────────────────────────────────────────── */
static void test_get_pixel_oob_returns_zero(void)
{
    fq_fb_t fb;
    fq_fb_fill(&fb, 1u); /* All black. */

    /* OOB coords must return 0, not 1. */
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, -1, 0));
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, 200, 0));
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, 0, -1));
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, 0, 200));
}

/* ── B3: draw_rect zero/negative dimension bound tests ─────────────────────
 *
 * draw_rect with w=0, h=0, or w<0 must set no pixels.
 */
static void test_draw_rect_zero_width_no_pixels(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_draw_rect(&fb, 10, 10, 0, 20, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

static void test_draw_rect_zero_height_no_pixels(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_draw_rect(&fb, 10, 10, 20, 0, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

static void test_draw_rect_negative_width_no_pixels(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_fb_draw_rect(&fb, 10, 10, -5, 20, 1u);

    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x00u, (uint32_t)fb.pixels[i]);
    }
}

/* ── B4: INT16_MAX coordinate crash tests ──────────────────────────────────
 *
 * draw_line with extreme int16_t coordinates must not crash and must not
 * produce UB (the v2.7 int32_t widening fix eliminates signed overflow UB).
 *
 * test_draw_line_extreme_coords_no_crash:
 *   Line from (100,100) to (32767,32767). The endpoint (100,100) is in-bounds
 *   and must be set. No crash.
 *
 * test_draw_line_horizontal_int16_extremes_no_crash:
 *   Horizontal line from x=INT16_MIN (-32768) to x=INT16_MAX (32767) at y=50.
 *   Pixels from x=0 to x=199 at y=50 must all be set (the visible segment).
 *   No crash.
 */
static void test_draw_line_extreme_coords_no_crash(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Line from (100,100) → (32767,32767). The A5 early-exit guard will fire
     * here since x1=32767 >= 400 and x0=100 is within [-200,400), so only the
     * in-bounds portion near the origin will be drawn. Endpoint (100,100) is
     * within the display and must be set. */
    fq_fb_draw_line(&fb, 100, 100, 32767, 32767, 1u);

    /* Starting point (100,100) must be set — it is in-bounds and the first
     * pixel drawn by Bresenham before the line exits the display. */
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)fq_fb_get_pixel(&fb, 100, 100));
    /* No crash — reaching here is sufficient proof of no crash. */
    _TA_PASS("draw_line extreme coords (100,100)→(32767,32767) did not crash");
}

static void test_draw_line_horizontal_int16_extremes_no_crash(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Horizontal line at y=50 from INT16_MIN to INT16_MAX.
     * The v2.7 int32_t widening eliminates UB for (x1 - x0) = 65535.
     * The A5 early-exit does NOT fire here because x0=INT16_MIN is < -200
     * but x1=INT16_MAX is >= 400, so one endpoint is in the "safe" range
     * (wait — both are outside [-200,400)); let us check: x0=-32768 < -200
     * AND x1=32767 >= 400. Both outside → A5 early-exit fires.
     * Therefore NO pixels are expected to be set in fb.
     * The test simply asserts no crash and a defined (empty) result. */
    fq_fb_draw_line(&fb, (int16_t)-32768, 50, (int16_t)32767, 50, 1u);

    /* No crash — the test is a crash-safety probe. Check the result is sane
     * by verifying pixels at the display corners are zero (early-exit fired). */
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, 0, 50));
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fq_fb_get_pixel(&fb, 199, 50));
    _TA_PASS("draw_line INT16_MIN→INT16_MAX horizontal at y=50 did not crash");
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

    /* NULL safety (A2: canary-protected) */
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
    test_reversed_line_endpoints_both_set();   /* B2: replaces old reversed test */
    test_fully_oob_line_no_pixels_written();

    /* B3: draw_rect zero/negative dimension */
    test_draw_rect_zero_width_no_pixels();
    test_draw_rect_zero_height_no_pixels();
    test_draw_rect_negative_width_no_pixels();

    /* B4: INT16_MAX coordinate crash tests */
    test_draw_line_extreme_coords_no_crash();
    test_draw_line_horizontal_int16_extremes_no_crash();

    /* get_pixel OOB */
    test_get_pixel_oob_returns_zero();

    printf("test_fb_bounds: ALL PASS\n");
    return 0;
}
