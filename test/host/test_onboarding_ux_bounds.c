/**
 * test_onboarding_ux_bounds.c — Bound Tests: Onboarding UX Bug Fixes
 *
 * Rule 22 BOUND RED: These tests MUST fail before the GREEN implementation.
 * They prove the system REJECTS the incorrect behaviors that were found on
 * hardware:
 *
 *   BUG 1 — BTN_A in ONBOARDING must not change FSM state.
 *   BUG 2 — Save-fail must NOT force state back to ONBOARDING from HOME.
 *   PARTIAL — fq_vm_onboarding_t size must not have grown (no new fields needed).
 *
 * Architecture: FSM dispatch is stateless (pure function over ctx + event).
 * Tests use add_app_test linkage (game + presentation + fsm).
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "test_assert.h"
#include "types.h"
#include "event_bus.h"
#include "app_fsm.h"
#include "view_models.h"
#include "vm_builder.h"
#include "character.h"

/* ===========================================================================
 * Helpers
 * =========================================================================*/

static void init_onboarding_ctx(fq_app_ctx_t  *ctx,
                                 fq_character_t *player,
                                 fq_inventory_t *inv)
{
    memset(player, 0, sizeof(*player));
    memset(inv,    0, sizeof(*inv));
    fq_app_init(ctx, player, inv);
    ctx->state                 = FQ_STATE_ONBOARDING;
    ctx->onboarding_class_index = 0u;
    ctx->onboarding_save_failed = 0u;
}

/* ===========================================================================
 * BOUND 1 — BTN_A in ONBOARDING must keep state == FQ_STATE_ONBOARDING.
 *
 * Verifies: dispatch does NOT change state on a class-cycle event.
 * This bound is necessary because the redraw path checks
 *   app.state != last_state  (false → no redraw WITHOUT the fix).
 * =========================================================================*/
static void test_bound_btn_a_onboarding_state_unchanged(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    init_onboarding_ctx(&ctx, &player, &inv);

    fq_event_t evt = { FQ_EVT_BTN_A_PRESS, 0u };
    fq_app_dispatch(&ctx, &evt);

    /* State must remain ONBOARDING — only class_index changes. */
    TEST_ASSERT_EQUAL_INT(FQ_STATE_ONBOARDING, (int)ctx.state);
    printf("[BOUND 1] BTN_A in ONBOARDING keeps state == ONBOARDING: PASS\n");
}

/* ===========================================================================
 * BOUND 2 — BTN_A cycles class_index, never exceeds FQ_CLASS_COUNT - 1.
 *
 * Drive BTN_A from index 0 for FQ_CLASS_COUNT iterations.
 * Each result must be in [0, FQ_CLASS_COUNT-1].
 * After FQ_CLASS_COUNT presses, index must return to 0 (wrap).
 * =========================================================================*/
static void test_bound_btn_a_class_index_stays_in_range(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    init_onboarding_ctx(&ctx, &player, &inv);
    ctx.onboarding_class_index = 0u;

    fq_event_t evt = { FQ_EVT_BTN_A_PRESS, 0u };

    for (uint8_t i = 0u; i < (uint8_t)FQ_CLASS_COUNT; i++) {
        fq_app_dispatch(&ctx, &evt);
        /* Each index after dispatch must be within valid range. */
        TEST_ASSERT_TRUE(ctx.onboarding_class_index < (uint8_t)FQ_CLASS_COUNT);
    }
    /* After FQ_CLASS_COUNT presses from 0: must wrap back to 0. */
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.onboarding_class_index);
    printf("[BOUND 2] BTN_A class_index always in [0, FQ_CLASS_COUNT-1] and wraps: PASS\n");
}

/* ===========================================================================
 * BOUND 3 — Save-fail flag must NOT prevent transition to HOME on fresh press.
 *
 * onboarding_save_failed starts at 0 (fresh first boot).
 * BTN_B must transition ONBOARDING → HOME.
 * The state rollback (bug 2) belongs ONLY in app_main.c, NOT in FSM.
 * The FSM guard only fires when onboarding_save_failed == 1.
 *
 * This bound proves: with onboarding_save_failed == 0, BTN_B → HOME.
 * =========================================================================*/
static void test_bound_btn_b_onboarding_fresh_goes_home(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    init_onboarding_ctx(&ctx, &player, &inv);
    ctx.onboarding_save_failed = 0u; /* fresh: no prior failure */

    fq_event_t evt = { FQ_EVT_BTN_B_PRESS, 0u };
    fq_app_dispatch(&ctx, &evt);

    /* FSM must transition to HOME on a clean first press. */
    TEST_ASSERT_EQUAL_INT(FQ_STATE_HOME, (int)ctx.state);
    printf("[BOUND 3] BTN_B in ONBOARDING (save_failed=0) → HOME: PASS\n");
}

/* ===========================================================================
 * BOUND 4 — With save_failed == 1, BTN_B must NOT transition to HOME.
 *
 * The FSM guard (app_fsm.c line 535) must block the transition.
 * This proves the FSM correctly gates the confirmed-save-failed path.
 * =========================================================================*/
static void test_bound_btn_b_onboarding_save_failed_stays(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    init_onboarding_ctx(&ctx, &player, &inv);
    ctx.onboarding_save_failed = 1u; /* simulated prior save failure */

    fq_event_t evt = { FQ_EVT_BTN_B_PRESS, 0u };
    fq_app_dispatch(&ctx, &evt);

    /* FSM guard must block the transition: state stays ONBOARDING. */
    TEST_ASSERT_EQUAL_INT(FQ_STATE_ONBOARDING, (int)ctx.state);
    printf("[BOUND 4] BTN_B in ONBOARDING (save_failed=1) stays ONBOARDING: PASS\n");
}

/* ===========================================================================
 * BOUND 5 — BTN_A at maximum class index (FQ_CLASS_COUNT-1) must wrap to 0.
 *
 * Integer overflow guard: (4 + 1) % 5 == 0.
 * A modulo with a non-power-of-two divisor is used — this must not overflow.
 * =========================================================================*/
static void test_bound_btn_a_max_index_wrap(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    init_onboarding_ctx(&ctx, &player, &inv);
    ctx.onboarding_class_index = (uint8_t)(FQ_CLASS_COUNT - 1u);

    fq_event_t evt = { FQ_EVT_BTN_A_PRESS, 0u };
    fq_app_dispatch(&ctx, &evt);

    TEST_ASSERT_EQUAL_UINT8(0u, ctx.onboarding_class_index);
    printf("[BOUND 5] BTN_A at max index wraps to 0: PASS\n");
}

/* ===========================================================================
 * BOUND 6 — fq_vm_onboarding_t must remain exactly 24 bytes.
 *
 * The partial refresh path for onboarding reuses the same view model.
 * No new fields are required — only a new render path in app_main.c.
 * Pinning the size prevents silent struct bloat.
 * =========================================================================*/
static void test_bound_vm_onboarding_size_unchanged(void)
{
    TEST_ASSERT_EQUAL_UINT32(24u, (uint32_t)sizeof(fq_vm_onboarding_t));
    printf("[BOUND 6] fq_vm_onboarding_t size == 24 bytes: PASS\n");
}

/* ===========================================================================
 * BOUND 7 — TIMER_TICK in ONBOARDING must not change FSM state.
 *
 * TIMER_TICK is dispatched every main-loop iteration.
 * In ONBOARDING the FSM must silently ignore it (no state change, no class
 * index change). This ensures the redraw-on-class-cycle logic is safe to
 * trigger on every TIMER_TICK without corrupting state.
 * =========================================================================*/
static void test_bound_timer_tick_onboarding_state_unchanged(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    init_onboarding_ctx(&ctx, &player, &inv);
    ctx.onboarding_class_index = 2u;

    fq_event_t evt = { FQ_EVT_TIMER_TICK, 0u };
    fq_app_dispatch(&ctx, &evt);

    TEST_ASSERT_EQUAL_INT(FQ_STATE_ONBOARDING, (int)ctx.state);
    TEST_ASSERT_EQUAL_UINT8(2u, ctx.onboarding_class_index);
    printf("[BOUND 7] TIMER_TICK in ONBOARDING does not change state or index: PASS\n");
}

/* ===========================================================================
 * main
 * =========================================================================*/
int main(void)
{
    printf("=== Onboarding UX Bug Fixes — Bound Tests ===\n");

    test_bound_btn_a_onboarding_state_unchanged();
    test_bound_btn_a_class_index_stays_in_range();
    test_bound_btn_b_onboarding_fresh_goes_home();
    test_bound_btn_b_onboarding_save_failed_stays();
    test_bound_btn_a_max_index_wrap();
    test_bound_vm_onboarding_size_unchanged();
    test_bound_timer_tick_onboarding_state_unchanged();

    printf("=== ALL ONBOARDING UX BOUND TESTS PASSED ===\n");
    return 0;
}
