/**
 * test_text_bounds.c — Phase 7: Font Renderer Bound/Edge-Case Tests (BOUND RED)
 *
 * Rule 22: Boundary tests committed BEFORE any implementation.
 * Covers:
 *   N16: Empty string → no pixels changed, returns x
 *   N17: NULL string → returns x unchanged, no crash
 *   N18: NULL font → returns x unchanged, no crash
 *   N19: ASCII 0x20 and 0x7E are valid; 0x1F and 0x7F skipped
 *   N20: Cursor position uses int16_t (value can exceed 200)
 *   N22: Text stops at right edge (cursor never exceeds 200+ glyph start)
 *   N16b: fq_text_width on NULL font / NULL string
 *   N23: Non-printable chars in middle of string are silently skipped
 */

#include "test_assert.h"
#include "fq_framebuffer.h"
#include "fq_text.h"
#include <string.h>
#include <stdint.h>

/* ── Minimal inline test font ──────────────────────────────────────────── */
/*
 * 8x8 monospace font covering only ASCII 32 (space) through 34 ('"').
 * We define 95 entries (covering 32-126), but only fill 3 meaningful ones.
 * Glyph data: each character is 8 rows of 1 byte = 1 byte per row.
 * All glyphs are 8 pixels wide × 8 pixels tall.
 *
 * glyph_index = ch - 32
 * bitmap offset = glyph_index * glyph_h bytes (since ceil(8/8)=1 byte/row)
 *
 * For simplicity, the test font has all glyphs as a repeating pattern.
 * The actual pixel content doesn't matter for bound tests — only the
 * structural correctness (no crash, correct cursor advancement) matters.
 */

#define TEST_FONT_GLYPHS   95u   /* ASCII 32..126 */
#define TEST_FONT_H         8u
#define TEST_FONT_MAX_W     8u

/* One full row of glyph data = 1 byte (width=8 → ceil(8/8)=1). */
static const uint8_t g_test_bitmap[TEST_FONT_GLYPHS * TEST_FONT_H] = { 0u };

/* Advance widths: space=4, all others=8. */
static uint8_t g_test_widths[TEST_FONT_GLYPHS];

/* x/y offsets: all zero for this test font. */
static int8_t g_test_offsets_x[TEST_FONT_GLYPHS];
static int8_t g_test_offsets_y[TEST_FONT_GLYPHS];

static void init_test_font_tables(void)
{
    for (uint32_t i = 0; i < TEST_FONT_GLYPHS; i++) {
        g_test_widths[i]    = 8u;
        g_test_offsets_x[i] = 0;
        g_test_offsets_y[i] = 0;
    }
    /* Space (index 0) is narrower. */
    g_test_widths[0] = 4u;
}

static fq_font_t make_test_font(void)
{
    fq_font_t f;
    f.bitmap      = g_test_bitmap;
    f.widths      = g_test_widths;
    f.offsets_x   = g_test_offsets_x;
    f.offsets_y   = g_test_offsets_y;
    f.glyph_h     = TEST_FONT_H;
    f.glyph_max_w = TEST_FONT_MAX_W;
    return f;
}

/* ── N17: NULL font ────────────────────────────────────────────────────── */
static void test_null_font_draw_returns_x(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    int16_t result = fq_draw_text(&fb, NULL, 10, 10, "ABC");
    /* Must return the same x=10, no crash. */
    TEST_ASSERT_EQUAL_INT(10, (int)result);
    /* No pixels modified. */
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[i]);
    }
}

static void test_null_font_width_returns_zero(void)
{
    int16_t w = fq_text_width(NULL, "ABC");
    TEST_ASSERT_EQUAL_INT(0, (int)w);
}

/* ── N18: NULL string ──────────────────────────────────────────────────── */
static void test_null_string_draw_returns_x(void)
{
    init_test_font_tables();
    fq_font_t font = make_test_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    int16_t result = fq_draw_text(&fb, &font, 20, 5, NULL);
    TEST_ASSERT_EQUAL_INT(20, (int)result);
    for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8(0x00u, fb.pixels[i]);
    }
}

static void test_null_string_width_returns_zero(void)
{
    init_test_font_tables();
    fq_font_t font = make_test_font();
    int16_t w = fq_text_width(&font, NULL);
    TEST_ASSERT_EQUAL_INT(0, (int)w);
}

/* ── N16: Empty string ─────────────────────────────────────────────────── */
static void test_empty_string_returns_x_unchanged(void)
{
    init_test_font_tables();
    fq_font_t font = make_test_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    int16_t result = fq_draw_text(&fb, &font, 50, 0, "");
    TEST_ASSERT_EQUAL_INT(50, (int)result);
}

static void test_empty_string_width_zero(void)
{
    init_test_font_tables();
    fq_font_t font = make_test_font();
    int16_t w = fq_text_width(&font, "");
    TEST_ASSERT_EQUAL_INT(0, (int)w);
}

/* ── N19: ASCII boundaries ─────────────────────────────────────────────── */
/*
 * Characters 0x20 (space, glyph_index=0) and 0x7E ('~', glyph_index=94) are
 * VALID and must advance the cursor.
 * Characters 0x1F and 0x7F are OUT-OF-RANGE and must be silently skipped.
 * A string mixing valid and invalid chars: cursor advances only for valid ones.
 */
static void test_ascii_boundaries_valid_chars_advance_cursor(void)
{
    init_test_font_tables();
    fq_font_t font = make_test_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Draw only one space (0x20) at x=0: cursor should advance by width[0]=4. */
    int16_t result = fq_draw_text(&fb, &font, 0, 0, " ");
    TEST_ASSERT_EQUAL_INT(4, (int)result);
}

static void test_ascii_0x7e_advances_cursor(void)
{
    init_test_font_tables();
    fq_font_t font = make_test_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* '~' = 0x7E, glyph_index=94, width=8 in our test font. */
    int16_t result = fq_draw_text(&fb, &font, 0, 0, "~");
    TEST_ASSERT_EQUAL_INT(8, (int)result);
}

static void test_ascii_0x1f_skipped(void)
{
    init_test_font_tables();
    fq_font_t font = make_test_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* String with 0x1F: non-printable below space. Cursor stays at 0. */
    char str[2] = { 0x1F, 0x00 };
    int16_t result = fq_draw_text(&fb, &font, 0, 0, str);
    TEST_ASSERT_EQUAL_INT(0, (int)result);
}

static void test_ascii_0x7f_skipped(void)
{
    init_test_font_tables();
    fq_font_t font = make_test_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    char str[2] = { 0x7F, 0x00 };
    int16_t result = fq_draw_text(&fb, &font, 0, 0, str);
    TEST_ASSERT_EQUAL_INT(0, (int)result);
}

/* ── N23: Non-printable in middle of string ─────────────────────────────── */
static void test_nonprintable_in_middle_skipped(void)
{
    init_test_font_tables();
    fq_font_t font = make_test_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* "A\x01\x02B" — two non-printables between two 'A' chars.
     * Each 'A' (0x41, glyph_index=33) has width=8.
     * Non-printables skipped → cursor advances 8+8 = 16.
     */
    char str[] = { 'A', 0x01, 0x02, 'B', 0x00 };
    int16_t result = fq_draw_text(&fb, &font, 0, 0, str);
    TEST_ASSERT_EQUAL_INT(16, (int)result);
}

/* ── N20: int16_t cursor — can exceed 200 ────────────────────────────────── */
static void test_cursor_uses_int16_not_uint8(void)
{
    init_test_font_tables();
    fq_font_t font = make_test_font();

    /* Measure a string that would overflow a uint8_t (>255 pixels wide). */
    /* 33 chars × 8 pixels each = 264 pixels. uint8_t would wrap at 256. */
    char long_str[34];
    memset(long_str, 'A', 33);
    long_str[33] = '\0';

    int16_t w = fq_text_width(&font, long_str);
    /* Must be 264, not 8 (uint8_t wrap would give 264 % 256 = 8). */
    TEST_ASSERT_EQUAL_INT(264, (int)w);
}

/* ── N22: Text stops at right edge ──────────────────────────────────────── */
static void test_text_stops_at_right_edge(void)
{
    init_test_font_tables();
    fq_font_t font = make_test_font();
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /* Start at x=196 with 8-wide glyphs: first glyph starts at 196,
     * ends at 204 — past the 200-px boundary. Should NOT draw and stop.
     * (Spec: stop rendering when next glyph would start past x=199.)
     * Actually spec says "stop rendering when next glyph would start past x=199".
     * At x=196, glyph ends at 196+8=204 > 200. The glyph START (196) is <=199,
     * so the PM API says we draw it (clipping handles the overflow via set_pixel).
     * Let's test: a 1000-char string starting at x=0 only draws chars that fit.
     */
    char str[201];
    memset(str, 'A', 200);
    str[200] = '\0';

    int16_t cursor = fq_draw_text(&fb, &font, 0, 0, str);

    /* Cursor must not exceed 200 + glyph_max_w (it stops before a glyph
     * whose START position is > 199). 200 / 8 = 25 chars fit exactly.
     * cursor = 25 * 8 = 200. */
    TEST_ASSERT_EQUAL_INT(200, (int)cursor);
}

/* ── NULL fb pointer ────────────────────────────────────────────────────── */
static void test_null_fb_draw_text_no_crash(void)
{
    init_test_font_tables();
    fq_font_t font = make_test_font();
    int16_t result = fq_draw_text(NULL, &font, 0, 0, "Hello");
    /* Must return input x=0 without crashing. */
    TEST_ASSERT_EQUAL_INT(0, (int)result);
}

/* ── main ──────────────────────────────────────────────────────────────── */
int main(void)
{
    test_null_font_draw_returns_x();
    test_null_font_width_returns_zero();
    test_null_string_draw_returns_x();
    test_null_string_width_returns_zero();
    test_empty_string_returns_x_unchanged();
    test_empty_string_width_zero();

    test_ascii_boundaries_valid_chars_advance_cursor();
    test_ascii_0x7e_advances_cursor();
    test_ascii_0x1f_skipped();
    test_ascii_0x7f_skipped();
    test_nonprintable_in_middle_skipped();

    test_cursor_uses_int16_not_uint8();
    test_text_stops_at_right_edge();

    test_null_fb_draw_text_no_crash();

    printf("test_text_bounds: ALL PASS\n");
    return 0;
}
