/**
 * combat.h — FiestaQuest Phase-4/5 Combat Engine Public API.
 *
 * Implements the deterministic, stepper-based combat engine.
 * All math is integer-only. No floating point. No malloc in the fast path.
 * All state is stack-allocatable via fq_combat_ctx_t.
 *
 * PRNG call ordering per round (per design doc Appendix A and B4 rework):
 *   1. Attacker 1 attack roll:             fq_prng_range(1,6)   — always
 *   2. [Conditional] Attacker 1 self-rr:   fq_prng_range(1,6)   — if atk_roll<=2 AND charges>0
 *   3. Dodge roll for attack 1:            fq_prng_range(1,100) — always
 *   4. Attacker 2 attack roll:             fq_prng_range(1,6)   — always
 *   5. [Conditional] Attacker 2 self-rr:   fq_prng_range(1,6)   — if atk_roll<=2 AND charges>0
 *   6. [Conditional] Attacker 1 def-rr:    fq_prng_range(1,6)   — if attacker2_roll>=5 AND atk1 charges>0
 *   7. Dodge roll for attack 2:            fq_prng_range(1,100) — always
 *   8. Lucky Star F1:                      fq_prng_range(1,20)  — ALWAYS consumed
 *   9. Lucky Star F2:                      fq_prng_range(1,20)  — ALWAYS consumed
 *
 * Phase 5 additions to PRNG ordering (items that use PRNG always consume):
 *   Pre-round (ON_ROUND_START, after initiative before attacks):
 *     Lucky Coin holders: fq_prng_range(1,100) — ALWAYS consumed per holder
 *     Chaos Orb holders:  fq_prng_range(0,3)   — ALWAYS consumed per holder
 *
 * Crit is determined from the ORIGINAL d6 raw_roll (before precision table adjustment),
 * NOT from a separate PRNG call (B4 fix — crit PRNG call removed entirely).
 *
 * Initiative (B1 fix): fq_prng_range(1,6) + eff_speed/3 (was d100 + full eff_speed).
 *
 * Dodge clamp (B3 fix): [5, 40] (was [5, 75]).
 *
 * Determinism guarantee: both devices must follow the same branch decisions
 * because they share the same seed. PRNG stream stays synchronized.
 *
 * Constitution Priority 0: This file MUST NOT import any hal_*.h headers.
 *
 * Architecture: belongs in components/game/ — pure logic, no HAL dependencies.
 *
 * v2.3 amendment: Phase 5 item-aware fq_combat_fighter_t added.
 *   equipped_items[5], equipped_count, damage_bonus, damage_mult_pct, dodge_bonus.
 *   fq_combat_ctx_t gains round_3_f1_hp, round_3_f2_hp, time_loop_used,
 *   item_recursion_depth.
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
 * Phase 5: expanded with item slots and per-round item state accumulators.
 *
 * HP tracking uses int16_t to allow signed arithmetic during damage
 * application without intermediate unsigned underflow. hp_max is also
 * int16_t here (clamped from uint16_t in fq_combat_init) so percentage
 * calculations stay in signed arithmetic.
 *
 * Field order: int16_t pair, then uint16_t array, then uint8_t cluster.
 * This minimizes padding on 32-bit aligned architectures (ESP32-S3).
 *
 * Layout (24 bytes, no hidden padding):
 *   int16_t  hp               (2)  offset 0
 *   int16_t  hp_max           (2)  offset 2
 *   uint16_t equipped_items[5](10) offset 4   — item IDs, 0 = empty
 *   uint8_t  strength         (1)  offset 14
 *   uint8_t  speed            (1)  offset 15
 *   uint8_t  precision        (1)  offset 16
 *   uint8_t  intelligence     (1)  offset 17
 *   uint8_t  reroll_charges   (1)  offset 18
 *   uint8_t  class_id         (1)  offset 19
 *   uint8_t  equipped_count   (1)  offset 20  — controls iteration boundary
 *   int8_t   damage_bonus     (1)  offset 21  — per-round DAMAGE_ADD accumulator
 *   uint8_t  damage_mult_pct  (1)  offset 22  — per-round mult (100=1x, 150=1.5x, 200=2x)
 *   uint8_t  dodge_bonus      (1)  offset 23  — per-round dodge chance addition
 * Total: 24 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    int16_t  hp;                   /**< Current HP. Signed: allows 0 as KO threshold. */
    int16_t  hp_max;               /**< Max HP for percentage calc. Clamped to INT16_MAX. */
    uint16_t equipped_items[5];    /**< Item IDs in equipped slots. 0 = FQ_ITEM_NONE. */
    uint8_t  strength;             /**< Raw strength stat (from character). */
    uint8_t  speed;                /**< Raw speed stat. */
    uint8_t  precision;            /**< Raw precision stat. */
    uint8_t  intelligence;         /**< Raw intelligence stat. */
    uint8_t  reroll_charges;       /**< eff_int / 4, decremented on use. */
    uint8_t  class_id;             /**< Copied from fq_character_t for future passives. */
    uint8_t  equipped_count;       /**< Active item count (0-5). Controls slot iteration. */
    int8_t   damage_bonus;         /**< Accumulated DAMAGE_ADD bonuses for this round. */
    uint8_t  damage_mult_pct;      /**< Damage multiplier % (100=normal, 150=crit, 200=Haymaker). */
    uint8_t  dodge_bonus;          /**< Added to dodge chance for this round. */
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
    uint8_t  f1_rerolled;      /**< 1 if F1 used a reroll charge this round (self or defensive). */
    uint8_t  f2_rerolled;      /**< 1 if F2 used a reroll charge this round (self or defensive). */
    uint8_t  f1_lucky_star;    /**< 1 if F1 Lucky Star bonus triggered (Phase 5+). */
    uint8_t  f2_lucky_star;    /**< 1 if F2 Lucky Star bonus triggered (Phase 5+). */
    uint8_t  overtime_damage;  /**< Overtime damage applied to both (0 if round <= 8). */
    uint8_t  finished;         /**< 1 if combat ended this round. */
    uint8_t  winner;           /**< 0=none/draw, 1=F1, 2=F2. Only valid if finished. */
} fq_round_result_t;

/* ---------------------------------------------------------------------------
 * fq_combat_ctx_t — full combat context; stack-allocatable.
 *
 * Phase 5: expanded with Time Loop snapshot fields and item recursion guard.
 *
 * Contains both fighter snapshots, the PRNG state, and combat metadata.
 * This struct is the sole mutable state for a fight — no globals, no heap.
 *
 * Layout (64 bytes, no hidden padding):
 *   fq_combat_fighter_t f1        (24)  offset 0
 *   fq_combat_fighter_t f2        (24)  offset 24
 *   fq_prng_t           rng       (4)   offset 48
 *   int16_t  round_3_f1_hp        (2)   offset 52  — Time Loop HP snapshot
 *   int16_t  round_3_f2_hp        (2)   offset 54  — Time Loop HP snapshot
 *   uint8_t  current_round        (1)   offset 56
 *   uint8_t  finished             (1)   offset 57
 *   uint8_t  winner               (1)   offset 58
 *   uint8_t  first_attacker       (1)   offset 59
 *   uint8_t  time_loop_used       (1)   offset 60  — once-per-fight flag
 *   uint8_t  item_recursion_depth (1)   offset 61  — recursion guard
 *   uint8_t  _pad[2]              (2)   offset 62  — explicit alignment pad
 * Total: 64 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    fq_combat_fighter_t f1;              /**< Fighter 1 snapshot. */
    fq_combat_fighter_t f2;              /**< Fighter 2 snapshot. */
    fq_prng_t           rng;             /**< PRNG state (advanced each round). */
    int16_t             round_3_f1_hp;   /**< Time Loop: F1 HP at end of round 3. */
    int16_t             round_3_f2_hp;   /**< Time Loop: F2 HP at end of round 3. */
    uint8_t             current_round;   /**< Current round number (starts at 1). */
    uint8_t             finished;        /**< 1 when combat is over. */
    uint8_t             winner;          /**< 0=none, 1=F1, 2=F2. */
    uint8_t             first_attacker;  /**< 1 or 2 — determined by initiative at init. */
    uint8_t             time_loop_used;  /**< 1 if Time Loop has already fired this fight. */
    uint8_t             item_recursion_depth; /**< Current recursion depth for item eval. */
    uint8_t             _pad[2];         /**< Explicit alignment pad — reserved, write as 0. */
} fq_combat_ctx_t;

/*
 * N16: Compile-time size check.
 * fq_combat_fighter_t: 2+2+10+8 = 24 bytes (no padding gaps expected).
 * fq_combat_ctx_t: 2*24 + 4(prng) + 2+2 + 6*1 + 2 = 64 bytes.
 * fq_round_result_t: 2+2+1+1+12*1 = 18 bytes.
 */
_Static_assert(sizeof(fq_combat_fighter_t) == 24u,
    "fq_combat_fighter_t must be exactly 24 bytes (Phase 5 expanded)");

_Static_assert(sizeof(fq_combat_ctx_t) == 64u,
    "fq_combat_ctx_t must be exactly 64 bytes (Phase 5 expanded)");

_Static_assert(sizeof(fq_round_result_t) == 18u,
    "fq_round_result_t must be exactly 18 bytes");

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/**
 * fq_combat_init() — Initialize a combat context from two character pointers.
 *
 * Performs NULL guard, stat copy, HP clamp, initiative roll (B1: d6 + eff_speed/3),
 * and reroll charge calculation. Phase 5: copies equipped item IDs from character
 * and initializes per-round item state (damage_bonus=0, damage_mult_pct=100,
 * dodge_bonus=0). Time Loop fields initialized to 0.
 *
 * After a successful return, ctx is ready for fq_combat_step() calls.
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
 * initiative order), checks for KO, applies Lucky Star PRNG calls (d20),
 * applies overtime damage, and checks the round limit.
 *
 * Phase 5: Calls fq_item_eval_trigger at these points:
 *   1. ON_ROUND_START — after initiative, before attacks (resets per-round state)
 *   2. ON_DEFEND      — before each dodge check resolves
 *   3. ON_ATTACK      — before each attack resolves (damage accumulation)
 *   4. ON_CRIT        — when a crit fires
 *   5. ON_DODGE       — when a dodge succeeds
 *   6. ON_KILL        — when a KO happens (first kill ends fight)
 *   7. ON_DEATH       — when a fighter reaches 0 HP
 *   8. ON_ROUND_END   — after overtime, before round limit check
 *   PASSIVE trigger is evaluated once during fq_combat_init.
 *
 * PRNG ordering (per Appendix A):
 *   Attack 1: d6 → [conditional self-reroll d6] → dodge d100
 *   Attack 2: d6 → [conditional self-reroll d6] → [conditional defensive-reroll d6] → dodge d100
 *   Lucky Star: d20 (F1) → d20 (F2)
 *
 * Item PRNG calls (always consumed):
 *   ON_ROUND_START: Lucky Coin d100 (per owner), Chaos Orb d4 (per owner)
 *
 * Crit is determined from original raw d6 roll, NOT a separate PRNG call.
 *
 * If ctx->finished is already 1, returns a zeroed result with
 * finished=1 and winner set — NO-OP, PRNG state is NOT advanced.
 *
 * @param ctx  Initialized combat context. Must not be NULL (undefined if NULL).
 * @return     fq_round_result_t describing what happened this round.
 */
fq_round_result_t fq_combat_step(fq_combat_ctx_t *ctx);

#endif /* FIESTAQUEST_COMBAT_H */
