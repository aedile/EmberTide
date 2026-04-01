/**
 * test_p17_asset_feature.c — Phase 17 Asset Pipeline: Feature Tests
 *
 * Rule 22 (FEATURE RED): Happy-path tests that prove the generated asset
 * headers produce correct pixel output when used with the presentation API.
 *
 * Tests:
 *   F01 — blit known sprite; verify specific fb bytes are non-zero in the
 *          sprite region (the Dark Knight sprite is NOT all-zeros).
 *   F02 — draw_text "A"; verify at least one non-zero byte in the glyph area.
 *
 * Specific-value assertions (Constitution Priority 4):
 *   F01: sprite_char_r00_c00 layout (4 bytes/row, 32 rows = 128 bytes):
 *       Rows 0-9:  all zeros (bytes 0-39 = 0x00).
 *       Row 10:    bytes[40..43] = 0x00, 0x0F, 0xF0, 0x00.
 *       Blit at (0,0): sprite row 10 maps to fb row 10.
 *       fb row 10 starts at byte 10*25=250; fb.pixels[251] covers sprite
 *       pixels x=8..15 of row 10.  sprite byte[41]=0x0F sets pixels x=12..15
 *       (lower nibble of fb.pixels[251] = 0x0F).
 *
 *   F02 draw_text "A" at (0,0): scan fb bytes in the glyph bounding column
 *       region for a non-zero byte.
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

/* ── F01: blit known sprite → non-zero bytes in sprite region ─────────── */
static void test_blit_sprite_produces_nonzero_pixels(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /*
     * Blit the Dark Knight sprite (row 0, col 0 of SPRITE_CHAR_TABLE) at
     * position (0, 0).  The sprite is 32×32 pixels.
     *
     * After blitting, at least one byte in the 32-row region of the
     * framebuffer must be non-zero.  Scan bytes 0 .. (32*FQ_FB_STRIDE - 1).
     */
    fq_blit_sprite(&fb, 0, 0, &SPRITE_CHAR_TABLE[0]);

    int found_nonzero = 0;
    uint32_t scan_bytes = (uint32_t)(32u * FQ_FB_STRIDE);
    for (uint32_t i = 0; i < scan_bytes; i++) {
        if (fb.pixels[i] != 0x00u) {
            found_nonzero = 1;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_nonzero);

    /*
     * Specific-value assertion:
     * Row 10 of sprite_char_r00_c00 is bytes[40..43] = 0x00, 0x0F, 0xF0, 0x00.
     * At blit position (0, 0), sprite row 10 maps to fb row 10.
     * fb row 10 starts at byte offset 10 * FQ_FB_STRIDE = 10 * 25 = 250.
     * Byte fb.pixels[251] covers pixels x=8..15.
     * sprite byte[41]=0x0F has bits 3..0 set → pixels x=12..15 are drawn.
     * fq_fb_set_pixel uses bit_mask = 0x80 >> (x%8):
     *   x=12 → mask=0x08, x=13 → mask=0x04, x=14 → mask=0x02, x=15 → mask=0x01
     * Result: fb.pixels[251] = 0x08 | 0x04 | 0x02 | 0x01 = 0x0F.
     */
    TEST_ASSERT_EQUAL_UINT32(0x0Fu, (uint32_t)fb.pixels[10u * FQ_FB_STRIDE + 1u]);
}

/* ── F02: draw_text "A" → non-zero byte in glyph area ───────────────── */
static void test_draw_text_A_produces_nonzero_pixels(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    /*
     * Render "A" at (0, 0).  FONT_REGS_12 has glyph_h=30, glyph_max_w=11.
     * At least one pixel must be set in the glyph region.
     *
     * Scan fb bytes in the region covering rows 0 .. 29 (glyph height).
     * Each fb row is FQ_FB_STRIDE=25 bytes wide; we scan the first 2 bytes
     * of each row (covering pixels 0..15, more than enough for an 11-px glyph).
     */
    fq_draw_text(&fb, &FONT_REGS_12_FONT, 0, 0, "A");

    int found_nonzero = 0;
    for (uint32_t row = 0; row < FONT_REGS_12_FONT.glyph_h; row++) {
        for (uint32_t col = 0; col < 2u; col++) {
            uint32_t idx = row * (uint32_t)FQ_FB_STRIDE + col;
            if (fb.pixels[idx] != 0x00u) {
                found_nonzero = 1;
                break;
            }
        }
        if (found_nonzero) {
            break;
        }
    }
    TEST_ASSERT_TRUE(found_nonzero);
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void)
{
    test_blit_sprite_produces_nonzero_pixels();
    test_draw_text_A_produces_nonzero_pixels();

    printf("test_p17_asset_feature: ALL PASS\n");
    return 0;
}
