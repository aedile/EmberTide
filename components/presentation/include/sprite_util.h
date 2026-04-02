/**
 * sprite_util.h — FiestaQuest Presentation Layer: Sprite & Text Utilities
 *
 * Shared rendering helpers used by all screen renderers.
 *
 * blit_sprite_2x:
 *   Renders a sprite at 2x magnification (each source pixel becomes a 2x2
 *   block). A 32x32 sprite becomes 64x64 on screen. Used to make character
 *   sprites the visual focus on the 200x200 e-paper display.
 *
 * fq_draw_text_2x:
 *   Renders a string at 2x magnification. Each source glyph pixel becomes a
 *   2x2 block on the framebuffer. The cursor advances by advance_width * 2
 *   after each character. Useful for decorative title text at larger apparent
 *   size without requiring a separate larger font asset.
 *
 * fq_draw_text_inverted:
 *   Renders a string using CLEAR-blit instead of OR-blit: glyph pixels
 *   CLEAR (set to 0 / white) the corresponding framebuffer pixel, leaving
 *   untouched pixels at their current value. Combined with a prior black
 *   fill_rect, this produces white text on a black background.
 *
 * draw_header_bar:
 *   Fills a horizontal band with black and renders the supplied text as
 *   white (inverted) text. Title pattern used by all screen headers.
 *
 * draw_footer_bar:
 *   Same as draw_header_bar but at arbitrary y coordinates. Used for
 *   the bottom status bands on Home, Training, and Stats screens.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 */

#ifndef FIESTAQUEST_PRESENTATION_SPRITE_UTIL_H
#define FIESTAQUEST_PRESENTATION_SPRITE_UTIL_H

#include "fq_framebuffer.h"
#include "fq_sprite.h"
#include "fq_text.h"
#include <stdint.h>

/**
 * fq_blit_sprite_2x — Blit a sprite at 2x magnification.
 *
 * Each source pixel becomes a 2x2 block on the framebuffer.
 * A 32x32 sprite renders as 64x64. Only SET bits (value 1) in the
 * sprite produce pixels; cleared bits leave the background unchanged.
 *
 * Clipping: any pixel that falls outside the framebuffer bounds is
 * silently skipped via fq_fb_set_pixel's built-in bounds check.
 *
 * @param fb   Target framebuffer. NULL-safe.
 * @param x    Top-left X destination.
 * @param y    Top-left Y destination.
 * @param spr  Sprite to magnify. NULL-safe.
 */
void fq_blit_sprite_2x(fq_fb_t *fb,
                       int16_t x,
                       int16_t y,
                       const fq_sprite_t *spr);

/**
 * fq_draw_text_2x — Render text at 2x pixel scale.
 *
 * Identical layout logic to fq_draw_text but each glyph source pixel is
 * expanded to a 2x2 block (same technique as fq_blit_sprite_2x). The cursor
 * advances by advance_width * 2 after each character, so the rendered string
 * occupies 2x the horizontal space. Vertical height is also doubled.
 *
 * Clipping: pixels outside the framebuffer bounds are silently skipped via
 * fq_fb_set_pixel's built-in bounds check. No allocation.
 *
 * Rendering stops when cursor >= FQ_FB_WIDTH * 2 would be needed for the
 * NEXT glyph start — in practice, clipping on the right edge is graceful.
 *
 * @param fb    Target framebuffer. NULL-safe.
 * @param font  Font descriptor. NULL-safe.
 * @param x     Starting X cursor position.
 * @param y     Starting Y cursor position.
 * @param str   Null-terminated string. NULL-safe.
 * @return      Final cursor X position after rendering all characters.
 */
int16_t fq_draw_text_2x(fq_fb_t         *fb,
                        const fq_font_t *font,
                        int16_t          x,
                        int16_t          y,
                        const char      *str);

/**
 * fq_draw_text_inverted — Render text by CLEARING pixels (white on black).
 *
 * Identical control flow to fq_draw_text but uses color=0 (clear) instead
 * of color=1 (set) when blitting glyph pixels. The caller must have already
 * filled the target region with black before calling this function.
 *
 * @param fb    Target framebuffer. NULL-safe.
 * @param font  Font descriptor. NULL-safe.
 * @param x     Starting X cursor.
 * @param y     Starting Y cursor.
 * @param str   Null-terminated string. NULL-safe.
 * @return      Final cursor X after rendering.
 */
int16_t fq_draw_text_inverted(fq_fb_t         *fb,
                               const fq_font_t *font,
                               int16_t          x,
                               int16_t          y,
                               const char      *str);

/**
 * fq_draw_header_bar — Fill a horizontal band black and render white text.
 *
 * @param fb    Target framebuffer. NULL-safe.
 * @param font  Font to use for the text.
 * @param y     Top y coordinate of the bar.
 * @param h     Height of the bar in pixels.
 * @param text  Text to render as white-on-black.
 */
void fq_draw_header_bar(fq_fb_t         *fb,
                        const fq_font_t *font,
                        int16_t          y,
                        int16_t          h,
                        const char      *text);

/**
 * fq_draw_header_bar2 — Like fq_draw_header_bar but with TWO text fields:
 * one left-aligned and one right-aligned.
 *
 * @param fb        Target framebuffer. NULL-safe.
 * @param font      Font to use.
 * @param y         Top y coordinate of the bar.
 * @param h         Height of the bar in pixels.
 * @param left_txt  Left-aligned white text.
 * @param right_txt Right-aligned white text (drawn near right edge).
 */
void fq_draw_header_bar2(fq_fb_t         *fb,
                         const fq_font_t *font,
                         int16_t          y,
                         int16_t          h,
                         const char      *left_txt,
                         const char      *right_txt);

#endif /* FIESTAQUEST_PRESENTATION_SPRITE_UTIL_H */
