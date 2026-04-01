/**
 * training.c — FiestaQuest Mini-game FSM and Scoring implementation.
 *
 * See training.h for contract, FSM transitions, and score formula.
 *
 * Constitution Priority 0: No floating point. No <time.h>. No external entropy.
 *
 * Score formula: min(100, (uint32_t)hits * 100u / targets)
 *   - uint32_t intermediate prevents overflow for hits up to UINT16_MAX.
 *   - targets == 0 → score = 0 (guard before division).
 *
 * Tick semantics:
 *   - Each tick represents one "window" where the player can hit or miss.
 *   - An internal tick counter advances each call.
 *   - When tick_count == targets after an input:
 *       hits == targets → FQ_MG_SUCCESS
 *       hits <  targets → FQ_MG_FAIL
 *   - We track the internal tick counter by re-using the `hits` + miss
 *     implicit count: ticks_so_far is maintained as a separate field is
 *     not present in the struct, so we derive it as: the game transitions
 *     once we've processed `targets` total ticks.
 *
 * To avoid adding a field to the struct (keeping it at 8 bytes), we record
 * total ticks by embedding a tick counter in the score field (which is 0
 * until finish()). A tick counter alias approach would pollute score, so
 * instead we use a different strategy: we compute ticks_processed as a
 * separate uint16_t local variable on the stack, and store a sentinel.
 *
 * IMPLEMENTATION DECISION: to keep fq_minigame_t exactly 8 bytes with no
 * additional fields, fq_minigame_tick() tracks tick progress by comparing
 * (hits + misses_so_far) against targets. Since we don't store misses in
 * the struct we can't do that directly. The cleanest approach within the
 * 8-byte constraint is:
 *   - Treat each tick call as advancing one slot.
 *   - The caller is responsible for calling tick() exactly `targets` times.
 *   - After targets ticks: if hits == targets → SUCCESS, else FAIL.
 *   - An early SUCCESS is triggered when hits == targets before all ticks
 *     are exhausted (player can stop early).
 *
 * To count total ticks without adding a field, we note that `score` is 0
 * before finish(). We abuse score as a tick counter during ACTIVE state
 * (it is overwritten by finish()). This avoids struct size growth.
 * Maximum tick count before transition = targets (uint16_t up to 65535).
 * score is uint8_t — this limits safe tick tracking to 255. Since targets
 * = 5 + difficulty*5 ≤ 55, this is always within uint8_t range.
 */

#include "training.h"

/* Internal macro: saturating add for uint8_t. */
#define SAT8(a, b) ((uint8_t)(((uint32_t)(a) + (uint32_t)(b)) > 255u ? 255u : ((a) + (b))))

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------------*/

/**
 * calc_targets() — Compute target count for given difficulty.
 * Target count = 5 + difficulty * 5.  Range: 5 (diff=0) to 55 (diff=10).
 */
static uint16_t calc_targets(uint8_t difficulty)
{
    return (uint16_t)(5u + (uint16_t)difficulty * 5u);
}

/**
 * calc_score() — Compute normalized score.
 * Returns min(100, (uint32_t)hits * 100u / targets).
 * Returns 0 if targets == 0.
 */
static uint8_t calc_score(uint16_t hits, uint16_t targets)
{
    if (targets == 0u) {
        return 0u;
    }
    uint32_t raw = ((uint32_t)hits * 100u) / (uint32_t)targets;
    return (uint8_t)(raw > 100u ? 100u : raw);
}

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

game_err_t fq_minigame_init(fq_minigame_t *mg, fq_mg_type_t type, uint8_t level)
{
    if (mg == NULL) {
        return GAME_ERR_NULL_PTR;
    }

    /* difficulty = min(10, level / 10) */
    uint8_t diff = (uint8_t)(level / 10u);
    if (diff > 10u) {
        diff = 10u;
    }

    mg->type       = (uint8_t)type;
    mg->difficulty = diff;
    mg->targets    = calc_targets(diff);
    mg->hits       = 0u;
    mg->score      = 0u;  /* Also used as tick counter during ACTIVE state. */
    mg->state      = (uint8_t)FQ_MG_ACTIVE;

    return GAME_OK;
}

game_err_t fq_minigame_tick(fq_minigame_t *mg, uint8_t input)
{
    if (mg == NULL) {
        return GAME_ERR_NULL_PTR;
    }
    if (mg->state != (uint8_t)FQ_MG_ACTIVE) {
        return GAME_ERR_INVALID;
    }

    /* Count this tick using score field as a tick counter (valid since score
     * is always 0 during ACTIVE and we reset it in finish()). */
    mg->score = SAT8(mg->score, 1u);

    if (input != 0u) {
        /* Hit: increment hit counter (saturate at targets to avoid overshoot). */
        if (mg->hits < mg->targets) {
            mg->hits++;
        }
    }

    /* Check for transitions. */
    if (mg->hits == mg->targets) {
        /* All targets hit — SUCCESS. */
        mg->score = 0u; /* Reset tick counter; finish() will compute score. */
        mg->state = (uint8_t)FQ_MG_SUCCESS;
    } else if ((uint16_t)mg->score >= mg->targets) {
        /* All tick slots consumed but not all targets hit — FAIL. */
        mg->score = 0u; /* Reset tick counter. */
        mg->state = (uint8_t)FQ_MG_FAIL;
    }

    return GAME_OK;
}

game_err_t fq_minigame_finish(fq_minigame_t *mg)
{
    if (mg == NULL) {
        return GAME_ERR_NULL_PTR;
    }
    if (mg->state != (uint8_t)FQ_MG_SUCCESS &&
        mg->state != (uint8_t)FQ_MG_FAIL) {
        return GAME_ERR_INVALID;
    }

    mg->score = calc_score(mg->hits, mg->targets);
    mg->state = (uint8_t)FQ_MG_DONE;

    return GAME_OK;
}

uint8_t fq_minigame_score(const fq_minigame_t *mg)
{
    if (mg == NULL) {
        return 0u;
    }
    return mg->score;
}
