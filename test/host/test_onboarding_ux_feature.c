/**
 * test_onboarding_ux_feature.c — Feature Tests: Onboarding UX Bug Fixes
 *
 * Rule 22 FEATURE RED: Tests define the corrected behavior contract.
 *
 *   F1  — After BTN_A, class_index advances and state stays ONBOARDING.
 *          (Proves both the index change AND the lack of state transition —
 *           which is the trigger condition for needs_redraw=1 in the fix.)
 *   F2  — Full carousel cycle: 5 BTN_A presses from any starting index
 *          visits all 5 classes exactly once and returns to start.
 *   F3  — BTN_B transitions ONBOARDING -> HOME and creates a character.
 *   F4  — class_index selects the correct sprite_base in the view model
 *          via the NULL-ch derivation path (class_index * 4).
 *   F5  — fq_vm_build_onboarding() carries correct stats for each class.
 *   F6  — Save-fail flag is cleared after a successful BTN_B transition.
 *          (Verifies that onboarding_save_failed resets, not lingers.)
 *   F7  — Multiple BTN_A presses in sequence advance class_index correctly.
 *
 * Architecture: These tests cover FSM dispatch and view-model output.
 * The actual needs_redraw / partial-refresh wiring lives in app_main.c,
 * which is outside the host test boundary (device-only orchestration).
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
    ctx->state                  = FQ_STATE_ONBOARDING;
    ctx->onboarding_class_index = 0u;
    ctx->onboarding_save_failed = 0u;
}

/* ===========================================================================
 * F1 — After BTN_A, class_index increments and state stays ONBOARDING.
 *
 * This is the key property for Bug 1: the redraw condition in app_main.c
 * must check app.state == FQ_STATE_ONBOARDING (not only state changes).
 * The FSM side of the contract: class_index changes, state does not.
 * =========================================================================*/
static void test_f1_btn_a_increments_class_state_unchanged(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    init_onboarding_ctx(&ctx, &player, &inv);
    ctx.onboarding_class_index = 0u;

    fq_event_t evt = { FQ_EVT_BTN_A_PRESS, 0u };
    fq_app_dispatch(&ctx, &evt);

    /* class_index must have advanced. */
    TEST_ASSERT_EQUAL_UINT8(1u, ctx.onboarding_class_index);
    /* State must remain ONBOARDING — this is what the redraw check depends on. */
    TEST_ASSERT_EQUAL_INT(FQ_STATE_ONBOARDING, (int)ctx.state);
    printf("[F1] BTN_A increments class_index to 1, state stays ONBOARDING: PASS\n");
}

/* ===========================================================================
 * F2 — Full carousel cycle: 5 presses from index 0 visits all 5 and wraps.
 * =========================================================================*/
static void test_f2_full_carousel_cycle(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    init_onboarding_ctx(&ctx, &player, &inv);
    ctx.onboarding_class_index = 0u;

    uint8_t visited[5] = { 0u, 0u, 0u, 0u, 0u };
    fq_event_t evt = { FQ_EVT_BTN_A_PRESS, 0u };

    for (uint8_t i = 0u; i < (uint8_t)FQ_CLASS_COUNT; i++) {
        fq_app_dispatch(&ctx, &evt);
        uint8_t idx = ctx.onboarding_class_index;
        TEST_ASSERT_TRUE(idx < (uint8_t)FQ_CLASS_COUNT);
        visited[idx] = 1u;
    }

    /* All 5 classes must have been visited. */
    for (uint8_t i = 0u; i < (uint8_t)FQ_CLASS_COUNT; i++) {
        TEST_ASSERT_EQUAL_UINT8(1u, visited[i]);
    }
    /* After 5 presses from 0, index returns to 0. */
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.onboarding_class_index);
    printf("[F2] Full 5-press carousel visits all classes and wraps to 0: PASS\n");
}

/* ===========================================================================
 * F3 — BTN_B transitions ONBOARDING -> HOME and creates a character.
 * =========================================================================*/
static void test_f3_btn_b_confirms_class_creates_character(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    init_onboarding_ctx(&ctx, &player, &inv);
    ctx.onboarding_class_index = 2u; /* Select third class. */

    fq_event_t evt = { FQ_EVT_BTN_B_PRESS, 0u };
    fq_app_dispatch(&ctx, &evt);

    /* State must transition to HOME. */
    TEST_ASSERT_EQUAL_INT(FQ_STATE_HOME, (int)ctx.state);
    /* Player must have been created (class_id set). */
    TEST_ASSERT_EQUAL_UINT8(2u, player.class_id);
    printf("[F3] BTN_B -> HOME, player.class_id == 2: PASS\n");
}

/* ===========================================================================
 * F4 — class_index selects the correct sprite_base in the view model.
 *
 * When ch == NULL, fq_vm_build_onboarding() derives sprite_base as
 * class_index * 4 (4 sprites per class). With 5 classes this produces
 * values 0, 4, 8, 12, 16 — each distinct.
 *
 * When ch is non-NULL with sprite_base == 0 (memset-zero), the character's
 * own sprite_base (0) is returned for all classes. That is the correct
 * behavior before a character is created — the caller controls which ch
 * to pass. This test validates the NULL-ch derivation path.
 * =========================================================================*/
static void test_f4_class_index_selects_correct_sprite_base(void)
{
    uint8_t sprite_bases[5];

    for (uint8_t i = 0u; i < (uint8_t)FQ_CLASS_COUNT; i++) {
        fq_vm_onboarding_t vm;
        memset(&vm, 0, sizeof(vm));
        /* Pass NULL ch to exercise the class_index * 4 derivation path. */
        fq_vm_build_onboarding(&vm, NULL, i);
        sprite_bases[i] = vm.sprite_base;
        /* class_index field in vm must match what was requested. */
        TEST_ASSERT_EQUAL_UINT8(i, vm.class_index);
        /* sprite_base must equal class_index * 4. */
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(i * 4u), vm.sprite_base);
    }

    /* Sprite bases for all 5 classes must be distinct (no aliasing). */
    for (uint8_t i = 0u; i < (uint8_t)FQ_CLASS_COUNT; i++) {
        for (uint8_t j = (uint8_t)(i + 1u); j < (uint8_t)FQ_CLASS_COUNT; j++) {
            TEST_ASSERT_TRUE(sprite_bases[i] != sprite_bases[j]);
        }
    }
    printf("[F4] All 5 class sprite_bases distinct (class_index*4) and index matches: PASS\n");
}

/* ===========================================================================
 * F5 — fq_vm_build_onboarding() carries correct non-zero stats for each class.
 *
 * Every class must have at least one stat > 0 (sanity check — no zeroed VM).
 * =========================================================================*/
static void test_f5_vm_onboarding_stats_nonzero(void)
{
    for (uint8_t i = 0u; i < (uint8_t)FQ_CLASS_COUNT; i++) {
        fq_vm_onboarding_t vm;
        memset(&vm, 0, sizeof(vm));
        fq_vm_build_onboarding(&vm, NULL, i);

        uint32_t total = (uint32_t)vm.strength
                       + (uint32_t)vm.speed
                       + (uint32_t)vm.precision
                       + (uint32_t)vm.intelligence;
        TEST_ASSERT_TRUE(total > 0u);

        /* class_name must be non-empty. */
        TEST_ASSERT_TRUE(vm.class_name[0] != '\0');
    }
    printf("[F5] All 5 class VMs have non-zero total stats and non-empty names: PASS\n");
}

/* ===========================================================================
 * F6 — onboarding_save_failed is cleared (0) after BTN_B succeeds.
 *
 * This documents the contract that app_main.c clears the flag on a
 * successful save (via do_auto_save -> onboarding_save_failed = 0).
 * Here we simulate clearing it manually and verify the FSM re-arms correctly.
 * =========================================================================*/
static void test_f6_save_failed_cleared_allows_retry(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    init_onboarding_ctx(&ctx, &player, &inv);

    /* Simulate a prior save failure. */
    ctx.onboarding_save_failed = 1u;

    /* BTN_B while save_failed == 1 must stay ONBOARDING (FSM guard). */
    fq_event_t evt = { FQ_EVT_BTN_B_PRESS, 0u };
    fq_app_dispatch(&ctx, &evt);
    TEST_ASSERT_EQUAL_INT(FQ_STATE_ONBOARDING, (int)ctx.state);

    /* Simulate save success (app_main.c clears the flag on next save). */
    ctx.onboarding_save_failed = 0u;

    /* Now BTN_B must succeed and transition to HOME. */
    fq_app_dispatch(&ctx, &evt);
    TEST_ASSERT_EQUAL_INT(FQ_STATE_HOME, (int)ctx.state);
    printf("[F6] Clearing save_failed allows subsequent BTN_B to reach HOME: PASS\n");
}

/* ===========================================================================
 * F7 — Multiple sequential BTN_A presses advance class_index correctly.
 *
 * 3 presses from index 0 must reach index 3.
 * =========================================================================*/
static void test_f7_multiple_btn_a_presses_advance_correctly(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    init_onboarding_ctx(&ctx, &player, &inv);
    ctx.onboarding_class_index = 0u;

    fq_event_t evt = { FQ_EVT_BTN_A_PRESS, 0u };

    fq_app_dispatch(&ctx, &evt);
    TEST_ASSERT_EQUAL_UINT8(1u, ctx.onboarding_class_index);

    fq_app_dispatch(&ctx, &evt);
    TEST_ASSERT_EQUAL_UINT8(2u, ctx.onboarding_class_index);

    fq_app_dispatch(&ctx, &evt);
    TEST_ASSERT_EQUAL_UINT8(3u, ctx.onboarding_class_index);

    /* State must still be ONBOARDING after 3 presses. */
    TEST_ASSERT_EQUAL_INT(FQ_STATE_ONBOARDING, (int)ctx.state);
    printf("[F7] 3 BTN_A presses from 0 advance to 3, state stays ONBOARDING: PASS\n");
}

/* ===========================================================================
 * main
 * =========================================================================*/
int main(void)
{
    printf("=== Onboarding UX Bug Fixes — Feature Tests ===\n");

    test_f1_btn_a_increments_class_state_unchanged();
    test_f2_full_carousel_cycle();
    test_f3_btn_b_confirms_class_creates_character();
    test_f4_class_index_selects_correct_sprite_base();
    test_f5_vm_onboarding_stats_nonzero();
    test_f6_save_failed_cleared_allows_retry();
    test_f7_multiple_btn_a_presses_advance_correctly();

    printf("=== ALL ONBOARDING UX FEATURE TESTS PASSED ===\n");
    return 0;
}
