/**
 * fq_framebuffer.c — FiestaQuest Presentation Layer: 1-bit Framebuffer
 *
 * Implementation of fq_fb_t primitives: clear, fill, set_pixel, get_pixel,
 * draw_line (Bresenham), draw_rect (outline), fill_rect (solid).
 *
 * Determinism contract:
 *   - No floating-point operations.
 *   - No malloc.
 *   - All functions are pure (no hidden state).
 *
 * Bit ordering: MSB first (pixel 0 of a row = bit 7 of byte 0).
 *   Byte index: y * FQ_FB_STRIDE + x / 8
 *   Bit  index: 7 - (x % 8)
 *
 * HOST-COMPILABLE — no hal_*.h, no ESP-IDF.
 */

#include "fq_framebuffer.h"
#include <string.h>
#include <stdint.h>

/* ── Internal helpers ─────────────────────────────────────────────────── */

/**
 * pixel_in_bounds — Returns 1 if (x,y) is within [0,FQ_FB_WIDTH) × [0,FQ_FB_HEIGHT).
 *
 * Uses int16_t inputs; negative values fail the >= 0 check before casting.
 */
static inline int pixel_in_bounds(int16_t x, int16_t y)
{
    return (x >= 0) && (y >= 0)
        && ((uint16_t)x < (uint16_t)FQ_FB_WIDTH)
        && ((uint16_t)y < (uint16_t)FQ_FB_HEIGHT);
}

/* ── fq_fb_clear ──────────────────────────────────────────────────────── */

void fq_fb_clear(fq_fb_t *fb)
{
    if (fb == NULL) { return; }
    memset(fb->pixels, 0x00u, FQ_FB_SIZE);
}

/* ── fq_fb_fill ───────────────────────────────────────────────────────── */

void fq_fb_fill(fq_fb_t *fb, uint8_t color)
{
    if (fb == NULL) { return; }
    memset(fb->pixels, (color != 0u) ? 0xFFu : 0x00u, FQ_FB_SIZE);
}

/* ── fq_fb_set_pixel ──────────────────────────────────────────────────── */

void fq_fb_set_pixel(fq_fb_t *fb, int16_t x, int16_t y, uint8_t color)
{
    if (fb == NULL) { return; }
    if (!pixel_in_bounds(x, y)) { return; }

    uint32_t byte_idx = (uint32_t)y * FQ_FB_STRIDE + (uint32_t)x / 8u;
    uint8_t  bit_mask = (uint8_t)(0x80u >> ((uint32_t)x % 8u));

    if (color != 0u) {
        fb->pixels[byte_idx] |= bit_mask;
    } else {
        fb->pixels[byte_idx] &= (uint8_t)(~bit_mask);
    }
}

/* ── fq_fb_get_pixel ──────────────────────────────────────────────────── */

uint8_t fq_fb_get_pixel(const fq_fb_t *fb, int16_t x, int16_t y)
{
    if (fb == NULL) { return 0u; }
    if (!pixel_in_bounds(x, y)) { return 0u; }

    uint32_t byte_idx = (uint32_t)y * FQ_FB_STRIDE + (uint32_t)x / 8u;
    uint8_t  bit_mask = (uint8_t)(0x80u >> ((uint32_t)x % 8u));

    return (fb->pixels[byte_idx] & bit_mask) ? 1u : 0u;
}

/* ── fq_fb_draw_line — Bresenham's line algorithm ─────────────────────── */
/*
 * Classic integer Bresenham with support for all eight octants.
 * Handles:
 *   - Vertical lines  (dx == 0)
 *   - Horizontal lines (dy == 0)
 *   - Single-point    (x0 == x1 && y0 == y1)
 *   - Reversed coords (x1 < x0 or y1 < y0)
 *   - Partially or fully out-of-bounds — per-pixel clipping via set_pixel.
 */
void fq_fb_draw_line(fq_fb_t *fb,
                     int16_t x0, int16_t y0,
                     int16_t x1, int16_t y1,
                     uint8_t color)
{
    if (fb == NULL) { return; }

    /* Deltas (may be negative). */
    int16_t dx = (int16_t)(x1 - x0);
    int16_t dy = (int16_t)(y1 - y0);

    /* Absolute values for step counting. */
    int16_t abs_dx = (dx < 0) ? (int16_t)(-dx) : dx;
    int16_t abs_dy = (dy < 0) ? (int16_t)(-dy) : dy;

    /* Step direction. */
    int16_t sx = (dx < 0) ? (int16_t)(-1) : (int16_t)(1);
    int16_t sy = (dy < 0) ? (int16_t)(-1) : (int16_t)(1);

    int16_t cx = x0;
    int16_t cy = y0;

    if (abs_dx >= abs_dy) {
        /* X-major (or horizontal) */
        int16_t err = (int16_t)(abs_dx / 2);
        while (cx != x1) {
            fq_fb_set_pixel(fb, cx, cy, color);
            err = (int16_t)(err - abs_dy);
            if (err < 0) {
                cy = (int16_t)(cy + sy);
                err = (int16_t)(err + abs_dx);
            }
            cx = (int16_t)(cx + sx);
        }
    } else {
        /* Y-major (or vertical) */
        int16_t err = (int16_t)(abs_dy / 2);
        while (cy != y1) {
            fq_fb_set_pixel(fb, cx, cy, color);
            err = (int16_t)(err - abs_dx);
            if (err < 0) {
                cx = (int16_t)(cx + sx);
                err = (int16_t)(err + abs_dy);
            }
            cy = (int16_t)(cy + sy);
        }
    }
    /* Always draw the final endpoint. */
    fq_fb_set_pixel(fb, x1, y1, color);
}

/* ── fq_fb_draw_rect ──────────────────────────────────────────────────── */

void fq_fb_draw_rect(fq_fb_t *fb,
                     int16_t x, int16_t y,
                     int16_t w, int16_t h,
                     uint8_t color)
{
    if (fb == NULL) { return; }
    if (w <= 0 || h <= 0) { return; }

    int16_t x1 = (int16_t)(x + w - 1);
    int16_t y1 = (int16_t)(y + h - 1);

    /* Top and bottom horizontal lines. */
    fq_fb_draw_line(fb, x,  y,  x1, y,  color);
    fq_fb_draw_line(fb, x,  y1, x1, y1, color);
    /* Left and right vertical lines. */
    fq_fb_draw_line(fb, x,  y,  x,  y1, color);
    fq_fb_draw_line(fb, x1, y,  x1, y1, color);
}

/* ── fq_fb_fill_rect ──────────────────────────────────────────────────── */

void fq_fb_fill_rect(fq_fb_t *fb,
                     int16_t x, int16_t y,
                     int16_t w, int16_t h,
                     uint8_t color)
{
    if (fb == NULL) { return; }
    if (w <= 0 || h <= 0) { return; }

    for (int16_t row = y; row < (int16_t)(y + h); row++) {
        for (int16_t col = x; col < (int16_t)(x + w); col++) {
            fq_fb_set_pixel(fb, col, row, color);
        }
    }
}
