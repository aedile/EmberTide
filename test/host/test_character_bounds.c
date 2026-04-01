/**
 * test_character_bounds.c — Bound tests for fq_character_create.
 *
 * Rule 22 (BOUND RED): These tests MUST be written and fail before any
 * fq_character_create implementation exists. They prove the system
 * correctly REJECTS:
 *   - NULL character pointer
 *   - Invalid class_id (>= FQ_CLASS_COUNT)
 *   - Name pointer NULL (should zero-fill name field gracefully)
 *
 * Constitution Priority 0: No float, no external entropy in game/.
 */

#include <stdint.h>
#include <inttypes.h>
#include "test_assert.h"
#include "character.h"

/* -------------------------------------------------------------------------
 * B-CH-01: NULL character pointer must return GAME_ERR_NULL_PTR.
 * ------------------------------------------------------------------------- */
static void test_null_character_ptr(void)
{
    game_err_t err = fq_character_create(NULL, FQ_CLASS_BRUISER, 1u, "Iron");
    TEST_ASSERT_EQUAL_UINT32((uint32_t)GAME_ERR_NULL_PTR, (uint32_t)err);
}

/* -------------------------------------------------------------------------
 * B-CH-02: class_id >= FQ_CLASS_COUNT must return GAME_ERR_INVALID.
 * ------------------------------------------------------------------------- */
static void test_invalid_class_id(void)
{
    fq_character_t ch;
    game_err_t err = fq_character_create(&ch, (fq_class_t)FQ_CLASS_COUNT, 1u, "Iron");
    TEST_ASSERT_EQUAL_UINT32((uint32_t)GAME_ERR_INVALID, (uint32_t)err);
}

/* -------------------------------------------------------------------------
 * B-CH-03: class_id way out of range (255) must return GAME_ERR_INVALID.
 * ------------------------------------------------------------------------- */
static void test_class_id_max_out_of_range(void)
{
    fq_character_t ch;
    game_err_t err = fq_character_create(&ch, (fq_class_t)255u, 99u, "Hero");
    TEST_ASSERT_EQUAL_UINT32((uint32_t)GAME_ERR_INVALID, (uint32_t)err);
}

/* -------------------------------------------------------------------------
 * B-CH-04: NULL name pointer must NOT crash — must zero-fill name field.
 * A NULL name is treated as an empty string (safe fallback).
 * Must return GAME_OK (not an error — defensive accept).
 * ------------------------------------------------------------------------- */
static void test_null_name_no_crash(void)
{
    fq_character_t ch;
    game_err_t err = fq_character_create(&ch, FQ_CLASS_BRUISER, 42u, NULL);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)GAME_OK, (uint32_t)err);
    /* Name field must be zero (null string) — not garbage. */
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)(unsigned char)ch.name[0]);
}

/* -------------------------------------------------------------------------
 * B-CH-05: hp_max must not overflow when base_hp + strength * 2 approaches
 * uint16_t max. Bruiser base: HP=60, STR=3 → hp_max = 60 + 3*2 = 66.
 * Confirm no overflow on legal input (not a typical overflow but validates
 * the formula doesn't silently underflow or produce zero).
 * ------------------------------------------------------------------------- */
static void test_hp_max_nonzero_on_valid_input(void)
{
    fq_character_t ch;
    game_err_t err = fq_character_create(&ch, FQ_CLASS_BRUISER, 1u, "Golem");
    TEST_ASSERT_EQUAL_UINT32((uint32_t)GAME_OK, (uint32_t)err);
    /* Bruiser: base_hp=60, base_str=3 → hp_max = 60 + 3*2 = 66 (no legacy). */
    TEST_ASSERT_TRUE(ch.hp_max > 0u);
    TEST_ASSERT_TRUE(ch.hp_max >= 66u);
}

int main(void)
{
    test_null_character_ptr();
    test_invalid_class_id();
    test_class_id_max_out_of_range();
    test_null_name_no_crash();
    test_hp_max_nonzero_on_valid_input();
    return 0;
}
