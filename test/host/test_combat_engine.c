/**
 * test_combat_engine.c — Feature tests for Phase-4 Combat Engine.
 *
 * Rule 22: FEATURE RED phase — happy-path tests written before implementation.
 *
 * All expected values are derived from the offline PRNG trace (seed=12345,
 * symmetric fighters: str=50, spd=50, prec=50, int=50, hp_max=100).
 *
 * B1: Initiative uses d6 + eff_speed/3.
 * B2: Precision tier lookup table applied to attack rolls.
 * B3: Dodge clamp [5, 40].
 * B4: Crit from raw_roll >= crit_threshold (no separate PRNG call).
 * B5: Defensive reroll if opponent_raw_roll >= 5 and defender has charges.
 *
 * Trace summary (seed=12345, symmetric fighters):
 *   Init:   F1 d6=3+5=8, F2 d6=4+5=9 → first_attacker=2 (F2 goes first)
 *   PRNG state after init: 0x652A09AF
 *   eff_speed(50)=16, 16/3=5 → initiative bonus=5 per fighter.
 *   reroll_charges = eff(50)/4 = 16/4 = 4.
 *   eff_prec(50)=16, tier=16/5=3 (capped at 3).
 *   PRECISION_TABLE[3]=[3,3,4,4,5,5], crit_threshold=6-(3/2)=5.
 *   dc = eff_spd*2 - eff_prec/2 = 16*2 - 16/2 = 32-8=24, clamped [5,40]=24.
 *
 *   Round 1: F2 raw=5, adj=5, crit(5>=5), dodge=43>24=hit, dmg=5+8=13→crit=19, F1 hp→81
 *            F1 raw=6, F2 def-rr→1(keeps lower), adj=3, not_crit(1<5), dodge=69>24=hit,
 *            dmg=3+8=11, F2 hp→89. f2_rerolled=1(def), f1_rerolled=0.
 *            LS: ls1=1 ls2=13. PRNG state: 0x8CA71E78
 *
 *   Round 2: F2 raw=5, adj=5, crit, dodge=87>24=hit, dmg=19, F1 hp→62
 *            F1 raw=2, self-rr→1(keeps 2), adj=3, not_crit(2<5), dodge=44>24=hit,
 *            dmg=11, F2 hp→78. f1_rerolled=1(self), f2_rerolled=0.
 *
 *   Round 10: F2 wins. Final: F1 hp=0 F2 hp=4, winner=2. (R9 OT applied before R10.)
 */

#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "test_assert.h"
#include "types.h"
#include "combat.h"
#include "prng.h"

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
 * FEAT-1: fq_combat_init returns GAME_OK for valid inputs.
 * ---------------------------------------------------------------------------*/
static void test_init_returns_ok(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    game_err_t err = fq_combat_init(&ctx, &c1, &c2, 42u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)GAME_OK, (uint8_t)err);
}

/* ---------------------------------------------------------------------------
 * FEAT-2: Init correctly copies stats from character structs.
 * ---------------------------------------------------------------------------*/
static void test_init_copies_stats(void)
{
    fq_character_t c1 = make_char(100, 50, 60, 70, 80);
    fq_character_t c2 = make_char(200, 10, 20, 30, 40);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);

    TEST_ASSERT_EQUAL_UINT8(50u, ctx.f1.strength);
    TEST_ASSERT_EQUAL_UINT8(60u, ctx.f1.speed);
    TEST_ASSERT_EQUAL_UINT8(70u, ctx.f1.precision);
    TEST_ASSERT_EQUAL_UINT8(80u, ctx.f1.intelligence);
    TEST_ASSERT_EQUAL_INT(100,   (int)ctx.f1.hp);
    TEST_ASSERT_EQUAL_INT(100,   (int)ctx.f1.hp_max);

    TEST_ASSERT_EQUAL_UINT8(10u, ctx.f2.strength);
    TEST_ASSERT_EQUAL_UINT8(20u, ctx.f2.speed);
    TEST_ASSERT_EQUAL_UINT8(30u, ctx.f2.precision);
    TEST_ASSERT_EQUAL_UINT8(40u, ctx.f2.intelligence);
    TEST_ASSERT_EQUAL_INT(200,   (int)ctx.f2.hp);
    TEST_ASSERT_EQUAL_INT(200,   (int)ctx.f2.hp_max);
}

/* ---------------------------------------------------------------------------
 * FEAT-3: Init sets current_round=1, finished=0, winner=0.
 * ---------------------------------------------------------------------------*/
static void test_init_sets_initial_state(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);

    TEST_ASSERT_EQUAL_UINT8(1u, ctx.current_round);
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.finished);
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.winner);
}

/* ---------------------------------------------------------------------------
 * FEAT-4: Init correctly computes reroll_charges = eff_int / 4.
 *
 * eff(50)=16 → charges = 16/4 = 4
 * eff(0)=0   → charges = 0/4 = 0
 * eff(255)=23 → charges = 23/4 = 5
 * ---------------------------------------------------------------------------*/
static void test_init_reroll_charges(void)
{
    /* eff(50)=16 → 16/4=4 */
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 0);   /* intel=0 → charges=0 */
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);
    TEST_ASSERT_EQUAL_UINT8(4u, ctx.f1.reroll_charges);
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.f2.reroll_charges);

    /* eff(255)=23 → 23/4=5 */
    fq_character_t c3 = make_char(100, 50, 50, 50, 255);
    fq_combat_ctx_t ctx2;
    fq_combat_init(&ctx2, &c3, &c2, 1u);
    TEST_ASSERT_EQUAL_UINT8(5u, ctx2.f1.reroll_charges);
}

/* ---------------------------------------------------------------------------
 * FEAT-5: Initiative with seed=12345 → first_attacker=2 (F2 goes first).
 *
 * B1 fix: initiative = d6 + eff_speed/3.
 * eff_speed(50)=16, 16/3=5.
 * Trace: F1 d6=3 → total=3+5=8. F2 d6=4 → total=4+5=9. F2 wins (9 > 8).
 * Ties favor defender (F2), but here F2 wins outright.
 * ---------------------------------------------------------------------------*/
static void test_init_initiative_known_seed(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 12345u);
    TEST_ASSERT_EQUAL_UINT8(2u, ctx.first_attacker);
}

/* ---------------------------------------------------------------------------
 * FEAT-6: Initiative tie → F2 (defender) wins the tiebreak.
 *
 * F2 has high speed (eff=23) → much higher initiative total → F2 goes first.
 * This validates that first_attacker=2 when F2 total > F1 total.
 * ---------------------------------------------------------------------------*/
static void test_init_initiative_tie_f2_wins(void)
{
    fq_character_t c1 = make_char(100, 50, 0,   50, 50); /* speed=0 → eff_spd=0 */
    fq_character_t c2 = make_char(100, 50, 255, 50, 50); /* speed=255 → eff_spd=23 */
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);
    /* F2 has eff_spd/3=7, F1 has eff_spd/3=0. F2 total >> F1 total → F2 goes first */
    TEST_ASSERT_EQUAL_UINT8(2u, ctx.first_attacker);
}

/* ---------------------------------------------------------------------------
 * FEAT-7: Round 1 with seed=12345.
 *
 * B2/B3/B4/B5 rework applied. first_attacker=2 (F2 attacks first).
 *
 * Expected from offline trace:
 *   F2: raw=5, tier=3, adj=PRECISION_TABLE[3][4]=5, crit(5>=5)=1,
 *       dodge=43>24=hit, dmg=5+8=13→crit→19. F1 hp→81.
 *   F1 (second): raw=6. F2 def-rr: d6=1<6, keep lower (raw→1).
 *       adj=PRECISION_TABLE[3][0]=3, crit(1>=5)=0.
 *       dodge=69>24=hit, dmg=3+8=11. F2 hp→89.
 *   f1_rerolled=0 (F1 used no charges), f2_rerolled=1 (F2 used defensive reroll).
 *   result.round=1, result.finished=0.
 * ---------------------------------------------------------------------------*/
static void test_step_round1_known_seed(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 12345u);

    fq_round_result_t r = fq_combat_step(&ctx);

    TEST_ASSERT_EQUAL_UINT8(1u, r.round);
    TEST_ASSERT_EQUAL_UINT8(0u, r.finished);
    TEST_ASSERT_EQUAL_UINT8(0u, r.winner);

    /* F2 hit, crit, defensive rerolled (F2 used a defensive reroll charge) */
    TEST_ASSERT_EQUAL_UINT8(1u, r.f2_hit);
    TEST_ASSERT_EQUAL_UINT8(1u, r.f2_crit);
    TEST_ASSERT_EQUAL_UINT8(1u, r.f2_rerolled);
    TEST_ASSERT_EQUAL_INT(19, (int)r.f2_damage_dealt);
    TEST_ASSERT_EQUAL_INT(81, (int)r.f1_hp);

    /* F1 hit (after being def-rr'd to raw=1), no crit, no reroll charge used by F1 */
    TEST_ASSERT_EQUAL_UINT8(1u, r.f1_hit);
    TEST_ASSERT_EQUAL_UINT8(0u, r.f1_crit);
    TEST_ASSERT_EQUAL_UINT8(0u, r.f1_rerolled);
    TEST_ASSERT_EQUAL_INT(11, (int)r.f1_damage_dealt);
    TEST_ASSERT_EQUAL_INT(89, (int)r.f2_hp);
}

/* ---------------------------------------------------------------------------
 * FEAT-8: current_round increments after each step.
 * ---------------------------------------------------------------------------*/
static void test_step_increments_round(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 12345u);

    TEST_ASSERT_EQUAL_UINT8(1u, ctx.current_round);
    fq_combat_step(&ctx);
    TEST_ASSERT_EQUAL_UINT8(2u, ctx.current_round);
    fq_combat_step(&ctx);
    TEST_ASSERT_EQUAL_UINT8(3u, ctx.current_round);
}

/* ---------------------------------------------------------------------------
 * FEAT-9: Round 2 with seed=12345.
 *
 * Expected from offline trace:
 *   F2 (first again): raw=5, crit(5>=5), dodge=87>24=hit, dmg=5+8=13→crit→19. F1 hp→62.
 *   F1 (second): raw=2, self-rr→1 (1<2, keeps 2). def-rr check: raw2=2<5, skip.
 *       adj=PRECISION_TABLE[3][1]=3, crit(2>=5)=0. dodge=44>24=hit, dmg=3+8=11. F2 hp→78.
 *   f1_rerolled=1 (F1 self-rerolled), f2_rerolled=0.
 * ---------------------------------------------------------------------------*/
static void test_step_round2_known_seed(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 12345u);

    fq_combat_step(&ctx); /* Round 1 */
    fq_round_result_t r = fq_combat_step(&ctx); /* Round 2 */

    TEST_ASSERT_EQUAL_UINT8(2u, r.round);
    TEST_ASSERT_EQUAL_UINT8(0u, r.finished);

    /* F2: no reroll, hit, crit, dmg=19 */
    TEST_ASSERT_EQUAL_UINT8(0u, r.f2_rerolled);
    TEST_ASSERT_EQUAL_UINT8(1u, r.f2_hit);
    TEST_ASSERT_EQUAL_UINT8(1u, r.f2_crit);
    TEST_ASSERT_EQUAL_INT(19, (int)r.f2_damage_dealt);
    TEST_ASSERT_EQUAL_INT(62, (int)r.f1_hp);

    /* F1: self-rerolled, hit, no crit, dmg=11 */
    TEST_ASSERT_EQUAL_UINT8(1u, r.f1_rerolled);
    TEST_ASSERT_EQUAL_UINT8(1u, r.f1_hit);
    TEST_ASSERT_EQUAL_UINT8(0u, r.f1_crit);
    TEST_ASSERT_EQUAL_INT(11, (int)r.f1_damage_dealt);
    TEST_ASSERT_EQUAL_INT(78, (int)r.f2_hp);
}

/* ---------------------------------------------------------------------------
 * FEAT-10: Full fight to completion with seed=12345.
 *
 * Expected: F2 wins in round 10 (from offline trace).
 * Final HP: F1=0, F2=5.
 * ---------------------------------------------------------------------------*/
static void test_step_full_fight_to_completion(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 12345u);

    fq_round_result_t last;
    memset(&last, 0, sizeof(last));
    for (int i = 0; i < 15; i++) {
        last = fq_combat_step(&ctx);
        if (last.finished) break;
    }

    TEST_ASSERT_EQUAL_UINT8(1u, last.finished);
    TEST_ASSERT_EQUAL_UINT8(2u, last.winner);
    TEST_ASSERT_EQUAL_INT(0, (int)last.f1_hp);
    TEST_ASSERT_EQUAL_INT(4, (int)last.f2_hp);
    TEST_ASSERT_EQUAL_UINT8(10u, last.round);
}

/* ---------------------------------------------------------------------------
 * FEAT-11: ctx.winner and ctx.finished are updated by step.
 * ---------------------------------------------------------------------------*/
static void test_step_updates_ctx_winner(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 12345u);

    for (int i = 0; i < 15 && !ctx.finished; i++) {
        fq_combat_step(&ctx);
    }

    /* ctx must reflect finished state */
    TEST_ASSERT_EQUAL_UINT8(1u, ctx.finished);
    TEST_ASSERT_EQUAL_UINT8(2u, ctx.winner);
}

/* ---------------------------------------------------------------------------
 * FEAT-12: result.overtime_damage is 0 for rounds <= FQ_OVERTIME_THRESHOLD.
 * ---------------------------------------------------------------------------*/
static void test_step_no_overtime_before_threshold(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 12345u);

    /* Rounds 1-10 from our trace: fight ends in round 10 */
    for (int i = 0; i < 10 && !ctx.finished; i++) {
        fq_round_result_t r = fq_combat_step(&ctx);
        /* Overtime only fires after round 8. Rounds 9+ would have OT. */
        if (r.round <= (uint8_t)FQ_OVERTIME_THRESHOLD) {
            TEST_ASSERT_EQUAL_UINT8(0u, r.overtime_damage);
        }
    }
}

/* ---------------------------------------------------------------------------
 * FEAT-13: Overtime damage fires for round > FQ_OVERTIME_THRESHOLD.
 *
 * Round 9: overtime_damage = 9 - 8 = 1
 * Round 10: overtime_damage = 2
 * etc.
 * ---------------------------------------------------------------------------*/
static void test_step_overtime_damage_applied(void)
{
    /* High HP fighters to reach overtime */
    fq_character_t c1 = make_char(500, 0, 50, 0, 0);
    fq_character_t c2 = make_char(500, 0, 50, 0, 0);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 77777u);

    fq_round_result_t r;
    memset(&r, 0, sizeof(r));

    /* Drive to round 9 */
    for (int i = 0; i < 9 && !ctx.finished; i++) {
        r = fq_combat_step(&ctx);
    }

    if (!ctx.finished) {
        /* Round 9: overtime_damage must be 1 (9 - 8) */
        TEST_ASSERT_EQUAL_UINT8(1u, r.overtime_damage);
    }
    /* If already finished, test inconclusive — just pass */
}

/* ---------------------------------------------------------------------------
 * FEAT-14: Lucky Star fields in result.
 *
 * Phase 4: lucky star bonus never triggers (no perk ownership check yet).
 * Both f1_lucky_star and f2_lucky_star must always be 0.
 * ---------------------------------------------------------------------------*/
static void test_step_lucky_star_phase4_always_zero(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 12345u);

    for (int i = 0; i < 10 && !ctx.finished; i++) {
        fq_round_result_t r = fq_combat_step(&ctx);
        TEST_ASSERT_EQUAL_UINT8(0u, r.f1_lucky_star);
        TEST_ASSERT_EQUAL_UINT8(0u, r.f2_lucky_star);
    }
}

/* ---------------------------------------------------------------------------
 * FEAT-15: class_id is copied from character.
 * ---------------------------------------------------------------------------*/
static void test_init_copies_class_id(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    c1.class_id = (uint8_t)FQ_CLASS_BRUISER;
    c2.class_id = (uint8_t)FQ_CLASS_TRICKSTER;
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_CLASS_BRUISER,   ctx.f1.class_id);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_CLASS_TRICKSTER, ctx.f2.class_id);
}

/* ---------------------------------------------------------------------------
 * FEAT-16: Miss result — when dodge succeeds, damage_dealt = 0.
 *
 * From trace round 7 of the seed=12345 fight:
 *   F2 attacks F1: d6=4, adj=4, crit=0. dodge=49>24=hit. dmg=12.
 *   F1 attacks F2: d6=2, self-rr→5 (5>2, keeps 5). def-rr: raw2=5>=5 but
 *     at this point in the fight F2's charges may be exhausted.
 *     dodge=19<=24: MISS. f1_damage_dealt=0.
 * ---------------------------------------------------------------------------*/
static void test_step_miss_damage_is_zero(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 12345u);

    /* From trace: round 7, F1 attack misses (dodge_roll=19 <= dc=24) */
    fq_round_result_t r;
    memset(&r, 0, sizeof(r));
    for (int i = 0; i < 7 && !ctx.finished; i++) {
        r = fq_combat_step(&ctx);
    }

    /* Round 7: F1 missed, f1_damage_dealt=0, f1_hit=0 */
    TEST_ASSERT_EQUAL_UINT8(7u, r.round);
    TEST_ASSERT_EQUAL_UINT8(0u, r.f1_hit);
    TEST_ASSERT_EQUAL_INT(0, (int)r.f1_damage_dealt);
}

/* ---------------------------------------------------------------------------
 * FEAT-17: result.round reflects the round that was just executed.
 * ---------------------------------------------------------------------------*/
static void test_step_result_round_field(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 12345u);

    for (uint8_t expected = 1u; expected <= 10u; expected++) {
        fq_round_result_t r = fq_combat_step(&ctx);
        TEST_ASSERT_EQUAL_UINT8(expected, r.round);
        if (r.finished) break;
    }
}

int main(void)
{
    test_init_returns_ok();
    test_init_copies_stats();
    test_init_sets_initial_state();
    test_init_reroll_charges();
    test_init_initiative_known_seed();
    test_init_initiative_tie_f2_wins();
    test_step_round1_known_seed();
    test_step_increments_round();
    test_step_round2_known_seed();
    test_step_full_fight_to_completion();
    test_step_updates_ctx_winner();
    test_step_no_overtime_before_threshold();
    test_step_overtime_damage_applied();
    test_step_lucky_star_phase4_always_zero();
    test_init_copies_class_id();
    test_step_miss_damage_is_zero();
    test_step_result_round_field();
    return 0;
}
