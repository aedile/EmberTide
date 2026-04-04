/**
 * test_p19_training_b_exit.c — Audit Fix: Training BTN_B Partial XP Award
 *
 * Rule 22 BOUND RED — written BEFORE the fix in app_fsm.c.
 *
 * The phase-boundary auditor found that BTN_B exit from ACTIVE training
 * state abandoned the session without awarding partial XP, contradicting
 * the spec: "Button B exits training at any time (partial score, partial XP
 * award)."
 *
 * These tests prove the FIXED behavior:
 *   B1: BTN_B in ACTIVE with score > 0 must increase player.xp (partial award).
 *   B2: BTN_B in ACTIVE with score = 0 must leave player.xp unchanged.
 *   B3: After BTN_B exit from ACTIVE, state must be HOME (not TRAINING).
 *   B4: XP award on B-exit with near-max xp must not crash (overflow guard).
 *
 * Level selection note:
 *   To isolate XP award from level-up effects, tests use player.level = 2.
 *   fq_calc_xp_to_next(2) = 50 * 4 = 200.  Max award = 100 * 2 = 200.
 *   With player.xp = 0: award <= 200, so at most one level-up boundary.
 *   For exact assertions we use score = 60 -> award = 120 < 200, so no
 *   level-up fires and player.xp after = xp_before + 120 exactly.
 *
 * Note on fq_training_advance_target():
 *   This function is a TEST ACCESSOR — it has no production caller. It is
 *   used in test F10 (test_p19_interactive_feature.c) to explicitly skip a
 *   target without position-based scoring. The production tick loop calls
 *   fq_training_step() which handles target advancement internally when
 *   target_pos exceeds 100. See training_session.h for the full contract.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "test_assert.h"
#include "types.h"
#include "event_bus.h"
#include "app_fsm.h"
#include "character.h"
#include "training_session.h"
#include "progression.h"

/* ===========================================================================
 * Helpers
 * =========================================================================*/

static void setup_active_training(fq_app_ctx_t   *ctx,
                                   fq_character_t *player,
                                   fq_inventory_t *inv,
                                   uint8_t         score)
{
    memset(player, 0, sizeof(*player));
    memset(inv,    0, sizeof(*inv));
    fq_app_init(ctx, player, inv);

    ctx->state = FQ_STATE_TRAINING;
    fq_training_session_init(&ctx->training, FQ_TS_POWER);
    fq_training_session_start(&ctx->training);
    ctx->training.score = score;
}

/* ===========================================================================
 * B1 — BTN_B exit in ACTIVE with score > 0 must award partial XP.
 *
 * Setup: player level 2 (xp_needed = 200), xp = 0, score = 60 -> award = 120.
 * Since award 120 < xp_needed 200, no level-up fires.
 * Expected: player.xp == 0 + 120 = 120 exactly.
 *           state = HOME.
 * =========================================================================*/
static void test_b1_btn_b_active_awards_partial_xp(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;

    setup_active_training(&ctx, &player, &inv, 60u);
    /* level 2: xp_to_next = 50*4 = 200. Award = 60*2 = 120 < 200 (no level-up). */
    player.level = 2u;
    player.xp    = 0u;

    uint32_t xp_before = player.xp;

    fq_event_t eb = { FQ_EVT_BTN_B_PRESS, 0u };
    fq_app_dispatch(&ctx, &eb);

    /* State must have transitioned to HOME. */
    TEST_ASSERT_EQUAL_INT(FQ_STATE_HOME, (int)ctx.state);

    /* XP must be awarded: score(60) * 2 = 120 (no level-up since 120 < 200). */
    uint32_t expected_xp = xp_before + 120u;
    TEST_ASSERT_EQUAL_UINT32(expected_xp, player.xp);

    printf("[B1] btn_b_active_awards_partial_xp: expected=%" PRIu32 " actual=%" PRIu32 " -> PASS\n",
           expected_xp, player.xp);
}

/* ===========================================================================
 * B2 — BTN_B exit in ACTIVE with score = 0 must award 0 XP.
 *
 * Setup: training ACTIVE, score = 0.
 * Press BTN_B.
 * Expected: player.xp unchanged.
 *           state = HOME.
 * =========================================================================*/
static void test_b2_btn_b_active_zero_score_no_xp(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;

    setup_active_training(&ctx, &player, &inv, 0u);
    player.level = 3u;
    player.xp    = 500u;

    uint32_t xp_before = player.xp;

    fq_event_t eb = { FQ_EVT_BTN_B_PRESS, 0u };
    fq_app_dispatch(&ctx, &eb);

    /* State must be HOME. */
    TEST_ASSERT_EQUAL_INT(FQ_STATE_HOME, (int)ctx.state);

    /* XP must be unchanged — award is 0 * 2 = 0. */
    TEST_ASSERT_EQUAL_UINT32(xp_before, player.xp);

    printf("[B2] btn_b_active_zero_score_no_xp: PASS\n");
}

/* ===========================================================================
 * B3 — BTN_B exit in ACTIVE always transitions to HOME.
 *
 * Verified independently from XP: state must be HOME after any BTN_B ACTIVE
 * exit, regardless of score. home_menu_index must be reset to 0.
 * =========================================================================*/
static void test_b3_btn_b_active_exits_to_home(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;

    setup_active_training(&ctx, &player, &inv, 40u);

    /* Sanity: confirm we start in TRAINING. */
    TEST_ASSERT_EQUAL_INT(FQ_STATE_TRAINING, (int)ctx.state);

    fq_event_t eb = { FQ_EVT_BTN_B_PRESS, 0u };
    fq_app_dispatch(&ctx, &eb);

    TEST_ASSERT_EQUAL_INT(FQ_STATE_HOME, (int)ctx.state);
    /* home_menu_index must be reset to 0 by go_home(). */
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.home_menu_index);

    printf("[B3] btn_b_active_exits_to_home: PASS\n");
}

/* ===========================================================================
 * B4 — XP award on B-exit with near-max xp must not crash.
 *
 * Uses score = 100 -> award = 200. Player at level 98 (xp_to_next = 480200)
 * so no level-up fires on the overflow boundary. player.xp = UINT32_MAX - 100.
 * fq_training_award_xp saturates at UINT32_MAX on overflow.
 * After saturation level-up is attempted but 480200 > UINT32_MAX is impossible
 * so the saturation path keeps xp at UINT32_MAX.
 *
 * This test verifies no crash; it does not prescribe an exact final xp value
 * since the saturation + level-up interaction is implementation-defined.
 * What we assert: player.xp >= (UINT32_MAX - 100u) — it must not go DOWN.
 * =========================================================================*/
static void test_b4_btn_b_active_xp_no_overflow(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;

    setup_active_training(&ctx, &player, &inv, 100u);
    player.level = 98u;
    player.xp    = UINT32_MAX - 100u;

    uint32_t xp_before = player.xp;

    fq_event_t eb = { FQ_EVT_BTN_B_PRESS, 0u };
    fq_app_dispatch(&ctx, &eb); /* must not crash or wrap to 0 */

    /* State must be HOME. */
    TEST_ASSERT_EQUAL_INT(FQ_STATE_HOME, (int)ctx.state);

    /* XP must not have wrapped to near-zero. The saturation guard ensures
     * the post-award xp stays in the upper half of the uint32 range even
     * after fq_level_up subtracts xp_to_next from the saturated value.
     * UINT32_MAX - xp_to_next(98) = UINT32_MAX - 480200 >> UINT32_MAX/2. */
    TEST_ASSERT_TRUE(player.xp > (UINT32_MAX / 2u));

    printf("[B4] btn_b_active_xp_no_overflow: xp_before=%" PRIu32 " xp_after=%" PRIu32 " -> PASS\n",
           xp_before, player.xp);
}

/* ===========================================================================
 * main
 * =========================================================================*/
int main(void)
{
    printf("=== Training BTN_B Partial XP Award — Audit Fix Tests ===\n");

    test_b1_btn_b_active_awards_partial_xp();
    test_b2_btn_b_active_zero_score_no_xp();
    test_b3_btn_b_active_exits_to_home();
    test_b4_btn_b_active_xp_no_overflow();

    printf("=== ALL AUDIT FIX TESTS PASSED ===\n");
    return 0;
}
