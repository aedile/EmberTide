/**
 * test_character.c — Feature tests for fq_character_create.
 *
 * Verifies:
 *   1. Bruiser with no legacy → base stats match class table.
 *   2. Warden with THICK_SKIN_1 set → hp_max includes +5 bonus.
 *   3. NULL pointer → GAME_ERR_NULL_PTR.
 *
 * Constitution Priority 0: No float, no external entropy.
 */

#include <string.h>
#include <inttypes.h>
#include "test_assert.h"
#include "character.h"
#include "legacy.h"

/* -------------------------------------------------------------------------
 * Class base stats table (mirrors k_class_base in legacy.c):
 *   Bruiser:   STR=3, SPD=0, PRC=0, INT=0, HP=60
 *   Trickster: STR=0, SPD=3, PRC=1, INT=0, HP=40
 *   Hex:       STR=0, SPD=0, PRC=1, INT=3, HP=45
 *   Warden:    STR=1, SPD=1, PRC=0, INT=1, HP=55
 *   Wildcard:  STR=1, SPD=1, PRC=1, INT=0, HP=50
 * ------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * F-CH-01: Bruiser with no legacy — verify base stats match class table.
 * hp_max = base_hp(60) + base_strength(3) * 2 = 66.
 * ------------------------------------------------------------------------- */
static void test_bruiser_no_legacy_base_stats(void)
{
    fq_character_t ch;
    game_err_t err = fq_character_create(&ch, FQ_CLASS_BRUISER, 7u, "Boulder");
    TEST_ASSERT_EQUAL_UINT32((uint32_t)GAME_OK, (uint32_t)err);

    /* ID and class set correctly. */
    TEST_ASSERT_EQUAL_UINT32(7u, ch.id);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)FQ_CLASS_BRUISER, (uint32_t)ch.class_id);

    /* Base stats for Bruiser: STR=3, SPD=0, PRC=0, INT=0 */
    TEST_ASSERT_EQUAL_UINT32(3u,  (uint32_t)ch.strength);
    TEST_ASSERT_EQUAL_UINT32(0u,  (uint32_t)ch.speed);
    TEST_ASSERT_EQUAL_UINT32(0u,  (uint32_t)ch.precision);
    TEST_ASSERT_EQUAL_UINT32(0u,  (uint32_t)ch.intelligence);

    /* hp_max = base_hp(60) + strength(3) * 2 = 66. No legacy bonuses. */
    TEST_ASSERT_EQUAL_UINT32(66u, (uint32_t)ch.hp_max);

    /* Name stored correctly. */
    TEST_ASSERT_EQUAL_UINT32(0, strncmp(ch.name, "Boulder", sizeof(ch.name) - 1u));

    /* Legacy tree is empty (no nodes unlocked at creation). */
    TEST_ASSERT_EQUAL_UINT32(0u, ch.legacy_tree);

    /* Level starts at 0, XP at 0. */
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)ch.level);
    TEST_ASSERT_EQUAL_UINT32(0u, ch.xp);

    /* Not dead. */
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)ch.is_dead);

    /* Equipped count defaults to 4. */
    TEST_ASSERT_EQUAL_UINT32(4u, (uint32_t)ch.equipped_count);
}

/* -------------------------------------------------------------------------
 * F-CH-02: Warden with THICK_SKIN_1 set → hp_max includes +5 bonus.
 * Warden base: HP=55, STR=1 → base hp_max = 55 + 1*2 = 57.
 * THICK_SKIN_1 adds +5 → hp_max = 57 + 5 = 62.
 * ------------------------------------------------------------------------- */
static void test_warden_with_thick_skin_1(void)
{
    fq_character_t ch;
    /* Pre-set legacy_tree with THICK_SKIN_1 before calling create. */
    /* create() must zero the struct, so we set it after zeroing by patching
     * the legacy_tree before calling fq_character_create won't work — create
     * zeroes the struct. Instead we create then manually set legacy_tree and
     * re-apply bonuses by calling fq_legacy_apply_bonuses.
     *
     * BUT: the contract of fq_character_create is that it calls
     * fq_legacy_apply_bonuses(ch) using ch->legacy_tree as-set.
     * So we need to create a helper character with legacy_tree pre-set.
     *
     * The correct approach per F-02 spec:
     *   "Calls fq_legacy_apply_bonuses(ch) to wire the legacy tree"
     *
     * This means character_create reads ch->legacy_tree AFTER zeroing, so
     * to test with a legacy node, we must set legacy_tree before calling
     * character_create. But character_create zeroes the struct first...
     *
     * Resolution: character_create accepts legacy_tree as part of the zeroed
     * struct — it zeros then applies. To test legacy wiring, we call
     * fq_character_create to get a fresh character, then set the legacy bit,
     * recalculate hp_max manually and compare, verifying the formula holds.
     *
     * Alternatively: per the spec, we can test by:
     *   1. Create character.
     *   2. Set legacy_tree = FQ_LEGACY_THICK_SKIN_1.
     *   3. Call fq_legacy_apply_bonuses directly.
     *   4. Verify hp_max matches expected.
     *
     * This tests the integration path (character_create + apply_bonuses).
     */
    game_err_t err = fq_character_create(&ch, FQ_CLASS_WARDEN, 10u, "Oak");
    TEST_ASSERT_EQUAL_UINT32((uint32_t)GAME_OK, (uint32_t)err);

    /* Base Warden: STR=1, HP=55 → hp_max = 55 + 1*2 = 57 (no legacy). */
    TEST_ASSERT_EQUAL_UINT32(57u, (uint32_t)ch.hp_max);

    /* Now set THICK_SKIN_1 and apply bonuses. */
    ch.legacy_tree = FQ_LEGACY_THICK_SKIN_1;
    fq_legacy_apply_bonuses(&ch);

    /* hp_max should now be 57 + 5 = 62. */
    TEST_ASSERT_EQUAL_UINT32(62u, (uint32_t)ch.hp_max);
}

/* -------------------------------------------------------------------------
 * F-CH-03: NULL pointer → GAME_ERR_NULL_PTR.
 * ------------------------------------------------------------------------- */
static void test_null_ptr_returns_error(void)
{
    game_err_t err = fq_character_create(NULL, FQ_CLASS_WARDEN, 1u, "Test");
    TEST_ASSERT_EQUAL_UINT32((uint32_t)GAME_ERR_NULL_PTR, (uint32_t)err);
}

/* -------------------------------------------------------------------------
 * F-CH-04: Trickster stats — STR=0, SPD=3, PRC=1, INT=0, HP=40.
 * hp_max = 40 + 0*2 = 40.
 * ------------------------------------------------------------------------- */
static void test_trickster_base_stats(void)
{
    fq_character_t ch;
    game_err_t err = fq_character_create(&ch, FQ_CLASS_TRICKSTER, 99u, "Swift");
    TEST_ASSERT_EQUAL_UINT32((uint32_t)GAME_OK, (uint32_t)err);

    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)ch.strength);
    TEST_ASSERT_EQUAL_UINT32(3u, (uint32_t)ch.speed);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)ch.precision);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)ch.intelligence);
    /* hp_max = 40 + 0*2 = 40. */
    TEST_ASSERT_EQUAL_UINT32(40u, (uint32_t)ch.hp_max);
}

/* -------------------------------------------------------------------------
 * F-CH-05: Name longer than 11 chars is truncated to 11 + null terminator.
 * (name field is char[12])
 * ------------------------------------------------------------------------- */
static void test_long_name_truncated(void)
{
    fq_character_t ch;
    game_err_t err = fq_character_create(&ch, FQ_CLASS_HEX, 5u,
                                          "Bartholomew99");
    TEST_ASSERT_EQUAL_UINT32((uint32_t)GAME_OK, (uint32_t)err);
    /* Must be null-terminated within 12-byte name buffer. */
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)(unsigned char)ch.name[11]);
}

/* -------------------------------------------------------------------------
 * F-CH-06: Struct is zeroed before applying class stats.
 * rival_log, equipped[], xp, wins, losses must all be 0 on creation.
 * ------------------------------------------------------------------------- */
static void test_fields_zeroed_on_creation(void)
{
    fq_character_t ch;
    /* Fill with garbage first to prove zeroing happens. */
    __builtin_memset(&ch, 0xFF, sizeof(ch));

    game_err_t err = fq_character_create(&ch, FQ_CLASS_WILDCARD, 3u, "Nova");
    TEST_ASSERT_EQUAL_UINT32((uint32_t)GAME_OK, (uint32_t)err);

    TEST_ASSERT_EQUAL_UINT32(0u, ch.xp);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)ch.wins);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)ch.losses);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)ch.rebirth_count);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)ch.is_dead);
    TEST_ASSERT_EQUAL_UINT32(0u, ch.rival_log[0].opponent_id);
    TEST_ASSERT_EQUAL_UINT32(0u, ch.rival_log[7].opponent_id);
}

int main(void)
{
    test_bruiser_no_legacy_base_stats();
    test_warden_with_thick_skin_1();
    test_null_ptr_returns_error();
    test_trickster_base_stats();
    test_long_name_truncated();
    test_fields_zeroed_on_creation();
    return 0;
}
