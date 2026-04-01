/**
 * fq_sprite.h — FiestaQuest Presentation Layer: 1-bit Sprite Blitter
 *
 * OR-blits a packed 1-bit sprite onto a framebuffer:
 *   - Bit = 1 in sprite → set corresponding framebuffer pixel to black.
 *   - Bit = 0 in sprite → leave framebuffer pixel unchanged (transparency).
 *
 * Handles:
 *   - Non-byte-aligned X positions (all 8 shift offsets).
 *   - Widths that are not multiples of 8.
 *   - 4-edge clipping (negative x/y, overflow past right/bottom).
 *   - NULL pointer safety on fb and sprite.
 *   - Zero-dimension sprites (immediate return).
 *
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 */

#ifndef FIESTAQUEST_PRESENTATION_FQ_SPRITE_H
#define FIESTAQUEST_PRESENTATION_FQ_SPRITE_H

#include "fq_framebuffer.h"
#include <stdint.h>

/* ── Sprite descriptor ────────────────────────────────────────────────── */

/**
 * fq_sprite_t — Immutable descriptor for a 1-bit packed sprite.
 *
 * @field data    Pointer to the packed bitmap data (MSB first, row-major).
 *                Each row occupies ceil(width/8) bytes.
 * @field width   Sprite width in pixels.
 * @field height  Sprite height in pixels.
 */
typedef struct {
    const uint8_t *data;    /**< Packed 1-bit bitmap (MSB first). */
    uint16_t       width;   /**< Sprite width in pixels.          */
    uint16_t       height;  /**< Sprite height in pixels.         */
} fq_sprite_t;

/* ── Blitter API ──────────────────────────────────────────────────────── */

/**
 * fq_blit_sprite — OR-blit a sprite onto a framebuffer.
 *
 * Transparency: bits set to 1 in the sprite are drawn black on the
 * framebuffer. Bits set to 0 leave the background pixel unchanged.
 *
 * Clipping: all four edges are clipped. Negative x/y values are valid
 * and result in the left/top portion of the sprite being skipped.
 * Sprites extending past the right or bottom edge are truncated.
 *
 * @param fb      Target framebuffer. NULL-safe: silently returns.
 * @param x       X destination (int16_t: may be negative).
 * @param y       Y destination (int16_t: may be negative).
 * @param sprite  Sprite to blit. NULL-safe: silently returns.
 *                sprite->data == NULL: silently returns.
 *                Zero width or height: silently returns.
 */
void fq_blit_sprite(fq_fb_t        *fb,
                    int16_t         x,
                    int16_t         y,
                    const fq_sprite_t *sprite);

#endif /* FIESTAQUEST_PRESENTATION_FQ_SPRITE_H */
