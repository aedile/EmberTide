/**
 * test_level_up_bounds.c — Bound/negative tests for XP curve and level-up.
 * Phase A: BOUND RED (Rule 22 — bound tests before feature tests).
 *
 * Covers:
 *   N5  — fq_calc_xp_to_next(99) returns 0 (no more leveling).
 *   N6  — fq_calc_xp_to_next(0) returns non-zero (== 50).
 *   N7  — fq_level_up() at level 99 rejected with GAME_ERR_INVALID.
 *   N8  — Class-biased stat gains for all 5 classes (pinned values).
 *   N9  — Stat cap: uint8_t saturation at 255 (never wraps to 0).
 *   N10 — NULL pointer guard for fq_level_up().
 *   N11 — fq_level_up() when xp < xp_to_next returns GAME_ERR_INVALID.
 */

#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "test_assert.h"
#include "types.h"
#include "progression.h"

/* N6: level 0 → xp_to_next must be 50 (50 * 0 * 0 = 0 but floor at 50 per spec). */
static void test_xp_to_next_level_0_is_50(void)
{
    uint32_t result = fq_calc_xp_to_next(0u);
    TEST_ASSERT_EQUAL_UINT32(50u, result);
}

/* N5: level 99 → xp_to_next must be 0 (sentinel: no more leveling). */
static void test_xp_to_next_level_99_is_zero(void)
{
    uint32_t result = fq_calc_xp_to_next(99u);
    TEST_ASSERT_EQUAL_UINT32(0u, result);
}

/* Pinned: level 1 → 50 * 1 * 1 = 50. */
static void test_xp_to_next_level_1_is_50(void)
{
    uint32_t result = fq_calc_xp_to_next(1u);
    TEST_ASSERT_EQUAL_UINT32(50u, result);
}

/* Pinned: level 10 → 50 * 10 * 10 = 5000. */
static void test_xp_to_next_level_10_is_5000(void)
{
    uint32_t result = fq_calc_xp_to_next(10u);
    TEST_ASSERT_EQUAL_UINT32(5000u, result);
}

/* Pinned: level 98 → 50 * 98 * 98 = 480200. Must NOT be 0. */
static void test_xp_to_next_level_98_is_nonzero(void)
{
    uint32_t result = fq_calc_xp_to_next(98u);
    TEST_ASSERT_EQUAL_UINT32(480200u, result);
}

/* N7: fq_level_up at level 99 must return GAME_ERR_INVALID. */
static void test_level_up_at_99_rejected(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.level = 99u;
    ch.xp    = 0xFFFFFFFFu; /* Max XP — does not matter, level cap applies first. */
    game_err_t err = fq_level_up(&ch);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
    /* Level must be unchanged. */
    TEST_ASSERT_EQUAL_UINT8(99u, ch.level);
}

/* N11: fq_level_up when xp < xp_to_next returns GAME_ERR_INVALID. */
static void test_level_up_insufficient_xp_rejected(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.level    = 1u;
    ch.xp       = 49u; /* xp_to_next(1) = 50; 49 < 50 → reject. */
    ch.class_id = (uint8_t)FQ_CLASS_BRUISER;
    game_err_t err = fq_level_up(&ch);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
    /* Level and XP must be unchanged. */
    TEST_ASSERT_EQUAL_UINT8(1u, ch.level);
    TEST_ASSERT_EQUAL_UINT32(49u, ch.xp);
}

/* N10: NULL pointer returns GAME_ERR_NULL_PTR. */
static void test_level_up_null_ptr_rejected(void)
{
    game_err_t err = fq_level_up(NULL);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_NULL_PTR, (int)err);
}

/* N9: Stat saturation — Bruiser gets +2 STR per level-up.
 * Starting at STR=254, one level-up must produce STR=255, not STR=0 (wrap). */
static void test_stat_saturates_at_255_not_wrap(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.level    = 1u;
    ch.class_id = (uint8_t)FQ_CLASS_BRUISER;
    ch.strength = 254u;    /* One +2 gain would push to 256 → must saturate at 255. */
    ch.xp       = 50u;     /* Exactly xp_to_next(1). */
    game_err_t err = fq_level_up(&ch);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8(255u, ch.strength); /* Saturated, not 0. */
}

/* N9: Verify all five classes produce the correct stat biases on a single level-up.
 * Confirms that stat gains are class-biased (3 total per level). */
static void test_bruiser_gains_2str_1spd(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.level    = 1u;
    ch.class_id = (uint8_t)FQ_CLASS_BRUISER;
    ch.strength    = 0u;
    ch.speed       = 0u;
    ch.precision   = 0u;
    ch.intelligence = 0u;
    ch.xp = 50u;
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_level_up(&ch));
    TEST_ASSERT_EQUAL_UINT8(2u, ch.strength);
    TEST_ASSERT_EQUAL_UINT8(1u, ch.speed);
    TEST_ASSERT_EQUAL_UINT8(0u, ch.precision);
    TEST_ASSERT_EQUAL_UINT8(0u, ch.intelligence);
}

static void test_trickster_gains_0str_2spd_1prc(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.level    = 1u;
    ch.class_id = (uint8_t)FQ_CLASS_TRICKSTER;
    ch.strength    = 0u;
    ch.speed       = 0u;
    ch.precision   = 0u;
    ch.intelligence = 0u;
    ch.xp = 50u;
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_level_up(&ch));
    TEST_ASSERT_EQUAL_UINT8(0u, ch.strength);
    TEST_ASSERT_EQUAL_UINT8(2u, ch.speed);
    TEST_ASSERT_EQUAL_UINT8(1u, ch.precision);
    TEST_ASSERT_EQUAL_UINT8(0u, ch.intelligence);
}

static void test_hex_gains_0str_0spd_1prc_2int(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.level    = 1u;
    ch.class_id = (uint8_t)FQ_CLASS_HEX;
    ch.strength    = 0u;
    ch.speed       = 0u;
    ch.precision   = 0u;
    ch.intelligence = 0u;
    ch.xp = 50u;
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_level_up(&ch));
    TEST_ASSERT_EQUAL_UINT8(0u, ch.strength);
    TEST_ASSERT_EQUAL_UINT8(0u, ch.speed);
    TEST_ASSERT_EQUAL_UINT8(1u, ch.precision);
    TEST_ASSERT_EQUAL_UINT8(2u, ch.intelligence);
}

static void test_warden_gains_1str_1spd_0prc_1int(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.level    = 1u;
    ch.class_id = (uint8_t)FQ_CLASS_WARDEN;
    ch.strength    = 0u;
    ch.speed       = 0u;
    ch.precision   = 0u;
    ch.intelligence = 0u;
    ch.xp = 50u;
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_level_up(&ch));
    TEST_ASSERT_EQUAL_UINT8(1u, ch.strength);
    TEST_ASSERT_EQUAL_UINT8(1u, ch.speed);
    TEST_ASSERT_EQUAL_UINT8(0u, ch.precision);
    TEST_ASSERT_EQUAL_UINT8(1u, ch.intelligence);
}

static void test_wildcard_gains_1str_1spd_1prc_0int(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.level    = 1u;
    ch.class_id = (uint8_t)FQ_CLASS_WILDCARD;
    ch.strength    = 0u;
    ch.speed       = 0u;
    ch.precision   = 0u;
    ch.intelligence = 0u;
    ch.xp = 50u;
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_level_up(&ch));
    TEST_ASSERT_EQUAL_UINT8(1u, ch.strength);
    TEST_ASSERT_EQUAL_UINT8(1u, ch.speed);
    TEST_ASSERT_EQUAL_UINT8(1u, ch.precision);
    TEST_ASSERT_EQUAL_UINT8(0u, ch.intelligence);
}

int main(void)
{
    test_xp_to_next_level_0_is_50();
    test_xp_to_next_level_99_is_zero();
    test_xp_to_next_level_1_is_50();
    test_xp_to_next_level_10_is_5000();
    test_xp_to_next_level_98_is_nonzero();
    test_level_up_at_99_rejected();
    test_level_up_insufficient_xp_rejected();
    test_level_up_null_ptr_rejected();
    test_stat_saturates_at_255_not_wrap();
    test_bruiser_gains_2str_1spd();
    test_trickster_gains_0str_2spd_1prc();
    test_hex_gains_0str_0spd_1prc_2int();
    test_warden_gains_1str_1spd_0prc_1int();
    test_wildcard_gains_1str_1spd_1prc_0int();
    return 0;
}
