/**
 * fq_framebuffer.h — FiestaQuest Presentation Layer: 1-bit Framebuffer
 *
 * 200x200 pixel, 1-bit-per-pixel packed framebuffer for the e-paper display.
 *
 * Bit ordering: MSB first, row-major.
 *   Pixel (x, y) → byte: pixels[y * FQ_FB_STRIDE + x / 8]
 *                   bit:  7 - (x % 8)
 *
 * Color encoding: 0 = white (bit clear), 1 = black (bit set).
 *
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 * Architecture constraint: presentation/ MUST NOT include hal_*.h.
 */

#ifndef FIESTAQUEST_PRESENTATION_FQ_FRAMEBUFFER_H
#define FIESTAQUEST_PRESENTATION_FQ_FRAMEBUFFER_H

#include <stdint.h>

/* ── Framebuffer dimensions ───────────────────────────────────────────── */

/** Display width in pixels. */
#define FQ_FB_WIDTH   200u

/** Display height in pixels. */
#define FQ_FB_HEIGHT  200u

/** Bytes per row: ceil(200/8) = 25. */
#define FQ_FB_STRIDE  25u

/** Total bytes: 200 * 25 = 5000. */
#define FQ_FB_SIZE    5000u

/* ── Framebuffer type ─────────────────────────────────────────────────── */

/**
 * fq_fb_t — 200x200 1-bit packed framebuffer.
 *
 * Pixels packed MSB-first, row-major. No padding.
 * Static allocation only (never malloc in the fast path).
 */
typedef struct {
    uint8_t pixels[FQ_FB_SIZE]; /**< 1-bit packed pixel data. */
} fq_fb_t;

_Static_assert(sizeof(fq_fb_t) == 5000u,
               "fq_fb_t must be exactly 5000 bytes (200x200/8)");

/* ── Primitive operations ─────────────────────────────────────────────── */

/**
 * fq_fb_clear — Clear the framebuffer to white (all bytes = 0x00).
 *
 * @param fb  Framebuffer to clear. NULL-safe: silently returns.
 */
void fq_fb_clear(fq_fb_t *fb);

/**
 * fq_fb_fill — Fill the entire framebuffer with a color.
 *
 * @param fb     Framebuffer to fill. NULL-safe: silently returns.
 * @param color  0 = white (0x00 per byte), 1 = black (0xFF per byte).
 */
void fq_fb_fill(fq_fb_t *fb, uint8_t color);

/**
 * fq_fb_set_pixel — Set a single pixel.
 *
 * Bounds-checked: coordinates outside [0, FQ_FB_WIDTH-1] × [0, FQ_FB_HEIGHT-1]
 * are dropped silently (no write, no crash).
 *
 * @param fb     Framebuffer. NULL-safe.
 * @param x      X coordinate (int16_t: supports negative for clipping callers).
 * @param y      Y coordinate (int16_t: supports negative for clipping callers).
 * @param color  0 = white, 1 = black.
 */
void fq_fb_set_pixel(fq_fb_t *fb, int16_t x, int16_t y, uint8_t color);

/**
 * fq_fb_get_pixel — Read a single pixel value.
 *
 * Returns 0 for any out-of-bounds coordinate or NULL framebuffer.
 *
 * @param fb  Framebuffer (const). NULL-safe.
 * @param x   X coordinate.
 * @param y   Y coordinate.
 * @return    0 (white) or 1 (black). 0 for OOB.
 */
uint8_t fq_fb_get_pixel(const fq_fb_t *fb, int16_t x, int16_t y);

/**
 * fq_fb_draw_line — Draw a line using Bresenham's algorithm.
 *
 * Handles all octants, vertical, horizontal, and single-point lines.
 * Coordinates may be negative or >= 200 — per-pixel clipping via
 * fq_fb_set_pixel bounds check. Guaranteed to terminate.
 *
 * @param fb            Framebuffer. NULL-safe.
 * @param x0, y0        Start point.
 * @param x1, y1        End point.
 * @param color         0 = white, 1 = black.
 */
void fq_fb_draw_line(fq_fb_t *fb,
                     int16_t x0, int16_t y0,
                     int16_t x1, int16_t y1,
                     uint8_t color);

/**
 * fq_fb_draw_rect — Draw the outline of an axis-aligned rectangle.
 *
 * Draws four sides (top, bottom, left, right). Does NOT fill interior.
 * Out-of-bounds sides are clipped per-pixel via fq_fb_set_pixel.
 *
 * @param fb         Framebuffer. NULL-safe.
 * @param x, y       Top-left corner (inclusive).
 * @param w, h       Width and height in pixels.
 * @param color      0 = white, 1 = black.
 */
void fq_fb_draw_rect(fq_fb_t *fb,
                     int16_t x, int16_t y,
                     int16_t w, int16_t h,
                     uint8_t color);

/**
 * fq_fb_fill_rect — Fill a solid axis-aligned rectangle.
 *
 * Fills all pixels in the rectangle [x, x+w) × [y, y+h).
 * Out-of-bounds pixels are clipped per-pixel via fq_fb_set_pixel.
 * Zero or negative w/h → no pixels modified.
 *
 * @param fb         Framebuffer. NULL-safe.
 * @param x, y       Top-left corner (inclusive).
 * @param w, h       Width and height in pixels.
 * @param color      0 = white, 1 = black.
 */
void fq_fb_fill_rect(fq_fb_t *fb,
                     int16_t x, int16_t y,
                     int16_t w, int16_t h,
                     uint8_t color);

#endif /* FIESTAQUEST_PRESENTATION_FQ_FRAMEBUFFER_H */
