/**
 * training.c — FiestaQuest Mini-game FSM and Scoring implementation.
 *
 * See training.h for contract, FSM transition table, and score formula.
 *
 * Constitution Priority 0: No floating point. No <time.h>. No external entropy.
 *
 * Tick counter aliasing: the `score` field is 0 during ACTIVE state and is
 * used as a tick counter. fq_minigame_finish() overwrites it with the final
 * score. This keeps fq_minigame_t at exactly 8 bytes with no hidden fields.
 * Maximum targets = 5 + 10*5 = 55, safely within uint8_t tick count range.
 */

#include "training.h"

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------------*/

/** calc_targets — target count = 5 + difficulty * 5. Range [5, 55]. */
static uint16_t calc_targets(uint8_t difficulty)
{
    return (uint16_t)(5u + (uint16_t)difficulty * 5u);
}

/**
 * calc_score — Normalized score in [0, 100].
 * Formula: min(100, (uint32_t)hits * 100u / targets).
 * Returns 0 when targets == 0 (no divide-by-zero).
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
    mg->score      = 0u; /* Doubles as tick counter until finish(). */
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

    /* Advance tick counter (saturating; targets max 55, well within uint8_t). */
    uint32_t next_tick = (uint32_t)mg->score + 1u;
    mg->score = (uint8_t)(next_tick > 255u ? 255u : next_tick);

    if (input != 0u) {
        /* Hit: cap at targets to prevent overshoot. */
        if (mg->hits < mg->targets) {
            mg->hits++;
        }
    }

    /* FSM transition evaluation. */
    if (mg->hits == mg->targets) {
        mg->score = 0u;                      /* Clear tick counter. */
        mg->state = (uint8_t)FQ_MG_SUCCESS;
    } else if ((uint16_t)mg->score >= mg->targets) {
        mg->score = 0u;                      /* Clear tick counter. */
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
