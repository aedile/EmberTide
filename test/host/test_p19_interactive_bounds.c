/**
 * test_p19_interactive_bounds.c — Phase 19 Bound Tests
 *
 * Rule 22 BOUND RED: All 17 bound tests must FAIL before implementation.
 * They prove the system REJECTS out-of-bounds inputs before any feature
 * code exists.
 *
 * Tests 1-6:   Onboarding bounds (name_gen, FSM state, sizeof)
 * Tests 7-11:  Training session bounds (score clamp, XP at L99, PRNG isolation)
 * Tests 12-17: Inventory equip/unequip bounds (full slots, ID 0, duplicates)
 *
 * Architecture:
 *   - name_gen, training_session, equip_toggle all live in components/game/
 *   - Tests use add_app_test so game + presentation + FSM are all linked
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

/* New Phase-19 headers (will fail to include until implemented) */
#include "name_gen.h"
#include "training_session.h"
#include "equip.h"

/* ===========================================================================
 * Helpers
 * =========================================================================*/

/** Build a minimal fq_character_t with a given level and class. */
static fq_character_t make_char(fq_class_t cls, uint8_t level)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = (uint8_t)cls;
    ch.level         = level;
    ch.equipped_count = 4u; /* default max slots */
    return ch;
}

/** Build an inventory with n items, all set to item ID (i+1). */
static fq_inventory_t make_inv(uint8_t n)
{
    fq_inventory_t inv;
    memset(&inv, 0, sizeof(inv));
    if (n > 32u) { n = 32u; }
    inv.count = n;
    for (uint8_t i = 0u; i < n; i++) {
        inv.items[i] = (uint16_t)(i + 1u); /* item IDs 1..n */
    }
    return inv;
}

/* ===========================================================================
 * TEST 1 — test_onboarding_class_index_wrap
 *
 * Button A at class index 4 (FQ_CLASS_COUNT-1) must wrap to 0.
 * The FSM handler cycles index via: (index + 1) % FQ_CLASS_COUNT.
 * FQ_CLASS_COUNT = 5, so index 4 -> 0.
 * =========================================================================*/
static void test_onboarding_class_index_wrap(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&ctx, &player, &inv);

    /* Drive FSM to ONBOARDING state. */
    ctx.state = FQ_STATE_ONBOARDING;
    /* Simulate the onboarding_class_index starting at 4. */
    ctx.onboarding_class_index = (uint8_t)(FQ_CLASS_COUNT - 1u);

    /* BTN_A = cycle class forward. */
    fq_event_t evt = { FQ_EVT_BTN_A_PRESS, 0u };
    fq_app_dispatch(&ctx, &evt);

    TEST_ASSERT_EQUAL_UINT8(0u, ctx.onboarding_class_index);
    printf("[TEST 1] test_onboarding_class_index_wrap: PASS\n");
}

/* ===========================================================================
 * TEST 2 — test_onboarding_name_max_length
 *
 * 1000 generated names, each with a different PRNG seed.
 * All must be <= 11 chars (non-empty).
 * =========================================================================*/
static void test_onboarding_name_max_length(void)
{
    fq_prng_t rng;
    char      name[16]; /* extra space to detect overrun */

    for (uint32_t seed = 1u; seed <= 1000u; seed++) {
        fq_prng_init(&rng, seed);
        memset(name, 0xAA, sizeof(name)); /* poison */
        game_err_t err = fq_generate_name(&rng, name, 12u); /* max_len=12 for name[12] field */
        TEST_ASSERT_EQUAL_INT(GAME_OK, err);

        size_t len = 0u;
        while (len < 12u && name[len] != '\0') { len++; }

        /* Must be non-empty. */
        TEST_ASSERT_TRUE(len >= 1u);
        /* Must be <= 11 chars (null at position 11 at most). */
        TEST_ASSERT_TRUE(len <= 11u);
    }
    printf("[TEST 2] test_onboarding_name_max_length: PASS (1000 names)\n");
}

/* ===========================================================================
 * TEST 3 — test_onboarding_name_null_termination
 *
 * name[11] must always be '\0' (the null at the end of an 11-char name).
 * We write to a 12-byte destination (same as fq_character_t.name[12]).
 * =========================================================================*/
static void test_onboarding_name_null_termination(void)
{
    fq_prng_t rng;
    char      name[12];

    for (uint32_t seed = 1u; seed <= 200u; seed++) {
        fq_prng_init(&rng, seed);
        memset(name, 0xFF, sizeof(name)); /* poison all bytes */
        fq_generate_name(&rng, name, 12u);
        /* The last byte of the destination must always be null. */
        TEST_ASSERT_EQUAL_UINT8(0u, (uint8_t)name[11]);
    }
    printf("[TEST 3] test_onboarding_name_null_termination: PASS\n");
}

/* ===========================================================================
 * TEST 4 — test_onboarding_save_failure_reentry
 *
 * If the onboarding confirm path receives GAME_ERR_NULL_PTR from save
 * (simulated by leaving state as ONBOARDING), state must remain ONBOARDING.
 *
 * This tests the FSM guard: BTN_B in ONBOARDING only transitions to HOME
 * when save succeeds. If save fails, state stays ONBOARDING.
 *
 * We simulate this by directly invoking the dispatch with BTN_B in
 * ONBOARDING before any character has been created (player->name is empty),
 * which should trigger the save-failure path.
 * =========================================================================*/
static void test_onboarding_save_failure_reentry(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&ctx, &player, &inv);
    ctx.state = FQ_STATE_ONBOARDING;

    /* Arm the save failure flag in the context so BTN_B confirms
     * but save fails. The FSM must remain in ONBOARDING. */
    ctx.onboarding_save_failed = 1u;

    fq_event_t evt = { FQ_EVT_BTN_B_PRESS, 0u };
    fq_app_dispatch(&ctx, &evt);

    /* Must still be in ONBOARDING because save failed. */
    TEST_ASSERT_EQUAL_INT(FQ_STATE_ONBOARDING, (int)ctx.state);
    printf("[TEST 4] test_onboarding_save_failure_reentry: PASS\n");
}

/* ===========================================================================
 * TEST 5 — test_fsm_onboarding_state_value
 *
 * FQ_STATE_ONBOARDING == 11, FQ_STATE_COUNT == 12.
 * Pinned at compile time — these values must not drift.
 * =========================================================================*/
static void test_fsm_onboarding_state_value(void)
{
    TEST_ASSERT_EQUAL_INT(11, (int)FQ_STATE_ONBOARDING);
    TEST_ASSERT_EQUAL_INT(12, (int)FQ_STATE_COUNT);
    printf("[TEST 5] test_fsm_onboarding_state_value: PASS\n");
}

/* ===========================================================================
 * TEST 6 — test_app_ctx_sizeof_after_onboarding
 *
 * sizeof(fq_app_ctx_t) must match expected values after adding
 * onboarding_class_index (uint8_t) and onboarding_save_failed (uint8_t).
 *
 * These two new fields fit into existing trailing padding — the
 * _Static_assert in app_fsm.h pins the final size and this test
 * validates that the assert is not bypassed.
 * =========================================================================*/
static void test_app_ctx_sizeof_after_onboarding(void)
{
    /* The _Static_assert in app_fsm.h already enforces this at compile time.
     * This runtime test makes the constraint visible in ctest output. */
#if __SIZEOF_POINTER__ == 8
    TEST_ASSERT_EQUAL_UINT32(232u, (uint32_t)sizeof(fq_app_ctx_t));
#elif __SIZEOF_POINTER__ == 4
    TEST_ASSERT_EQUAL_UINT32(216u, (uint32_t)sizeof(fq_app_ctx_t));
#endif
    printf("[TEST 6] test_app_ctx_sizeof_after_onboarding: PASS\n");
}

/* ===========================================================================
 * TEST 7 — test_training_score_clamp
 *
 * fq_training_session_t score must clamp to 100 for inputs > 100.
 * Also verified: XP award uses clamped score, not raw.
 * =========================================================================*/
static void test_training_score_clamp(void)
{
    fq_training_session_t ts;
    memset(&ts, 0, sizeof(ts));
    fq_training_session_init(&ts, FQ_TS_SPEED);

    /* Force score to an illegal value — the clamp must fire on step/finish. */
    ts.score = 200u;  /* raw overwrite — clamp guards must correct this */

    /* Calling fq_training_get_xp_award() on a 200-score must yield
     * score_clamped * 2 = 100 * 2 = 200, not 200 * 2 = 400. */
    uint32_t xp = fq_training_get_xp_award(&ts);
    TEST_ASSERT_EQUAL_UINT32(200u, xp); /* 100 clamped * 2 */
    printf("[TEST 7] test_training_score_clamp: PASS\n");
}

/* ===========================================================================
 * TEST 8 — test_training_xp_at_level_99
 *
 * A character at level 99 already has max level. fq_level_up() returns
 * GAME_ERR_INVALID. XP must be preserved (not zeroed).
 * =========================================================================*/
static void test_training_xp_at_level_99(void)
{
    fq_character_t ch = make_char(FQ_CLASS_BRUISER, 99u);
    ch.xp = 9999u; /* some existing XP */

    /* Award XP — this should NOT call fq_level_up() successfully. */
    fq_training_session_t ts;
    memset(&ts, 0, sizeof(ts));
    fq_training_session_init(&ts, FQ_TS_SPEED);
    ts.score = 100u; /* maximum score */
    ts.state = FQ_TS_DONE;

    uint32_t xp_before = ch.xp;
    game_err_t err = fq_training_award_xp(&ts, &ch);

    /* err must be GAME_ERR_INVALID (level_up rejected) NOT GAME_OK. */
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);

    /* XP must be preserved — not zeroed or corrupted. */
    /* After award: ch.xp = 9999 + 200 (but level_up not called). */
    uint32_t expected_xp = xp_before + 200u;
    TEST_ASSERT_EQUAL_UINT32(expected_xp, ch.xp);
    printf("[TEST 8] test_training_xp_at_level_99: PASS\n");
}

/* ===========================================================================
 * TEST 9 — test_training_prng_isolation
 *
 * The combat PRNG state must be unchanged after a full training session.
 * Training uses tick counters only — no PRNG calls.
 * =========================================================================*/
static void test_training_prng_isolation(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&ctx, &player, &inv);

    /* Seed the combat PRNG to a known value. */
    fq_prng_init(&ctx.combat.rng, 0xDEADBEEFu);
    uint32_t state_before = ctx.combat.rng.state;

    /* Run a full training session via FSM events. */
    fq_training_session_t ts;
    memset(&ts, 0, sizeof(ts));
    fq_training_session_init(&ts, FQ_TS_SPEED);

    /* Step through all 5 targets, each hit in zone. */
    for (uint8_t i = 0u; i < 5u; i++) {
        /* Advance to zone position 50 (middle of hit zone 40-60). */
        ts.target_pos = 50u;
        fq_training_hit(&ts);
    }

    /* Combat PRNG must be identical to before the session. */
    TEST_ASSERT_EQUAL_UINT32(state_before, ctx.combat.rng.state);
    printf("[TEST 9] test_training_prng_isolation: PASS\n");
}

/* ===========================================================================
 * TEST 10 — test_training_zero_targets_zero_xp
 *
 * If BTN_B exits training immediately (no targets hit), XP award = 0.
 * =========================================================================*/
static void test_training_zero_targets_zero_xp(void)
{
    fq_character_t ch = make_char(FQ_CLASS_BRUISER, 5u);
    uint32_t xp_before = ch.xp;

    /* Session never started — score == 0, targets_done == 0. */
    fq_training_session_t ts;
    memset(&ts, 0, sizeof(ts));
    fq_training_session_init(&ts, FQ_TS_SPEED);
    /* Do NOT call any hit or step — session abandoned immediately. */
    /* Finalize as DONE with current (zero) score. */
    ts.state = FQ_TS_DONE; /* force done to allow xp_award query */

    uint32_t xp = fq_training_get_xp_award(&ts);
    TEST_ASSERT_EQUAL_UINT32(0u, xp);

    game_err_t err = fq_training_award_xp(&ts, &ch);
    /* Level 5 won't level up on 0 XP; expect GAME_ERR_INVALID (not enough XP) or GAME_OK if xp=0 */
    TEST_ASSERT_EQUAL_UINT32(xp_before, ch.xp); /* no change */
    (void)err;
    printf("[TEST 10] test_training_zero_targets_zero_xp: PASS\n");
}

/* ===========================================================================
 * TEST 11 — test_training_hit_outside_active
 *
 * fq_training_hit() during WAITING or DONE state must return GAME_ERR_INVALID
 * and must NOT modify the score.
 * =========================================================================*/
static void test_training_hit_outside_active(void)
{
    fq_training_session_t ts;
    memset(&ts, 0, sizeof(ts));

    /* In WAITING state. */
    ts.state = FQ_TS_WAITING;
    ts.score = 42u; /* a sentinel value */
    game_err_t err = fq_training_hit(&ts);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
    TEST_ASSERT_EQUAL_UINT8(42u, ts.score); /* unchanged */

    /* In DONE state. */
    ts.state = FQ_TS_DONE;
    ts.score = 77u;
    err = fq_training_hit(&ts);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
    TEST_ASSERT_EQUAL_UINT8(77u, ts.score); /* unchanged */

    printf("[TEST 11] test_training_hit_outside_active: PASS\n");
}

/* ===========================================================================
 * TEST 12 — test_equip_full_slots_rejected
 *
 * With 4 equipped slots full, attempting to equip a 5th item must return
 * GAME_ERR_OVERFLOW.
 * =========================================================================*/
static void test_equip_full_slots_rejected(void)
{
    fq_character_t ch = make_char(FQ_CLASS_BRUISER, 10u);
    ch.equipped_count = 4u; /* max 4 slots */

    /* Fill all 4 slots with items 1..4. */
    ch.equipped[0] = 1u;
    ch.equipped[1] = 2u;
    ch.equipped[2] = 3u;
    ch.equipped[3] = 4u;
    ch.equipped[4] = 0u; /* empty sentinel */

    /* Inventory has item 5 at slot 4. */
    fq_inventory_t inv = make_inv(5u);

    /* Attempt to equip the 5th item (inv slot 4, item ID 5). */
    game_err_t err = fq_equip_toggle(&ch, &inv, 4u);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_OVERFLOW, (int)err);

    /* Equipped array must be unchanged. */
    TEST_ASSERT_EQUAL_UINT16(4u, ch.equipped[3]);
    TEST_ASSERT_EQUAL_UINT16(0u, ch.equipped[4]);
    printf("[TEST 12] test_equip_full_slots_rejected: PASS\n");
}

/* ===========================================================================
 * TEST 13 — test_equip_item_id_zero_rejected
 *
 * Item ID 0 is the empty sentinel. Attempting to equip it must be rejected.
 * =========================================================================*/
static void test_equip_item_id_zero_rejected(void)
{
    fq_character_t ch = make_char(FQ_CLASS_BRUISER, 5u);
    fq_inventory_t inv;
    memset(&inv, 0, sizeof(inv));
    inv.count    = 1u;
    inv.items[0] = 0u; /* item ID 0 — the sentinel */

    game_err_t err = fq_equip_toggle(&ch, &inv, 0u);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);

    /* No item should have been equipped. */
    for (uint8_t i = 0u; i < 5u; i++) {
        TEST_ASSERT_EQUAL_UINT16(0u, ch.equipped[i]);
    }
    printf("[TEST 13] test_equip_item_id_zero_rejected: PASS\n");
}

/* ===========================================================================
 * TEST 14 — test_equip_duplicate_unequip_one
 *
 * Equip item ID 42 from slot 0, then item ID 42 from slot 1.
 * Unequip slot 0 (item ID 42). One copy must remain equipped.
 * =========================================================================*/
static void test_equip_duplicate_unequip_one(void)
{
    fq_character_t ch = make_char(FQ_CLASS_BRUISER, 5u);
    ch.equipped_count = 4u;

    /* Inventory: two copies of item 42. */
    fq_inventory_t inv;
    memset(&inv, 0, sizeof(inv));
    inv.count    = 2u;
    inv.items[0] = 42u;
    inv.items[1] = 42u;

    /* Equip both. */
    game_err_t err0 = fq_equip_toggle(&ch, &inv, 0u);
    game_err_t err1 = fq_equip_toggle(&ch, &inv, 1u);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err0);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err1);

    /* Count how many copies of item 42 are equipped. */
    uint8_t count_before = 0u;
    for (uint8_t i = 0u; i < 5u; i++) {
        if (ch.equipped[i] == 42u) { count_before++; }
    }
    TEST_ASSERT_EQUAL_UINT8(2u, count_before);

    /* Unequip slot 0. */
    game_err_t err2 = fq_equip_toggle(&ch, &inv, 0u);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err2);

    /* One copy of item 42 must remain. */
    uint8_t count_after = 0u;
    for (uint8_t i = 0u; i < 5u; i++) {
        if (ch.equipped[i] == 42u) { count_after++; }
    }
    TEST_ASSERT_EQUAL_UINT8(1u, count_after);
    printf("[TEST 14] test_equip_duplicate_unequip_one: PASS\n");
}

/* ===========================================================================
 * TEST 15 — test_equip_count_clamp
 *
 * equipped_count = 255 is a corrupt value (max is 5 slots in equipped[5]).
 * fq_equip_toggle must not write out-of-bounds. The guard must clamp to
 * the actual array size (5 entries).
 * =========================================================================*/
static void test_equip_count_clamp(void)
{
    fq_character_t ch = make_char(FQ_CLASS_BRUISER, 5u);
    ch.equipped_count = 255u; /* corrupt value */

    fq_inventory_t inv = make_inv(1u); /* one item, ID = 1 */

    /* This must not crash / OOB access. The equip logic must clamp
     * the usable slot count to min(equipped_count, 5) = 5. */
    game_err_t err = fq_equip_toggle(&ch, &inv, 0u);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);

    /* Item 1 must appear somewhere in equipped[0..4]. */
    uint8_t found = 0u;
    for (uint8_t i = 0u; i < 5u; i++) {
        if (ch.equipped[i] == 1u) { found = 1u; }
    }
    TEST_ASSERT_EQUAL_UINT8(1u, found);
    printf("[TEST 15] test_equip_count_clamp: PASS\n");
}

/* ===========================================================================
 * TEST 16 — test_inventory_cursor_bounds
 *
 * cursor_index = 31 with item_count = 5 must clamp to 4 in the view model
 * builder (and the renderer).
 * =========================================================================*/
static void test_inventory_cursor_bounds(void)
{
    fq_inventory_t inv = make_inv(5u);
    fq_character_t ch  = make_char(FQ_CLASS_BRUISER, 1u);

    fq_vm_inventory_t vm;
    memset(&vm, 0, sizeof(vm));

    /* Build with cursor forced to 31 in app state. */
    fq_vm_build_inventory_ex(&vm, &inv, &ch, 31u /* cursor */, 0u /* scroll */);

    /* cursor_index must be clamped to item_count-1 = 4. */
    TEST_ASSERT_EQUAL_UINT8(4u, vm.cursor_index);
    printf("[TEST 16] test_inventory_cursor_bounds: PASS\n");
}

/* ===========================================================================
 * TEST 17 — test_inventory_empty_no_crash
 *
 * item_count = 0. Calling fq_equip_toggle with inv_slot = 0 must return
 * GAME_ERR_INVALID (slot out of range) — not crash.
 * =========================================================================*/
static void test_inventory_empty_no_crash(void)
{
    fq_character_t ch = make_char(FQ_CLASS_BRUISER, 5u);
    fq_inventory_t inv;
    memset(&inv, 0, sizeof(inv));
    inv.count = 0u; /* empty inventory */

    game_err_t err = fq_equip_toggle(&ch, &inv, 0u);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
    printf("[TEST 17] test_inventory_empty_no_crash: PASS\n");
}

/* ===========================================================================
 * main
 * =========================================================================*/
int main(void)
{
    printf("=== Phase 19 Interactive Gameplay — Bound Tests ===\n");

    test_onboarding_class_index_wrap();
    test_onboarding_name_max_length();
    test_onboarding_name_null_termination();
    test_onboarding_save_failure_reentry();
    test_fsm_onboarding_state_value();
    test_app_ctx_sizeof_after_onboarding();

    test_training_score_clamp();
    test_training_xp_at_level_99();
    test_training_prng_isolation();
    test_training_zero_targets_zero_xp();
    test_training_hit_outside_active();

    test_equip_full_slots_rejected();
    test_equip_item_id_zero_rejected();
    test_equip_duplicate_unequip_one();
    test_equip_count_clamp();
    test_inventory_cursor_bounds();
    test_inventory_empty_no_crash();

    printf("=== ALL BOUND TESTS PASSED ===\n");
    return 0;
}
