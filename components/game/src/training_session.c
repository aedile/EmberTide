/**
 * training_session.c — FiestaQuest Training Session: Timing Mini-Game
 *
 * Implementation of the Phase-19 tick-based training input system.
 *
 * Constitution Priority 0: No float, no malloc, no PRNG calls.
 * All timing is driven by the caller's tick counter — no internal entropy.
 *
 * PRNG isolation contract: this file does NOT include prng.h and MUST NOT
 * call fq_prng_next() or fq_prng_range(). Training timing is deterministic
 * tick-based — the combat PRNG stream is never touched.
 */

#include "training_session.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Target speed table (pos/tick) for each game type.
 *
 * SPEED: 10 pos/tick — target crosses 100 in ~10 ticks.
 * POWER:  6 pos/tick — target crosses 100 in ~17 ticks.
 * INTEL:  3 pos/tick — target crosses 100 in ~33 ticks.
 *
 * At 50ms/tick (20Hz), the hit window at zone [40,60] is:
 *   SPEED: ~1 tick (50ms) — reaction game.
 *   POWER: ~3 ticks (150ms) — timing game.
 *   INTEL: ~7 ticks (350ms) — pattern game.
 * ---------------------------------------------------------------------------*/
static const uint8_t k_target_speed[3u] = {
    10u, /* FQ_TS_SPEED */
     6u, /* FQ_TS_POWER */
     3u  /* FQ_TS_INTEL */
};

/* ---------------------------------------------------------------------------
 * finish_target — advance to the next target, checking for session end.
 * ---------------------------------------------------------------------------*/
static void finish_target(fq_training_session_t *ts)
{
    ts->target_pos = 0u;
    ts->targets_done++;
    if (ts->targets_done >= FQ_TS_TOTAL_TARGETS) {
        ts->state = (uint8_t)FQ_TS_DONE;
    }
}

/* ---------------------------------------------------------------------------
 * fq_training_session_init
 * ---------------------------------------------------------------------------*/
game_err_t fq_training_session_init(fq_training_session_t *ts, fq_ts_type_t type)
{
    if (ts == NULL) {
        return GAME_ERR_NULL_PTR;
    }
    memset(ts, 0, sizeof(*ts));
    ts->state     = (uint8_t)FQ_TS_WAITING;
    ts->game_type = (uint8_t)type;
    return GAME_OK;
}

/* ---------------------------------------------------------------------------
 * fq_training_session_start
 * ---------------------------------------------------------------------------*/
game_err_t fq_training_session_start(fq_training_session_t *ts)
{
    if (ts == NULL) {
        return GAME_ERR_NULL_PTR;
    }
    if (ts->state != (uint8_t)FQ_TS_WAITING) {
        return GAME_ERR_INVALID;
    }

    uint8_t type_idx = ts->game_type;
    if (type_idx >= 3u) {
        type_idx = 0u; /* defensive: clamp to SPEED */
    }
    ts->target_speed = k_target_speed[type_idx];
    ts->target_pos   = 0u;
    ts->targets_done = 0u;
    ts->score        = 0u;
    ts->state        = (uint8_t)FQ_TS_ACTIVE;

    return GAME_OK;
}

/* ---------------------------------------------------------------------------
 * fq_training_step
 * ---------------------------------------------------------------------------*/
game_err_t fq_training_step(fq_training_session_t *ts)
{
    if (ts == NULL) {
        return GAME_ERR_NULL_PTR;
    }
    if (ts->state != (uint8_t)FQ_TS_ACTIVE) {
        return GAME_ERR_INVALID;
    }

    /* Advance position. Use uint16_t intermediate to detect wrap. */
    uint16_t new_pos = (uint16_t)ts->target_pos + (uint16_t)ts->target_speed;

    if (new_pos > 100u) {
        /* Target passed without a hit — advance to next target. */
        finish_target(ts);
    } else {
        ts->target_pos = (uint8_t)new_pos;
    }

    return GAME_OK;
}

/* ---------------------------------------------------------------------------
 * fq_training_hit
 * ---------------------------------------------------------------------------*/
game_err_t fq_training_hit(fq_training_session_t *ts)
{
    if (ts == NULL) {
        return GAME_ERR_NULL_PTR;
    }
    if (ts->state != (uint8_t)FQ_TS_ACTIVE) {
        return GAME_ERR_INVALID;
    }

    /* Score adjustment. */
    if (ts->target_pos >= FQ_TS_HIT_ZONE_LO &&
        ts->target_pos <= FQ_TS_HIT_ZONE_HI) {
        /* In zone: +20, saturate at 100. */
        uint16_t new_score = (uint16_t)ts->score + FQ_TS_HIT_SCORE_ADD;
        ts->score = (uint8_t)(new_score > 100u ? 100u : new_score);
    } else {
        /* Out of zone: -5, clamp at 0. */
        if (ts->score >= FQ_TS_MISS_SCORE_SUB) {
            ts->score = (uint8_t)(ts->score - FQ_TS_MISS_SCORE_SUB);
        } else {
            ts->score = 0u;
        }
    }

    /* Advance to next target. */
    finish_target(ts);

    return GAME_OK;
}

/* ---------------------------------------------------------------------------
 * fq_training_advance_target
 * ---------------------------------------------------------------------------*/
game_err_t fq_training_advance_target(fq_training_session_t *ts)
{
    if (ts == NULL) {
        return GAME_ERR_NULL_PTR;
    }
    if (ts->state != (uint8_t)FQ_TS_ACTIVE) {
        return GAME_ERR_INVALID;
    }

    finish_target(ts);
    return GAME_OK;
}

/* ---------------------------------------------------------------------------
 * fq_training_get_target_speed
 * ---------------------------------------------------------------------------*/
uint8_t fq_training_get_target_speed(const fq_training_session_t *ts)
{
    if (ts == NULL) {
        return 0u;
    }
    return ts->target_speed;
}

/* ---------------------------------------------------------------------------
 * fq_training_get_xp_award
 * ---------------------------------------------------------------------------*/
uint32_t fq_training_get_xp_award(const fq_training_session_t *ts)
{
    if (ts == NULL) {
        return 0u;
    }
    /* Clamp score to 100 before multiply — defense against corrupt state. */
    uint32_t clamped = (ts->score > 100u) ? 100u : (uint32_t)ts->score;
    return clamped * 2u;
}

/* ---------------------------------------------------------------------------
 * fq_training_award_xp
 * ---------------------------------------------------------------------------*/
game_err_t fq_training_award_xp(const fq_training_session_t *ts,
                                 fq_character_t              *ch)
{
    if (ts == NULL || ch == NULL) {
        return GAME_ERR_NULL_PTR;
    }

    uint32_t award = fq_training_get_xp_award(ts);

    /* Add XP to character. Guard against uint32_t overflow. */
    uint32_t new_xp = ch->xp + award;
    if (new_xp < ch->xp) {
        /* Overflow: saturate at UINT32_MAX. */
        new_xp = UINT32_MAX;
    }
    ch->xp = new_xp;

    /* Attempt level-up. At level 99, fq_level_up() returns GAME_ERR_INVALID.
     * XP is already added above — we preserve it and return the error. */
    if (award > 0u) {
        return fq_level_up(ch);
    }

    return GAME_OK;
}
