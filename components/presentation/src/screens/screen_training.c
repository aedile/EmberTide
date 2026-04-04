/**
 * screen_training.c — FiestaQuest Presentation: Training Mini-Game Screen
 *
 * Renders the training mini-game screen onto the 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   y=0..33   : Black header bar — white game_name text (e.g. "Speed")
 *   y=36..115 : Activity area (80px tall bordered panel)
 *               WAITING: "Press [SUN] to start" + game type selection dots
 *               ACTIVE:  Moving target indicator bar (target_pos 0-100)
 *               DONE:    "DONE! score=XX" summary fill
 *   y=118..131: Score bar — "SCR" label + fill proportional to score
 *   y=133..144: Target counter — "TGT: X/5" text
 *   y=152..159: Difficulty dots
 *   y=166..199: Black footer bar — nav hints
 *
 * Phase-19 update:
 *   - ACTIVE state now renders a target bar with a moving indicator at
 *     target_pos (0-100) mapped to the bar width.
 *   - Hit zone region [40%..60%] is highlighted with a shaded band.
 *   - targets_done counter shown below the score bar.
 *   - Footer footer updated: WAITING shows type selection hints.
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

#define TRAINING_BAR_H         34

#define TRAINING_MARGIN_X       10
#define TRAINING_ACTIVITY_Y     36
#define TRAINING_ACTIVITY_W    180
#define TRAINING_ACTIVITY_H     80

#define TRAINING_SCORE_LABEL_X   5
#define TRAINING_SCORE_BAR_X    55
#define TRAINING_SCORE_BAR_Y   118
#define TRAINING_SCORE_BAR_W   135
#define TRAINING_SCORE_BAR_H    14
#define TRAINING_SCORE_FILL_W  133u

#define TRAINING_SCORE_VAL_PATCH_W  28

/** Target counter row (below score bar). */
#define TRAINING_TARGET_CTR_Y  134

#define TRAINING_DIFF_Y        152
#define TRAINING_DIFF_DOT_W      8
#define TRAINING_DIFF_DOT_H      8
#define TRAINING_DIFF_DOT_GAP   15
#define TRAINING_DIFF_MAX       10

#define TRAINING_FOOTER_Y      166

/* ── Target bar layout (within activity area when ACTIVE) ───────────────── */

/** Target bar spans the inner activity area. */
#define TGT_BAR_INNER_X    (TRAINING_MARGIN_X + 10)
#define TGT_BAR_INNER_Y    (TRAINING_ACTIVITY_Y + TRAINING_ACTIVITY_H / 2 - 4)
#define TGT_BAR_INNER_W    (TRAINING_ACTIVITY_W - 20)
#define TGT_BAR_HEIGHT     8

/** Hit zone: positions [40,60] → pixel range on bar. */
#define TGT_ZONE_LO_PCT    40u
#define TGT_ZONE_HI_PCT    60u

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
        case 0u: /* WAITING — type selection hint. */
            fq_draw_text(fb, font,
                         TRAINING_MARGIN_X + 10,
                         (int16_t)(TRAINING_ACTIVITY_Y + TRAINING_ACTIVITY_H / 2 - 4),
                         "[SUN]Start [PWR]Type");
            break;

        case 1u: { /* ACTIVE — moving target bar. */
            /* Draw static target track line. */
            fq_fb_draw_rect(fb,
                            TGT_BAR_INNER_X, TGT_BAR_INNER_Y,
                            TGT_BAR_INNER_W, TGT_BAR_HEIGHT, 1u);

            /* Shade the hit zone [40%..60%] with a slightly denser fill.
             * Draw vertical tick marks at zone boundaries. */
            uint32_t zone_lo_x = (uint32_t)TGT_ZONE_LO_PCT
                                  * (uint32_t)(TGT_BAR_INNER_W - 2u) / 100u;
            uint32_t zone_hi_x = (uint32_t)TGT_ZONE_HI_PCT
                                  * (uint32_t)(TGT_BAR_INNER_W - 2u) / 100u;

            /* Tick marks at zone edges. */
            fq_fb_draw_line(fb,
                            (int16_t)(TGT_BAR_INNER_X + (int16_t)zone_lo_x),
                            (int16_t)(TGT_BAR_INNER_Y - 3),
                            (int16_t)(TGT_BAR_INNER_X + (int16_t)zone_lo_x),
                            (int16_t)(TGT_BAR_INNER_Y + TGT_BAR_HEIGHT + 2),
                            1u);
            fq_fb_draw_line(fb,
                            (int16_t)(TGT_BAR_INNER_X + (int16_t)zone_hi_x),
                            (int16_t)(TGT_BAR_INNER_Y - 3),
                            (int16_t)(TGT_BAR_INNER_X + (int16_t)zone_hi_x),
                            (int16_t)(TGT_BAR_INNER_Y + TGT_BAR_HEIGHT + 2),
                            1u);

            /* Draw target indicator (3px wide vertical bar at target_pos). */
            uint8_t pos = (vm->target_pos > 100u) ? 100u : vm->target_pos;
            uint32_t tgt_x = (uint32_t)pos
                             * (uint32_t)(TGT_BAR_INNER_W - 2u) / 100u;
            fq_fb_fill_rect(fb,
                            (int16_t)(TGT_BAR_INNER_X + 1 + (int16_t)tgt_x),
                            (int16_t)(TGT_BAR_INNER_Y + 1),
                            3, TGT_BAR_HEIGHT - 2, 1u);
            break;
        }

        case 2u: /* DONE — solid black fill inside borders. */
            fq_fb_fill_rect(fb,
                            TRAINING_MARGIN_X + 6, TRAINING_ACTIVITY_Y + 6,
                            TRAINING_ACTIVITY_W - 12, TRAINING_ACTIVITY_H - 12,
                            1u);
            /* "DONE" text in white. */
            fq_draw_text(fb, font,
                         TRAINING_MARGIN_X + 12,
                         (int16_t)(TRAINING_ACTIVITY_Y + TRAINING_ACTIVITY_H / 2 - 4),
                         "DONE");
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

    /* Score value at right end of bar. */
    {
        char score_buf[8];
        snprintf(score_buf, sizeof(score_buf), "%u", (unsigned)vm->score);
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

    /* ── Target counter "TGT: X/5" ───────────────────────────────────────── */
    {
        char tgt_buf[12];
        snprintf(tgt_buf, sizeof(tgt_buf), "TGT:%u/5",
                 (unsigned)vm->targets_done);
        fq_draw_text(fb, font, TRAINING_SCORE_LABEL_X, TRAINING_TARGET_CTR_Y,
                     tgt_buf);
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

    /* ── Footer bar: nav hints ────────────────────────────────────────────── */
    {
        const char *hint_str;
        if (vm->state == 0u) {
            hint_str = "[PWR]Type [SUN]Start";
        } else if (vm->state == 1u) {
            hint_str = "[PWR]Hit  [SUN]Exit";
        } else {
            hint_str = "[SUN] Back";
        }
        fq_draw_header_bar(fb, font, TRAINING_FOOTER_Y, TRAINING_BAR_H, hint_str);
    }
}
