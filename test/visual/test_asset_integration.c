/**
 * test_asset_integration.c -- Phase 17 Visual Integration Test
 *
 * Verifies that the Phase 17 asset pipeline produces headers that:
 *   1. Compile cleanly against fq_sprite_t and fq_font_t.
 *   2. Produce a visible sprite when blit to a framebuffer.
 *   3. Produce readable text when rendered with fq_draw_text.
 *
 * Output: output/asset_integration.png
 *
 * Visual pass criteria (inspected by human/reviewer):
 *   - A 32x32 character sprite appears near the centre of the display (84, 60).
 *   - "FiestaQuest!" appears in small pixel font near the top (10, 10).
 *   - "Lv.1 Bruiser" appears on the next text line (10, 50).
 *
 * Constitution Priority 0: no float arithmetic.
 * Architecture: presentation layer only -- no hal headers, no game headers.
 *
 * Note on stb_image_write.h:
 *   The IMPLEMENTATION macro must be defined exactly ONCE per link unit.
 *   test_asset_integration is a SEPARATE executable in CMakeLists.txt,
 *   so it defines the implementation here independently of render_all_screens.
 */

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendors/stb_image_write.h"

#include "fq_framebuffer.h"
#include "fq_sprite.h"
#include "fq_text.h"

/* Generated asset headers (Phase 17 output) */
#include "sprites/sprite_chars.h"
#include "fonts/font_regs_12.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* ---------------------------------------------------------------------------
 * ensure_output_dir -- create output/ directory if it does not exist.
 * --------------------------------------------------------------------------- */

static int ensure_output_dir(const char *dir)
{
    struct stat st;
    if (stat(dir, &st) == 0) {
        return 0;  /* already exists */
    }
#ifdef _WIN32
    return _mkdir(dir);
#else
    return mkdir(dir, 0755);
#endif
}

/* ---------------------------------------------------------------------------
 * convert_fb_to_greyscale
 *
 * Expand the packed 1-bit framebuffer to an 8-bit greyscale buffer for
 * stbi_write_png(). Bit = 1 (black) maps to 0x00; bit = 0 (white) to 0xFF.
 * --------------------------------------------------------------------------- */

static void convert_fb_to_greyscale(const fq_fb_t *fb, uint8_t *grey_buf)
{
    uint32_t pixel = 0;
    uint32_t byte_idx;
    for (byte_idx = 0; byte_idx < FQ_FB_SIZE; byte_idx++) {
        uint8_t b = fb->pixels[byte_idx];
        int bit;
        for (bit = 7; bit >= 0; bit--) {
            grey_buf[pixel++] = ((b >> (uint8_t)bit) & 1u) ? 0x00u : 0xFFu;
        }
    }
}

/* ---------------------------------------------------------------------------
 * main
 * --------------------------------------------------------------------------- */

int main(void)
{
    /* 1. Initialise framebuffer (all white). */
    static fq_fb_t fb;
    fq_fb_clear(&fb);

    /* 2. Draw a thin border so the 200x200 canvas boundary is visible. */
    fq_fb_draw_rect(&fb,
                    0, 0,
                    (int16_t)(FQ_FB_WIDTH  - 1),
                    (int16_t)(FQ_FB_HEIGHT - 1),
                    1u);

    /* 3. Render "FiestaQuest!" at (10, 10). */
    fq_draw_text(&fb, &FONT_REGS_12_FONT, 10, 10, "FiestaQuest!");

    /* 4. Render "Lv.1 Bruiser" at (10, 50). */
    fq_draw_text(&fb, &FONT_REGS_12_FONT, 10, 50, "Lv.1 Bruiser");

    /*
     * 5. Blit the Dark Knight sprite (row 0, col 0) at (84, 60).
     *    SPRITE_CHAR_TABLE is row-major: index = row * FM_CHAR_COLS + col.
     *    Row 0, Col 0 -> index 0 = Dark Knight idle frame.
     */
    fq_blit_sprite(&fb, 84, 60, &SPRITE_CHAR_TABLE[0]);

    /* 6. Write output PNG. */
    if (ensure_output_dir("output") != 0) {
        fprintf(stderr,
                "test_asset_integration: failed to create output/ dir: %s\n",
                strerror(errno));
        return 1;
    }

    static uint8_t grey_buf[FQ_FB_WIDTH * FQ_FB_HEIGHT];
    convert_fb_to_greyscale(&fb, grey_buf);

    int ok = stbi_write_png(
        "output/asset_integration.png",
        FQ_FB_WIDTH, FQ_FB_HEIGHT,
        1,              /* 1 channel = greyscale */
        grey_buf,
        FQ_FB_WIDTH     /* stride in bytes */
    );

    if (!ok) {
        fprintf(stderr, "test_asset_integration: stbi_write_png failed\n");
        return 1;
    }

    printf("test_asset_integration: wrote output/asset_integration.png\n");
    printf("  Sprite:  Dark Knight (row 0, col 0) at (84, 60) -- 32x32 px\n");
    printf("  Text 1:  'FiestaQuest!' at (10, 10)\n");
    printf("  Text 2:  'Lv.1 Bruiser' at (10, 50)\n");
    printf("  Font:    FONT_REGS_12  glyph_max_w=%u  glyph_h=%u\n",
           (unsigned)FONT_REGS_12_FONT.glyph_max_w,
           (unsigned)FONT_REGS_12_FONT.glyph_h);

    return 0;
}
