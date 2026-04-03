/**
 * test_p17_title_fix_feature.c — Feature Tests: Title Screen Fix
 *
 * Happy-path contracts for the Phase 17 title screen patch:
 *
 *   F1: TITLE → HOME on BTN_A_PRESS (existing contract preserved).
 *   F2: TITLE → HOME on BTN_B_PRESS (new: any button advances title).
 *   F3: fq_get_font_title() returns non-NULL (FONT_SCRIPT_36 registered).
 *   F4: fq_text_width("EmberTide", font_title) > 0 (non-empty render).
 *   F5: fq_text_width("EmberTide", font_title) <= 180 (fits on one line).
 *   F6: fq_text_width("Press Any Button", font_small) <= 190 (fits display).
 *   F7: fq_text_width("Press Any Button", font_small) > 0 (non-empty render).
 *
 * Width measurements use the actual fq_font_t pixel widths from the generated
 * header (not the fm_fonts.h design-intent advance table).  At 36px, the
 * Jacquard script font renders "EmberTide" in ~142px — within the 180px limit.
 *
 * Constitution Priority 0: no PRNG touched.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "test_assert.h"
#include "types.h"
#include "event_bus.h"
#include "app_fsm.h"
#include "asset_data.h"
#include "fq_text.h"

/* Helper: init a fresh context starting at TITLE */
static void init_fresh(fq_app_ctx_t   *ctx,
                        fq_character_t *player,
                        fq_inventory_t *inv)
{
    memset(player, 0, sizeof(*player));
    memset(inv,    0, sizeof(*inv));
    game_err_t err = fq_app_init(ctx, player, inv);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_TITLE, (int)ctx->state);
}

int main(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;

    /* -------------------------------------------------------------------
     * F1: TITLE → HOME on BTN_A_PRESS (existing contract must still hold).
     * ------------------------------------------------------------------- */
    init_fresh(&ctx, &player, &inv);
    fq_event_t btn_a = { FQ_EVT_BTN_A_PRESS, 0u };
    game_err_t err   = fq_app_dispatch(&ctx, &btn_a);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)ctx.state);

    /* -------------------------------------------------------------------
     * F2: TITLE → HOME on BTN_B_PRESS (new: any button advances title).
     * ------------------------------------------------------------------- */
    init_fresh(&ctx, &player, &inv);
    fq_event_t btn_b = { FQ_EVT_BTN_B_PRESS, 0u };
    err = fq_app_dispatch(&ctx, &btn_b);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)ctx.state);

    /* -------------------------------------------------------------------
     * F3: fq_get_font_title() must return non-NULL.
     *     After the fix this is FONT_SCRIPT_36.
     * ------------------------------------------------------------------- */
    const fq_font_t *font_title = fq_get_font_title();
    TEST_ASSERT_TRUE(font_title != NULL);

    /* -------------------------------------------------------------------
     * F4: "EmberTide" must render non-zero width at the title font.
     * ------------------------------------------------------------------- */
    int16_t w_title = fq_text_width(font_title, "EmberTide");
    TEST_ASSERT_TRUE(w_title > 0);

    /* -------------------------------------------------------------------
     * F5: "EmberTide" must fit in ≤180 px on one line (single-line layout).
     *
     * At FONT_SCRIPT_36 the Jacquard typeface renders compact glyphs.
     * The actual pixel width from the generated glyph bitmaps is ~142px —
     * well within the 180px display budget.
     * ------------------------------------------------------------------- */
    TEST_ASSERT_TRUE(w_title <= 180);

    /* -------------------------------------------------------------------
     * F6: "Press Any Button" must fit on screen at small font.
     * ------------------------------------------------------------------- */
    const fq_font_t *font_small = fq_get_font_small();
    TEST_ASSERT_TRUE(font_small != NULL);
    int16_t w_prompt = fq_text_width(font_small, "Press Any Button");
    TEST_ASSERT_TRUE(w_prompt <= 190);

    /* -------------------------------------------------------------------
     * F7: "Press Any Button" must render non-zero width.
     * ------------------------------------------------------------------- */
    TEST_ASSERT_TRUE(w_prompt > 0);

    printf("test_p17_title_fix_feature: PASS\n");
    return 0;
}
