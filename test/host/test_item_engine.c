/**
 * test_item_engine.c — Phase 5 feature tests for the Item Engine.
 *
 * Rule 22: FEATURE RED phase — written after bound tests, before implementation.
 *
 * Covers all 8 representative items:
 *   001 Iron Fist     (Common,    PASSIVE,        +1 damage)
 *   003 Tough Hide    (Common,    ON_DEFEND,       -1 damage taken, min 1)
 *   004 Lucky Coin    (Common,    ON_ROUND_START,  10% d100 +2 damage)
 *   104 Vampire Fang  (Uncommon,  ON_KILL,         heal 5 HP)
 *   105 Haymaker      (Uncommon,  ON_CRIT,         damage x2 instead of x1.5)
 *   109 Bandage       (Uncommon,  ON_ROUND_END,    heal 1 HP if below 50%)
 *   204 Chaos Orb     (Rare,      ON_ROUND_START,  swap random effective stat)
 *   207 Time Loop     (Legendary, ON_ROUND_END,    snapshot r3 HP, restore at r6)
 *
 * For Lucky Coin and Chaos Orb (PRNG consumers), tests verify PRNG is
 * consumed unconditionally (NTR-A1: PRNG call even when check fails).
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
 * Helper: init a combat context with two bare characters (no items).
 * ---------------------------------------------------------------------------*/
static fq_combat_ctx_t make_ctx(uint32_t seed)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, seed);
    return ctx;
}

/* ---------------------------------------------------------------------------
 * FEAT-01: Iron Fist (001) — PASSIVE +1 damage.
 *
 * F1 equips Iron Fist. After PASSIVE trigger evaluation, F1's damage_bonus
 * must be +1.
 * ---------------------------------------------------------------------------*/
static void test_iron_fist_passive_damage_bonus(void)
{
    fq_combat_ctx_t ctx = make_ctx(1u);
    ctx.f1.equipped_items[0] = 1u;   /* Iron Fist */
    ctx.f1.equipped_count    = 1u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_PASSIVE, 1u);

    TEST_ASSERT_EQUAL_INT(1, (int)ctx.f1.damage_bonus);
}

/* ---------------------------------------------------------------------------
 * FEAT-02: Tough Hide (003) — ON_DEFEND, -1 incoming damage.
 *
 * F2 equips Tough Hide. When ON_DEFEND fires (attacking_fighter=1),
 * F2's damage_bonus is set to -1 (reduces incoming damage by 1).
 * ---------------------------------------------------------------------------*/
static void test_tough_hide_reduces_incoming_damage(void)
{
    fq_combat_ctx_t ctx = make_ctx(1u);
    ctx.f2.equipped_items[0] = 3u;   /* Tough Hide */
    ctx.f2.equipped_count    = 1u;
    ctx.f2.damage_bonus      = 0;
    ctx.f2.damage_mult_pct   = 100u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_DEFEND, 1u);

    TEST_ASSERT_EQUAL_INT(-1, (int)ctx.f2.damage_bonus);
}

/* ---------------------------------------------------------------------------
 * FEAT-02b: Tough Hide minimum floor — damage cannot go below 1.
 *
 * A full combat step with F2 having Tough Hide: the damage dealt to F2
 * must be at least 1, even if base damage after penalty would be 0.
 *
 * We use minimal attackers (str=0) where raw damage would be ≤1, and verify
 * F2 still takes exactly 1 (the floor) rather than 0 when Tough Hide fires.
 *
 * NOTE: With str=0, eff_strength=0, adjusted_roll + 0/2 = adjusted_roll (1-6).
 * Even with -1 from Tough Hide, minimum hit damage = max(adj_roll-1, 1) ≥ 1.
 * We verify via full combat run with str=0 fighters.
 * ---------------------------------------------------------------------------*/
static void test_tough_hide_minimum_damage_floor(void)
{
    /* Use seed and stats chosen so that F1's attack roll produces adj_roll=1.
     * With Tough Hide: damage = max(1-1, 1) = 1 (floor).
     * We cannot easily pin this without knowing exact PRNG trace, so we
     * instead verify via the item engine interface: after 12 full rounds
     * with Tough Hide, F2's HP must never go BELOW a value consistent with
     * minimum-1 damage per hit (F2 HP can't be exactly 100 - N*0 which
     * would violate the floor). */

    /* Simpler approach: set damage_bonus to a large negative already,
     * confirm that the contract clamps final damage. We test combat_step
     * where we can observe the damage dealt vs HP delta. */

    fq_character_t c1 = make_char(100, 0, 50, 50, 0); /* str=0: eff_str=0 */
    fq_character_t c2 = make_char(100, 0, 50, 50, 0);
    fq_combat_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    fq_combat_init(&ctx, &c1, &c2, 42u);

    /* F2 equips Tough Hide. */
    ctx.f2.equipped_items[0] = 3u;
    ctx.f2.equipped_count    = 1u;
    ctx.f2.damage_bonus      = 0;
    ctx.f2.damage_mult_pct   = 100u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    /* Run several rounds and verify F2's HP never decreases by more than
     * the expected amount, and crucially, damage_dealt to F2 is never 0
     * when a hit lands (the floor kicks in at 1). */
    for (int r = 0; r < 6; r++) {
        int16_t hp_before = ctx.f2.hp;
        fq_round_result_t res = fq_combat_step(&ctx);
        if (res.f1_hit) {
            /* F1 hit F2 — F2 took damage. With floor, delta ≥ 1. */
            int delta = (int)hp_before - (int)ctx.f2.hp;
            TEST_ASSERT_TRUE(delta >= 1);
        }
        if (ctx.finished) break;
    }
}

/* ---------------------------------------------------------------------------
 * FEAT-03: Lucky Coin (004) — ON_ROUND_START, 10% d100 → +2 damage.
 *
 * Lucky Coin ALWAYS consumes 1 PRNG call (NTR-A1).
 *
 * Strategy: seed chosen so that first d100 call ≤ 10 → bonus fires.
 * Verify PRNG state advances by exactly 1 call and damage_bonus = +2.
 *
 * Lucky Coin uses fq_prng_range(1, 100). Fires when result ≤ 10.
 *
 * Seed selection: we need a seed where the NEXT fq_prng_range(1,100) call
 * (first call after ON_ROUND_START trigger point) yields ≤ 10.
 *
 * We find this by running the PRNG ourselves to find a suitable seed.
 * Using seed=999 and checking the first fq_prng_range(1,100) result.
 * ---------------------------------------------------------------------------*/
static void test_lucky_coin_prng_always_consumed(void)
{
    /* Find a seed where lucky coin check will and won't fire, and verify
     * that PRNG advances by exactly 1 call in both cases. */
    fq_combat_ctx_t ctx = make_ctx(1u);
    ctx.f1.equipped_items[0] = 4u;   /* Lucky Coin */
    ctx.f1.equipped_count    = 1u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    /* Capture PRNG state before trigger. */
    uint32_t state_before = ctx.rng.state;

    /* Compute what the PRNG call will produce. */
    fq_prng_t rng_clone;
    rng_clone.state = state_before;
    uint32_t lucky_roll = fq_prng_range(&rng_clone, 1u, 100u);
    uint32_t state_after_one_call = rng_clone.state;

    /* Evaluate Lucky Coin ON_ROUND_START. */
    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_ROUND_START, 1u);

    /* PRNG must have advanced by exactly 1 call regardless of result. */
    TEST_ASSERT_EQUAL_UINT32(state_after_one_call, ctx.rng.state);

    /* Check if bonus fired based on roll. */
    if (lucky_roll <= 10u) {
        TEST_ASSERT_EQUAL_INT(2, (int)ctx.f1.damage_bonus);
    } else {
        TEST_ASSERT_EQUAL_INT(0, (int)ctx.f1.damage_bonus);
    }
}

/* ---------------------------------------------------------------------------
 * FEAT-03b: Lucky Coin +2 damage bonus fires when roll ≤ 10.
 *
 * We find a seed where the first fq_prng_range(1,100) call after init
 * produces ≤ 10, then verify damage_bonus = +2.
 * ---------------------------------------------------------------------------*/
static void test_lucky_coin_fires_when_roll_low(void)
{
    /* Brute-force find a seed for testing purposes — this is test setup,
     * not production code. Find seed where after init (2 PRNG calls for
     * initiative), next range(1,100) ≤ 10. */
    uint32_t test_seed = 0u;
    uint32_t lucky_roll = 100u;

    for (uint32_t s = 1u; s < 100000u; s++) {
        fq_prng_t rng;
        fq_prng_init(&rng, s);
        /* Simulate initiative (2 calls). */
        fq_prng_range(&rng, 1u, 6u);
        fq_prng_range(&rng, 1u, 6u);
        uint32_t r = fq_prng_range(&rng, 1u, 100u);
        if (r <= 10u) {
            test_seed  = s;
            lucky_roll = r;
            break;
        }
    }

    TEST_ASSERT_TRUE(test_seed != 0u); /* Sanity: found a suitable seed. */
    TEST_ASSERT_TRUE(lucky_roll <= 10u);

    /* Now run the actual test. */
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, test_seed);

    ctx.f1.equipped_items[0] = 4u;  /* Lucky Coin */
    ctx.f1.equipped_count    = 1u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_ROUND_START, 1u);
    TEST_ASSERT_EQUAL_INT(2, (int)ctx.f1.damage_bonus);
}

/* ---------------------------------------------------------------------------
 * FEAT-04: Vampire Fang (104) — ON_KILL, heal 5 HP.
 *
 * F1 equips Vampire Fang. After F1 kills F2, F1's HP must increase by 5
 * (clamped to hp_max).
 * ---------------------------------------------------------------------------*/
static void test_vampire_fang_on_kill_heal(void)
{
    fq_combat_ctx_t ctx = make_ctx(1u);
    ctx.f1.hp              = 80;
    ctx.f1.hp_max          = 100;
    ctx.f1.equipped_items[0] = 104u;  /* Vampire Fang */
    ctx.f1.equipped_count    = 1u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    /* Simulate: F2 just died (hp ≤ 0) — fire ON_KILL for attacker F1. */
    ctx.f2.hp = 0;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_KILL, 1u);

    /* F1 should have gained 5 HP. */
    TEST_ASSERT_EQUAL_INT(85, (int)ctx.f1.hp);
}

/* ---------------------------------------------------------------------------
 * FEAT-04b: Vampire Fang heal capped at hp_max.
 *
 * F1 HP = 98, hp_max = 100. After ON_KILL heal 5: HP = 100, not 103.
 * ---------------------------------------------------------------------------*/
static void test_vampire_fang_heal_capped_at_max(void)
{
    fq_combat_ctx_t ctx = make_ctx(1u);
    ctx.f1.hp              = 98;
    ctx.f1.hp_max          = 100;
    ctx.f1.equipped_items[0] = 104u;
    ctx.f1.equipped_count    = 1u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_KILL, 1u);

    TEST_ASSERT_EQUAL_INT(100, (int)ctx.f1.hp);
}

/* ---------------------------------------------------------------------------
 * FEAT-05: Haymaker (105) — ON_CRIT, damage x2 instead of x1.5.
 *
 * F1 equips Haymaker. After ON_CRIT fires, F1's damage_mult_pct must be
 * set to 200 (representing x2 damage multiplier).
 * ---------------------------------------------------------------------------*/
static void test_haymaker_crit_multiplier_x2(void)
{
    fq_combat_ctx_t ctx = make_ctx(1u);
    ctx.f1.equipped_items[0] = 105u;  /* Haymaker */
    ctx.f1.equipped_count    = 1u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 150u;  /* Normal crit starts at 150 */

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_CRIT, 1u);

    /* Haymaker upgrades crit to x2 = 200. */
    TEST_ASSERT_EQUAL_UINT8(200u, ctx.f1.damage_mult_pct);
}

/* ---------------------------------------------------------------------------
 * FEAT-06: Bandage (109) — ON_ROUND_END, heal 1 HP if below 50%.
 *
 * F1 HP = 40, hp_max = 100 (40% → below 50%). After ON_ROUND_END, HP = 41.
 * ---------------------------------------------------------------------------*/
static void test_bandage_heals_one_below_50_pct(void)
{
    fq_combat_ctx_t ctx = make_ctx(1u);
    ctx.f1.hp              = 40;
    ctx.f1.hp_max          = 100;
    ctx.f1.equipped_items[0] = 109u;  /* Bandage */
    ctx.f1.equipped_count    = 1u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_ROUND_END, 1u);

    TEST_ASSERT_EQUAL_INT(41, (int)ctx.f1.hp);
}

/* ---------------------------------------------------------------------------
 * FEAT-06b: Bandage does NOT fire at or above 50%.
 *
 * F1 HP = 50, hp_max = 100 (exactly 50%). No heal.
 * ---------------------------------------------------------------------------*/
static void test_bandage_no_heal_at_50_pct(void)
{
    fq_combat_ctx_t ctx = make_ctx(1u);
    ctx.f1.hp              = 50;
    ctx.f1.hp_max          = 100;
    ctx.f1.equipped_items[0] = 109u;  /* Bandage */
    ctx.f1.equipped_count    = 1u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_ROUND_END, 1u);

    TEST_ASSERT_EQUAL_INT(50, (int)ctx.f1.hp);
}

/* ---------------------------------------------------------------------------
 * FEAT-07: Chaos Orb (204) — ON_ROUND_START, swap random effective stat.
 *
 * Chaos Orb must always consume 1 PRNG call (for stat selection [0,3]).
 * After ON_ROUND_START: exactly 1 PRNG call consumed.
 * ---------------------------------------------------------------------------*/
static void test_chaos_orb_prng_consumed(void)
{
    fq_combat_ctx_t ctx = make_ctx(1u);
    ctx.f1.equipped_items[0] = 204u;  /* Chaos Orb */
    ctx.f1.equipped_count    = 1u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    /* Clone PRNG and advance by 1 call to predict post-trigger state. */
    fq_prng_t rng_clone = ctx.rng;
    fq_prng_range(&rng_clone, 0u, 3u); /* Chaos Orb stat selection call. */
    uint32_t expected_state = rng_clone.state;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_ROUND_START, 1u);

    TEST_ASSERT_EQUAL_UINT32(expected_state, ctx.rng.state);
}

/* ---------------------------------------------------------------------------
 * FEAT-08: Time Loop (207) — ON_ROUND_END, snapshot HP at r3, restore at r6.
 *
 * Sub-test 1: At end of round 3, ctx.round_3_f1_hp and round_3_f2_hp are set.
 * Sub-test 2: At end of round 6, f1.hp and f2.hp are restored to r3 values.
 * Sub-test 3: Once fired (time_loop_used=1), does not fire again.
 * ---------------------------------------------------------------------------*/
static void test_time_loop_snapshot_at_round_3(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);

    /* F1 equips Time Loop. */
    ctx.f1.equipped_items[0] = 207u;
    ctx.f1.equipped_count    = 1u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    /* Manually damage both fighters. */
    ctx.f1.hp = 75;
    ctx.f2.hp = 60;

    /* Simulate end of round 3. */
    ctx.current_round = 3u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_ROUND_END, 1u);

    /* Snapshot must be captured. */
    TEST_ASSERT_EQUAL_INT(75, (int)ctx.round_3_f1_hp);
    TEST_ASSERT_EQUAL_INT(60, (int)ctx.round_3_f2_hp);
}

static void test_time_loop_restore_at_round_6(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);

    /* F1 equips Time Loop. */
    ctx.f1.equipped_items[0] = 207u;
    ctx.f1.equipped_count    = 1u;

    /* Set up: snapshot already captured at round 3. */
    ctx.round_3_f1_hp = 75;
    ctx.round_3_f2_hp = 60;
    ctx.time_loop_used = 0u;

    /* Damage both fighters in "rounds 4-5". */
    ctx.f1.hp = 50;
    ctx.f2.hp = 30;

    /* Simulate end of round 6. */
    ctx.current_round = 6u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_ROUND_END, 1u);

    /* HP must be restored to round 3 values. */
    TEST_ASSERT_EQUAL_INT(75, (int)ctx.f1.hp);
    TEST_ASSERT_EQUAL_INT(60, (int)ctx.f2.hp);

    /* Once-per-fight flag set. */
    TEST_ASSERT_EQUAL_UINT8(1u, ctx.time_loop_used);
}

static void test_time_loop_does_not_fire_twice(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);

    /* F1 equips Time Loop, already fired. */
    ctx.f1.equipped_items[0] = 207u;
    ctx.f1.equipped_count    = 1u;
    ctx.round_3_f1_hp  = 75;
    ctx.round_3_f2_hp  = 60;
    ctx.time_loop_used = 1u;  /* Already used! */

    /* Current HP different from snapshot. */
    ctx.f1.hp = 20;
    ctx.f2.hp = 10;
    ctx.current_round        = 6u;
    ctx.f1.damage_bonus      = 0;
    ctx.f1.damage_mult_pct   = 100u;

    fq_item_eval_trigger(&ctx, FQ_TRIGGER_ON_ROUND_END, 1u);

    /* Must NOT restore — flag was already set. */
    TEST_ASSERT_EQUAL_INT(20, (int)ctx.f1.hp);
    TEST_ASSERT_EQUAL_INT(10, (int)ctx.f2.hp);
}

/* ---------------------------------------------------------------------------
 * FEAT-09: fq_item_lookup returns correct definitions.
 * ---------------------------------------------------------------------------*/
static void test_item_lookup_known_ids(void)
{
    const fq_item_def_t *iron_fist = fq_item_lookup(1u);
    TEST_ASSERT_NOT_NULL(iron_fist);
    TEST_ASSERT_EQUAL_UINT16(1u, iron_fist->id);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_RARITY_COMMON,   iron_fist->rarity);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_TRIGGER_PASSIVE, iron_fist->trigger);

    const fq_item_def_t *vamp_fang = fq_item_lookup(104u);
    TEST_ASSERT_NOT_NULL(vamp_fang);
    TEST_ASSERT_EQUAL_UINT16(104u, vamp_fang->id);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_RARITY_UNCOMMON, vamp_fang->rarity);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_TRIGGER_ON_KILL, vamp_fang->trigger);

    const fq_item_def_t *time_loop = fq_item_lookup(207u);
    TEST_ASSERT_NOT_NULL(time_loop);
    TEST_ASSERT_EQUAL_UINT16(207u, time_loop->id);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_RARITY_LEGENDARY,   time_loop->rarity);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_TRIGGER_ON_ROUND_END, time_loop->trigger);
}

/* ---------------------------------------------------------------------------
 * FEAT-10: Full combat step with Iron Fist — damage increases by 1.
 *
 * Two identical fighters. F1 gets Iron Fist (PASSIVE +1 damage).
 * Run one round and confirm F2 took more damage than a no-item run.
 *
 * We run two parallel fights: one with Iron Fist, one without.
 * With Iron Fist, F2 HP after round 1 should be lower by 1 (if F1 hit).
 * ---------------------------------------------------------------------------*/
static void test_full_combat_iron_fist_increases_damage(void)
{
    /* Baseline fight: no items. */
    fq_character_t c1_base = make_char(100, 50, 50, 50, 50);
    fq_character_t c2_base = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx_base;
    fq_combat_init(&ctx_base, &c1_base, &c2_base, 42u);
    fq_round_result_t base_res = fq_combat_step(&ctx_base);

    /* Iron Fist fight: F1 has Iron Fist. */
    fq_character_t c1_item = make_char(100, 50, 50, 50, 50);
    fq_character_t c2_item = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx_item;
    fq_combat_init(&ctx_item, &c1_item, &c2_item, 42u);
    ctx_item.f1.equipped_items[0] = 1u;   /* Iron Fist */
    ctx_item.f1.equipped_count    = 1u;

    fq_round_result_t item_res = fq_combat_step(&ctx_item);

    /* If F1 hit F2 in baseline, then with Iron Fist F1 must deal 1 more damage. */
    if (base_res.f1_hit) {
        TEST_ASSERT_EQUAL_INT((int)base_res.f1_damage_dealt + 1,
                              (int)item_res.f1_damage_dealt);
    } else {
        /* F1 missed — no damage difference; both should be 0. */
        TEST_ASSERT_EQUAL_INT(0, (int)base_res.f1_damage_dealt);
        TEST_ASSERT_EQUAL_INT(0, (int)item_res.f1_damage_dealt);
    }
}

/* ---------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------*/
int main(void)
{
    test_iron_fist_passive_damage_bonus();
    test_tough_hide_reduces_incoming_damage();
    test_tough_hide_minimum_damage_floor();
    test_lucky_coin_prng_always_consumed();
    test_lucky_coin_fires_when_roll_low();
    test_vampire_fang_on_kill_heal();
    test_vampire_fang_heal_capped_at_max();
    test_haymaker_crit_multiplier_x2();
    test_bandage_heals_one_below_50_pct();
    test_bandage_no_heal_at_50_pct();
    test_chaos_orb_prng_consumed();
    test_time_loop_snapshot_at_round_3();
    test_time_loop_restore_at_round_6();
    test_time_loop_does_not_fire_twice();
    test_item_lookup_known_ids();
    test_full_combat_iron_fist_increases_damage();
    return 0;
}
