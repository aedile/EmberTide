/**
 * screen_training.c — FiestaQuest Presentation: Training Mini-Game Screen
 *
 * Renders the training mini-game screen onto the 200x200 1-bit framebuffer.
 *
 * Layout:
 *   y=0..15   : Border + game name placeholder band
 *   y=20..75  : Activity area (state-dependent visual indicator)
 *   y=85..105 : Score bar (proportional to vm->score / 100)
 *   y=110..130: Difficulty dots (vm->difficulty, max 10)
 *   y=140..170: State label placeholder
 *
 * Score bar formula (integer-only):
 *   fill_w = (uint32_t)vm->score * TRAINING_SCORE_FILL_W / 100u
 *   Saturates at TRAINING_SCORE_FILL_W regardless of input.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 */

#include "screens/screen_training.h"
#include <stddef.h>
#include <stdint.h>

/* ── Layout constants ────────────────────────────────────────────────────── */

#define TRAINING_MARGIN_X       10
#define TRAINING_BORDER_W      180

/* Game name band */
#define TRAINING_NAME_Y          5
#define TRAINING_NAME_H         12

/* Activity area */
#define TRAINING_ACTIVITY_Y     20
#define TRAINING_ACTIVITY_H     56
#define TRAINING_ACTIVITY_W    180

/* Score bar */
#define TRAINING_SCORE_BAR_Y    85
#define TRAINING_SCORE_BAR_H    10
#define TRAINING_SCORE_BAR_W   180
#define TRAINING_SCORE_FILL_W  178u  /**< Inner fill at score=100. */

/* Difficulty dots: 4px squares, spaced 10px apart */
#define TRAINING_DIFF_Y        112
#define TRAINING_DIFF_DOT_W      6
#define TRAINING_DIFF_DOT_H      6
#define TRAINING_DIFF_DOT_GAP   10
#define TRAINING_DIFF_MAX       10

/* State label placeholder */
#define TRAINING_STATE_Y       145
#define TRAINING_STATE_H        14
#define TRAINING_STATE_W        80

/* ── fq_render_training ───────────────────────────────────────────────────── */

void fq_render_training(fq_fb_t *fb, const fq_vm_training_t *vm)
{
    if (fb == NULL || vm == NULL) {
        return;
    }

    fq_fb_clear(fb);

    /* ── Display border ───────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb, 0, 0,
                    (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* ── Game name band ───────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb,
                    TRAINING_MARGIN_X, TRAINING_NAME_Y,
                    TRAINING_BORDER_W, TRAINING_NAME_H, 1u);
    /* Divider under name band */
    fq_fb_draw_line(fb,
                    TRAINING_MARGIN_X,
                    TRAINING_NAME_Y + TRAINING_NAME_H + 2,
                    TRAINING_MARGIN_X + TRAINING_BORDER_W - 1,
                    TRAINING_NAME_Y + TRAINING_NAME_H + 2, 1u);

    /* ── Activity area (state-dependent) ─────────────────────────────────── */
    fq_fb_draw_rect(fb,
                    TRAINING_MARGIN_X, TRAINING_ACTIVITY_Y,
                    TRAINING_ACTIVITY_W, TRAINING_ACTIVITY_H, 1u);

    /* State indicator inside activity area:
     *   state=0 (WAITING):  single horizontal line
     *   state=1 (ACTIVE):   double horizontal lines (active energy)
     *   state=2 (DONE):     filled inner rect (completion marker) */
    switch (vm->state) {
        case 0u: /* WAITING */
            fq_fb_draw_line(fb,
                            TRAINING_MARGIN_X + 10,
                            TRAINING_ACTIVITY_Y + (TRAINING_ACTIVITY_H / 2),
                            TRAINING_MARGIN_X + TRAINING_ACTIVITY_W - 10,
                            TRAINING_ACTIVITY_Y + (TRAINING_ACTIVITY_H / 2),
                            1u);
            break;

        case 1u: /* ACTIVE */
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

        case 2u: /* DONE */
            fq_fb_fill_rect(fb,
                            TRAINING_MARGIN_X + 4, TRAINING_ACTIVITY_Y + 4,
                            TRAINING_ACTIVITY_W - 8, TRAINING_ACTIVITY_H - 8,
                            1u);
            break;

        default:
            /* Unknown state — draw nothing inside activity area */
            break;
    }

    /* ── Score bar ────────────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb,
                    TRAINING_MARGIN_X, TRAINING_SCORE_BAR_Y,
                    TRAINING_SCORE_BAR_W, TRAINING_SCORE_BAR_H, 1u);

    /* Score fill — saturate at score=100 */
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
        /* Clamp to TRAINING_DIFF_MAX */
        uint8_t dots = (vm->difficulty > TRAINING_DIFF_MAX)
                           ? (uint8_t)TRAINING_DIFF_MAX
                           : vm->difficulty;
        for (uint8_t i = 0u; i < TRAINING_DIFF_MAX; i++) {
            int16_t dot_x = (int16_t)(TRAINING_MARGIN_X
                                      + (int16_t)i * TRAINING_DIFF_DOT_GAP);
            if (i < dots) {
                /* Filled dot — difficulty reached */
                fq_fb_fill_rect(fb,
                                dot_x, TRAINING_DIFF_Y,
                                TRAINING_DIFF_DOT_W, TRAINING_DIFF_DOT_H, 1u);
            } else {
                /* Empty dot outline */
                fq_fb_draw_rect(fb,
                                dot_x, TRAINING_DIFF_Y,
                                TRAINING_DIFF_DOT_W, TRAINING_DIFF_DOT_H, 1u);
            }
        }
    }

    /* ── State label placeholder ──────────────────────────────────────────── */
    fq_fb_draw_rect(fb,
                    TRAINING_MARGIN_X, TRAINING_STATE_Y,
                    TRAINING_STATE_W, TRAINING_STATE_H, 1u);
}
