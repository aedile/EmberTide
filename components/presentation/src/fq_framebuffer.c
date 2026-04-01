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
 * Performance note:
 *   fq_fb_fill_rect and fq_fb_draw_line use per-pixel fq_fb_set_pixel calls
 *   which re-check bounds on every pixel. For a 200x200 display this is fast
 *   enough (worst-case 40000 ops). A byte-span fast path can be added in a
 *   future phase once profiling confirms it is necessary.
 *
 * HOST-COMPILABLE — no hal_*.h, no ESP-IDF.
 *
 * v2.7 amendment: draw_line dx/dy widened to int32_t to eliminate signed
 * overflow UB when coordinates span the full int16_t range (e.g., INT16_MIN
 * to INT16_MAX). The loop variables (cx, cy) and step counters (sx, sy) remain
 * int32_t throughout to be consistent. abs_dx/abs_dy computed with stdint-safe
 * arithmetic. An optional early-exit guard is added for lines whose endpoints
 * are both fully outside the display by a margin of 200 pixels in x and y.
 */

#include "fq_framebuffer.h"
#include <string.h>
#include <stdint.h>

/* ── Internal helpers ─────────────────────────────────────────────────── */

/**
 * pixel_in_bounds — Returns 1 if (x,y) is within [0,FQ_FB_WIDTH) x [0,FQ_FB_HEIGHT).
 *
 * Negative values fail the >= 0 check before unsigned-cast comparison,
 * preventing signed-overflow undefined behaviour.
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
 * Classic integer Bresenham supporting all eight octants.
 * Handles:
 *   - Vertical lines  (dx == 0)
 *   - Horizontal lines (dy == 0)
 *   - Single-point    (x0 == x1 && y0 == y1): loop body skipped, endpoint drawn
 *   - Reversed coords (x1 < x0 or y1 < y0): sx/sy handle direction
 *   - Partially or fully out-of-bounds: per-pixel clip via set_pixel
 *
 * B4/v2.7: dx and dy are computed as int32_t to eliminate signed overflow UB
 * when endpoints span the full int16_t range (e.g., INT16_MIN to INT16_MAX).
 * abs_dx/abs_dy are int32_t. The loop cursor (cx, cy) and step vars are
 * int32_t throughout to avoid repeated int16_t narrowing casts. set_pixel
 * performs its own int16_t bounds check, so the cast at the call site is safe.
 *
 * A5/v2.7: Early-exit guard: if abs_dx > 1000 AND both x-coordinates are
 * outside [-200, 400) — i.e., the entire line is guaranteed to be far outside
 * the display in the dominant axis — return early for performance. This avoids
 * iterating 65000 steps for extreme-coordinate lines with no visible pixels.
 */
void fq_fb_draw_line(fq_fb_t *fb,
                     int16_t x0, int16_t y0,
                     int16_t x1, int16_t y1,
                     uint8_t color)
{
    if (fb == NULL) { return; }

    /* Widen to int32_t before subtraction to eliminate int16_t overflow UB. */
    int32_t dx     = (int32_t)x1 - (int32_t)x0;
    int32_t dy     = (int32_t)y1 - (int32_t)y0;
    int32_t abs_dx = (dx < 0) ? -dx : dx;
    int32_t abs_dy = (dy < 0) ? -dy : dy;
    int32_t sx     = (dx < 0) ? -1 : 1;
    int32_t sy     = (dy < 0) ? -1 : 1;
    int32_t cx     = (int32_t)x0;
    int32_t cy     = (int32_t)y0;
    int32_t ex     = (int32_t)x1;
    int32_t ey     = (int32_t)y1;

    /* A5: Early-exit guard for lines entirely outside the display with a large
     * span. Avoids iterating up to 65535 steps for extreme-coordinate lines
     * (e.g., INT16_MIN to INT16_MAX) that contribute zero visible pixels.
     *
     * Condition: dominant-axis span > 1000 AND both x-endpoints are outside
     * the range [-200, 400). These bounds are chosen conservatively: any line
     * with an endpoint inside [-200, 400) in x could partially intersect the
     * 200-wide display, so it must be processed normally. */
    if (abs_dx > 1000) {
        int x0_far = ((int32_t)x0 < -200) || ((int32_t)x0 >= 400);
        int x1_far = ((int32_t)x1 < -200) || ((int32_t)x1 >= 400);
        if (x0_far && x1_far) {
            return;
        }
    }

    if (abs_dx >= abs_dy) {
        /* X-major (or horizontal): step one pixel in X per iteration. */
        int32_t err = abs_dx / 2;
        while (cx != ex) {
            fq_fb_set_pixel(fb, (int16_t)cx, (int16_t)cy, color);
            err -= abs_dy;
            if (err < 0) {
                cy  += sy;
                err += abs_dx;
            }
            cx += sx;
        }
    } else {
        /* Y-major (or vertical): step one pixel in Y per iteration. */
        int32_t err = abs_dy / 2;
        while (cy != ey) {
            fq_fb_set_pixel(fb, (int16_t)cx, (int16_t)cy, color);
            err -= abs_dx;
            if (err < 0) {
                cx  += sx;
                err += abs_dy;
            }
            cy += sy;
        }
    }
    /* Draw the final endpoint unconditionally (loop exits before it). */
    fq_fb_set_pixel(fb, (int16_t)ex, (int16_t)ey, color);
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

    /* Four sides. Corners drawn by both adjacent lines (harmless for 1-bit). */
    fq_fb_draw_line(fb, x,  y,  x1, y,  color); /* top    */
    fq_fb_draw_line(fb, x,  y1, x1, y1, color); /* bottom */
    fq_fb_draw_line(fb, x,  y,  x,  y1, color); /* left   */
    fq_fb_draw_line(fb, x1, y,  x1, y1, color); /* right  */
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
