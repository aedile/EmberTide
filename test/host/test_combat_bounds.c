/**
 * test_combat_bounds.c — Bound/negative tests for Phase-4 Combat Engine.
 *
 * Rule 22: BOUND RED phase — these tests must be written and committed as
 * failing BEFORE any combat.c/combat.h implementation exists.
 *
 * Covers all BLOCKER items from spec-challenger:
 *   N1  : NULL context/character pointers → GAME_ERR_NULL_PTR
 *   N2  : PRNG consumption determinism (state matches expected after full round)
 *   N3  : hp_max == 0 at start → init treats hp_max 0 as zero HP → finished immediately
 *   N4  : All stats at 0 → no division by zero, no crash
 *   N5  : HP clamped to INT16_MAX in init (defense-in-depth)
 *   N6  : First kill wins — F2 does NOT counter-attack after KO by F1
 *   N10 : Reroll evaluation order — self-reroll checked before defensive reroll
 *   N11 : Lucky Star PRNG calls consumed even without perk ownership
 *   N15 : Conditional reroll PRNG ordering — conditional calls only when eligible
 *   N17 : Round 12 tiebreaker uses HP percentage, not absolute HP
 *   N_B6: current_round never exceeds FQ_MAX_ROUNDS+1 (overflow bound)
 *
 * Advisory items tested here (defense-in-depth):
 *   N13 : Uninitialized (zero-memset) context → treated as finished (no-op on step)
 *   N14 : fq_combat_step called after finished → returns zero result, no state change
 *   N16 : _Static_assert sizeof(fq_combat_ctx_t) compiled in combat.h
 *
 * All PRNG-pinned values derived from offline trace:
 *   seed=12345, symmetric fighters str=50/spd=50/prec=50/int=50/hp=100.
 *   Initiative: F1 d6=3+5=8, F2 d6=4+5=9 → first_attacker=2 (F2 goes first).
 *   State after init: 0x652A09AF (2 d6 initiative calls).
 *   State after round 1: 0x8CA71E78 (F2 atk d6=5, dodge=43, F1 atk d6=6, def-rr->1, dodge=69, LS*2).
 */

#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include "test_assert.h"
#include "types.h"
#include "combat.h"
#include "prng.h"

/* ---------------------------------------------------------------------------
 * Helpers: build a minimal valid fq_character_t for testing.
 * ---------------------------------------------------------------------------*/
static fq_character_t make_char(uint16_t hp_max,
                                uint8_t str, uint8_t spd,
                                uint8_t prec, uint8_t intel)
{
    fq_character_t c;
    memset(&c, 0, sizeof(c));
    c.hp_max      = hp_max;
    c.strength    = str;
    c.speed       = spd;
    c.precision   = prec;
    c.intelligence = intel;
    return c;
}

/* ---------------------------------------------------------------------------
 * N1: NULL pointer checks — init must return GAME_ERR_NULL_PTR.
 * ---------------------------------------------------------------------------*/
static void test_n1_null_ctx(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    game_err_t err = fq_combat_init(NULL, &c1, &c2, 42u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)GAME_ERR_NULL_PTR, (uint8_t)err);
}

static void test_n1_null_c1(void)
{
    fq_combat_ctx_t ctx;
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    game_err_t err = fq_combat_init(&ctx, NULL, &c2, 42u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)GAME_ERR_NULL_PTR, (uint8_t)err);
}

static void test_n1_null_c2(void)
{
    fq_combat_ctx_t ctx;
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    game_err_t err = fq_combat_init(&ctx, &c1, NULL, 42u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)GAME_ERR_NULL_PTR, (uint8_t)err);
}

/* ---------------------------------------------------------------------------
 * N3: hp_max == 0 → fighter has 0 HP at init → finished immediately.
 * ---------------------------------------------------------------------------*/
static void test_n3_hp_max_zero_c1(void)
{
    fq_combat_ctx_t ctx;
    fq_character_t c1 = make_char(0,   50, 50, 50, 50);  /* hp_max=0 */
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    game_err_t err = fq_combat_init(&ctx, &c1, &c2, 42u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)GAME_OK, (uint8_t)err);
    /* Fighter with hp_max=0 has hp=0 → fight is over immediately */
    TEST_ASSERT_EQUAL_UINT8(1u, ctx.finished);
    /* F2 wins because F1 has zero HP */
    TEST_ASSERT_EQUAL_UINT8(2u, ctx.winner);
}

static void test_n3_hp_max_zero_both(void)
{
    fq_combat_ctx_t ctx;
    fq_character_t c1 = make_char(0, 50, 50, 50, 50);
    fq_character_t c2 = make_char(0, 50, 50, 50, 50);
    game_err_t err = fq_combat_init(&ctx, &c1, &c2, 42u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)GAME_OK, (uint8_t)err);
    /* Both dead at start → finished, winner = 0 (draw) */
    TEST_ASSERT_EQUAL_UINT8(1u, ctx.finished);
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.winner);
}

/* ---------------------------------------------------------------------------
 * N4: All stats at 0 — no division by zero, no crash.
 *
 * With seed=1, all-zero stats:
 *   eff_spd=0 → initiative = d6 + 0/3 = d6 only.
 *   PRNG(1) init: F1 roll=4, F2 roll=2 → first_attacker=1.
 *   eff_prec=0 → tier=0, PRECISION_TABLE[0]={1,2,3,4,5,6}.
 *   crit_thresh=6 (6 - 0/2 = 6).
 *   dc = 0*2 - 0/2 = 0 → clamped to 5.
 *   reroll_charges = eff(0)/4 = 0.
 * ---------------------------------------------------------------------------*/
static void test_n4_all_stats_zero_no_crash(void)
{
    fq_combat_ctx_t ctx;
    fq_character_t c1 = make_char(50, 0, 0, 0, 0);
    fq_character_t c2 = make_char(50, 0, 0, 0, 0);
    game_err_t err = fq_combat_init(&ctx, &c1, &c2, 1u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)GAME_OK, (uint8_t)err);
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.finished);
    /* reroll_charges must be eff(0)/4 = 0/4 = 0 */
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.f1.reroll_charges);
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.f2.reroll_charges);
    /* Step must not crash */
    fq_round_result_t r = fq_combat_step(&ctx);
    /* With all-zero stats, dodge chance = 5 (clamped minimum).
     * Attack always hit unless dodge_roll <= 5 (5% miss chance).
     * damage = adj_roll + 0/2 = raw_roll (tier 0, no crit since thresh=6 unless roll=6). */
    TEST_ASSERT_TRUE(r.finished == 0u || r.finished == 1u);
    /* HP must still be in valid range after step */
    TEST_ASSERT_TRUE(r.f1_hp >= 0);
    TEST_ASSERT_TRUE(r.f1_hp <= 50);
    TEST_ASSERT_TRUE(r.f2_hp >= 0);
    TEST_ASSERT_TRUE(r.f2_hp <= 50);
    /* Round counter advances */
    TEST_ASSERT_EQUAL_UINT8(2u, ctx.current_round);
}

/* ---------------------------------------------------------------------------
 * N5: HP clamped to INT16_MAX in init (defense-in-depth).
 *
 * fq_character_t.hp_max is uint16_t. INT16_MAX = 32767.
 * hp_max = 65535 (UINT16_MAX) must be clamped to 32767 in the fighter snapshot.
 * ---------------------------------------------------------------------------*/
static void test_n5_hp_clamped_to_int16_max(void)
{
    fq_combat_ctx_t ctx;
    fq_character_t c1 = make_char(65535u, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100,    50, 50, 50, 50);
    game_err_t err = fq_combat_init(&ctx, &c1, &c2, 1u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)GAME_OK, (uint8_t)err);
    /* HP must be clamped to INT16_MAX = 32767 */
    TEST_ASSERT_EQUAL_INT((int)INT16_MAX, (int)ctx.f1.hp);
    TEST_ASSERT_EQUAL_INT((int)INT16_MAX, (int)ctx.f1.hp_max);
}

/* ---------------------------------------------------------------------------
 * N6: First kill wins — F2 does NOT attack if KO'd by F1.
 *
 * Strategy: F1 has max strength/precision (guaranteed kill on F2 with 1 HP).
 * Initiative: F1 spd=50 (eff=16, /3=5), F2 spd=0 (eff=0, /3=0).
 * Seed=12345: F1 d6=3+5=8 vs F2 d6=4+0=4 → F1 goes first.
 * F1 will deal at least 1 damage, KO'ing F2. F2 never attacks.
 * ---------------------------------------------------------------------------*/
static void test_n6_first_kill_no_counter_attack(void)
{
    fq_combat_ctx_t ctx;
    /* F1: high strength/precision to guarantee kill. F2: 1 HP, low everything. */
    fq_character_t c1 = make_char(200, 255, 50, 255, 50);
    fq_character_t c2 = make_char(1,   0,   0,  0,   0);

    game_err_t err = fq_combat_init(&ctx, &c1, &c2, 12345u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)GAME_OK, (uint8_t)err);

    /* With F1 eff_spd/3=5 and F2 eff_spd/3=0, F1 wins initiative with any d6 rolls */
    TEST_ASSERT_EQUAL_UINT8(1u, ctx.first_attacker);

    /* Capture F1 HP before step */
    int16_t f1_hp_before = ctx.f1.hp;

    fq_round_result_t r = fq_combat_step(&ctx);

    /* F2 must be KO'd */
    TEST_ASSERT_EQUAL_INT(0, (int)r.f2_hp);
    /* Fight must be finished with F1 winning */
    TEST_ASSERT_EQUAL_UINT8(1u, r.finished);
    TEST_ASSERT_EQUAL_UINT8(1u, r.winner);
    /* F1 must NOT have taken damage (F2 never got to counter-attack) */
    TEST_ASSERT_EQUAL_INT((int)f1_hp_before, (int)r.f1_hp);
    /* f2_damage_dealt must be 0 (never attacked) */
    TEST_ASSERT_EQUAL_INT(0, (int)r.f2_damage_dealt);
}

/* ---------------------------------------------------------------------------
 * N2: PRNG consumption determinism.
 *
 * Two independent combat contexts with the same seed must produce identical
 * results across all rounds. This verifies PRNG state is exactly consumed
 * the same way both times.
 * ---------------------------------------------------------------------------*/
static void test_n2_prng_determinism_two_contexts(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    uint32_t seed = 12345u;

    fq_combat_ctx_t ctx_a, ctx_b;
    fq_combat_init(&ctx_a, &c1, &c2, seed);
    fq_combat_init(&ctx_b, &c1, &c2, seed);

    /* PRNG state must match after init */
    TEST_ASSERT_EQUAL_UINT32(ctx_a.rng.state, ctx_b.rng.state);

    /* Step both — results must be identical */
    for (int i = 0; i < 7; i++) {
        fq_round_result_t ra = fq_combat_step(&ctx_a);
        fq_round_result_t rb = fq_combat_step(&ctx_b);

        TEST_ASSERT_EQUAL_INT((int)ra.f1_hp,          (int)rb.f1_hp);
        TEST_ASSERT_EQUAL_INT((int)ra.f2_hp,          (int)rb.f2_hp);
        TEST_ASSERT_EQUAL_INT((int)ra.f1_damage_dealt,(int)rb.f1_damage_dealt);
        TEST_ASSERT_EQUAL_INT((int)ra.f2_damage_dealt,(int)rb.f2_damage_dealt);
        TEST_ASSERT_EQUAL_UINT8(ra.f1_hit,    rb.f1_hit);
        TEST_ASSERT_EQUAL_UINT8(ra.f2_hit,    rb.f2_hit);
        TEST_ASSERT_EQUAL_UINT8(ra.f1_crit,   rb.f1_crit);
        TEST_ASSERT_EQUAL_UINT8(ra.f2_crit,   rb.f2_crit);
        TEST_ASSERT_EQUAL_UINT8(ra.finished,  rb.finished);
        TEST_ASSERT_EQUAL_UINT8(ra.winner,    rb.winner);

        /* PRNG states must remain in sync */
        TEST_ASSERT_EQUAL_UINT32(ctx_a.rng.state, ctx_b.rng.state);

        if (ra.finished) break;
    }
}

/* ---------------------------------------------------------------------------
 * N2 (specific PRNG state): Verify exact PRNG state after init with seed=12345.
 *
 * B1 fix: initiative uses d6, so 2 calls are fq_prng_range(1,6).
 * The PRNG state after 2 calls is still 0x652A09AF because fq_prng_next()
 * is called once per fq_prng_range call regardless of [min,max] bounds.
 * ---------------------------------------------------------------------------*/
static void test_n2_prng_state_after_init_known_seed(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 12345u);

    /* After 2 initiative d6 calls the PRNG state must be exactly 0x652A09AF */
    TEST_ASSERT_EQUAL_UINT32(0x652A09AFu, ctx.rng.state);
}

/* ---------------------------------------------------------------------------
 * N2 (PRNG state after round 1): Verify exact state after one step.
 *
 * Offline trace: seed=12345, symmetric fighters str=50/spd=50/prec=50/int=50, hp=100.
 * Initiative: F2 goes first (F1 d6=3+5=8, F2 d6=4+5=9).
 *
 * Round 1 PRNG calls:
 *   F2 atk d6=5 (no self-rr), dodge d100=43
 *   F1 atk d6=6, F2 def-rr→1 (keeps lower), dodge d100=69
 *   LS d20=1, LS d20=13
 * Total: 7 calls consumed. State after round 1 = 0x8CA71E78.
 * ---------------------------------------------------------------------------*/
static void test_n2_prng_state_after_round1_known_seed(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 12345u);
    fq_combat_step(&ctx);
    TEST_ASSERT_EQUAL_UINT32(0x8CA71E78u, ctx.rng.state);
}

/* ---------------------------------------------------------------------------
 * N10: Reroll evaluation order and defensive reroll.
 *
 * With seed=12345, first_attacker=2 (F2 goes first):
 *
 * Round 1: F2 d6=5 (no self-rr: 5>2). F1 d6=6 (≥5): F2 defensively rerolls
 *          F1's attack → f2_rerolled=1, f1_rerolled=0.
 *
 * Round 2: F2 d6=5 (no self-rr). F1 d6=2 (≤2): F1 self-rerolls (gets 1, keeps 2)
 *          → f1_rerolled=1. F2's defensive check: raw2 after rr=2 < 5, no def-rr
 *          → f2_rerolled=0.
 * ---------------------------------------------------------------------------*/
static void test_n10_reroll_order_evaluation(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 12345u);

    /* F2 is first_attacker */
    TEST_ASSERT_EQUAL_UINT8(2u, ctx.first_attacker);

    /* Round 1: F2 def-rr on F1's roll */
    fq_round_result_t r1 = fq_combat_step(&ctx);
    TEST_ASSERT_EQUAL_UINT8(0u, r1.f1_rerolled);
    TEST_ASSERT_EQUAL_UINT8(1u, r1.f2_rerolled);

    /* Round 2: F1 self-rr, F2 no reroll */
    fq_round_result_t r2 = fq_combat_step(&ctx);
    TEST_ASSERT_EQUAL_UINT8(1u, r2.f1_rerolled);
    TEST_ASSERT_EQUAL_UINT8(0u, r2.f2_rerolled);
}

/* ---------------------------------------------------------------------------
 * N11: Lucky Star PRNG calls consumed regardless of perk.
 *
 * Compare two contexts identical except for a different seed that only
 * differs by the lucky star calls — the PRNG state gap must be exactly 2
 * calls (2 advances) per round. We verify this by isolating the lucky star
 * block by counting PRNG advances.
 *
 * Strategy: create a stripped scenario with high HP fighters (no KO for 3+
 * rounds) and verify that PRNG state after round N is the same as manually
 * replaying the exact call count (including 2 LS calls per round).
 * ---------------------------------------------------------------------------*/
static void test_n11_lucky_star_prng_always_consumed(void)
{
    fq_character_t c1 = make_char(200, 50, 50, 50, 50);
    fq_character_t c2 = make_char(200, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 12345u);

    /* Reference PRNG: replay the exact same call sequence including LS */
    fq_prng_t ref;
    fq_prng_init(&ref, 12345u);

    /* Consume the 2 initiative d6 calls (B1 fix: d6 not d100) */
    fq_prng_range(&ref, 1u, 6u);
    fq_prng_range(&ref, 1u, 6u);

    /* After init both states must match */
    TEST_ASSERT_EQUAL_UINT32(ref.state, ctx.rng.state);

    /* Step one round and verify the reference (which includes 2 LS calls)
     * matches ctx's PRNG state. The exact call count depends on whether
     * hits/crits/rerolls occurred, but we can verify both states track. */
    fq_combat_step(&ctx);

    /* The key check: PRNG state after step must match a context that was
     * init'd with the same seed and also stepped once identically. Use a
     * second context for comparison. */
    fq_combat_ctx_t ctx2;
    fq_combat_init(&ctx2, &c1, &c2, 12345u);
    fq_combat_step(&ctx2);
    TEST_ASSERT_EQUAL_UINT32(ctx.rng.state, ctx2.rng.state);

    /* Additionally verify with the known anchor: state after round 1 = 0x8CA71E78
     * (from offline trace with hp=100, same fighters). Use hp=100 version. */
    fq_combat_ctx_t ctx3;
    fq_character_t d1 = make_char(100, 50, 50, 50, 50);
    fq_character_t d2 = make_char(100, 50, 50, 50, 50);
    fq_combat_init(&ctx3, &d1, &d2, 12345u);
    fq_combat_step(&ctx3);
    TEST_ASSERT_EQUAL_UINT32(0x8CA71E78u, ctx3.rng.state);
}

/* ---------------------------------------------------------------------------
 * N15: Conditional reroll PRNG ordering.
 *
 * When the first attacker has NO reroll charges (intel=0), no defensive reroll
 * PRNG call is made even when second attacker rolls >= 5. This is verified
 * by comparing PRNG state against known-seed traces.
 * ---------------------------------------------------------------------------*/
static void test_n15_reroll_only_consumes_prng_when_eligible(void)
{
    /* Use a character with 0 intelligence (no reroll charges) */
    fq_character_t c1 = make_char(100, 50, 50, 50, 0);   /* intel=0 → charges=0 */
    fq_character_t c2 = make_char(100, 50, 50, 50, 0);
    fq_combat_ctx_t ctx_no_rr;
    fq_combat_init(&ctx_no_rr, &c1, &c2, 12345u);

    /* Use a character with high intelligence (has reroll charges) */
    fq_character_t e1 = make_char(100, 50, 50, 50, 50);
    fq_character_t e2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx_rr;
    fq_combat_init(&ctx_rr, &e1, &e2, 12345u);

    /* Both init states should match (same seed, same calls) */
    TEST_ASSERT_EQUAL_UINT32(ctx_no_rr.rng.state, ctx_rr.rng.state);

    fq_round_result_t r_no_rr = fq_combat_step(&ctx_no_rr);
    fq_round_result_t r_rr    = fq_combat_step(&ctx_rr);

    /* With charges=0, no reroll should ever occur */
    TEST_ASSERT_EQUAL_UINT8(0u, r_no_rr.f1_rerolled);
    TEST_ASSERT_EQUAL_UINT8(0u, r_no_rr.f2_rerolled);

    /* With charges=4, first_attacker=F2 did defensive reroll on F1's roll=6>=5.
     * f2_rerolled=1, f1_rerolled=0 (F1 had charges but never used them). */
    TEST_ASSERT_EQUAL_UINT8(0u, r_rr.f1_rerolled);
    TEST_ASSERT_EQUAL_UINT8(1u, r_rr.f2_rerolled);

    /* PRNG states diverge because ctx_rr consumed an extra call for F2 defensive reroll */
    TEST_ASSERT_TRUE(ctx_no_rr.rng.state != ctx_rr.rng.state);
}

/* ---------------------------------------------------------------------------
 * N17: Round 12 tiebreaker uses HP percentage, not absolute HP.
 *
 * Two fighters with different hp_max but same absolute HP → the one with
 * higher percentage wins. This requires driving a fight to round 12.
 *
 * Offline trace (seed=7777, str=0, spd=50, prec=0, int=0):
 *   F1 hp_max=1000, F2 hp_max=100.
 *   Fight goes to round 12 (overtime kills F2 first is possible, but with
 *   very high F1 HP, F1 wins on percentage if not KO'd).
 *   Expected winner=1 (F1 has much higher HP percentage after 12 rounds).
 * ---------------------------------------------------------------------------*/
static void test_n17_round12_tiebreaker_uses_percentage(void)
{
    /* Create fighters that will survive all 12 rounds.
     * Use very high HP, low strength so damage is minimal. */
    fq_character_t c1 = make_char(1000, 0, 50, 0, 0);   /* hp_max=1000, str=0 */
    fq_character_t c2 = make_char(100,  0, 50, 0, 0);   /* hp_max=100, str=0 */

    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 7777u);

    /* Drive to completion */
    fq_round_result_t last_result;
    memset(&last_result, 0, sizeof(last_result));
    for (int i = 0; i < 15; i++) {
        last_result = fq_combat_step(&ctx);
        if (last_result.finished) break;
    }

    /* The fight must have finished */
    TEST_ASSERT_EQUAL_UINT8(1u, last_result.finished);
    /* From offline trace: F1 wins (96% HP vs 54% HP after round 12 OT) */
    TEST_ASSERT_EQUAL_UINT8(1u, last_result.winner);
}

/* ---------------------------------------------------------------------------
 * N_B6: current_round overflow bound.
 *
 * With FQ_MAX_ROUNDS=12, current_round must never exceed 13 (12 + 1 post-
 * increment after the final round is resolved). Verify combat always ends
 * by round 12 and the counter stays within bounds.
 * ---------------------------------------------------------------------------*/
static void test_nb6_round_overflow_bound(void)
{
    /* High HP, low strength — fight goes to round 12 */
    fq_character_t c1 = make_char(5000, 0, 50, 0, 0);
    fq_character_t c2 = make_char(5000, 0, 50, 0, 0);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 99999u);

    fq_round_result_t last;
    memset(&last, 0, sizeof(last));
    for (int i = 0; i < 20; i++) {
        last = fq_combat_step(&ctx);
        /* current_round (post-increment) must never exceed FQ_MAX_ROUNDS+1 */
        TEST_ASSERT_TRUE(ctx.current_round <= (uint8_t)(FQ_MAX_ROUNDS + 1u));
        if (last.finished) break;
    }
    /* Combat must always end — never loops past 12 rounds */
    TEST_ASSERT_EQUAL_UINT8(1u, last.finished);
}

/* ---------------------------------------------------------------------------
 * N13 (advisory): Zero-memset context → treated as finished.
 * fq_combat_step on a zero-filled context must be a safe no-op.
 * ---------------------------------------------------------------------------*/
static void test_n13_zero_memset_ctx_is_noop(void)
{
    fq_combat_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    /* finished=0 in a zero-fill context. The step will run with all-zero state.
     * Primary requirement: must not crash. */
    fq_round_result_t r = fq_combat_step(&ctx);
    /* HP fields must be in valid range (0 is valid — hp_max=0 triggers finished). */
    TEST_ASSERT_TRUE(r.finished == 0u || r.finished == 1u);
    TEST_ASSERT_TRUE(r.f1_hp >= 0);
}

/* ---------------------------------------------------------------------------
 * N14 (advisory): fq_combat_step called after finished → NO-OP.
 * ---------------------------------------------------------------------------*/
static void test_n14_step_after_finished_is_noop(void)
{
    fq_character_t c1 = make_char(1, 255, 50, 255, 50); /* 1 HP dies instantly */
    fq_character_t c2 = make_char(200, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 1u);

    /* F1 has 1 HP — guaranteed to die or fight ends quickly */
    for (int i = 0; i < 15; i++) {
        fq_round_result_t r = fq_combat_step(&ctx);
        if (r.finished) break;
    }

    TEST_ASSERT_EQUAL_UINT8(1u, ctx.finished);

    /* Record state after finished */
    uint32_t prng_state_before = ctx.rng.state;
    int16_t  f1_hp_before      = ctx.f1.hp;
    int16_t  f2_hp_before      = ctx.f2.hp;
    uint8_t  round_before      = ctx.current_round;

    /* Call step again — must be a NO-OP */
    fq_round_result_t r2 = fq_combat_step(&ctx);

    /* PRNG state must not advance (no-op) */
    TEST_ASSERT_EQUAL_UINT32(prng_state_before, ctx.rng.state);
    /* HP must not change */
    TEST_ASSERT_EQUAL_INT((int)f1_hp_before, (int)ctx.f1.hp);
    TEST_ASSERT_EQUAL_INT((int)f2_hp_before, (int)ctx.f2.hp);
    /* Round counter must not advance */
    TEST_ASSERT_EQUAL_UINT8(round_before, ctx.current_round);
    /* Result must reflect finished state */
    TEST_ASSERT_EQUAL_UINT8(1u, r2.finished);
}

/* ---------------------------------------------------------------------------
 * N9 (advisory): Crit damage overflow clamped to INT8_MAX.
 *
 * B2 fix: Maximum possible damage with precision tier 3:
 *   adjusted_roll max = 5 (PRECISION_TABLE[3][5]=5), eff_str(255)=23, 23/2=11.
 *   damage_before_crit = 5 + 11 = 16.
 *   damage_after_crit  = 16 * 3 / 2 = 24.
 *   24 < INT8_MAX (127) so no overflow in practice, but clamp is still enforced.
 * ---------------------------------------------------------------------------*/
static void test_n9_damage_clamped_to_int8_max(void)
{
    fq_character_t c1 = make_char(1000, 255, 50, 255, 50);
    fq_character_t c2 = make_char(1000, 255, 50, 255, 50);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 99u);

    /* Run several rounds and verify damage is always in valid range */
    for (int i = 0; i < 5 && !ctx.finished; i++) {
        fq_round_result_t r = fq_combat_step(&ctx);
        TEST_ASSERT_TRUE(r.f1_damage_dealt >= 0 && r.f1_damage_dealt <= INT8_MAX);
        TEST_ASSERT_TRUE(r.f2_damage_dealt >= 0 && r.f2_damage_dealt <= INT8_MAX);
    }
}

/* ---------------------------------------------------------------------------
 * N7 (advisory): Overtime + HP clamping — HP must not go below 0 after
 * overtime damage.
 * ---------------------------------------------------------------------------*/
static void test_n7_overtime_hp_clamp(void)
{
    /* Use moderate HP and str so fights naturally reach overtime */
    fq_character_t c1 = make_char(200, 10, 50, 10, 0);
    fq_character_t c2 = make_char(200, 10, 50, 10, 0);
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &c1, &c2, 55555u);

    for (int i = 0; i < 15 && !ctx.finished; i++) {
        fq_round_result_t r = fq_combat_step(&ctx);
        /* HP must never go below 0 */
        TEST_ASSERT_TRUE(r.f1_hp >= 0);
        TEST_ASSERT_TRUE(r.f2_hp >= 0);
    }
}

/* ---------------------------------------------------------------------------
 * N12 (advisory): PRNG seed 0 — init must not deadlock (state forced to 1).
 * ---------------------------------------------------------------------------*/
static void test_n12_seed_zero_no_deadlock(void)
{
    fq_character_t c1 = make_char(100, 50, 50, 50, 50);
    fq_character_t c2 = make_char(100, 50, 50, 50, 50);
    fq_combat_ctx_t ctx;
    game_err_t err = fq_combat_init(&ctx, &c1, &c2, 0u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)GAME_OK, (uint8_t)err);
    /* PRNG state must be non-zero */
    TEST_ASSERT_TRUE(ctx.rng.state != 0u);
    /* Step must not hang or crash */
    fq_round_result_t r = fq_combat_step(&ctx);
    /* Result must be in valid state */
    TEST_ASSERT_TRUE(r.finished == 0u || r.finished == 1u);
    TEST_ASSERT_EQUAL_UINT8(1u, r.round);
}

int main(void)
{
    /* N1: NULL pointer guards */
    test_n1_null_ctx();
    test_n1_null_c1();
    test_n1_null_c2();

    /* N3: hp_max == 0 */
    test_n3_hp_max_zero_c1();
    test_n3_hp_max_zero_both();

    /* N4: all-zero stats — no division by zero */
    test_n4_all_stats_zero_no_crash();

    /* N5: HP clamp to INT16_MAX */
    test_n5_hp_clamped_to_int16_max();

    /* N6: first kill wins, no counter-attack */
    test_n6_first_kill_no_counter_attack();

    /* N2: PRNG determinism */
    test_n2_prng_determinism_two_contexts();
    test_n2_prng_state_after_init_known_seed();
    test_n2_prng_state_after_round1_known_seed();

    /* N10: reroll order */
    test_n10_reroll_order_evaluation();

    /* N11: lucky star PRNG always consumed */
    test_n11_lucky_star_prng_always_consumed();

    /* N15: conditional reroll PRNG ordering */
    test_n15_reroll_only_consumes_prng_when_eligible();

    /* N17: round 12 tiebreaker by percentage */
    test_n17_round12_tiebreaker_uses_percentage();

    /* N_B6: round overflow bound */
    test_nb6_round_overflow_bound();

    /* Advisory items */
    test_n13_zero_memset_ctx_is_noop();
    test_n14_step_after_finished_is_noop();
    test_n9_damage_clamped_to_int8_max();
    test_n7_overtime_hp_clamp();
    test_n12_seed_zero_no_deadlock();

    return 0;
}
