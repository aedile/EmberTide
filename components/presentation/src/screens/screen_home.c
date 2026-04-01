/**
 * screen_home.c — FiestaQuest Presentation: Home Screen Renderer
 *
 * Renders the home/dashboard screen onto the 200x200 1-bit framebuffer.
 * Uses only fq_fb_* primitives (framebuffer geometry operations).
 *
 * Layout rationale (200x200, all coordinates inclusive top-left):
 *   - Top border rect:      (0,0)-(199,199) — full display outline
 *   - Name/level band:      row 4-16, left-aligned placeholder
 *   - Sprite placeholder:   centred rect (60,25)-(139,104) — 80x80 px
 *   - HP bar outline:       (10,115)-(189,127)
 *   - HP bar fill:          (11,116) wide = hp_percent * 178 / 100
 *   - Wins/losses divider:  horizontal line at row 140
 *   - Wins block:           (10,143)-(94,157)  — left column
 *   - Losses block:         (105,143)-(189,157) — right column
 *
 * No text rendering in this implementation — fonts are device-side.
 * Visual structure is conveyed through rectangles and filled bars.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 */

#include "screens/screen_home.h"
#include <stddef.h>

/* HP bar geometry constants. */
#define HOME_HP_BAR_X      10
#define HOME_HP_BAR_Y      115
#define HOME_HP_BAR_W      180
#define HOME_HP_BAR_H      12
#define HOME_HP_FILL_MAX_W 178u  /**< Inner fill width at 100%. */

/* Sprite placeholder geometry. */
#define HOME_SPRITE_X  60
#define HOME_SPRITE_Y  25
#define HOME_SPRITE_W  80
#define HOME_SPRITE_H  80

/* ---------------------------------------------------------------------------
 * fq_render_home
 * ---------------------------------------------------------------------------*/
void fq_render_home(fq_fb_t *fb, const fq_vm_home_t *vm)
{
    if (fb == NULL || vm == NULL) {
        return;
    }

    /* Clear to white. */
    fq_fb_clear(fb);

    /* ── Display border ─────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb, 0, 0,
                    (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* ── Name / level placeholder band ─────────────────────────────────── */
    /* Draw a thin horizontal rule under the name area. */
    fq_fb_draw_line(fb, 5, 20, 194, 20, 1u);

    /* ── Sprite placeholder rect ─────────────────────────────────────────
     * E-paper convention: inverted (filled black) rect marks the sprite zone.
     * A thin inner outline distinguishes it from a solid fill. */
    fq_fb_draw_rect(fb,
                    HOME_SPRITE_X, HOME_SPRITE_Y,
                    HOME_SPRITE_W, HOME_SPRITE_H, 1u);
    /* Inner outline (2px inset). */
    fq_fb_draw_rect(fb,
                    HOME_SPRITE_X + 2, HOME_SPRITE_Y + 2,
                    HOME_SPRITE_W - 4, HOME_SPRITE_H - 4, 1u);

    /* ── HP bar ─────────────────────────────────────────────────────────── */
    /* Outline. */
    fq_fb_draw_rect(fb,
                    HOME_HP_BAR_X, HOME_HP_BAR_Y,
                    HOME_HP_BAR_W, HOME_HP_BAR_H, 1u);

    /* Fill proportional to hp_percent (integer-only, no float). */
    uint32_t fill_w = (uint32_t)vm->hp_percent * HOME_HP_FILL_MAX_W / 100u;
    if (fill_w > 0u) {
        fq_fb_fill_rect(fb,
                        HOME_HP_BAR_X + 1, HOME_HP_BAR_Y + 1,
                        (int16_t)fill_w, HOME_HP_BAR_H - 2, 1u);
    }

    /* ── Divider line ───────────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 5, 135, 194, 135, 1u);

    /* ── Wins block (left column) ───────────────────────────────────────── */
    fq_fb_draw_rect(fb, 10, 143, 85, 15, 1u);

    /* ── Losses block (right column) ────────────────────────────────────── */
    fq_fb_draw_rect(fb, 105, 143, 85, 15, 1u);

    /* ── Bottom label placeholder ────────────────────────────────────────
     * A small filled rect at the bottom represents the class label. */
    fq_fb_draw_line(fb, 5, 170, 194, 170, 1u);
}
