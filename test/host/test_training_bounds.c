/**
 * test_training_bounds.c — Bound/negative tests for mini-game FSM and scoring.
 * Phase A: BOUND RED (Rule 22 — bound tests before feature tests).
 *
 * Covers:
 *   N1  — FSM invalid state transitions rejected with GAME_ERR_INVALID.
 *   N2  — Score overflow: hits=65000, targets=65000 stays within uint8_t [0,100].
 *   N3  — Score underflow/divide-by-zero: targets==0 yields score=0, not panic.
 *   N4  — NULL pointer guard on all three public functions.
 */

#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "test_assert.h"
#include "types.h"
#include "training.h"

/* N4: NULL pointer → GAME_ERR_NULL_PTR ------------------------------------ */
static void test_init_null_ptr_rejected(void)
{
    game_err_t err = fq_minigame_init(NULL, FQ_MG_SPEED, 1u);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_NULL_PTR, (int)err);
}

static void test_tick_null_ptr_rejected(void)
{
    game_err_t err = fq_minigame_tick(NULL, 1u);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_NULL_PTR, (int)err);
}

static void test_finish_null_ptr_rejected(void)
{
    game_err_t err = fq_minigame_finish(NULL);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_NULL_PTR, (int)err);
}

static void test_score_null_ptr_returns_zero(void)
{
    uint8_t score = fq_minigame_score(NULL);
    TEST_ASSERT_EQUAL_UINT8(0u, score);
}

/* N1: Calling tick() on a DONE state must return GAME_ERR_INVALID. */
static void test_tick_on_done_state_rejected(void)
{
    fq_minigame_t mg;
    memset(&mg, 0, sizeof(mg));
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_SPEED, 1u));
    /* Manually force to DONE state to test the invalid transition. */
    mg.state = FQ_MG_DONE;
    game_err_t err = fq_minigame_tick(&mg, 1u);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
}

/* N1: Calling finish() on a WAIT state must return GAME_ERR_INVALID. */
static void test_finish_on_wait_state_rejected(void)
{
    fq_minigame_t mg;
    memset(&mg, 0, sizeof(mg));
    /* WAIT state — no init yet, manually set */
    mg.state = FQ_MG_WAIT;
    game_err_t err = fq_minigame_finish(&mg);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
}

/* N1: Calling finish() on an ACTIVE state must return GAME_ERR_INVALID. */
static void test_finish_on_active_state_rejected(void)
{
    fq_minigame_t mg;
    memset(&mg, 0, sizeof(mg));
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_SPEED, 1u));
    /* After init, state should be ACTIVE — finish() requires SUCCESS or FAIL. */
    TEST_ASSERT_EQUAL_INT(FQ_MG_ACTIVE, (int)mg.state);
    game_err_t err = fq_minigame_finish(&mg);
    TEST_ASSERT_EQUAL_INT(GAME_ERR_INVALID, (int)err);
}

/* N3: targets==0 → score must be 0, not a divide-by-zero panic. */
static void test_score_targets_zero_returns_zero(void)
{
    fq_minigame_t mg;
    memset(&mg, 0, sizeof(mg));
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_SPEED, 1u));
    /* Force targets to 0 to simulate degenerate case. */
    mg.targets = 0u;
    mg.hits    = 0u;
    /* Tick with input=0 should drive to FAIL since hits < targets (both 0 treated as fail). */
    /* To specifically test score calculation with targets=0, advance to SUCCESS manually. */
    mg.state = FQ_MG_SUCCESS;
    game_err_t err = fq_minigame_finish(&mg);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8(0u, mg.score);
}

/* N2: Score overflow guard — hits=65000, targets=65000 must yield exactly 100.
 * The intermediate multiplication (uint32_t)hits * 100u must not overflow
 * a uint16_t before division. Verified via uint32_t intermediate. */
static void test_score_no_overflow_large_values(void)
{
    fq_minigame_t mg;
    memset(&mg, 0, sizeof(mg));
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_SPEED, 1u));
    /* Force extreme values to exercise the overflow path. */
    mg.hits    = 65000u;
    mg.targets = 65000u;
    mg.state   = FQ_MG_SUCCESS;
    game_err_t err = fq_minigame_finish(&mg);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    /* 65000 * 100 = 6,500,000 — safe in uint32_t; / 65000 = 100; min(100, 100) = 100 */
    TEST_ASSERT_EQUAL_UINT8(100u, mg.score);
}

/* Score must never exceed 100 regardless of hits > targets. */
static void test_score_never_exceeds_100(void)
{
    fq_minigame_t mg;
    memset(&mg, 0, sizeof(mg));
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)fq_minigame_init(&mg, FQ_MG_SPEED, 1u));
    mg.hits    = 200u; /* More hits than targets is impossible in normal play but
                        * we clamp at min(100, ...) defensively. */
    mg.targets = 100u;
    mg.state   = FQ_MG_SUCCESS;
    (void)fq_minigame_finish(&mg);
    TEST_ASSERT_TRUE(mg.score <= 100u);
}

int main(void)
{
    test_init_null_ptr_rejected();
    test_tick_null_ptr_rejected();
    test_finish_null_ptr_rejected();
    test_score_null_ptr_returns_zero();
    test_tick_on_done_state_rejected();
    test_finish_on_wait_state_rejected();
    test_finish_on_active_state_rejected();
    test_score_targets_zero_returns_zero();
    test_score_no_overflow_large_values();
    test_score_never_exceeds_100();
    return 0;
}
