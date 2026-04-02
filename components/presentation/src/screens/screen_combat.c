/**
 * screen_combat.c — FiestaQuest Presentation: Combat HUD Screen Renderer
 *
 * Renders the split-screen combat HUD onto the 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   y=0..33   : Enemy (f2) black header bar — white text "NAME" only
 *   y=34..45  : Enemy HP bar (left half, width 110), 12px tall
 *   y=34..97  : Enemy 2x sprite right-aligned (64x64) at x=128
 *   y=98..99  : Centre divider (2px thick)
 *   y=100..111: Player HP bar (right half, x=90, width 104), 12px tall
 *   y=100..163: Player 2x sprite left-aligned (64x64) at x=4
 *   y=166..199: Player (f1) black footer bar — white "NAME" left, "R:N" right
 *
 * Action banner (when action_text non-empty):
 *   Centred panel overlaid at y=70..109, x=10..189, white fill, black border.
 *
 * HP bar formula (integer-only, int32_t intermediate):
 *   fill_w = (hp_max > 0) ? clamp(hp * BAR_FILL / hp_max, 0, BAR_FILL) : 0
 *
 * NULL-safe: fq_render_combat(NULL, ...) and fq_render_combat(..., NULL) are
 * silent no-ops.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 */

#include "screens/screen_combat.h"
#include "asset_data.h"
#include "sprite_util.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/* ── Layout constants ────────────────────────────────────────────────────── */

#define COMBAT_BAR_H             34  /**< Header / footer bar height. */

/* Enemy (f2) zone — top half y=0..99. */
#define COMBAT_F2_SPRITE_X      128  /**< 2x sprite: 64px, at x=128 → right edge 192. */
#define COMBAT_F2_SPRITE_Y       34  /**< Sprite starts just below header bar. */

#define COMBAT_F2_HP_X            5  /**< Enemy HP bar left edge. */
#define COMBAT_F2_HP_Y           34  /**< Enemy HP bar top (immediately below header). */
#define COMBAT_F2_HP_W          118  /**< Enemy HP bar width (leaves room for sprite). */
#define COMBAT_F2_HP_H           12  /**< Enemy HP bar height. */
#define COMBAT_F2_HP_FILL_W     116u /**< Inner fill pixels at 100%. */

/* Divider band. */
#define COMBAT_DIVIDER_Y         98  /**< First divider line y. */

/* Player (f1) zone — bottom half y=100..199. */
#define COMBAT_F1_SPRITE_X        4  /**< 2x sprite: 64px wide, x=4. */
#define COMBAT_F1_SPRITE_Y      100  /**< Sprite starts just below divider. */

#define COMBAT_F1_HP_X           72  /**< Player HP bar right of sprite. */
#define COMBAT_F1_HP_Y          100  /**< Player HP bar top (same row as sprite start). */
#define COMBAT_F1_HP_W          124  /**< Player HP bar width. */
#define COMBAT_F1_HP_H           12  /**< Player HP bar height. */
#define COMBAT_F1_HP_FILL_W     122u /**< Inner fill pixels at 100%. */

/** Footer bar top y. */
#define COMBAT_FOOTER_Y         166

/* Action banner — centred overlay. */
#define COMBAT_BANNER_X          10
#define COMBAT_BANNER_Y          70
#define COMBAT_BANNER_W         180
#define COMBAT_BANNER_H          36

/* ── Internal helpers ────────────────────────────────────────────────────── */

/**
 * calc_hp_fill — Compute HP bar fill width with int32_t intermediate.
 * Guards: hp_max<=0 → 0; negative hp → 0; overflow → clamp.
 */
static int16_t calc_hp_fill(int16_t hp, int16_t hp_max, int16_t max_fill)
{
    if (hp_max <= 0) { return 0; }
    int32_t w = ((int32_t)hp * (int32_t)max_fill) / (int32_t)hp_max;
    if (w < 0)               { w = 0; }
    if (w > (int32_t)max_fill) { w = (int32_t)max_fill; }
    return (int16_t)w;
}

/**
 * draw_hp_bar — Outlined bar with proportional fill and "HP" label.
 */
static void draw_hp_bar(fq_fb_t *fb, const fq_font_t *font,
                        int16_t bar_x, int16_t bar_y,
                        int16_t bar_w, int16_t bar_h,
                        int16_t fill_max,
                        int16_t hp, int16_t hp_max)
{
    /* Outline. */
    fq_fb_draw_rect(fb, bar_x, bar_y, bar_w, bar_h, 1u);

    /* Proportional fill. */
    int16_t fill_w = calc_hp_fill(hp, hp_max, fill_max);
    if (fill_w > 0) {
        fq_fb_fill_rect(fb,
                        (int16_t)(bar_x + 1), (int16_t)(bar_y + 1),
                        fill_w, (int16_t)(bar_h - 2), 1u);
    }

    /* "HP" label — white patch then black text. */
    fq_fb_fill_rect(fb,
                    (int16_t)(bar_x + 1), (int16_t)(bar_y + 1),
                    26, (int16_t)(bar_h - 2), 0u);
    fq_draw_text(fb, font,
                 (int16_t)(bar_x + 2), (int16_t)(bar_y + 1), "HP");
}

/* ── fq_render_combat ────────────────────────────────────────────────────── */

void fq_render_combat(fq_fb_t *fb, const fq_vm_combat_t *vm)
{
    if (fb == NULL || vm == NULL) {
        return;
    }

    const fq_font_t *font = fq_get_font_small();

    fq_fb_clear(fb);

    /* ── Display border ───────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb, 0, 0,
                    (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* ── Enemy (f2) header bar: name ─────────────────────────────────────── */
    fq_draw_header_bar(fb, font, 0, COMBAT_BAR_H, vm->f2_name);

    /* ── Enemy HP bar (left side, below header) ──────────────────────────── */
    draw_hp_bar(fb, font,
                COMBAT_F2_HP_X, COMBAT_F2_HP_Y,
                COMBAT_F2_HP_W, COMBAT_F2_HP_H,
                (int16_t)COMBAT_F2_HP_FILL_W,
                vm->f2_hp, vm->f2_hp_max);

    /* ── Enemy 2x sprite (right-aligned) ─────────────────────────────────── */
    {
        const fq_sprite_t *sp = fq_get_char_sprite(vm->f2_class_id, 0u);
        if (sp != NULL) {
            fq_blit_sprite_2x(fb, COMBAT_F2_SPRITE_X, COMBAT_F2_SPRITE_Y, sp);
        }
    }

    /* ── Divider (2px thick) ─────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 0, COMBAT_DIVIDER_Y,
                    (int16_t)(FQ_FB_WIDTH - 1u), COMBAT_DIVIDER_Y, 1u);
    fq_fb_draw_line(fb, 0, (int16_t)(COMBAT_DIVIDER_Y + 1),
                    (int16_t)(FQ_FB_WIDTH - 1u), (int16_t)(COMBAT_DIVIDER_Y + 1), 1u);

    /* ── Player (f1) HP bar (right of sprite) ────────────────────────────── */
    draw_hp_bar(fb, font,
                COMBAT_F1_HP_X, COMBAT_F1_HP_Y,
                COMBAT_F1_HP_W, COMBAT_F1_HP_H,
                (int16_t)COMBAT_F1_HP_FILL_W,
                vm->f1_hp, vm->f1_hp_max);

    /* ── Player 2x sprite (left-aligned) ────────────────────────────────── */
    {
        const fq_sprite_t *sp = fq_get_char_sprite(vm->f1_class_id, 0u);
        if (sp != NULL) {
            fq_blit_sprite_2x(fb, COMBAT_F1_SPRITE_X, COMBAT_F1_SPRITE_Y, sp);
        }
    }

    /* ── Player footer bar: name left, round right ───────────────────────── */
    {
        char r_buf[8];
        snprintf(r_buf, sizeof(r_buf), "R:%u", (unsigned)vm->round);
        fq_draw_header_bar2(fb, font, COMBAT_FOOTER_Y, COMBAT_BAR_H,
                            vm->f1_name, r_buf);
    }

    /* ── Action banner (only when action_text is non-empty) ─────────────── */
    {
        size_t text_len = strnlen(vm->action_text, sizeof(vm->action_text) - 1u);
        if (text_len > 0u) {
            /* White fill, then double-rect ornate border. */
            fq_fb_fill_rect(fb, COMBAT_BANNER_X, COMBAT_BANNER_Y,
                            COMBAT_BANNER_W, COMBAT_BANNER_H, 0u);
            fq_fb_draw_rect(fb, COMBAT_BANNER_X, COMBAT_BANNER_Y,
                            COMBAT_BANNER_W, COMBAT_BANNER_H, 1u);
            fq_fb_draw_rect(fb,
                            COMBAT_BANNER_X + 2, COMBAT_BANNER_Y + 2,
                            COMBAT_BANNER_W - 4, COMBAT_BANNER_H - 4, 1u);
            /* Text centred vertically in banner. */
            fq_draw_text(fb, font,
                         COMBAT_BANNER_X + 6,
                         COMBAT_BANNER_Y + 3,
                         vm->action_text);
        }
    }
}
