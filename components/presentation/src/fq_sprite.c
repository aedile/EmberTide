/**
 * fq_sprite.c — FiestaQuest Presentation Layer: 1-bit Sprite Blitter
 *
 * OR-blit implementation with 4-edge clipping and non-byte-aligned X support.
 *
 * Algorithm summary:
 *   For each row of the sprite (after top/bottom clip):
 *     For each pixel in the row (after left/right clip):
 *       Read the sprite bit at (sprite_x, sprite_row).
 *       If bit == 1: set the corresponding framebuffer pixel to black.
 *
 * Non-byte-aligned optimization: handled via per-pixel fq_fb_set_pixel
 * calls, which correctly computes the bit address for any x position.
 * This avoids the complexity of multi-byte shift logic while remaining
 * correct for all 8 alignment cases and all widths.
 *
 * No floating-point. No malloc. Pure function.
 * HOST-COMPILABLE — no hal_*.h.
 */

#include "fq_sprite.h"
#include "fq_framebuffer.h"
#include <stdint.h>
#include <stddef.h>

/* ── Internal helper ──────────────────────────────────────────────────── */

/**
 * sprite_get_bit — Read a single bit from a packed sprite bitmap.
 *
 * @param data    Sprite data pointer (MSB first, row-major).
 * @param width   Sprite width in pixels.
 * @param sx      X position within the sprite [0, width-1].
 * @param sy      Y position within the sprite [0, height-1].
 * @return        1 if the bit is set, 0 otherwise.
 */
static inline uint8_t sprite_get_bit(const uint8_t *data,
                                     uint16_t       width,
                                     int32_t        sx,
                                     int32_t        sy)
{
    /* Bytes per row = ceil(width / 8). */
    uint32_t row_bytes = ((uint32_t)width + 7u) / 8u;
    uint32_t byte_idx  = (uint32_t)sy * row_bytes + (uint32_t)sx / 8u;
    uint8_t  bit_mask  = (uint8_t)(0x80u >> ((uint32_t)sx % 8u));
    return (data[byte_idx] & bit_mask) ? 1u : 0u;
}

/* ── fq_blit_sprite ───────────────────────────────────────────────────── */

void fq_blit_sprite(fq_fb_t           *fb,
                    int16_t            x,
                    int16_t            y,
                    const fq_sprite_t *sprite)
{
    /* NULL and zero-dimension guards. */
    if (fb == NULL)                  { return; }
    if (sprite == NULL)              { return; }
    if (sprite->data == NULL)        { return; }
    if (sprite->width == 0u)         { return; }
    if (sprite->height == 0u)        { return; }

    int32_t sp_w = (int32_t)sprite->width;
    int32_t sp_h = (int32_t)sprite->height;

    /* Compute visible region in framebuffer coordinates. */
    int32_t fb_x0 = (int32_t)x;
    int32_t fb_y0 = (int32_t)y;
    int32_t fb_x1 = fb_x0 + sp_w - 1;  /* inclusive */
    int32_t fb_y1 = fb_y0 + sp_h - 1;  /* inclusive */

    /* Clip to framebuffer boundaries. */
    int32_t vis_x0 = (fb_x0 < 0) ? 0 : fb_x0;
    int32_t vis_y0 = (fb_y0 < 0) ? 0 : fb_y0;
    int32_t vis_x1 = (fb_x1 >= (int32_t)FQ_FB_WIDTH)  ? (int32_t)FQ_FB_WIDTH  - 1 : fb_x1;
    int32_t vis_y1 = (fb_y1 >= (int32_t)FQ_FB_HEIGHT) ? (int32_t)FQ_FB_HEIGHT - 1 : fb_y1;

    /* Fully off-screen check. */
    if (vis_x0 > vis_x1 || vis_y0 > vis_y1) { return; }

    /* Iterate over the visible rectangle. */
    for (int32_t fy = vis_y0; fy <= vis_y1; fy++) {
        /* Corresponding sprite Y. */
        int32_t sy = fy - fb_y0;

        for (int32_t fx = vis_x0; fx <= vis_x1; fx++) {
            /* Corresponding sprite X. */
            int32_t sx = fx - fb_x0;

            /* OR-blit: only draw when sprite bit is 1. */
            if (sprite_get_bit(sprite->data, sprite->width, sx, sy)) {
                fq_fb_set_pixel(fb, (int16_t)fx, (int16_t)fy, 1u);
            }
        }
    }
}
