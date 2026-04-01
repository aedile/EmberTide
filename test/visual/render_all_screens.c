/**
 * render_all_screens.c
 *
 * Visual regression harness — Phase 7 updated version.
 *
 * B1 (review): Replaced raw uint8_t framebuffer[5000] with fq_fb_t from
 * fq_framebuffer.h. Uses fq_fb_clear/fq_fb_fill instead of raw memset.
 * Passes framebuffer.pixels to the PNG writer. Local constants (FB_WIDTH_PX
 * etc.) removed; FQ_FB_WIDTH, FQ_FB_HEIGHT, FQ_FB_STRIDE, FQ_FB_SIZE used.
 *
 * B1 visual demo: In addition to output/blank.png (all-black), writes
 * output/fb_test.png — a framebuffer with:
 *   - A border rect (full display outline)
 *   - An X drawn from corner to corner using fq_fb_draw_line
 * This exercises fq_fb_draw_line and fq_fb_draw_rect in the visual harness.
 *
 * Per spec-challenger N3: the return value of stbi_write_png() is checked.
 * Exit code is non-zero if the write fails.
 *
 * Per review advisory A2: the failure path test uses NULL as the filepath.
 *
 * The output/ directory is created if it does not exist (POSIX mkdir).
 */

/* STB single-header implementation — define EXACTLY ONCE in this translation
 * unit. All other files that include stb_image_write.h must NOT define this. */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendors/stb_image_write.h"

#include "fq_framebuffer.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* ---------------------------------------------------------------------------
 * ensure_output_dir
 *
 * Creates the output/ directory relative to the current working directory.
 * Returns 0 on success, -1 on failure.
 * ---------------------------------------------------------------------------
 */
static int ensure_output_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        /* Path exists — verify it is a directory. */
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }
        fprintf(stderr, "render_all_screens: '%s' exists but is not a directory\n",
                path);
        return -1;
    }
    /* Directory does not exist — create it. */
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "render_all_screens: cannot create '%s': %s\n",
                path, strerror(errno));
        return -1;
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * expand_1bit_to_grayscale_row
 *
 * Converts one row of 1-bit packed data to a 1-byte-per-pixel grayscale row
 * suitable for stbi_write_png (which expects byte-aligned channel data).
 *
 * Bit convention: 1 = black (0x00), 0 = white (0xFF).
 * This matches typical e-paper display convention.
 *
 * packed_row : pointer to FQ_FB_STRIDE packed bytes for one row.
 * out_row    : caller-allocated buffer of FQ_FB_WIDTH bytes.
 * ---------------------------------------------------------------------------
 */
static void expand_1bit_to_grayscale_row(const uint8_t *packed_row,
                                          uint8_t       *out_row)
{
    for (uint32_t x = 0; x < FQ_FB_WIDTH; x++) {
        uint32_t byte_idx = x / 8u;
        uint32_t bit_idx  = 7u - (x % 8u);  /* MSB first */
        uint8_t  bit      = (packed_row[byte_idx] >> bit_idx) & 0x01u;
        /* 1-bit = black (0x00), 0-bit = white (0xFF) */
        out_row[x] = bit ? 0x00u : 0xFFu;
    }
}

/* ---------------------------------------------------------------------------
 * write_framebuffer_png
 *
 * Expands the 1-bit framebuffer to grayscale and writes a PNG file.
 * Takes a pointer to fq_fb_t and uses fb->pixels.
 *
 * Returns 0 on success, -1 on failure.
 * ---------------------------------------------------------------------------
 */
static int write_framebuffer_png(const fq_fb_t *fb,
                                  const char    *filepath)
{
    /* Temporary row buffer — one grayscale byte per pixel. */
    uint8_t row_gray[FQ_FB_WIDTH];

    /* stbi_write_png expects a flat RGBA or grayscale byte array.
     * We build a full grayscale image buffer (FQ_FB_WIDTH * FQ_FB_HEIGHT bytes). */
    uint8_t *gray_image = (uint8_t *)malloc(FQ_FB_WIDTH * FQ_FB_HEIGHT);
    if (!gray_image) {
        fprintf(stderr, "render_all_screens: malloc failed for gray_image\n");
        return -1;
    }

    for (uint32_t y = 0; y < FQ_FB_HEIGHT; y++) {
        const uint8_t *src_row = fb->pixels + (y * FQ_FB_STRIDE);
        expand_1bit_to_grayscale_row(src_row, row_gray);
        memcpy(gray_image + (y * FQ_FB_WIDTH), row_gray, FQ_FB_WIDTH);
    }

    /* N3: Check stbi_write_png return value — 0 = failure. */
    int result = stbi_write_png(
        filepath,
        (int)FQ_FB_WIDTH,
        (int)FQ_FB_HEIGHT,
        1,                  /* 1 channel = grayscale */
        gray_image,
        (int)FQ_FB_WIDTH    /* stride in bytes for the grayscale buffer */
    );

    free(gray_image);

    if (result == 0) {
        fprintf(stderr, "render_all_screens: stbi_write_png failed for '%s'\n",
                filepath);
        return -1;
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------
 */
int main(void)
{
    /* B1: Static fq_fb_t — no malloc in the fast path. */
    static fq_fb_t framebuffer;

    /* Ensure the output directory exists. */
    if (ensure_output_dir("output") != 0) {
        return EXIT_FAILURE;
    }

    /* -----------------------------------------------------------------------
     * Screen 1: blank.png — all-black (fq_fb_fill with color=1).
     * ----------------------------------------------------------------------- */
    fq_fb_fill(&framebuffer, 1u);

    printf("render_all_screens: writing output/blank.png ...\n");
    if (write_framebuffer_png(&framebuffer, "output/blank.png") != 0) {
        return EXIT_FAILURE;
    }
    printf("render_all_screens: output/blank.png written successfully "
           "(%u x %u, 1-bit, %u bytes framebuffer)\n",
           (unsigned)FQ_FB_WIDTH, (unsigned)FQ_FB_HEIGHT, (unsigned)FQ_FB_SIZE);

    /* -----------------------------------------------------------------------
     * Screen 2: fb_test.png — visual demo: X from corners + border rect.
     *
     * Demonstrates fq_fb_draw_line and fq_fb_draw_rect on a white background.
     *   - Border rect: full display outline (0,0) to (199,199)
     *   - Diagonal 1:  top-left (0,0) to bottom-right (199,199)
     *   - Diagonal 2:  top-right (199,0) to bottom-left (0,199)
     * ----------------------------------------------------------------------- */
    fq_fb_clear(&framebuffer);

    /* Border rect: outline of the full 200x200 display. */
    fq_fb_draw_rect(&framebuffer, 0, 0,
                    (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* Diagonal: top-left → bottom-right. */
    fq_fb_draw_line(&framebuffer, 0, 0, (int16_t)(FQ_FB_WIDTH - 1u),
                    (int16_t)(FQ_FB_HEIGHT - 1u), 1u);

    /* Diagonal: top-right → bottom-left. */
    fq_fb_draw_line(&framebuffer, (int16_t)(FQ_FB_WIDTH - 1u), 0,
                    0, (int16_t)(FQ_FB_HEIGHT - 1u), 1u);

    printf("render_all_screens: writing output/fb_test.png ...\n");
    if (write_framebuffer_png(&framebuffer, "output/fb_test.png") != 0) {
        return EXIT_FAILURE;
    }
    printf("render_all_screens: output/fb_test.png written successfully "
           "(border rect + X from corners)\n");

    /* -----------------------------------------------------------------------
     * Failure path test — pass NULL as the filepath.
     *
     * stbi_write_png(NULL, ...) returns 0 without attempting any I/O.
     * ----------------------------------------------------------------------- */
    printf("render_all_screens: testing stbi_write_png failure path (NULL filepath) ...\n");
    int bad_result = stbi_write_png(
        NULL,
        (int)FQ_FB_WIDTH,
        (int)FQ_FB_HEIGHT,
        1,
        framebuffer.pixels,
        (int)FQ_FB_STRIDE
    );
    if (bad_result != 0) {
        fprintf(stderr,
                "render_all_screens: FAIL — expected stbi_write_png to return 0 "
                "for NULL filepath, got %d\n", bad_result);
        return EXIT_FAILURE;
    }
    printf("render_all_screens: stbi_write_png correctly returned 0 for NULL filepath\n");

    printf("render_all_screens: ALL SCREENS OK\n");
    return EXIT_SUCCESS;
}
