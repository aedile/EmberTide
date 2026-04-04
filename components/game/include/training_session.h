/**
 * training_session.h — FiestaQuest Training Session: Timing Mini-Game
 *
 * Implements the Phase-19 training input system: a tick-based timing game
 * where the player presses Button A at the right moment to score hits.
 *
 * This module is SEPARATE from the Phase-6 fq_minigame_t system. The
 * fq_minigame_t is for the older abstract FSM (WAIT/ACTIVE/SUCCESS/FAIL/DONE).
 * fq_training_session_t is the Phase-19 real-time timing game with an
 * explicit target position, hit zone, and tick-driven movement.
 *
 * State machine:
 *   FQ_TS_WAITING  (0) — game type selection screen.
 *   FQ_TS_ACTIVE   (1) — target moving; player can hit.
 *   FQ_TS_DONE     (2) — session complete; score final.
 *
 * Timing model:
 *   target_pos: uint8_t in [0, 100]. Advances by target_speed per tick.
 *   Wraps: when target_pos reaches 100, target is considered passed
 *   (no hit scored) and the next target begins from position 0.
 *
 * Hit zone: positions [40, 60] inclusive.
 *   Hit in zone  → score += 20 (clamped to 100).
 *   Hit out of zone → score -= 5 (clamped to 0, no underflow).
 *
 * 5 targets per session. After 5 targets: state = FQ_TS_DONE.
 *
 * XP award:
 *   xp = score * 2   (range 0-200).
 *   Added to ch->xp. Then fq_level_up() attempted.
 *   Level 99: fq_level_up() returns GAME_ERR_INVALID — XP preserved, no crash.
 *
 * PRNG isolation: NO fq_prng_t calls. Uses tick counter only.
 *
 * Architecture constraint: MUST NOT include hal_*.h, presentation/,
 * or connectivity/ headers.
 *
 * Constitution Priority 0: No float, no malloc, no entropy.
 *
 * Phase-19 addition.
 */

#ifndef FIESTAQUEST_GAME_TRAINING_SESSION_H
#define FIESTAQUEST_GAME_TRAINING_SESSION_H

#include <stdint.h>
#include "types.h"
#include "progression.h"

/* ---------------------------------------------------------------------------
 * fq_ts_state_t — training session state.
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_TS_WAITING = 0, /**< Pre-game: type selection. */
    FQ_TS_ACTIVE  = 1, /**< Game running: target moving, hits allowed. */
    FQ_TS_DONE    = 2  /**< Session complete: score final. */
} fq_ts_state_t;

/* ---------------------------------------------------------------------------
 * fq_ts_type_t — training game type (affects target_speed).
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_TS_SPEED = 0, /**< Fast target — trains speed. Speed = 10 pos/tick. */
    FQ_TS_POWER = 1, /**< Medium target — trains strength. Speed = 6 pos/tick. */
    FQ_TS_INTEL = 2  /**< Slow target — trains intelligence. Speed = 3 pos/tick. */
} fq_ts_type_t;

/* ---------------------------------------------------------------------------
 * Hit zone constants (inclusive range for a scoring hit).
 * ---------------------------------------------------------------------------*/
#define FQ_TS_HIT_ZONE_LO   40u  /**< Lower bound of hit zone (position >= 40). */
#define FQ_TS_HIT_ZONE_HI   60u  /**< Upper bound of hit zone (position <= 60). */

/** Score delta for an in-zone hit. */
#define FQ_TS_HIT_SCORE_ADD  20u

/** Score delta for an out-of-zone hit (subtracted, clamped to 0). */
#define FQ_TS_MISS_SCORE_SUB  5u

/** Total targets per session. */
#define FQ_TS_TOTAL_TARGETS   5u

/* ---------------------------------------------------------------------------
 * fq_training_session_t — training session state (16 bytes).
 *
 * Field layout:
 *   uint8_t  state          (1)  offset 0
 *   uint8_t  game_type      (1)  offset 1
 *   uint8_t  target_pos     (1)  offset 2   — 0-100
 *   uint8_t  targets_done   (1)  offset 3   — 0-5
 *   uint8_t  score          (1)  offset 4   — 0-100
 *   uint8_t  target_speed   (1)  offset 5   — pos/tick
 *   uint8_t  _pad[2]        (2)  offset 6
 * Total: 8 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint8_t state;          /**< fq_ts_state_t: current session state. */
    uint8_t game_type;      /**< fq_ts_type_t: which type is selected. */
    uint8_t target_pos;     /**< Current target position [0, 100]. */
    uint8_t targets_done;   /**< Targets completed so far [0, FQ_TS_TOTAL_TARGETS]. */
    uint8_t score;          /**< Current score [0, 100]. */
    uint8_t target_speed;   /**< Positions advanced per tick. */
    uint8_t _pad[2];        /**< Explicit alignment pad. */
} fq_training_session_t;

_Static_assert(sizeof(fq_training_session_t) == 8u,
    "fq_training_session_t must be exactly 8 bytes");

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/**
 * fq_training_session_init() — Initialize a session in WAITING state.
 *
 * Zeroes the session and sets game_type. State = FQ_TS_WAITING.
 * target_speed is NOT set here — it is set by fq_training_session_start().
 *
 * @param ts    Session to initialize. Must not be NULL.
 * @param type  Game type: SPEED, POWER, or INTEL.
 * @return      GAME_OK or GAME_ERR_NULL_PTR.
 */
game_err_t fq_training_session_init(fq_training_session_t *ts, fq_ts_type_t type);

/**
 * fq_training_session_start() — Transition WAITING -> ACTIVE.
 *
 * Sets target_speed based on game_type and advances to FQ_TS_ACTIVE.
 * Resets target_pos to 0.
 *
 * Returns GAME_ERR_INVALID if state != FQ_TS_WAITING.
 *
 * @param ts  Initialized session. Must not be NULL.
 * @return    GAME_OK, GAME_ERR_NULL_PTR, or GAME_ERR_INVALID.
 */
game_err_t fq_training_session_start(fq_training_session_t *ts);

/**
 * fq_training_step() — Advance the target position by one tick.
 *
 * Adds target_speed to target_pos. If target_pos > 100:
 *   - Wraps target_pos back toward 0 (target passed without hit).
 *   - Increments targets_done.
 *   - If targets_done == FQ_TS_TOTAL_TARGETS: sets state = FQ_TS_DONE.
 *
 * Returns GAME_ERR_INVALID if state != FQ_TS_ACTIVE.
 *
 * @param ts  Active session. Must not be NULL.
 * @return    GAME_OK, GAME_ERR_NULL_PTR, or GAME_ERR_INVALID.
 */
game_err_t fq_training_step(fq_training_session_t *ts);

/**
 * fq_training_hit() — Register a button-A press (hit attempt).
 *
 * Evaluates target_pos against the hit zone [FQ_TS_HIT_ZONE_LO,
 * FQ_TS_HIT_ZONE_HI].
 *   - In zone:     score += FQ_TS_HIT_SCORE_ADD (saturating at 100).
 *   - Out of zone: score -= FQ_TS_MISS_SCORE_SUB (clamped at 0).
 *
 * Then advances to the next target (increments targets_done, resets
 * target_pos to 0). If targets_done == FQ_TS_TOTAL_TARGETS: state = DONE.
 *
 * Returns GAME_ERR_INVALID if state != FQ_TS_ACTIVE.
 *
 * @param ts  Active session. Must not be NULL.
 * @return    GAME_OK, GAME_ERR_NULL_PTR, or GAME_ERR_INVALID.
 */
game_err_t fq_training_hit(fq_training_session_t *ts);

/**
 * fq_training_advance_target() — Skip current target without hit.
 *
 * Test accessor — no production caller.
 * Used in test/host/test_p19_interactive_feature.c (test F10) to advance
 * targets explicitly. Production tick loop calls fq_training_step() which
 * advances the target internally when target_pos exceeds 100.
 *
 * Increments targets_done and resets target_pos to 0. If
 * targets_done == FQ_TS_TOTAL_TARGETS, transitions to DONE.
 * Score unchanged.
 *
 * Returns GAME_ERR_INVALID if state != FQ_TS_ACTIVE.
 *
 * @param ts  Active session. Must not be NULL.
 * @return    GAME_OK, GAME_ERR_NULL_PTR, or GAME_ERR_INVALID.
 */
game_err_t fq_training_advance_target(fq_training_session_t *ts);

/**
 * fq_training_get_target_speed() — Return target_speed for a session.
 *
 * Test accessor — no production caller.
 * Used in test/host/test_p19_interactive_feature.c (test F8, F12) to verify
 * speed values after fq_training_session_start().
 *
 * Returns 0 if ts == NULL.
 *
 * @param ts  Session (may be in any state). NULL-safe.
 * @return    target_speed in [1, 255].
 */
uint8_t fq_training_get_target_speed(const fq_training_session_t *ts);

/**
 * fq_training_get_xp_award() — Compute XP award from session score.
 *
 * Formula: min(100, score) * 2.  Max award = 200.
 * Returns 0 if ts == NULL.
 *
 * @param ts  Session (state should be FQ_TS_DONE for valid score). NULL-safe.
 * @return    XP award in [0, 200].
 */
uint32_t fq_training_get_xp_award(const fq_training_session_t *ts);

/**
 * fq_training_award_xp() — Add session XP to character and attempt level-up.
 *
 * 1. Computes award = fq_training_get_xp_award(ts).
 * 2. Adds award to ch->xp.
 * 3. Attempts fq_level_up(ch). If level == 99, returns GAME_ERR_INVALID
 *    (XP already added; no crash).
 *
 * Returns GAME_OK if level-up succeeded or XP = 0 (no level-up needed).
 * Returns GAME_ERR_INVALID if level-up was attempted but rejected (L99).
 * Returns GAME_ERR_NULL_PTR if ts or ch is NULL.
 *
 * @param ts  Session (score must be final). Must not be NULL.
 * @param ch  Character to receive XP. Must not be NULL.
 * @return    GAME_OK, GAME_ERR_NULL_PTR, or GAME_ERR_INVALID.
 */
game_err_t fq_training_award_xp(const fq_training_session_t *ts,
                                 fq_character_t              *ch);

#endif /* FIESTAQUEST_GAME_TRAINING_SESSION_H */
