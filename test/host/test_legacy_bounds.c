/**
 * test_legacy_bounds.c — Bound/negative tests for rebirth and legacy tree.
 * Phase A: BOUND RED (Rule 22 — bound tests before feature tests).
 *
 * Covers:
 *   N12 — Rebirth stat floor: stats never drop below class base.
 *   N13 — Retention perks: SOFT_LANDING=60%, PHOENIX_FLAME=75%, plain=50%.
 *   N14 — rebirth_count saturates at 255 (never wraps).
 *   N15 — legacy_points saturates at 255 (never wraps).
 *   N16 — Legacy tree tier prerequisites enforced.
 *   N17 — Spending legacy points with 0 points rejected.
 *   N18 — Re-unlocking an already-unlocked node rejected.
 *   N19 — fq_calc_rebirth_tokens: edge case level=0, wins=0 → tokens=0.
 *   N20 — fq_calc_rebirth_tokens: saturation at 255.
 *   N21 — Full legacy tree bonuses applied without stat overflow.
 *   N22 — NULL pointer guard on fq_rebirth() and fq_legacy_unlock_node().
 *   N23 — Rebirth on alive character rejected.
 *   N24 — Wildcard passive reroll uses PRNG (result pinned to exact value).
 *   N25 — fq_calc_rebirth_tokens: large inputs handled without overflow.
 *   N26 — T4 prerequisite rejection: only 1 T3 node is insufficient.
 *   N27 — OOB node index (16, 255) rejected.
 *   N28 — Dual-perk (SOFT_LANDING + PHOENIX_FLAME): max rate wins at 75%.
 */

#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "test_assert.h"
#include "types.h"
#include "prng.h"
#include "legacy.h"

/* ---------------------------------------------------------------------------
 * Helper: build a minimal dead character of the given class.
 * ---------------------------------------------------------------------------*/
static fq_character_t make_dead_char(fq_class_t cls)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id = (uint8_t)cls;
    ch.is_dead  = 1u;
    ch.level    = 10u;
    ch.wins     = 0u;
    /* Set stats well above base so we can observe retention. */
    ch.strength    = 20u;
    ch.speed       = 20u;
    ch.precision   = 20u;
    ch.intelligence = 20u;
    return ch;
}

/* N22: NULL pointer guard on fq_rebirth(). */
static void test_rebirth_null_char_rejected(void)
{
    fq_prng_t rng;
    fq_prng_init(&rng, 0x1234u);
    game_err_t err = fq_rebirth(NULL, &rng);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_NULL_PTR, (int)err);
}

static void test_rebirth_null_rng_rejected(void)
{
    fq_character_t ch = make_dead_char(FQ_CLASS_BRUISER);
    game_err_t err = fq_rebirth(&ch, NULL);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_NULL_PTR, (int)err);
}

/* N22: NULL pointer guard on fq_legacy_unlock_node(). */
static void test_unlock_node_null_rejected(void)
{
    game_err_t err = fq_legacy_unlock_node(NULL, 0u);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_NULL_PTR, (int)err);
}

/* N23: Rebirth on alive character (is_dead=0) rejected. */
static void test_rebirth_alive_char_rejected(void)
{
    fq_character_t ch = make_dead_char(FQ_CLASS_BRUISER);
    ch.is_dead = 0u;  /* Alive — rebirth should be refused. */
    fq_prng_t rng;
    fq_prng_init(&rng, 0xABCDu);
    game_err_t err = fq_rebirth(&ch, &rng);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
}

/* N12: Rebirth stat floor — stats may not drop below class base.
 * Bruiser base STR=3. Character with STR=3 (at base) must not drop below 3. */
static void test_rebirth_stat_floor_at_class_base(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id  = (uint8_t)FQ_CLASS_BRUISER;
    ch.is_dead   = 1u;
    ch.level     = 1u;
    ch.strength  = 3u;  /* Exactly at Bruiser base. */
    ch.speed     = 0u;  /* At Bruiser SPD base (0). */
    ch.precision = 0u;
    ch.intelligence = 0u;
    fq_prng_t rng;
    fq_prng_init(&rng, 1u);
    game_err_t err = fq_rebirth(&ch, &rng);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    /* After rebirth, STR must equal Bruiser base (3): gained=0 so retain(3, 3, 50%)=3. */
    TEST_ASSERT_EQUAL_UINT8(3u, ch.strength);
}

/* N14: rebirth_count saturates at 255, never wraps to 0. */
static void test_rebirth_count_saturates_at_255(void)
{
    fq_character_t ch = make_dead_char(FQ_CLASS_WARDEN);
    ch.rebirth_count = 255u;  /* Already at max. */
    fq_prng_t rng;
    fq_prng_init(&rng, 42u);
    game_err_t err = fq_rebirth(&ch, &rng);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    /* Must remain 255, not wrap to 0. */
    TEST_ASSERT_EQUAL_UINT8(255u, ch.rebirth_count);
}

/* N15: legacy_points saturates at 255, never wraps. */
static void test_legacy_points_saturate_at_255(void)
{
    fq_character_t ch = make_dead_char(FQ_CLASS_WARDEN);
    ch.legacy_points = 255u; /* Already at max. */
    ch.level = 10u;          /* fq_calc_rebirth_tokens(10, 0) = 1 → would overflow. */
    fq_prng_t rng;
    fq_prng_init(&rng, 7u);
    game_err_t err = fq_rebirth(&ch, &rng);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    /* Must remain 255. */
    TEST_ASSERT_EQUAL_UINT8(255u, ch.legacy_points);
}

/* N16: Tier-2 node requires at least 1 tier-1 perk unlocked.
 * Unlocking node 4 (FQ_LEGACY_IRON_WILL, tier 2) with empty tree must fail. */
static void test_tier2_node_requires_tier1_prerequisite(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_tree   = 0u;  /* No perks. */
    ch.legacy_points = 5u;  /* Enough points. */
    /* Node index 4 is tier 2 — should be blocked without any tier-1 perk. */
    game_err_t err = fq_legacy_unlock_node(&ch, 4u);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
}

/* N16: Tier-3 node requires at least 2 tier-2 perks. */
static void test_tier3_node_requires_two_tier2_perks(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    /* Give one tier-1 and one tier-2 perk only (insufficient for tier 3). */
    ch.legacy_tree   = (1u << 0) | (1u << 4); /* T1 bit0 + T2 bit4. */
    ch.legacy_points = 5u;
    /* Node index 8 is tier 3 — needs 2 tier-2 nodes. */
    game_err_t err = fq_legacy_unlock_node(&ch, 8u);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
}

/* N17: Unlocking a node with 0 legacy_points rejected. */
static void test_unlock_node_zero_points_rejected(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_tree   = 0u;
    ch.legacy_points = 0u; /* No points to spend. */
    game_err_t err = fq_legacy_unlock_node(&ch, 0u); /* Tier-1 node, no prereqs needed. */
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
}

/* N18: Re-unlocking an already-unlocked node rejected. */
static void test_unlock_node_already_unlocked_rejected(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_tree   = (1u << 0); /* Node 0 already unlocked. */
    ch.legacy_points = 5u;
    game_err_t err = fq_legacy_unlock_node(&ch, 0u);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
}

/* N19: fq_calc_rebirth_tokens(0, 0) == 0. */
static void test_rebirth_tokens_level0_wins0_is_zero(void)
{
    uint8_t tokens = fq_calc_rebirth_tokens(0u, 0u);
    TEST_ASSERT_EQUAL_UINT8(0u, tokens);
}

/* N20: fq_calc_rebirth_tokens saturation at 255.
 * (255/10) + (65535/100) = 25 + 655 = 680 → saturated at 255. */
static void test_rebirth_tokens_saturate_at_255(void)
{
    uint8_t tokens = fq_calc_rebirth_tokens(255u, 65535u);
    TEST_ASSERT_EQUAL_UINT8(255u, tokens);
}

/* N21: Full legacy tree (all 16 nodes) does not overflow hp_max or stats.
 * fq_legacy_apply_bonuses with all bits set must produce exact expected hp_max.
 * Warden base HP=55. Bonuses with all 16 bits set:
 *   THICK_SKIN_1 (+5) + THICK_SKIN_2 (+5) + VETERAN_MARK (+10) + DIAMOND_SKIN (+15) = +35
 *   Total: 55 + 5 + 5 + 10 + 15 = 90. */
static void test_full_legacy_tree_no_overflow(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id  = (uint8_t)FQ_CLASS_WARDEN;
    ch.hp_max    = 55u;  /* Warden base HP. */
    ch.strength  = 0u;
    ch.speed     = 0u;
    ch.precision = 0u;
    ch.intelligence = 0u;
    /* Bits 16-31 are reserved — only bits 0-15 defined.
     * Apply all defined bits; reserved bits must be silently ignored. */
    ch.legacy_tree = 0xFFFFFFFFu;
    fq_legacy_apply_bonuses(&ch);
    /* hp_max: 55 + 5 (THICK_SKIN_1) + 5 (THICK_SKIN_2) + 10 (VETERAN_MARK)
     *            + 15 (DIAMOND_SKIN) = 90. */
    TEST_ASSERT_EQUAL_UINT16(90u, ch.hp_max);
}

/* N24: Wildcard passive reroll on rebirth must produce a pinned exact value.
 * seed=0xDEADBEEF: fq_prng_init(&rng, 0xDEADBEEF); fq_prng_range(&rng, 0, 3) == 3. */
static void test_wildcard_passive_reroll_pinned_deadbeef(void)
{
    fq_character_t ch = make_dead_char(FQ_CLASS_WILDCARD);
    ch.wildcard_passive = 0u;
    fq_prng_t rng;
    fq_prng_init(&rng, 0xDEADBEEFu);
    game_err_t err = fq_rebirth(&ch, &rng);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    /* Pinned: seed=0xDEADBEEF, fq_prng_range(0, 3) == 3. */
    TEST_ASSERT_EQUAL_UINT8(3u, ch.wildcard_passive);
}

/* N13: Retention perk SOFT_LANDING (60%) vs plain 50%.
 * Bruiser base STR=3. Character with STR=23 (20 gained above base).
 * 50% retention: base(3) + floor((23-3)*50/100) = 3 + 10 = 13.
 * 60% retention: base(3) + floor((23-3)*60/100) = 3 + 12 = 15.
 * With SOFT_LANDING, result must be 15, not 13. */
static void test_soft_landing_gives_60pct_retention(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id  = (uint8_t)FQ_CLASS_BRUISER;
    ch.is_dead   = 1u;
    ch.level     = 5u;
    ch.strength  = 23u;  /* 20 above base of 3. */
    ch.speed     = 3u;   /* 3 above base of 0. */
    ch.precision = 3u;
    ch.intelligence = 3u;
    /* Unlock SOFT_LANDING (bit 6). To satisfy prerequisites, also set 1 T1 and 1 T2. */
    ch.legacy_tree = FQ_LEGACY_THICK_SKIN_1 | FQ_LEGACY_IRON_WILL | FQ_LEGACY_SOFT_LANDING;
    fq_prng_t rng;
    fq_prng_init(&rng, 1u);
    game_err_t err = fq_rebirth(&ch, &rng);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    /* 60% retention: 3 + floor(20 * 60 / 100) = 3 + 12 = 15. */
    TEST_ASSERT_EQUAL_UINT8(15u, ch.strength);
}

/* N25: fq_calc_rebirth_tokens with large inputs verifies correct intermediate
 * arithmetic. level=99, wins=65535:
 *   (99/10) + (65535/100) = 9 + 655 = 664 → clamped to 255. */
static void test_rebirth_tokens_large_inputs_no_intermediate_overflow(void)
{
    uint8_t tokens = fq_calc_rebirth_tokens(99u, 65535u);
    /* Pre-clamp = 9 + 655 = 664 — fits in uint32_t intermediate without overflow. */
    TEST_ASSERT_EQUAL_UINT8(255u, tokens);
}

/* N26: T4 prerequisite rejection — only 1 T3 node set (need 2).
 * Set exactly 1 T3 node, attempt to unlock T4 node (index 12), assert GAME_ERR_INVALID
 * and legacy_points unchanged. */
static void test_t4_unlock_requires_two_t3_prereqs(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    /* One T3 node only — insufficient for T4. */
    ch.legacy_tree   = FQ_LEGACY_PHOENIX_FLAME; /* bit 8, T3, only 1 node. */
    ch.legacy_points = 5u;
    uint8_t points_before = ch.legacy_points;
    game_err_t err = fq_legacy_unlock_node(&ch, 12u); /* MASTER_MIND, T4 node. */
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
    /* Points must be unchanged — rejection must not consume a point. */
    TEST_ASSERT_EQUAL_UINT8(points_before, ch.legacy_points);
}

/* N27: Out-of-bounds node index (16 and 255) both return GAME_ERR_INVALID. */
static void test_oob_node_index_16_rejected(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_points = 5u;
    game_err_t err = fq_legacy_unlock_node(&ch, 16u);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
}

static void test_oob_node_index_255_rejected(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_points = 5u;
    game_err_t err = fq_legacy_unlock_node(&ch, 255u);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
}

/* N28: Dual-perk (SOFT_LANDING + PHOENIX_FLAME) — max rate wins at 75%.
 * Bruiser base STR=3. Character STR=23 (gained=20).
 * Expected: 3 + floor(20 * 75 / 100) = 3 + 15 = 18. */
static void test_dual_perk_soft_landing_and_phoenix_flame_uses_75pct(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id  = (uint8_t)FQ_CLASS_BRUISER;
    ch.is_dead   = 1u;
    ch.level     = 5u;
    ch.strength  = 23u;  /* base=3; gained=20. */
    ch.speed     = 0u;
    ch.precision = 0u;
    ch.intelligence = 0u;
    /* Set both retention perks directly in the tree bitmask. */
    ch.legacy_tree = FQ_LEGACY_SOFT_LANDING | FQ_LEGACY_PHOENIX_FLAME;
    ch.legacy_points = 0u;
    fq_prng_t rng;
    fq_prng_init(&rng, 1u);
    game_err_t err = fq_rebirth(&ch, &rng);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    /* PHOENIX_FLAME (75%) dominates SOFT_LANDING (60%):
     * 3 + floor(20 * 75 / 100) = 3 + 15 = 18. */
    TEST_ASSERT_EQUAL_UINT8(18u, ch.strength);
}

int main(void)
{
    test_rebirth_null_char_rejected();
    test_rebirth_null_rng_rejected();
    test_unlock_node_null_rejected();
    test_rebirth_alive_char_rejected();
    test_rebirth_stat_floor_at_class_base();
    test_rebirth_count_saturates_at_255();
    test_legacy_points_saturate_at_255();
    test_tier2_node_requires_tier1_prerequisite();
    test_tier3_node_requires_two_tier2_perks();
    test_unlock_node_zero_points_rejected();
    test_unlock_node_already_unlocked_rejected();
    test_rebirth_tokens_level0_wins0_is_zero();
    test_rebirth_tokens_saturate_at_255();
    test_full_legacy_tree_no_overflow();
    test_wildcard_passive_reroll_pinned_deadbeef();
    test_soft_landing_gives_60pct_retention();
    test_rebirth_tokens_large_inputs_no_intermediate_overflow();
    test_t4_unlock_requires_two_t3_prereqs();
    test_oob_node_index_16_rejected();
    test_oob_node_index_255_rejected();
    test_dual_perk_soft_landing_and_phoenix_flame_uses_75pct();
    return 0;
}
