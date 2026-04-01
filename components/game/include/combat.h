/**
 * combat.h — FiestaQuest Phase-4 Combat Engine Public API.
 *
 * Implements the deterministic, stepper-based combat engine.
 * All math is integer-only. No floating point. No malloc in the fast path.
 * All state is stack-allocatable via fq_combat_ctx_t.
 *
 * PRNG call ordering per round (per Appendix A):
 *   1. Attacker 1 attack roll:       fq_prng_range(1,6)
 *   2. Dodge roll for attack 1:      fq_prng_range(1,100)
 *   3. [Conditional] Reroll:         fq_prng_range(1,6)  — only if atk<=2 and charges>0
 *   4. [Conditional] Crit check 1:   fq_prng_range(1,100) — only if hit
 *   5. Attacker 2 attack roll:       fq_prng_range(1,6)
 *   6. Dodge roll for attack 2:      fq_prng_range(1,100)
 *   7. [Conditional] Reroll:         fq_prng_range(1,6)  — only if atk<=2 and charges>0
 *   8. [Conditional] Crit check 2:   fq_prng_range(1,100) — only if hit
 *   9. Lucky Star F1:                fq_prng_range(1,100) — ALWAYS consumed
 *  10. Lucky Star F2:                fq_prng_range(1,100) — ALWAYS consumed
 *
 * Determinism guarantee: both devices must follow the same branch decisions
 * because they share the same seed. PRNG stream stays synchronized.
 *
 * Constitution Priority 0: This file MUST NOT import any hal_*.h headers.
 *
 * Architecture: belongs in components/game/ — pure logic, no HAL dependencies.
 */

#ifndef FIESTAQUEST_COMBAT_H
#define FIESTAQUEST_COMBAT_H

#include <stdint.h>
#include <limits.h>
#include "types.h"
#include "prng.h"

/* ---------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------------*/

/** Maximum number of rounds before fight is decided by HP percentage. */
#define FQ_MAX_ROUNDS          12u

/** Round number at which overtime damage begins (round > threshold). */
#define FQ_OVERTIME_THRESHOLD   8u

/* ---------------------------------------------------------------------------
 * fq_combat_fighter_t — per-fighter snapshot copied at init.
 *
 * HP tracking uses int16_t to allow signed arithmetic during damage
 * application without intermediate unsigned underflow. hp_max is also
 * int16_t here (clamped from uint16_t in fq_combat_init) so percentage
 * calculations stay in signed arithmetic.
 *
 * Field order: int16_t pair first (largest), then uint8_t cluster.
 * ---------------------------------------------------------------------------*/
typedef struct {
    int16_t  hp;              /**< Current HP. Signed: allows 0 as KO threshold. */
    int16_t  hp_max;          /**< Max HP for percentage calc. Clamped to INT16_MAX. */
    uint8_t  strength;        /**< Raw strength stat (from character). */
    uint8_t  speed;           /**< Raw speed stat. */
    uint8_t  precision;       /**< Raw precision stat. */
    uint8_t  intelligence;    /**< Raw intelligence stat. */
    uint8_t  reroll_charges;  /**< eff_int / 4, decremented on use. */
    uint8_t  class_id;        /**< Copied from fq_character_t for future passives. */
} fq_combat_fighter_t;

/* ---------------------------------------------------------------------------
 * fq_round_result_t — returned by fq_combat_step() after each round.
 *
 * All damage fields are int8_t (clamped to [0, INT8_MAX]).
 * Flags are uint8_t (0 or 1) for explicit binary representation.
 * ---------------------------------------------------------------------------*/
typedef struct {
    int16_t  f1_hp;            /**< F1 HP after this round. */
    int16_t  f2_hp;            /**< F2 HP after this round. */
    int8_t   f1_damage_dealt;  /**< Damage F1 dealt (0 if missed, clamped to INT8_MAX). */
    int8_t   f2_damage_dealt;  /**< Damage F2 dealt (0 if missed, clamped to INT8_MAX). */
    uint8_t  round;            /**< Which round this was (1-12). */
    uint8_t  f1_hit;           /**< 1 if F1's attack landed, 0 if dodged. */
    uint8_t  f2_hit;           /**< 1 if F2's attack landed, 0 if dodged. */
    uint8_t  f1_crit;          /**< 1 if F1 scored a critical hit. */
    uint8_t  f2_crit;          /**< 1 if F2 scored a critical hit. */
    uint8_t  f1_rerolled;      /**< 1 if F1 used a reroll charge this round. */
    uint8_t  f2_rerolled;      /**< 1 if F2 used a reroll charge this round. */
    uint8_t  f1_lucky_star;    /**< 1 if F1 Lucky Star bonus triggered (Phase 5+). */
    uint8_t  f2_lucky_star;    /**< 1 if F2 Lucky Star bonus triggered (Phase 5+). */
    uint8_t  overtime_damage;  /**< Overtime damage applied to both (0 if round <= 8). */
    uint8_t  finished;         /**< 1 if combat ended this round. */
    uint8_t  winner;           /**< 0=none/draw, 1=F1, 2=F2. Only valid if finished. */
} fq_round_result_t;

/* ---------------------------------------------------------------------------
 * fq_combat_ctx_t — full combat context; stack-allocatable.
 *
 * Contains both fighter snapshots, the PRNG state, and combat metadata.
 * This struct is the sole mutable state for a fight — no globals, no heap.
 * ---------------------------------------------------------------------------*/
typedef struct {
    fq_combat_fighter_t f1;             /**< Fighter 1 snapshot. */
    fq_combat_fighter_t f2;             /**< Fighter 2 snapshot. */
    fq_prng_t           rng;            /**< PRNG state (advanced each round). */
    uint8_t             current_round;  /**< Current round number (starts at 1). */
    uint8_t             finished;       /**< 1 when combat is over. */
    uint8_t             winner;         /**< 0=none, 1=F1, 2=F2. */
    uint8_t             first_attacker; /**< 1 or 2 — determined by initiative at init. */
} fq_combat_ctx_t;

/*
 * N16: Compile-time size check.
 * fq_combat_fighter_t: 2+2+6 = 10 bytes (no padding gaps expected).
 * fq_combat_ctx_t: 2*10 + 4(prng) + 4*1 = 28 bytes.
 * Verified at compile time:
 */
_Static_assert(sizeof(fq_combat_fighter_t) == 10u,
    "fq_combat_fighter_t must be exactly 10 bytes");

_Static_assert(sizeof(fq_combat_ctx_t) == 28u,
    "fq_combat_ctx_t must be exactly 28 bytes");

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/**
 * fq_combat_init() — Initialize a combat context from two character pointers.
 *
 * Performs NULL guard, stat copy, HP clamp, initiative roll, and reroll
 * charge calculation. After a successful return, ctx is ready for
 * fq_combat_step() calls.
 *
 * @param ctx   Output context. Must not be NULL.
 * @param c1    Fighter 1 character. Must not be NULL.
 * @param c2    Fighter 2 character. Must not be NULL.
 * @param seed  PRNG seed. If 0, treated as 1 (xorshift zero guard).
 * @return      GAME_OK on success, GAME_ERR_NULL_PTR if any pointer is NULL.
 */
game_err_t fq_combat_init(fq_combat_ctx_t        *ctx,
                           const fq_character_t   *c1,
                           const fq_character_t   *c2,
                           uint32_t                seed);

/**
 * fq_combat_step() — Execute one round of combat.
 *
 * Advances the combat state by one round: resolves both attacks (in
 * initiative order), checks for KO, applies Lucky Star PRNG calls,
 * applies overtime damage, and checks the round limit.
 *
 * If ctx->finished is already 1, returns a zeroed result with
 * finished=1 and winner set — NO-OP, PRNG state is NOT advanced.
 *
 * @param ctx  Initialized combat context. Must not be NULL (undefined if NULL).
 * @return     fq_round_result_t describing what happened this round.
 */
fq_round_result_t fq_combat_step(fq_combat_ctx_t *ctx);

#endif /* FIESTAQUEST_COMBAT_H */
