/**
 * test_level_up.c — Feature/happy-path tests for XP curve and level-up.
 * Phase B: FEATURE RED.
 *
 * Tests:
 *   - fq_calc_xp_to_next: pinned values at levels 0, 1, 10, 50, 98.
 *   - Single level-up: XP subtracted, level incremented, stats biased.
 *   - Bruiser level-up from level 1 to level 3 (multi-step).
 *   - XP=UINT32_MAX continuous level-up until level 99 (overflow robustness).
 *   - XP remaining after level-up is correct (subtraction contract).
 */

#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "test_assert.h"
#include "types.h"
#include "progression.h"

/* fq_calc_xp_to_next pinned values. */
static void test_xp_to_next_pinned_values(void)
{
    /* level 0: formula gives 50*0*0=0, but spec says min cost = 50. */
    TEST_ASSERT_EQUAL_UINT32(50u,     fq_calc_xp_to_next(0u));
    TEST_ASSERT_EQUAL_UINT32(50u,     fq_calc_xp_to_next(1u));
    TEST_ASSERT_EQUAL_UINT32(5000u,   fq_calc_xp_to_next(10u));
    TEST_ASSERT_EQUAL_UINT32(125000u, fq_calc_xp_to_next(50u));
    TEST_ASSERT_EQUAL_UINT32(480200u, fq_calc_xp_to_next(98u));
    /* level 99: sentinel, no more leveling. */
    TEST_ASSERT_EQUAL_UINT32(0u,      fq_calc_xp_to_next(99u));
}

/* Single level-up from level 1: XP exact, stats increment. */
static void test_bruiser_single_level_up(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.level      = 1u;
    ch.class_id   = (uint8_t)FQ_CLASS_BRUISER;
    ch.xp         = 50u;  /* fq_calc_xp_to_next(1) = 50 exactly. */
    ch.strength   = 3u;
    ch.speed      = 0u;
    ch.precision  = 0u;
    ch.intelligence = 0u;

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_level_up(&ch));
    TEST_ASSERT_EQUAL_UINT8(2u,  ch.level);     /* Level incremented. */
    TEST_ASSERT_EQUAL_UINT32(0u, ch.xp);        /* XP fully consumed. */
    TEST_ASSERT_EQUAL_UINT8(5u,  ch.strength);  /* 3 + 2 = 5. */
    TEST_ASSERT_EQUAL_UINT8(1u,  ch.speed);     /* 0 + 1 = 1. */
    TEST_ASSERT_EQUAL_UINT8(0u,  ch.precision);
    TEST_ASSERT_EQUAL_UINT8(0u,  ch.intelligence);
}

/* XP remainder is preserved after level-up. */
static void test_level_up_xp_remainder_preserved(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.level    = 1u;
    ch.class_id = (uint8_t)FQ_CLASS_HEX;
    ch.xp       = 75u;  /* 50 needed, 25 leftover. */

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_level_up(&ch));
    TEST_ASSERT_EQUAL_UINT8(2u,  ch.level);
    TEST_ASSERT_EQUAL_UINT32(25u, ch.xp);  /* Remainder. */
}

/* Multi-step: Bruiser from level 1 to level 3 using exact XP sums. */
static void test_bruiser_level_1_to_3(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.level    = 1u;
    ch.class_id = (uint8_t)FQ_CLASS_BRUISER;
    ch.strength = 3u;
    ch.speed    = 0u;
    /* xp_to_next(1)=50, xp_to_next(2)=200. Total = 250. */
    ch.xp = 50u + 200u;

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_level_up(&ch));
    TEST_ASSERT_EQUAL_UINT8(2u, ch.level);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_level_up(&ch));
    TEST_ASSERT_EQUAL_UINT8(3u, ch.level);
    TEST_ASSERT_EQUAL_UINT32(0u, ch.xp);
    /* STR after 2 level-ups: 3 + 2 + 2 = 7. */
    TEST_ASSERT_EQUAL_UINT8(7u, ch.strength);
    /* SPD after 2 level-ups: 0 + 1 + 1 = 2. */
    TEST_ASSERT_EQUAL_UINT8(2u, ch.speed);
}

/* XP=UINT32_MAX at level 1: pump level-ups until level 99 (overflow robustness).
 * This is the N8 spec-challenger case from the backlog. */
static void test_bruiser_max_xp_reaches_level_99_not_overflow(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.level    = 1u;
    ch.class_id = (uint8_t)FQ_CLASS_BRUISER;
    ch.strength = 3u;
    ch.speed    = 0u;
    ch.xp       = 0xFFFFFFFFu; /* UINT32_MAX. */

    /* Level up as many times as possible. */
    uint32_t iterations = 0u;
    while (ch.level < 99u) {
        game_err_t err = fq_level_up(&ch);
        if (err != GAME_OK) {
            break;
        }
        iterations++;
        /* Safety: never more than 99 iterations. */
        if (iterations > 100u) {
            TEST_ASSERT_FAIL("level-up loop exceeded 100 iterations");
        }
    }

    /* Must have reached exactly level 99, not wrapped or exceeded. */
    TEST_ASSERT_EQUAL_UINT8(99u, ch.level);

    /* Further level-up must be rejected. */
    game_err_t final_err = fq_level_up(&ch);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)final_err);

    /* STR must be 255 (saturated), not 0. */
    TEST_ASSERT_EQUAL_UINT8(255u, ch.strength);
}

/* Warden level-up: +1 STR, +1 SPD, +0 PRC, +1 INT. */
static void test_warden_single_level_up(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.level    = 1u;
    ch.class_id = (uint8_t)FQ_CLASS_WARDEN;
    ch.strength    = 1u;
    ch.speed       = 1u;
    ch.precision   = 0u;
    ch.intelligence = 1u;
    ch.xp = 50u;

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_level_up(&ch));
    TEST_ASSERT_EQUAL_UINT8(2u, ch.level);
    TEST_ASSERT_EQUAL_UINT8(2u, ch.strength);
    TEST_ASSERT_EQUAL_UINT8(2u, ch.speed);
    TEST_ASSERT_EQUAL_UINT8(0u, ch.precision);
    TEST_ASSERT_EQUAL_UINT8(2u, ch.intelligence);
}

/* Wildcard level-up: +1 STR, +1 SPD, +1 PRC, +0 INT. */
static void test_wildcard_single_level_up(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.level    = 1u;
    ch.class_id = (uint8_t)FQ_CLASS_WILDCARD;
    ch.strength    = 1u;
    ch.speed       = 1u;
    ch.precision   = 1u;
    ch.intelligence = 0u;
    ch.xp = 50u;

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_level_up(&ch));
    TEST_ASSERT_EQUAL_UINT8(2u, ch.level);
    TEST_ASSERT_EQUAL_UINT8(2u, ch.strength);
    TEST_ASSERT_EQUAL_UINT8(2u, ch.speed);
    TEST_ASSERT_EQUAL_UINT8(2u, ch.precision);
    TEST_ASSERT_EQUAL_UINT8(0u, ch.intelligence);
}

int main(void)
{
    test_xp_to_next_pinned_values();
    test_bruiser_single_level_up();
    test_level_up_xp_remainder_preserved();
    test_bruiser_level_1_to_3();
    test_bruiser_max_xp_reaches_level_99_not_overflow();
    test_warden_single_level_up();
    test_wildcard_single_level_up();
    return 0;
}
