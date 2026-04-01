/**
 * render_all_screens.c
 *
 * Visual regression harness — Phase 1 skeleton version.
 *
 * Allocates a 5000-byte 1-bit framebuffer (200x200 pixels, 1 bit per pixel,
 * 25 bytes per row), fills it with 1s (all-black for this e-paper display),
 * then writes output/blank.png using stb_image_write.
 *
 * Per spec-challenger N3: the return value of stbi_write_png() is checked.
 * Exit code is non-zero if the write fails.
 *
 * Per spec-challenger N7: _Static_assert validates framebuffer is exactly
 * 5000 bytes.
 *
 * The output/ directory is created if it does not exist (POSIX mkdir).
 */

/* STB single-header implementation — define EXACTLY ONCE in this translation
 * unit. All other files that include stb_image_write.h must NOT define this. */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendors/stb_image_write.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* ---------------------------------------------------------------------------
 * Framebuffer constants
 * ---------------------------------------------------------------------------
 * Display: 200 x 200 pixels, 1 bit per pixel.
 * Packed row-major: 200 pixels / 8 bits = 25 bytes per row.
 * Total: 200 rows * 25 bytes = 5000 bytes.
 */
#define FB_WIDTH_PX    (200u)
#define FB_HEIGHT_PX   (200u)
#define FB_STRIDE_BYTES ((FB_WIDTH_PX + 7u) / 8u)   /* 25 bytes per row */
#define FB_SIZE_BYTES  (FB_HEIGHT_PX * FB_STRIDE_BYTES) /* 5000 bytes */

/* N7: Compile-time assertion that framebuffer is exactly 5000 bytes. */
_Static_assert(FB_SIZE_BYTES == 5000u,
               "Framebuffer must be exactly 5000 bytes (200x200 1-bit)");
_Static_assert(FB_STRIDE_BYTES == 25u,
               "Framebuffer stride must be exactly 25 bytes (200 pixels / 8)");

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
 * packed_row : pointer to FB_STRIDE_BYTES packed bytes for one row.
 * out_row    : caller-allocated buffer of FB_WIDTH_PX bytes.
 * ---------------------------------------------------------------------------
 */
static void expand_1bit_to_grayscale_row(const uint8_t *packed_row,
                                          uint8_t       *out_row)
{
    for (uint32_t x = 0; x < FB_WIDTH_PX; x++) {
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
 *
 * Returns 0 on success, -1 on failure.
 * ---------------------------------------------------------------------------
 */
static int write_framebuffer_png(const uint8_t *framebuffer,
                                  const char    *filepath)
{
    /* Temporary row buffer — one grayscale byte per pixel. */
    uint8_t row_gray[FB_WIDTH_PX];

    /* stbi_write_png expects a flat RGBA or grayscale byte array.
     * We build a full grayscale image buffer (200*200 bytes). */
    uint8_t *gray_image = (uint8_t *)malloc(FB_WIDTH_PX * FB_HEIGHT_PX);
    if (!gray_image) {
        fprintf(stderr, "render_all_screens: malloc failed for gray_image\n");
        return -1;
    }

    for (uint32_t y = 0; y < FB_HEIGHT_PX; y++) {
        const uint8_t *src_row = framebuffer + (y * FB_STRIDE_BYTES);
        expand_1bit_to_grayscale_row(src_row, row_gray);
        memcpy(gray_image + (y * FB_WIDTH_PX), row_gray, FB_WIDTH_PX);
    }

    /* N3: Check stbi_write_png return value — 0 = failure. */
    int result = stbi_write_png(
        filepath,
        (int)FB_WIDTH_PX,
        (int)FB_HEIGHT_PX,
        1,              /* 1 channel = grayscale */
        gray_image,
        (int)FB_WIDTH_PX  /* stride in bytes for the grayscale buffer */
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
    /* Static 5000-byte 1-bit framebuffer. No malloc in the fast path. */
    static uint8_t framebuffer[FB_SIZE_BYTES];

    /* Fill with 0xFF — every bit set = all pixels black (e-paper convention). */
    memset(framebuffer, 0xFF, sizeof(framebuffer));

    /* Ensure the output directory exists. */
    if (ensure_output_dir("output") != 0) {
        return EXIT_FAILURE;
    }

    /* Write blank (all-black) screen. */
    printf("render_all_screens: writing output/blank.png ...\n");
    if (write_framebuffer_png(framebuffer, "output/blank.png") != 0) {
        return EXIT_FAILURE;
    }
    printf("render_all_screens: output/blank.png written successfully "
           "(%u x %u, 1-bit, %u bytes framebuffer)\n",
           FB_WIDTH_PX, FB_HEIGHT_PX, FB_SIZE_BYTES);

    /* ---------------------------------------------------------------------------
     * N3: Failure path test — attempt to write to an invalid path.
     * This proves stbi_write_png returns 0 on failure (not a segfault).
     * We do NOT exit non-zero for this — we only verify the return value is 0.
     * ---------------------------------------------------------------------------
     */
    printf("render_all_screens: testing stbi_write_png failure path ...\n");
    int bad_result = stbi_write_png(
        "/nonexistent_dir_that_cannot_exist/test.png",
        (int)FB_WIDTH_PX,
        (int)FB_HEIGHT_PX,
        1,
        framebuffer,   /* raw 1-bit data — just checking the error path */
        (int)FB_STRIDE_BYTES
    );
    if (bad_result != 0) {
        fprintf(stderr,
                "render_all_screens: FAIL — expected stbi_write_png to return 0 "
                "for invalid path, got %d\n", bad_result);
        return EXIT_FAILURE;
    }
    printf("render_all_screens: stbi_write_png correctly returned 0 for invalid path\n");

    printf("render_all_screens: ALL SCREENS OK\n");
    return EXIT_SUCCESS;
}
