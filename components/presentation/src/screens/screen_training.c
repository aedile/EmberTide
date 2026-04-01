/**
 * screen_training.c — FiestaQuest Presentation: Training Mini-Game Screen
 *
 * Renders the training mini-game screen onto the 200x200 1-bit framebuffer.
 * Phase 18: replaced placeholder geometry with real text calls.
 *
 * Layout:
 *   y=3..32   : Game name text (glyph_h=30) + separator at y=34
 *   y=38..93  : Activity area (state-dependent visual indicator)
 *   y=97..107 : Score bar + "SCORE:" text + numeric score
 *   y=112..117: Difficulty dots (vm->difficulty, max 10)
 *   y=145..174: State label text ("READY" / "GO!" / "DONE")
 *
 * Score bar formula (integer-only):
 *   fill_w = (uint32_t)vm->score * TRAINING_SCORE_FILL_W / 100u
 *   Saturates at TRAINING_SCORE_FILL_W regardless of input.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 */

#include "screens/screen_training.h"
#include "asset_data.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ── Layout constants ────────────────────────────────────────────────────── */

#define TRAINING_MARGIN_X       10
#define TRAINING_BORDER_W      180

/* Game name text y */
#define TRAINING_NAME_TEXT_Y     3

/* Activity area */
#define TRAINING_ACTIVITY_Y     38
#define TRAINING_ACTIVITY_H     56
#define TRAINING_ACTIVITY_W    180

/* Score bar */
#define TRAINING_SCORE_BAR_Y    97
#define TRAINING_SCORE_BAR_H    10
#define TRAINING_SCORE_BAR_W   180
#define TRAINING_SCORE_FILL_W  178u  /**< Inner fill at score=100. */

/* Score text (to the left of bar, above it) */
#define TRAINING_SCORE_TEXT_Y   83

/* Difficulty dots: 6px squares, spaced 10px apart */
#define TRAINING_DIFF_Y        112
#define TRAINING_DIFF_DOT_W      6
#define TRAINING_DIFF_DOT_H      6
#define TRAINING_DIFF_DOT_GAP   10
#define TRAINING_DIFF_MAX       10

/* State label text y */
#define TRAINING_STATE_TEXT_Y  145

/* ── State label strings ─────────────────────────────────────────────────── */
static const char * const s_state_labels[3] = {
    "READY",  /* state 0 */
    "GO!",    /* state 1 */
    "DONE",   /* state 2 */
};

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

    /* ── Game name text ──────────────────────────────────────────────────── */
    fq_draw_text(fb, font, TRAINING_MARGIN_X, TRAINING_NAME_TEXT_Y,
                 vm->game_name);

    /* Separator under game name */
    fq_fb_draw_line(fb,
                    TRAINING_MARGIN_X, 34,
                    TRAINING_MARGIN_X + TRAINING_BORDER_W - 1, 34, 1u);

    /* ── Activity area (state-dependent) ─────────────────────────────────── */
    fq_fb_draw_rect(fb,
                    TRAINING_MARGIN_X, TRAINING_ACTIVITY_Y,
                    TRAINING_ACTIVITY_W, TRAINING_ACTIVITY_H, 1u);

    switch (vm->state) {
        case 0u: /* WAITING — single horizontal line */
            fq_fb_draw_line(fb,
                            TRAINING_MARGIN_X + 10,
                            TRAINING_ACTIVITY_Y + (TRAINING_ACTIVITY_H / 2),
                            TRAINING_MARGIN_X + TRAINING_ACTIVITY_W - 10,
                            TRAINING_ACTIVITY_Y + (TRAINING_ACTIVITY_H / 2),
                            1u);
            break;

        case 1u: /* ACTIVE — double horizontal lines */
            fq_fb_draw_line(fb,
                            TRAINING_MARGIN_X + 10,
                            TRAINING_ACTIVITY_Y + (TRAINING_ACTIVITY_H / 3),
                            TRAINING_MARGIN_X + TRAINING_ACTIVITY_W - 10,
                            TRAINING_ACTIVITY_Y + (TRAINING_ACTIVITY_H / 3),
                            1u);
            fq_fb_draw_line(fb,
                            TRAINING_MARGIN_X + 10,
                            TRAINING_ACTIVITY_Y + (2u * TRAINING_ACTIVITY_H / 3u),
                            TRAINING_MARGIN_X + TRAINING_ACTIVITY_W - 10,
                            TRAINING_ACTIVITY_Y + (2u * TRAINING_ACTIVITY_H / 3u),
                            1u);
            break;

        case 2u: /* DONE — filled inner rect */
            fq_fb_fill_rect(fb,
                            TRAINING_MARGIN_X + 4, TRAINING_ACTIVITY_Y + 4,
                            TRAINING_ACTIVITY_W - 8, TRAINING_ACTIVITY_H - 8,
                            1u);
            break;

        default:
            break;
    }

    /* ── Score label + number ────────────────────────────────────────────── */
    {
        char score_buf[16];
        snprintf(score_buf, sizeof(score_buf), "SCORE: %u",
                 (unsigned)vm->score);
        fq_draw_text(fb, font, TRAINING_MARGIN_X, TRAINING_SCORE_TEXT_Y,
                     score_buf);
    }

    /* ── Score bar ────────────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb,
                    TRAINING_MARGIN_X, TRAINING_SCORE_BAR_Y,
                    TRAINING_SCORE_BAR_W, TRAINING_SCORE_BAR_H, 1u);
    {
        uint32_t score_clamped = (vm->score > 100u) ? 100u : (uint32_t)vm->score;
        uint32_t fill_w        = score_clamped * TRAINING_SCORE_FILL_W / 100u;
        if (fill_w > 0u) {
            fq_fb_fill_rect(fb,
                            TRAINING_MARGIN_X + 1, TRAINING_SCORE_BAR_Y + 1,
                            (int16_t)fill_w, TRAINING_SCORE_BAR_H - 2, 1u);
        }
    }

    /* ── Difficulty dots ──────────────────────────────────────────────────── */
    {
        uint8_t dots = (vm->difficulty > TRAINING_DIFF_MAX)
                           ? (uint8_t)TRAINING_DIFF_MAX
                           : vm->difficulty;
        for (uint8_t i = 0u; i < TRAINING_DIFF_MAX; i++) {
            int16_t dot_x = (int16_t)(TRAINING_MARGIN_X
                                      + (int16_t)i * TRAINING_DIFF_DOT_GAP);
            if (i < dots) {
                fq_fb_fill_rect(fb,
                                dot_x, TRAINING_DIFF_Y,
                                TRAINING_DIFF_DOT_W, TRAINING_DIFF_DOT_H, 1u);
            } else {
                fq_fb_draw_rect(fb,
                                dot_x, TRAINING_DIFF_Y,
                                TRAINING_DIFF_DOT_W, TRAINING_DIFF_DOT_H, 1u);
            }
        }
    }

    /* ── State label text ─────────────────────────────────────────────────── */
    {
        const char *state_str = (vm->state < 3u)
                                    ? s_state_labels[vm->state]
                                    : "?";
        fq_draw_text(fb, font, TRAINING_MARGIN_X, TRAINING_STATE_TEXT_Y,
                     state_str);
    }
}
