/**
 * test_legacy.c — Feature/happy-path tests for rebirth and legacy tree.
 * Phase B: FEATURE RED.
 *
 * Tests:
 *   - fq_calc_rebirth_tokens: pinned values.
 *   - fq_legacy_tier(): correct tier for every node index.
 *   - fq_legacy_count_tier(): correct count in each tier.
 *   - fq_legacy_unlock_node(): happy-path unlock of T1 node.
 *   - fq_legacy_apply_bonuses(): stat bonuses applied correctly.
 *   - fq_rebirth(): basic stat halving (50% retention), is_dead cleared.
 *   - fq_rebirth(): rebirth_count increments, tokens added to legacy_points.
 *   - fq_rebirth(): PHOENIX_FLAME (75%) retention.
 *   - fq_rebirth(): Wildcard passive reroll.
 *   - Legacy tree: full T1→T2→T3→T4 unlock chain.
 */

#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "test_assert.h"
#include "types.h"
#include "prng.h"
#include "legacy.h"

/* ---------------------------------------------------------------------------
 * fq_calc_rebirth_tokens
 * ---------------------------------------------------------------------------*/
static void test_rebirth_tokens_level10_wins0_is_1(void)
{
    /* (10/10) + (0/100) = 1 + 0 = 1. */
    TEST_ASSERT_EQUAL_UINT8(1u, fq_calc_rebirth_tokens(10u, 0u));
}

static void test_rebirth_tokens_level20_wins100_is_3(void)
{
    /* (20/10) + (100/100) = 2 + 1 = 3. */
    TEST_ASSERT_EQUAL_UINT8(3u, fq_calc_rebirth_tokens(20u, 100u));
}

static void test_rebirth_tokens_level99_wins0_is_9(void)
{
    /* (99/10) + (0/100) = 9 + 0 = 9. */
    TEST_ASSERT_EQUAL_UINT8(9u, fq_calc_rebirth_tokens(99u, 0u));
}

/* ---------------------------------------------------------------------------
 * fq_legacy_tier
 * ---------------------------------------------------------------------------*/
static void test_legacy_tier_assignments(void)
{
    /* Nodes 0-3 → tier 1 (value 0). */
    TEST_ASSERT_EQUAL_UINT8(0u, fq_legacy_tier(0u));
    TEST_ASSERT_EQUAL_UINT8(0u, fq_legacy_tier(1u));
    TEST_ASSERT_EQUAL_UINT8(0u, fq_legacy_tier(2u));
    TEST_ASSERT_EQUAL_UINT8(0u, fq_legacy_tier(3u));
    /* Nodes 4-7 → tier 2 (value 1). */
    TEST_ASSERT_EQUAL_UINT8(1u, fq_legacy_tier(4u));
    TEST_ASSERT_EQUAL_UINT8(1u, fq_legacy_tier(7u));
    /* Nodes 8-11 → tier 3 (value 2). */
    TEST_ASSERT_EQUAL_UINT8(2u, fq_legacy_tier(8u));
    TEST_ASSERT_EQUAL_UINT8(2u, fq_legacy_tier(11u));
    /* Nodes 12-15 → tier 4 (value 3). */
    TEST_ASSERT_EQUAL_UINT8(3u, fq_legacy_tier(12u));
    TEST_ASSERT_EQUAL_UINT8(3u, fq_legacy_tier(15u));
}

/* ---------------------------------------------------------------------------
 * fq_legacy_count_tier
 * ---------------------------------------------------------------------------*/
static void test_legacy_count_tier_empty_tree(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, fq_legacy_count_tier(0u, 0u));
    TEST_ASSERT_EQUAL_UINT8(0u, fq_legacy_count_tier(0u, 1u));
}

static void test_legacy_count_tier_all_t1_set(void)
{
    /* Bits 0-3 set (T1 nodes). */
    uint32_t tree = 0x0Fu;
    TEST_ASSERT_EQUAL_UINT8(4u, fq_legacy_count_tier(tree, 0u)); /* All 4 T1. */
    TEST_ASSERT_EQUAL_UINT8(0u, fq_legacy_count_tier(tree, 1u)); /* No T2. */
}

static void test_legacy_count_tier_mixed(void)
{
    /* Bit 0 (T1) + bit 4 (T2) + bit 8 (T3). */
    uint32_t tree = (1u << 0) | (1u << 4) | (1u << 8);
    TEST_ASSERT_EQUAL_UINT8(1u, fq_legacy_count_tier(tree, 0u)); /* 1 T1. */
    TEST_ASSERT_EQUAL_UINT8(1u, fq_legacy_count_tier(tree, 1u)); /* 1 T2. */
    TEST_ASSERT_EQUAL_UINT8(1u, fq_legacy_count_tier(tree, 2u)); /* 1 T3. */
    TEST_ASSERT_EQUAL_UINT8(0u, fq_legacy_count_tier(tree, 3u)); /* 0 T4. */
}

/* ---------------------------------------------------------------------------
 * fq_legacy_unlock_node — happy path
 * ---------------------------------------------------------------------------*/
static void test_unlock_t1_node_succeeds(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_tree   = 0u;
    ch.legacy_points = 1u;

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_legacy_unlock_node(&ch, 0u));
    /* Node 0 bit is now set. */
    TEST_ASSERT_TRUE((ch.legacy_tree & FQ_LEGACY_THICK_SKIN_1) != 0u);
    /* One legacy point was spent. */
    TEST_ASSERT_EQUAL_UINT8(0u, ch.legacy_points);
}

static void test_unlock_t2_node_with_t1_prereq_succeeds(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    /* One T1 node already unlocked. */
    ch.legacy_tree   = FQ_LEGACY_THICK_SKIN_1;
    ch.legacy_points = 1u;

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_legacy_unlock_node(&ch, 4u));
    TEST_ASSERT_TRUE((ch.legacy_tree & FQ_LEGACY_IRON_WILL) != 0u);
    TEST_ASSERT_EQUAL_UINT8(0u, ch.legacy_points);
}

static void test_unlock_t3_node_with_two_t2_prereqs_succeeds(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    /* Two T2 nodes unlocked. */
    ch.legacy_tree   = FQ_LEGACY_IRON_WILL | FQ_LEGACY_SCAVENGER;
    ch.legacy_points = 1u;

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_legacy_unlock_node(&ch, 8u));
    TEST_ASSERT_TRUE((ch.legacy_tree & FQ_LEGACY_PHOENIX_FLAME) != 0u);
}

static void test_unlock_t4_node_with_two_t3_prereqs_succeeds(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    /* Two T3 nodes unlocked. */
    ch.legacy_tree   = FQ_LEGACY_PHOENIX_FLAME | FQ_LEGACY_VETERAN_MARK;
    ch.legacy_points = 1u;

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_legacy_unlock_node(&ch, 12u));
    TEST_ASSERT_TRUE((ch.legacy_tree & FQ_LEGACY_MASTER_MIND) != 0u);
}

/* ---------------------------------------------------------------------------
 * fq_legacy_apply_bonuses
 * ---------------------------------------------------------------------------*/
static void test_apply_bonuses_thick_skin_1_adds_5hp(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id  = (uint8_t)FQ_CLASS_WARDEN;
    ch.hp_max    = 55u;  /* Warden base HP. */
    ch.legacy_tree = FQ_LEGACY_THICK_SKIN_1;
    fq_legacy_apply_bonuses(&ch);
    TEST_ASSERT_EQUAL_UINT16(60u, ch.hp_max); /* 55 + 5 = 60. */
}

static void test_apply_bonuses_both_thick_skin_adds_10hp(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id    = (uint8_t)FQ_CLASS_WARDEN;
    ch.hp_max      = 55u;
    ch.legacy_tree = FQ_LEGACY_THICK_SKIN_1 | FQ_LEGACY_THICK_SKIN_2;
    fq_legacy_apply_bonuses(&ch);
    TEST_ASSERT_EQUAL_UINT16(65u, ch.hp_max); /* 55 + 5 + 5 = 65. */
}

static void test_apply_bonuses_keen_eye_adds_2prc(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.precision   = 0u;
    ch.legacy_tree = FQ_LEGACY_KEEN_EYE;
    fq_legacy_apply_bonuses(&ch);
    TEST_ASSERT_EQUAL_UINT8(2u, ch.precision);
}

static void test_apply_bonuses_quick_feet_adds_2spd(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.speed       = 0u;
    ch.legacy_tree = FQ_LEGACY_QUICK_FEET;
    fq_legacy_apply_bonuses(&ch);
    TEST_ASSERT_EQUAL_UINT8(2u, ch.speed);
}

static void test_apply_bonuses_empty_tree_no_change(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id  = (uint8_t)FQ_CLASS_BRUISER;
    ch.hp_max    = 60u;
    ch.strength  = 3u;
    ch.legacy_tree = 0u;
    fq_legacy_apply_bonuses(&ch);
    TEST_ASSERT_EQUAL_UINT16(60u, ch.hp_max);
    TEST_ASSERT_EQUAL_UINT8(3u, ch.strength);
}

/* ---------------------------------------------------------------------------
 * fq_rebirth — basic 50% stat retention
 * ---------------------------------------------------------------------------*/
static void test_rebirth_50pct_retention_bruiser(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = (uint8_t)FQ_CLASS_BRUISER;
    ch.is_dead       = 1u;
    ch.level         = 10u;
    ch.strength      = 23u;  /* base=3; gained=20; half=10; result=13. */
    ch.speed         = 10u;  /* base=0; gained=10; half=5; result=5. */
    ch.precision     = 4u;   /* base=0; gained=4; half=2; result=2. */
    ch.intelligence  = 4u;   /* base=0; gained=4; half=2; result=2. */
    ch.legacy_tree   = 0u;   /* No retention perks. */
    ch.rebirth_count = 0u;
    ch.legacy_points = 0u;
    ch.wins          = 0u;

    fq_prng_t rng;
    fq_prng_init(&rng, 42u);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_rebirth(&ch, &rng));

    TEST_ASSERT_EQUAL_UINT8(0u,  ch.is_dead);
    TEST_ASSERT_EQUAL_UINT8(1u,  ch.rebirth_count);
    /* Stats: base + half of gained. */
    TEST_ASSERT_EQUAL_UINT8(13u, ch.strength);   /* 3 + 10. */
    TEST_ASSERT_EQUAL_UINT8(5u,  ch.speed);      /* 0 + 5. */
    TEST_ASSERT_EQUAL_UINT8(2u,  ch.precision);  /* 0 + 2. */
    TEST_ASSERT_EQUAL_UINT8(2u,  ch.intelligence);/* 0 + 2. */
}

static void test_rebirth_clears_is_dead_and_increments_count(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = (uint8_t)FQ_CLASS_WARDEN;
    ch.is_dead       = 1u;
    ch.level         = 5u;
    ch.strength      = 5u;
    ch.speed         = 5u;
    ch.precision     = 2u;
    ch.intelligence  = 5u;
    ch.rebirth_count = 2u;
    ch.legacy_points = 0u;

    fq_prng_t rng;
    fq_prng_init(&rng, 1u);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_rebirth(&ch, &rng));
    TEST_ASSERT_EQUAL_UINT8(0u, ch.is_dead);
    TEST_ASSERT_EQUAL_UINT8(3u, ch.rebirth_count);  /* Incremented from 2. */
}

static void test_rebirth_adds_tokens_to_legacy_points(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = (uint8_t)FQ_CLASS_WARDEN;
    ch.is_dead       = 1u;
    ch.level         = 20u;  /* tokens = (20/10) + (0/100) = 2. */
    ch.wins          = 0u;
    ch.legacy_points = 5u;

    fq_prng_t rng;
    fq_prng_init(&rng, 1u);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_rebirth(&ch, &rng));
    /* 5 + 2 = 7. */
    TEST_ASSERT_EQUAL_UINT8(7u, ch.legacy_points);
}

/* PHOENIX_FLAME (75%) must override SOFT_LANDING (60%). */
static void test_rebirth_phoenix_flame_75pct_retention(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = (uint8_t)FQ_CLASS_BRUISER;
    ch.is_dead       = 1u;
    ch.level         = 5u;
    ch.strength      = 23u;  /* base=3; gained=20. */
    ch.speed         = 0u;
    ch.precision     = 0u;
    ch.intelligence  = 0u;
    /* PHOENIX_FLAME requires 2 T3 prerequisites — but fq_rebirth reads the
     * legacy_tree at runtime, not from the prerequisite validator. We set
     * the bit directly to test the stat calculation path. */
    ch.legacy_tree   = FQ_LEGACY_PHOENIX_FLAME;
    ch.legacy_points = 0u;

    fq_prng_t rng;
    fq_prng_init(&rng, 1u);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_rebirth(&ch, &rng));
    /* 75% retention: 3 + floor(20 * 75 / 100) = 3 + 15 = 18. */
    TEST_ASSERT_EQUAL_UINT8(18u, ch.strength);
}

/* Wildcard passive must be rerolled on rebirth and stay in [0, 3]. */
static void test_rebirth_wildcard_passive_rerolled(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id         = (uint8_t)FQ_CLASS_WILDCARD;
    ch.is_dead          = 1u;
    ch.wildcard_passive = 0u;

    fq_prng_t rng;
    fq_prng_init(&rng, 0xCAFEu);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_rebirth(&ch, &rng));
    TEST_ASSERT_TRUE(ch.wildcard_passive <= 3u);
}

/* Non-Wildcard class: wildcard_passive must NOT change on rebirth. */
static void test_rebirth_non_wildcard_passive_unchanged(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id         = (uint8_t)FQ_CLASS_BRUISER;
    ch.is_dead          = 1u;
    ch.wildcard_passive = 2u; /* Set to some value. */

    fq_prng_t rng;
    fq_prng_init(&rng, 0xCAFEu);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_rebirth(&ch, &rng));
    /* PRNG was NOT consumed for a non-Wildcard. */
    TEST_ASSERT_EQUAL_UINT8(2u, ch.wildcard_passive);
}

/* Rebirth stat floor: Trickster base SPD=3. If character SPD=3 (at base),
 * after rebirth SPD must still be 3. */
static void test_rebirth_trickster_spd_floor_at_base(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id  = (uint8_t)FQ_CLASS_TRICKSTER;
    ch.is_dead   = 1u;
    ch.level     = 1u;
    ch.strength  = 0u;
    ch.speed     = 3u;  /* Exactly at Trickster base SPD. */
    ch.precision = 1u;  /* Trickster base PRC=1. */
    ch.intelligence = 0u;
    ch.legacy_tree = 0u;

    fq_prng_t rng;
    fq_prng_init(&rng, 1u);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_rebirth(&ch, &rng));
    /* After rebirth with 50% of 0 gained → floor at base 3. */
    TEST_ASSERT_TRUE(ch.speed >= 3u);
}

int main(void)
{
    test_rebirth_tokens_level10_wins0_is_1();
    test_rebirth_tokens_level20_wins100_is_3();
    test_rebirth_tokens_level99_wins0_is_9();
    test_legacy_tier_assignments();
    test_legacy_count_tier_empty_tree();
    test_legacy_count_tier_all_t1_set();
    test_legacy_count_tier_mixed();
    test_unlock_t1_node_succeeds();
    test_unlock_t2_node_with_t1_prereq_succeeds();
    test_unlock_t3_node_with_two_t2_prereqs_succeeds();
    test_unlock_t4_node_with_two_t3_prereqs_succeeds();
    test_apply_bonuses_thick_skin_1_adds_5hp();
    test_apply_bonuses_both_thick_skin_adds_10hp();
    test_apply_bonuses_keen_eye_adds_2prc();
    test_apply_bonuses_quick_feet_adds_2spd();
    test_apply_bonuses_empty_tree_no_change();
    test_rebirth_50pct_retention_bruiser();
    test_rebirth_clears_is_dead_and_increments_count();
    test_rebirth_adds_tokens_to_legacy_points();
    test_rebirth_phoenix_flame_75pct_retention();
    test_rebirth_wildcard_passive_rerolled();
    test_rebirth_non_wildcard_passive_unchanged();
    test_rebirth_trickster_spd_floor_at_base();
    return 0;
}
