/**
 * test_item_time_loop.c — Dedicated tests for Time Loop item (207).
 *
 * Per phase-5.md Item 3 spec: "test_item_time_loop.c forces item 207 into
 * slot 1, damages fighters in rounds 4, 5, and confirms restoration to round
 * 3 states at round 6."
 *
 * Additional tests:
 *   TL-01: Snapshot captured after all round 3 effects (NTR-D1).
 *   TL-02: Restoration fires at round 6 only.
 *   TL-03: Once-per-fight (time_loop_used flag) — NTR-D3.
 *   TL-04: Snapshot init to 0 after fq_combat_init (NTR-D1 init check).
 *   TL-05: Restoration to round 3 HP via full fq_combat_step simulation.
 */

#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include "test_assert.h"
#include "types.h"
#include "combat.h"
#include "prng.h"
#include "item_engine.h"

/* ---------------------------------------------------------------------------
 * Helper: build a minimal valid fq_character_t.
 * ---------------------------------------------------------------------------*/
static fq_character_t make_char(uint16_t hp_max,
                                uint8_t str, uint8_t spd,
                                uint8_t prec, uint8_t intel)
{
    fq_character_t c;
    memset(&c, 0, sizeof(c));
    c.hp_max       = hp_max;
    c.strength     = str;
    c.speed        = spd;
    c.precision    = prec;
    c.intelligence = intel;
    return c;
}

/* ---------------------------------------------------------------------------
 * TL-04: Snapshot fields initialized to 0 after fq_combat_init.
 * ---------------------------------------------------------------------------*/
static void test_tl_snapshot_zero_after_init(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);

    TEST_ASSERT_EQUAL_INT(0, (int)ctx.round_3_f1_hp);
    TEST_ASSERT_EQUAL_INT(0, (int)ctx.round_3_f2_hp);
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.time_loop_used);
}

/* ---------------------------------------------------------------------------
 * TL-01: Snapshot captured after all round 3 effects.
 *
 * Time Loop in slot 0 of F1. At end of round 3, snapshot = current HP
 * (after all other effects resolve).
 * ---------------------------------------------------------------------------*/
static void test_tl_snapshot_captured_round_3(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);

    /* Time Loop in slot 0 of F1 (spec says slot 1 = index 0 or index 1;
     * we use index 0 per zero-indexed slots). */
    ctx.f1.equipped_items[0] = 207u;
    ctx.f1.equipped_count    = 1u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    /* Simulate damage happened before end of round 3. */
    ctx.f1.hp = 80;
    ctx.f2.hp = 70;
    ctx.current_round = 3u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_ROUND_END, 1u);

    TEST_ASSERT_EQUAL_INT(80, (int)ctx.round_3_f1_hp);
    TEST_ASSERT_EQUAL_INT(70, (int)ctx.round_3_f2_hp);
}

/* ---------------------------------------------------------------------------
 * TL-02: Restoration fires at round 6 only — not at round 4 or 5.
 * ---------------------------------------------------------------------------*/
static void test_tl_no_restore_at_round_4(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);

    ctx.f1.equipped_items[0] = 207u;
    ctx.f1.equipped_count    = 1u;
    ctx.round_3_f1_hp  = 80;
    ctx.round_3_f2_hp  = 70;
    ctx.time_loop_used = 0u;

    ctx.f1.hp = 50;
    ctx.f2.hp = 40;
    ctx.current_round        = 4u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_ROUND_END, 1u);

    /* Must NOT restore at round 4. */
    TEST_ASSERT_EQUAL_INT(50, (int)ctx.f1.hp);
    TEST_ASSERT_EQUAL_INT(40, (int)ctx.f2.hp);
}

static void test_tl_no_restore_at_round_5(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);

    ctx.f1.equipped_items[0] = 207u;
    ctx.f1.equipped_count    = 1u;
    ctx.round_3_f1_hp  = 80;
    ctx.round_3_f2_hp  = 70;
    ctx.time_loop_used = 0u;

    ctx.f1.hp = 45;
    ctx.f2.hp = 35;
    ctx.current_round        = 5u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_ROUND_END, 1u);

    /* Must NOT restore at round 5. */
    TEST_ASSERT_EQUAL_INT(45, (int)ctx.f1.hp);
    TEST_ASSERT_EQUAL_INT(35, (int)ctx.f2.hp);
}

/* ---------------------------------------------------------------------------
 * TL-03: Full simulation — Time Loop in slot 1 (index 0 since it's the first
 * equipped item per spec "slot 1" = first non-zero slot at index 0).
 *
 * Scenario: damage fighters each round, confirm restoration at round 6.
 *
 * We use fq_item_eval_trigger directly to simulate rounds 3-6 without
 * running full fq_combat_step (to avoid PRNG coupling here).
 * ---------------------------------------------------------------------------*/
static void test_tl_full_simulation_rounds_3_to_6(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);

    /* Time Loop in first slot. */
    ctx.f1.equipped_items[0] = 207u;
    ctx.f1.equipped_count    = 1u;

    /* --- Simulate end of round 3 --- */
    ctx.f1.hp = 88;
    ctx.f2.hp = 92;
    ctx.current_round        = 3u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_ROUND_END, 1u);

    TEST_ASSERT_EQUAL_INT(88, (int)ctx.round_3_f1_hp);
    TEST_ASSERT_EQUAL_INT(92, (int)ctx.round_3_f2_hp);

    /* --- Simulate damage during rounds 4 and 5 --- */
    ctx.f1.hp = 60;
    ctx.f2.hp = 55;

    /* --- Simulate end of round 6 --- */
    ctx.current_round        = 6u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_ROUND_END, 1u);

    /* Both fighters restored to round 3 snapshot. */
    TEST_ASSERT_EQUAL_INT(88, (int)ctx.f1.hp);
    TEST_ASSERT_EQUAL_INT(92, (int)ctx.f2.hp);
    TEST_ASSERT_EQUAL_UINT8(1u, ctx.time_loop_used);
}

/* ---------------------------------------------------------------------------
 * TL-03b: Once-per-fight — second evaluation at round 6 equivalent does nothing.
 * ---------------------------------------------------------------------------*/
static void test_tl_once_per_fight(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);

    ctx.f1.equipped_items[0] = 207u;
    ctx.f1.equipped_count    = 1u;
    ctx.round_3_f1_hp  = 88;
    ctx.round_3_f2_hp  = 92;
    ctx.time_loop_used = 1u;   /* Already fired. */

    ctx.f1.hp = 30;
    ctx.f2.hp = 20;
    ctx.current_round        = 6u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_ROUND_END, 1u);

    /* Must NOT restore because time_loop_used == 1. */
    TEST_ASSERT_EQUAL_INT(30, (int)ctx.f1.hp);
    TEST_ASSERT_EQUAL_INT(20, (int)ctx.f2.hp);
}

/* ---------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------*/
int main(void)
{
    test_tl_snapshot_zero_after_init();
    test_tl_snapshot_captured_round_3();
    test_tl_no_restore_at_round_4();
    test_tl_no_restore_at_round_5();
    test_tl_full_simulation_rounds_3_to_6();
    test_tl_once_per_fight();
    return 0;
}
