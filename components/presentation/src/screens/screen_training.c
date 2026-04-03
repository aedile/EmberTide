/**
 * screen_training.c — FiestaQuest Presentation: Training Mini-Game Screen
 *
 * Renders the training mini-game screen onto the 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   y=0..33   : Black header bar — white game_name text (e.g. "Speed")
 *   y=36..115 : Activity area (80px tall bordered panel, inner double-rect)
 *               State visual: single line (WAIT), two lines (ACTIVE), fill (DONE)
 *   y=118..131: Score bar — "SCR" label left, full-width bar x=55..189 (135px)
 *   y=152..159: Difficulty dots — 10 squares (8x8), filled=active level
 *   y=166..199: Black footer bar — nav hint only (no state label collision)
 *               Active  (state=1): "[SUN] Hit  [PWR] Exit"
 *               Other   (state=0/2): "[PWR] Back"
 *
 * Score bar (integer-only, no float):
 *   fill_w = clamp(score, 0, 100) * TRAINING_SCORE_FILL_W / 100
 *   Score value shown as white-cleared patch with black text at bar right.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 */

#include "screens/screen_training.h"
#include "asset_data.h"
#include "sprite_util.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ── Layout constants ────────────────────────────────────────────────────── */

#define TRAINING_BAR_H         34  /**< Header / footer bar height. */

/** Activity area (margins 10px each side, height 80px). */
#define TRAINING_MARGIN_X       10
#define TRAINING_ACTIVITY_Y     36
#define TRAINING_ACTIVITY_W    180
#define TRAINING_ACTIVITY_H     80

/** Score bar — label at x=5, bar starts at x=55, bar ends at x=189. */
#define TRAINING_SCORE_LABEL_X   5
#define TRAINING_SCORE_BAR_X    55   /**< Bar outline left edge. */
#define TRAINING_SCORE_BAR_Y   118
#define TRAINING_SCORE_BAR_W   135   /**< Bar width: 55+135=190 → right edge 189. */
#define TRAINING_SCORE_BAR_H    14
#define TRAINING_SCORE_FILL_W  133u  /**< Inner fill pixels at score=100. */

/** Score value patch width inside bar's right end. */
#define TRAINING_SCORE_VAL_PATCH_W  28

/** Difficulty dots: 8×8 squares, gap 15px → 10 dots span x=10..145. */
#define TRAINING_DIFF_Y        152
#define TRAINING_DIFF_DOT_W      8
#define TRAINING_DIFF_DOT_H      8
#define TRAINING_DIFF_DOT_GAP   15
#define TRAINING_DIFF_MAX       10

/** Footer y — nav hint only; the state is visible from the activity content. */
#define TRAINING_FOOTER_Y      166

/* ── fq_render_training ───────────────────────────────────────────────────── */

void fq_render_training(fq_fb_t *fb, const fq_vm_training_t *vm)
{
    if (fb == NULL || vm == NULL) {
        return;
    }

    const fq_font_t *font = fq_get_font_small();

    fq_fb_clear(fb);

    /* ── Display border ───────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb, 0, 0,
                    (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* ── Header bar: game name ────────────────────────────────────────────── */
    fq_draw_header_bar(fb, font, 0, TRAINING_BAR_H, vm->game_name);

    /* ── Activity area (bordered double-rect panel) ───────────────────────── */
    fq_fb_draw_rect(fb,
                    TRAINING_MARGIN_X, TRAINING_ACTIVITY_Y,
                    TRAINING_ACTIVITY_W, TRAINING_ACTIVITY_H, 1u);
    fq_fb_draw_rect(fb,
                    TRAINING_MARGIN_X + 2, TRAINING_ACTIVITY_Y + 2,
                    TRAINING_ACTIVITY_W - 4, TRAINING_ACTIVITY_H - 4, 1u);

    /* State-dependent activity indicator. */
    switch (vm->state) {
        case 0u: /* WAITING — single centre horizontal line. */
            fq_fb_draw_line(fb,
                            TRAINING_MARGIN_X + 10,
                            (int16_t)(TRAINING_ACTIVITY_Y + TRAINING_ACTIVITY_H / 2),
                            (int16_t)(TRAINING_MARGIN_X + TRAINING_ACTIVITY_W - 10),
                            (int16_t)(TRAINING_ACTIVITY_Y + TRAINING_ACTIVITY_H / 2),
                            1u);
            break;

        case 1u: /* ACTIVE — two parallel lines (target zone). */
            fq_fb_draw_line(fb,
                            TRAINING_MARGIN_X + 10,
                            (int16_t)(TRAINING_ACTIVITY_Y + TRAINING_ACTIVITY_H / 3),
                            (int16_t)(TRAINING_MARGIN_X + TRAINING_ACTIVITY_W - 10),
                            (int16_t)(TRAINING_ACTIVITY_Y + TRAINING_ACTIVITY_H / 3),
                            1u);
            fq_fb_draw_line(fb,
                            TRAINING_MARGIN_X + 10,
                            (int16_t)(TRAINING_ACTIVITY_Y + 2u * TRAINING_ACTIVITY_H / 3u),
                            (int16_t)(TRAINING_MARGIN_X + TRAINING_ACTIVITY_W - 10),
                            (int16_t)(TRAINING_ACTIVITY_Y + 2u * TRAINING_ACTIVITY_H / 3u),
                            1u);
            break;

        case 2u: /* DONE — solid black fill inside borders. */
            fq_fb_fill_rect(fb,
                            TRAINING_MARGIN_X + 6, TRAINING_ACTIVITY_Y + 6,
                            TRAINING_ACTIVITY_W - 12, TRAINING_ACTIVITY_H - 12,
                            1u);
            break;

        default:
            break;
    }

    /* ── Score label + bar ────────────────────────────────────────────────── */
    fq_draw_text(fb, font, TRAINING_SCORE_LABEL_X, TRAINING_SCORE_BAR_Y, "SCR");

    fq_fb_draw_rect(fb,
                    TRAINING_SCORE_BAR_X, TRAINING_SCORE_BAR_Y,
                    TRAINING_SCORE_BAR_W, TRAINING_SCORE_BAR_H, 1u);

    {
        uint32_t score_clamp = (vm->score > 100u) ? 100u : (uint32_t)vm->score;
        uint32_t fill_w      = score_clamp * TRAINING_SCORE_FILL_W / 100u;
        if (fill_w > TRAINING_SCORE_FILL_W) { fill_w = TRAINING_SCORE_FILL_W; }
        if (fill_w > 0u) {
            fq_fb_fill_rect(fb,
                            TRAINING_SCORE_BAR_X + 1, TRAINING_SCORE_BAR_Y + 1,
                            (int16_t)fill_w, TRAINING_SCORE_BAR_H - 2, 1u);
        }
    }

    /* Score value drawn as white patch at the right end of the bar. */
    {
        char score_buf[8];
        snprintf(score_buf, sizeof(score_buf), "%u", (unsigned)vm->score);
        /* Clear a patch at the right end of the bar interior. */
        fq_fb_fill_rect(fb,
                        (int16_t)(TRAINING_SCORE_BAR_X + TRAINING_SCORE_BAR_W
                                  - TRAINING_SCORE_VAL_PATCH_W - 1),
                        TRAINING_SCORE_BAR_Y + 1,
                        TRAINING_SCORE_VAL_PATCH_W, TRAINING_SCORE_BAR_H - 2, 0u);
        fq_draw_text(fb, font,
                     (int16_t)(TRAINING_SCORE_BAR_X + TRAINING_SCORE_BAR_W
                                - TRAINING_SCORE_VAL_PATCH_W),
                     TRAINING_SCORE_BAR_Y + 1,
                     score_buf);
    }

    /* ── Difficulty dots ──────────────────────────────────────────────────── */
    {
        uint8_t active = (vm->difficulty > TRAINING_DIFF_MAX)
                             ? (uint8_t)TRAINING_DIFF_MAX
                             : vm->difficulty;
        for (uint8_t i = 0u; i < TRAINING_DIFF_MAX; i++) {
            int16_t dot_x = (int16_t)(TRAINING_MARGIN_X
                                      + (int16_t)i * TRAINING_DIFF_DOT_GAP);
            if (i < active) {
                fq_fb_fill_rect(fb, dot_x, TRAINING_DIFF_Y,
                                TRAINING_DIFF_DOT_W, TRAINING_DIFF_DOT_H, 1u);
            } else {
                fq_fb_draw_rect(fb, dot_x, TRAINING_DIFF_Y,
                                TRAINING_DIFF_DOT_W, TRAINING_DIFF_DOT_H, 1u);
            }
        }
    }

    /* ── Footer bar: nav hint ONLY — avoids state-label vs hint collision ── */
    /* Active state shows both buttons; waiting/done shows just [PWR] Back.  */
    {
        const char *hint_str = (vm->state == 1u) ? "[SUN] Hit  [PWR] Exit"
                                                  : "[PWR] Back";
        fq_draw_header_bar(fb, font, TRAINING_FOOTER_Y, TRAINING_BAR_H, hint_str);
    }
}
