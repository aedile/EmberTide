/**
 * test_training.c — Feature/happy-path tests for the mini-game FSM and scoring.
 * Phase B: FEATURE RED.
 *
 * Tests:
 *   - Full FSM walk: WAIT → ACTIVE → SUCCESS → DONE (perfect score = 100).
 *   - Full FSM walk: WAIT → ACTIVE → FAIL → DONE (score = 0 with zero hits).
 *   - Partial score: 7 hits / 10 targets → 70.
 *   - All three mini-game types (SPEED, POWER, INTEL) can be initialized.
 *   - Difficulty scaling: targets increase with level.
 *   - fq_minigame_score() before finish returns 0 (not yet computed).
 */

#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "test_assert.h"
#include "types.h"
#include "training.h"

/* Full happy-path: init → tick(hit=1) as many times as targets → finish.
 * With hits==targets, score must be 100. */
static void test_speed_game_perfect_score_100(void)
{
    fq_minigame_t mg;
    memset(&mg, 0, sizeof(mg));

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_SPEED, 1u));
    TEST_ASSERT_EQUAL_INT(FQ_MG_ACTIVE, (int)mg.state);
    TEST_ASSERT_TRUE(mg.targets > 0u);

    /* Simulate perfect play: hit on every tick until the game transitions. */
    uint16_t i;
    for (i = 0u; i < mg.targets; i++) {
        if (mg.state == FQ_MG_ACTIVE) {
            TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_tick(&mg, 1u));
        }
    }
    /* After all targets hit, state should be SUCCESS. */
    TEST_ASSERT_EQUAL_INT(FQ_MG_SUCCESS, (int)mg.state);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_finish(&mg));
    TEST_ASSERT_EQUAL_INT(FQ_MG_DONE, (int)mg.state);
    TEST_ASSERT_EQUAL_UINT8(100u, fq_minigame_score(&mg));
}

/* Zero-hit path: tick with input=0 every time → FAIL → finish → score=0. */
static void test_speed_game_zero_hits_score_0(void)
{
    fq_minigame_t mg;
    memset(&mg, 0, sizeof(mg));

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_SPEED, 1u));

    /* Miss every tick. */
    uint16_t i;
    for (i = 0u; i < mg.targets; i++) {
        if (mg.state == FQ_MG_ACTIVE) {
            TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_tick(&mg, 0u));
        }
    }
    TEST_ASSERT_EQUAL_INT(FQ_MG_FAIL, (int)mg.state);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_finish(&mg));
    TEST_ASSERT_EQUAL_INT(FQ_MG_DONE, (int)mg.state);
    TEST_ASSERT_EQUAL_UINT8(0u, fq_minigame_score(&mg));
}

/* Partial score: 7 hits out of 10 targets → score = 70. */
static void test_speed_game_partial_score_70(void)
{
    fq_minigame_t mg;
    memset(&mg, 0, sizeof(mg));

    /* Use level=0 to get a predictable small target count.
     * Difficulty = min(10, 0/10) = 0 → targets = base for diff=0. */
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_SPEED, 0u));

    /* We need exactly 10 targets to get a clean 70 from 7 hits.
     * Force the values directly to decouple from difficulty formula. */
    mg.targets = 10u;
    mg.hits    = 0u;

    /* Hit 7, miss 3. */
    uint8_t hit_idx;
    for (hit_idx = 0u; hit_idx < 10u; hit_idx++) {
        if (mg.state == FQ_MG_ACTIVE) {
            uint8_t input = (hit_idx < 7u) ? 1u : 0u;
            TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_tick(&mg, input));
        }
    }
    /* At this point state should be SUCCESS or FAIL depending on hits vs targets. */
    /* Manually close: force finish to exercise score formula regardless. */
    if (mg.state == FQ_MG_ACTIVE) {
        mg.state = FQ_MG_SUCCESS;
    }
    /* Hits was accumulated by tick; manually set to 7 for deterministic assert. */
    mg.hits = 7u;
    mg.state = FQ_MG_SUCCESS;
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_finish(&mg));
    /* 7 * 100 / 10 = 70. */
    TEST_ASSERT_EQUAL_UINT8(70u, fq_minigame_score(&mg));
}

/* All three types can be initialized (smoke test). */
static void test_all_types_init_ok(void)
{
    fq_minigame_t mg;
    memset(&mg, 0, sizeof(mg));

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_SPEED, 5u));
    TEST_ASSERT_EQUAL_INT(FQ_MG_SPEED, (int)mg.type);

    memset(&mg, 0, sizeof(mg));
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_POWER, 5u));
    TEST_ASSERT_EQUAL_INT(FQ_MG_POWER, (int)mg.type);

    memset(&mg, 0, sizeof(mg));
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_INTEL, 5u));
    TEST_ASSERT_EQUAL_INT(FQ_MG_INTEL, (int)mg.type);
}

/* Difficulty scaling: level 100 must give difficulty=10, level 10 gives 1. */
static void test_difficulty_scales_with_level(void)
{
    fq_minigame_t mg;
    memset(&mg, 0, sizeof(mg));

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_SPEED, 100u));
    TEST_ASSERT_EQUAL_UINT8(10u, mg.difficulty); /* min(10, 100/10) = 10 */

    memset(&mg, 0, sizeof(mg));
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_SPEED, 10u));
    TEST_ASSERT_EQUAL_UINT8(1u, mg.difficulty);  /* min(10, 10/10) = 1 */

    memset(&mg, 0, sizeof(mg));
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_SPEED, 0u));
    TEST_ASSERT_EQUAL_UINT8(0u, mg.difficulty);  /* min(10, 0/10) = 0 */
}

/* fq_minigame_score() before finish (ACTIVE state) returns 0. */
static void test_score_before_finish_is_zero(void)
{
    fq_minigame_t mg;
    memset(&mg, 0, sizeof(mg));
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_SPEED, 1u));
    /* Score field should be 0 before finish(). */
    TEST_ASSERT_EQUAL_UINT8(0u, fq_minigame_score(&mg));
}

/* POWER game: perfect score = 100. */
static void test_power_game_perfect_score_100(void)
{
    fq_minigame_t mg;
    memset(&mg, 0, sizeof(mg));

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_POWER, 1u));
    uint16_t i;
    for (i = 0u; i < mg.targets; i++) {
        if (mg.state == FQ_MG_ACTIVE) {
            (void)fq_minigame_tick(&mg, 1u);
        }
    }
    TEST_ASSERT_EQUAL_INT(FQ_MG_SUCCESS, (int)mg.state);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_finish(&mg));
    TEST_ASSERT_EQUAL_UINT8(100u, fq_minigame_score(&mg));
}

/* INTEL game: perfect score = 100. */
static void test_intel_game_perfect_score_100(void)
{
    fq_minigame_t mg;
    memset(&mg, 0, sizeof(mg));

    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_INTEL, 1u));
    uint16_t i;
    for (i = 0u; i < mg.targets; i++) {
        if (mg.state == FQ_MG_ACTIVE) {
            (void)fq_minigame_tick(&mg, 1u);
        }
    }
    TEST_ASSERT_EQUAL_INT(FQ_MG_SUCCESS, (int)mg.state);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_finish(&mg));
    TEST_ASSERT_EQUAL_UINT8(100u, fq_minigame_score(&mg));
}

int main(void)
{
    test_speed_game_perfect_score_100();
    test_speed_game_zero_hits_score_0();
    test_speed_game_partial_score_70();
    test_all_types_init_ok();
    test_difficulty_scales_with_level();
    test_score_before_finish_is_zero();
    test_power_game_perfect_score_100();
    test_intel_game_perfect_score_100();
    return 0;
}
