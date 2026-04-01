/**
 * screen_combat.c — FiestaQuest Presentation: Combat HUD Screen Renderer
 *
 * Renders the split-screen combat HUD onto the 200x200 1-bit framebuffer.
 *
 * Layout:
 *   y=0..97   : Enemy (f2) zone — name band, HP bar, sprite placeholder
 *   y=98      : Divider line (full width)
 *   y=100..199: Player (f1) zone — HP bar, name band, sprite placeholder
 *   Top-right : Round indicator placeholder "R:XX"
 *   Mid-screen: Action banner rect (when action_text[0] != '\0')
 *
 * HP bar formula (integer-only, const int32_t intermediate):
 *   int32_t bar_w = (hp_max > 0)
 *                     ? ((int32_t)hp * COMBAT_BAR_FILL_W / (int32_t)hp_max)
 *                     : 0;
 *   if (bar_w < 0)                bar_w = 0;
 *   if (bar_w > COMBAT_BAR_FILL_W) bar_w = COMBAT_BAR_FILL_W;
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 */

#include "screens/screen_combat.h"
#include <stddef.h>
#include <string.h>

/* ── Layout constants ────────────────────────────────────────────────────── */

/** Y coordinate of the horizontal divider line. */
#define COMBAT_DIVIDER_Y        98

/** HP bar left edge X (margin from left). */
#define COMBAT_BAR_X            10

/** HP bar height in pixels. */
#define COMBAT_BAR_H            10

/** HP bar outer width in pixels (includes 1px border on each side). */
#define COMBAT_BAR_W            160

/** HP bar inner fill max width = COMBAT_BAR_W - 2 border pixels. */
#define COMBAT_BAR_FILL_W       158

/* Enemy (f2) sub-layout — top half y=0..97 */
#define COMBAT_F2_HP_BAR_Y      60
#define COMBAT_F2_NAME_Y        5
#define COMBAT_F2_SPRITE_X      130
#define COMBAT_F2_SPRITE_Y      10
#define COMBAT_F2_SPRITE_W      60
#define COMBAT_F2_SPRITE_H      45

/* Player (f1) sub-layout — bottom half y=100..199 */
#define COMBAT_F1_HP_BAR_Y      110
#define COMBAT_F1_NAME_Y        135
#define COMBAT_F1_SPRITE_X      10
#define COMBAT_F1_SPRITE_Y      100
#define COMBAT_F1_SPRITE_W      60
#define COMBAT_F1_SPRITE_H      45

/* Round indicator — top-right band */
#define COMBAT_ROUND_X          155
#define COMBAT_ROUND_Y          2
#define COMBAT_ROUND_W          43
#define COMBAT_ROUND_H          12

/* Action banner geometry — centred around the divider */
#define COMBAT_BANNER_Y         80
#define COMBAT_BANNER_H         40
#define COMBAT_BANNER_X         10
#define COMBAT_BANNER_W         180

/* Name placeholder rect geometry */
#define COMBAT_NAME_W           100
#define COMBAT_NAME_H           12

/* ── Internal helper ─────────────────────────────────────────────────────── */

/**
 * calc_bar_width — Compute HP bar fill width safely.
 *
 * Uses int32_t intermediate to hold hp * BAR_FILL_W without overflow.
 * Clamps to [0, COMBAT_BAR_FILL_W].
 *
 * @param hp      Current HP (may be negative).
 * @param hp_max  Maximum HP. 0 → returns 0 (divide-by-zero guard).
 * @return        Fill width in pixels, clamped to [0, COMBAT_BAR_FILL_W].
 */
static int16_t calc_bar_width(int16_t hp, int16_t hp_max)
{
    if (hp_max <= 0) {
        return 0;
    }
    int32_t bar_w = ((int32_t)hp * (int32_t)COMBAT_BAR_FILL_W) / (int32_t)hp_max;
    if (bar_w < 0) {
        bar_w = 0;
    }
    if (bar_w > (int32_t)COMBAT_BAR_FILL_W) {
        bar_w = (int32_t)COMBAT_BAR_FILL_W;
    }
    return (int16_t)bar_w;
}

/* ── fq_render_combat ────────────────────────────────────────────────────── */

void fq_render_combat(fq_fb_t *fb, const fq_vm_combat_t *vm)
{
    if (fb == NULL || vm == NULL) {
        return;
    }

    fq_fb_clear(fb);

    /* ── Display border ──────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb, 0, 0,
                    (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* ── Divider line at y=98 ────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 0, COMBAT_DIVIDER_Y,
                    (int16_t)(FQ_FB_WIDTH - 1u), COMBAT_DIVIDER_Y, 1u);

    /* ── Round indicator placeholder (top-right) ─────────────────────────── */
    fq_fb_draw_rect(fb,
                    COMBAT_ROUND_X, COMBAT_ROUND_Y,
                    COMBAT_ROUND_W, COMBAT_ROUND_H, 1u);

    /* ── Enemy (f2) zone — top half ─────────────────────────────────────── */

    /* f2 name placeholder */
    fq_fb_draw_rect(fb,
                    COMBAT_BAR_X, COMBAT_F2_NAME_Y,
                    COMBAT_NAME_W, COMBAT_NAME_H, 1u);

    /* f2 sprite placeholder (top-right of top half) */
    fq_fb_draw_rect(fb,
                    COMBAT_F2_SPRITE_X, COMBAT_F2_SPRITE_Y,
                    COMBAT_F2_SPRITE_W, COMBAT_F2_SPRITE_H, 1u);
    fq_fb_draw_rect(fb,
                    COMBAT_F2_SPRITE_X + 2, COMBAT_F2_SPRITE_Y + 2,
                    COMBAT_F2_SPRITE_W - 4, COMBAT_F2_SPRITE_H - 4, 1u);

    /* f2 HP bar outline */
    fq_fb_draw_rect(fb,
                    COMBAT_BAR_X, COMBAT_F2_HP_BAR_Y,
                    COMBAT_BAR_W, COMBAT_BAR_H, 1u);

    /* f2 HP bar fill */
    {
        int16_t fill_w = calc_bar_width(vm->f2_hp, vm->f2_hp_max);
        if (fill_w > 0) {
            fq_fb_fill_rect(fb,
                            COMBAT_BAR_X + 1, COMBAT_F2_HP_BAR_Y + 1,
                            fill_w, COMBAT_BAR_H - 2, 1u);
        }
    }

    /* ── Player (f1) zone — bottom half ──────────────────────────────────── */

    /* f1 sprite placeholder (bottom-left of bottom half) */
    fq_fb_draw_rect(fb,
                    COMBAT_F1_SPRITE_X, COMBAT_F1_SPRITE_Y,
                    COMBAT_F1_SPRITE_W, COMBAT_F1_SPRITE_H, 1u);
    fq_fb_draw_rect(fb,
                    COMBAT_F1_SPRITE_X + 2, COMBAT_F1_SPRITE_Y + 2,
                    COMBAT_F1_SPRITE_W - 4, COMBAT_F1_SPRITE_H - 4, 1u);

    /* f1 HP bar outline */
    fq_fb_draw_rect(fb,
                    COMBAT_BAR_X, COMBAT_F1_HP_BAR_Y,
                    COMBAT_BAR_W, COMBAT_BAR_H, 1u);

    /* f1 HP bar fill */
    {
        int16_t fill_w = calc_bar_width(vm->f1_hp, vm->f1_hp_max);
        if (fill_w > 0) {
            fq_fb_fill_rect(fb,
                            COMBAT_BAR_X + 1, COMBAT_F1_HP_BAR_Y + 1,
                            fill_w, COMBAT_BAR_H - 2, 1u);
        }
    }

    /* f1 name placeholder */
    fq_fb_draw_rect(fb,
                    COMBAT_BAR_X, COMBAT_F1_NAME_Y,
                    COMBAT_NAME_W, COMBAT_NAME_H, 1u);

    /* ── Action banner (only when action_text is non-empty) ──────────────── */
    {
        /* Bounded check: strnlen with limit = sizeof(vm->action_text) - 1 */
        size_t text_len = strnlen(vm->action_text, sizeof(vm->action_text) - 1u);
        if (text_len > 0u) {
            /* Outer banner rect — white fill clears background */
            fq_fb_fill_rect(fb,
                            COMBAT_BANNER_X, COMBAT_BANNER_Y,
                            COMBAT_BANNER_W, COMBAT_BANNER_H, 0u);
            /* Banner border */
            fq_fb_draw_rect(fb,
                            COMBAT_BANNER_X, COMBAT_BANNER_Y,
                            COMBAT_BANNER_W, COMBAT_BANNER_H, 1u);
            /* Inner border (2px inset) */
            fq_fb_draw_rect(fb,
                            COMBAT_BANNER_X + 2, COMBAT_BANNER_Y + 2,
                            COMBAT_BANNER_W - 4, COMBAT_BANNER_H - 4, 1u);
            /* Text placeholder line in centre of banner */
            fq_fb_draw_line(fb,
                            COMBAT_BANNER_X + 10,
                            COMBAT_BANNER_Y + (COMBAT_BANNER_H / 2),
                            COMBAT_BANNER_X + COMBAT_BANNER_W - 10,
                            COMBAT_BANNER_Y + (COMBAT_BANNER_H / 2),
                            1u);
        }
    }
}
