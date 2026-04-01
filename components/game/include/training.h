/**
 * training.h — FiestaQuest Mini-game FSM and Scoring.
 *
 * Defines a pure-functional finite state machine for "training" mini-games.
 * Each mini-game type (Speed, Power, Intel) exercises a different stat axis.
 *
 * FSM state transitions:
 *   WAIT   → ACTIVE  : fq_minigame_init() called successfully.
 *   ACTIVE → SUCCESS : fq_minigame_tick() called with sufficient hits to
 *                      reach the target count.
 *   ACTIVE → FAIL    : fq_minigame_tick() called with a miss on the final
 *                      opportunity (hits < targets after all ticks).
 *   SUCCESS → DONE   : fq_minigame_finish() called.
 *   FAIL    → DONE   : fq_minigame_finish() called.
 *
 * Invalid transitions return GAME_ERR_INVALID (see types.h).
 *
 * Score formula: min(100, (uint32_t)hits * 100u / targets)
 *   - Uses uint32_t intermediate to prevent overflow for hits up to 65535.
 *   - targets == 0 → score = 0 (no divide-by-zero).
 *
 * Difficulty scaling:
 *   difficulty = min(10, level / 10)
 *   This scales target count upward as the beast levels.
 *
 * Architecture constraint: this header MUST NOT include hal_*.h,
 * presentation/, or connectivity/ headers. Pure game logic only.
 */

#ifndef FIESTAQUEST_TRAINING_H
#define FIESTAQUEST_TRAINING_H

#include <stdint.h>
#include "types.h"

/* ---------------------------------------------------------------------------
 * fq_mg_state_t — mini-game FSM states.
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_MG_WAIT    = 0, /**< Initial state before init() is called. */
    FQ_MG_ACTIVE  = 1, /**< Game is running; ticks are being processed. */
    FQ_MG_SUCCESS = 2, /**< All targets hit; awaiting finish(). */
    FQ_MG_FAIL    = 3, /**< Missed too many targets; awaiting finish(). */
    FQ_MG_DONE    = 4  /**< Game complete; score is valid. */
} fq_mg_state_t;

/* ---------------------------------------------------------------------------
 * fq_mg_type_t — which stat axis this mini-game trains.
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_MG_SPEED = 0, /**< Trains speed (reflex timing). */
    FQ_MG_POWER = 1, /**< Trains strength (button mashing / pattern). */
    FQ_MG_INTEL = 2  /**< Trains intelligence (sequence matching). */
} fq_mg_type_t;

/* ---------------------------------------------------------------------------
 * fq_minigame_t — mini-game instance (statically allocated, 8 bytes).
 *
 * Field layout:
 *   uint16_t hits       (2)  offset 0
 *   uint16_t targets    (2)  offset 2
 *   uint8_t  state      (1)  offset 4
 *   uint8_t  type       (1)  offset 5
 *   uint8_t  difficulty (1)  offset 6
 *   uint8_t  score      (1)  offset 7
 * Total: 8 bytes, no hidden padding.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint16_t hits;       /**< Number of successful inputs recorded. */
    uint16_t targets;    /**< Total targets to hit in this game. */
    uint8_t  state;      /**< fq_mg_state_t: current FSM state. */
    uint8_t  type;       /**< fq_mg_type_t: which stat is being trained. */
    uint8_t  difficulty; /**< Difficulty tier [0, 10] = min(10, level/10). */
    uint8_t  score;      /**< Normalized score [0, 100]; valid after finish(). */
} fq_minigame_t;

_Static_assert(sizeof(fq_minigame_t) == 8u,
    "fq_minigame_t must be exactly 8 bytes");

/* ---------------------------------------------------------------------------
 * fq_minigame_init() — Initialize a mini-game instance.
 *
 * Sets state to FQ_MG_ACTIVE, computes difficulty and target count.
 * Score is zeroed. Hits are zeroed.
 *
 * Difficulty = min(10, level / 10).
 * Target count = 5 + difficulty * 5  (ranges from 5 to 55).
 *
 * @param mg    Pointer to the fq_minigame_t to initialize. Must not be NULL.
 * @param type  Mini-game type (SPEED, POWER, or INTEL).
 * @param level Character level used to compute difficulty [0, 255].
 * @return      GAME_OK on success, GAME_ERR_NULL_PTR if mg == NULL.
 * ---------------------------------------------------------------------------*/
game_err_t fq_minigame_init(fq_minigame_t *mg, fq_mg_type_t type, uint8_t level);

/* ---------------------------------------------------------------------------
 * fq_minigame_tick() — Advance the FSM by one input step.
 *
 * Must be called only in ACTIVE state; any other state returns
 * GAME_ERR_INVALID.
 *
 * input != 0 → hit: increments hits counter.
 * input == 0 → miss: no change to hits.
 *
 * After each tick, if hits == targets the FSM transitions to SUCCESS.
 * After each tick, if the tick count reaches targets (regardless of hits)
 * and hits < targets, the FSM transitions to FAIL.
 *
 * @param mg     Pointer to an initialized fq_minigame_t. Must not be NULL.
 * @param input  Non-zero for a successful input, zero for a miss.
 * @return       GAME_OK, GAME_ERR_NULL_PTR, or GAME_ERR_INVALID.
 * ---------------------------------------------------------------------------*/
game_err_t fq_minigame_tick(fq_minigame_t *mg, uint8_t input);

/* ---------------------------------------------------------------------------
 * fq_minigame_finish() — Compute the final score and advance to DONE.
 *
 * Must be called only in SUCCESS or FAIL state; any other state returns
 * GAME_ERR_INVALID.
 *
 * Score formula: min(100, (uint32_t)hits * 100u / targets)
 * Special case:  targets == 0 → score = 0.
 *
 * @param mg  Pointer to a mini-game in SUCCESS or FAIL state. Must not be NULL.
 * @return    GAME_OK, GAME_ERR_NULL_PTR, or GAME_ERR_INVALID.
 * ---------------------------------------------------------------------------*/
game_err_t fq_minigame_finish(fq_minigame_t *mg);

/* ---------------------------------------------------------------------------
 * fq_minigame_score() — Read the current score.
 *
 * Returns mg->score. Valid only after fq_minigame_finish() transitions state
 * to DONE; returns 0 for all other states (score field is zeroed on init).
 *
 * @param mg  Pointer to an fq_minigame_t. Returns 0 if mg == NULL.
 * @return    Score in [0, 100], or 0 on NULL input.
 * ---------------------------------------------------------------------------*/
uint8_t fq_minigame_score(const fq_minigame_t *mg);

#endif /* FIESTAQUEST_TRAINING_H */
