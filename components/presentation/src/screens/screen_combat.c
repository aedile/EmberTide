/**
 * screen_combat.c — FiestaQuest Presentation: Combat HUD Screen Renderer
 *
 * Renders the split-screen combat HUD onto the 200x200 1-bit framebuffer.
 *
 * Layout:
 *   y=0..97   : Enemy (f2) zone — name band, HP bar, sprite placeholder
 *   y=98      : Divider line (full width)
 *   y=100..199: Player (f1) zone — HP bar, name band, sprite placeholder
 *   Top-right : Round indicator placeholder rect
 *   Mid-screen: Action banner rect (when action_text[0] != '\0')
 *
 * HP bar formula (integer-only, int32_t intermediate):
 *   int32_t bar_w = (hp_max > 0)
 *                     ? ((int32_t)hp * COMBAT_BAR_FILL_W / (int32_t)hp_max)
 *                     : 0;
 *   bar_w clamped to [0, COMBAT_BAR_FILL_W].
 *
 * NULL-safe: fq_render_combat(NULL, ...) and fq_render_combat(..., NULL)
 * are silent no-ops.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 */

#include "screens/screen_combat.h"
#include <stddef.h>
#include <string.h>

/* ── Layout constants ────────────────────────────────────────────────────── */

/** Y coordinate of the horizontal divider line. */
#define COMBAT_DIVIDER_Y    98

/** HP bar: left edge X (margin), outer width, inner fill width. */
#define COMBAT_BAR_X        10
#define COMBAT_BAR_H        10
#define COMBAT_BAR_W        160
#define COMBAT_BAR_FILL_W   158  /**< Inner fill pixels at 100% HP. */

/* Enemy (f2) sub-layout — top half y=0..97 */
#define COMBAT_F2_NAME_Y    5
#define COMBAT_F2_HP_BAR_Y  60
#define COMBAT_F2_SPRITE_X  130
#define COMBAT_F2_SPRITE_Y  10
#define COMBAT_F2_SPRITE_W  60
#define COMBAT_F2_SPRITE_H  45

/* Player (f1) sub-layout — bottom half y=100..199 */
#define COMBAT_F1_HP_BAR_Y  110
#define COMBAT_F1_NAME_Y    135
#define COMBAT_F1_SPRITE_X  10
#define COMBAT_F1_SPRITE_Y  100
#define COMBAT_F1_SPRITE_W  60
#define COMBAT_F1_SPRITE_H  45

/* Round indicator — top-right corner */
#define COMBAT_ROUND_X      155
#define COMBAT_ROUND_Y      2
#define COMBAT_ROUND_W      43
#define COMBAT_ROUND_H      12

/* Action banner — centred over divider */
#define COMBAT_BANNER_Y     80
#define COMBAT_BANNER_H     40
#define COMBAT_BANNER_X     10
#define COMBAT_BANNER_W     180

/* Name placeholder rectangle */
#define COMBAT_NAME_W       100
#define COMBAT_NAME_H       12

/* ── Internal helpers ────────────────────────────────────────────────────── */

/**
 * calc_bar_fill — Compute HP bar fill width with int32_t intermediate.
 *
 * Guards:
 *   hp_max <= 0 → returns 0 (divide-by-zero protection).
 *   Negative hp → product is negative → clamped to 0.
 *   hp > hp_max → clamped to COMBAT_BAR_FILL_W.
 *
 * The int32_t intermediate safely holds INT16_MAX * 158 = 5,177,186 < INT32_MAX.
 *
 * @param hp      Current HP (may be negative — fighter KO'd past zero).
 * @param hp_max  Maximum HP. 0 → 0 returned (no divide-by-zero).
 * @return        Fill width in pixels, clamped to [0, COMBAT_BAR_FILL_W].
 */
static int16_t calc_bar_fill(int16_t hp, int16_t hp_max)
{
    if (hp_max <= 0) {
        return 0;
    }
    int32_t w = ((int32_t)hp * (int32_t)COMBAT_BAR_FILL_W) / (int32_t)hp_max;
    if (w < 0) {
        w = 0;
    }
    if (w > (int32_t)COMBAT_BAR_FILL_W) {
        w = (int32_t)COMBAT_BAR_FILL_W;
    }
    return (int16_t)w;
}

/**
 * draw_hp_bar — Draw the HP bar outline and proportional fill.
 *
 * @param fb      Target framebuffer.
 * @param bar_y   Top-left Y of the HP bar.
 * @param hp      Current HP.
 * @param hp_max  Maximum HP.
 */
static void draw_hp_bar(fq_fb_t *fb, int16_t bar_y,
                        int16_t hp, int16_t hp_max)
{
    /* Outline */
    fq_fb_draw_rect(fb, COMBAT_BAR_X, bar_y, COMBAT_BAR_W, COMBAT_BAR_H, 1u);

    /* Fill */
    int16_t fill_w = calc_bar_fill(hp, hp_max);
    if (fill_w > 0) {
        fq_fb_fill_rect(fb,
                        COMBAT_BAR_X + 1, bar_y + 1,
                        fill_w, COMBAT_BAR_H - 2, 1u);
    }
}

/**
 * draw_sprite_placeholder — Draw a double-rect sprite zone.
 *
 * @param fb  Target framebuffer.
 * @param x, y, w, h  Outer rect coordinates.
 */
static void draw_sprite_placeholder(fq_fb_t *fb,
                                    int16_t x, int16_t y,
                                    int16_t w, int16_t h)
{
    fq_fb_draw_rect(fb, x, y, w, h, 1u);
    fq_fb_draw_rect(fb, x + 2, y + 2, w - 4, h - 4, 1u);
}

/* ── fq_render_combat ────────────────────────────────────────────────────── */

void fq_render_combat(fq_fb_t *fb, const fq_vm_combat_t *vm)
{
    if (fb == NULL || vm == NULL) {
        return;
    }

    fq_fb_clear(fb);

    /* ── Display border ───────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb, 0, 0,
                    (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* ── Divider line at y=98 ─────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 0, COMBAT_DIVIDER_Y,
                    (int16_t)(FQ_FB_WIDTH - 1u), COMBAT_DIVIDER_Y, 1u);

    /* ── Round indicator (top-right) ──────────────────────────────────────── */
    fq_fb_draw_rect(fb, COMBAT_ROUND_X, COMBAT_ROUND_Y,
                    COMBAT_ROUND_W, COMBAT_ROUND_H, 1u);

    /* ── Enemy (f2) zone — top half ──────────────────────────────────────── */
    fq_fb_draw_rect(fb, COMBAT_BAR_X, COMBAT_F2_NAME_Y,
                    COMBAT_NAME_W, COMBAT_NAME_H, 1u);
    draw_sprite_placeholder(fb,
                            COMBAT_F2_SPRITE_X, COMBAT_F2_SPRITE_Y,
                            COMBAT_F2_SPRITE_W, COMBAT_F2_SPRITE_H);
    draw_hp_bar(fb, COMBAT_F2_HP_BAR_Y, vm->f2_hp, vm->f2_hp_max);

    /* ── Player (f1) zone — bottom half ──────────────────────────────────── */
    draw_sprite_placeholder(fb,
                            COMBAT_F1_SPRITE_X, COMBAT_F1_SPRITE_Y,
                            COMBAT_F1_SPRITE_W, COMBAT_F1_SPRITE_H);
    draw_hp_bar(fb, COMBAT_F1_HP_BAR_Y, vm->f1_hp, vm->f1_hp_max);
    fq_fb_draw_rect(fb, COMBAT_BAR_X, COMBAT_F1_NAME_Y,
                    COMBAT_NAME_W, COMBAT_NAME_H, 1u);

    /* ── Action banner (only when action_text is non-empty) ──────────────── */
    {
        size_t text_len = strnlen(vm->action_text, sizeof(vm->action_text) - 1u);
        if (text_len > 0u) {
            /* Fill banner background white, then draw border */
            fq_fb_fill_rect(fb, COMBAT_BANNER_X, COMBAT_BANNER_Y,
                            COMBAT_BANNER_W, COMBAT_BANNER_H, 0u);
            fq_fb_draw_rect(fb, COMBAT_BANNER_X, COMBAT_BANNER_Y,
                            COMBAT_BANNER_W, COMBAT_BANNER_H, 1u);
            fq_fb_draw_rect(fb,
                            COMBAT_BANNER_X + 2, COMBAT_BANNER_Y + 2,
                            COMBAT_BANNER_W - 4, COMBAT_BANNER_H - 4, 1u);
            /* Centred text placeholder line */
            fq_fb_draw_line(fb,
                            COMBAT_BANNER_X + 10,
                            COMBAT_BANNER_Y + (COMBAT_BANNER_H / 2),
                            COMBAT_BANNER_X + COMBAT_BANNER_W - 10,
                            COMBAT_BANNER_Y + (COMBAT_BANNER_H / 2),
                            1u);
        }
    }
}
