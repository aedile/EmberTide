/**
 * progression.h — FiestaQuest Progression System.
 *
 * Section 1: Effective Stat Curve Lookup.
 *   Maps a raw uint8_t stat value (0-255) to an effective stat using a frozen
 *   256-entry logarithmic lookup table. The table embodies:
 *
 *     effective = round(10 * ln(raw + 1) / ln(11))
 *
 *   This formula was evaluated offline in Python; the results are embedded as
 *   literal integers. No floating-point math ever executes at runtime.
 *
 *   Domain:  raw in [0, 255]
 *   Range:   effective in [0, 23]
 *   Properties: monotonically non-decreasing, pure function (no side effects).
 *
 *   Pinned values (from frozen table):
 *     raw=0   → 0
 *     raw=1   → 3
 *     raw=10  → 10
 *     raw=50  → 16
 *     raw=100 → 19
 *     raw=200 → 22
 *     raw=255 → 23
 *
 * Section 2: XP Curve and Level-up.
 *   XP required to advance from a given level:
 *     fq_calc_xp_to_next(level) = 50 * level * level
 *     Special: level 0 → returns 50 (minimum cost floor).
 *     Special: level 99 → returns 0 (no more leveling sentinel).
 *
 *   Level-up applies class-biased stat gains (+3 total per level):
 *     Bruiser:   +2 STR, +1 SPD, +0 PRC, +0 INT
 *     Trickster: +0 STR, +2 SPD, +1 PRC, +0 INT
 *     Hex:       +0 STR, +0 SPD, +1 PRC, +2 INT
 *     Warden:    +1 STR, +1 SPD, +0 PRC, +1 INT
 *     Wildcard:  +1 STR, +1 SPD, +1 PRC, +0 INT
 *
 *   All stats saturate at 255 (uint8_t max) — never wrap.
 *
 * Architecture constraint: this header MUST NOT include hal_*.h,
 * presentation/, or connectivity/ headers.
 */

#ifndef FIESTAQUEST_PROGRESSION_H
#define FIESTAQUEST_PROGRESSION_H

#include <stdint.h>
#include "types.h"

/* ---------------------------------------------------------------------------
 * fq_effective_stat() — Look up the effective stat for a raw stat value.
 *
 * Pure function: result depends only on `raw`. No global mutable state.
 * Safe for all uint8_t inputs including 0 and 255.
 *
 * @param raw  Raw stat value in [0, 255].
 * @return     Effective stat in [0, 23].
 * ---------------------------------------------------------------------------*/
uint8_t fq_effective_stat(uint8_t raw);

/* ---------------------------------------------------------------------------
 * fq_calc_xp_to_next() — XP required to advance from current_level.
 *
 * Formula: 50u * (uint32_t)level * (uint32_t)level
 * Special cases:
 *   - level == 0  → returns 50 (minimum cost floor; formula gives 0).
 *   - level == 99 → returns 0  (sentinel: max level, no more leveling).
 *
 * Pure function: no side effects.
 *
 * @param current_level  Level to compute cost for [0, 99].
 * @return               XP required to reach current_level + 1, or 0 if
 *                       already at max level (99).
 * ---------------------------------------------------------------------------*/
uint32_t fq_calc_xp_to_next(uint8_t current_level);

/* ---------------------------------------------------------------------------
 * fq_level_up() — Spend XP and increment level with class-biased stat gains.
 *
 * Preconditions (returns GAME_ERR_INVALID if violated):
 *   1. ch->level < 99 (already at max → reject).
 *   2. ch->xp >= fq_calc_xp_to_next(ch->level) (insufficient XP → reject).
 *
 * On success:
 *   - Subtracts xp_to_next from ch->xp.
 *   - Increments ch->level by 1.
 *   - Applies class-biased stat gains (saturating add at 255).
 *
 * @param ch  Pointer to the character to level up. Must not be NULL.
 * @return    GAME_OK, GAME_ERR_NULL_PTR, or GAME_ERR_INVALID.
 * ---------------------------------------------------------------------------*/
game_err_t fq_level_up(fq_character_t *ch);


/* ---------------------------------------------------------------------------
 * fq_combat_award_xp() — Award XP for a combat result and update win/loss.
 *
 * Phase 20: Called after BLE combat resolves.
 *
 * XP formula (on win): xp_award = 50 + (opponent_level * 10).
 * XP formula (on loss): 0 XP.
 * XP addition saturates at UINT32_MAX (never wraps).
 *
 * Increments ch->wins (on win) or ch->losses (on loss). Both uint16_t,
 * saturate at UINT16_MAX.
 *
 * After XP addition, calls fq_level_up() in a loop to process any pending
 * level-ups (single-call chains: one XP award can trigger multiple levels
 * if XP was already banked).
 *
 * NULL-safe: no-op if ch is NULL.
 *
 * @param ch              Character to award XP to. Must not be NULL.
 * @param opponent_level  Opponent character level [0..99].
 * @param won             1 = player won, 0 = player lost.
 * ---------------------------------------------------------------------------*/
void fq_combat_award_xp(fq_character_t *ch,
                         uint8_t         opponent_level,
                         uint8_t         won);

/* ---------------------------------------------------------------------------
 * fq_rebirth_reset() — Apply permadeath rebirth: reset level, halve stats, earn tokens.
 *
 * Phase 20: Called when a dead player (is_dead==1) enters the REBIRTH screen
 * and confirms via BTN_B.
 *
 * Operations (all saturating):
 *   1. tokens_earned = max(1u, ch->level / 10u)
 *   2. ch->legacy_points += tokens_earned  (saturate at 255)
 *   3. ch->rebirth_count += 1              (saturate at 255)
 *   4. ch->level = 1
 *   5. ch->strength     = ch->strength    / 2u
 *      ch->speed        = ch->speed       / 2u
 *      ch->precision    = ch->precision   / 2u
 *      ch->intelligence = ch->intelligence / 2u
 *   6. ch->xp    = 0
 *   7. ch->is_dead = 0
 *   8. ch->hp_max recalculated from new stats (same formula as fq_character_create)
 *
 * Stat halving uses integer division (floor). The minimum value after halving
 * is 0 (for odd stats like 1 -> 0). Stats are uint8_t; no negative underflow
 * is possible.
 *
 * NULL-safe: returns GAME_ERR_NULL_PTR if ch is NULL.
 *
 * @param ch  Character to rebirth. Must not be NULL.
 * @return    GAME_OK, or GAME_ERR_NULL_PTR.
 * ---------------------------------------------------------------------------*/
game_err_t fq_rebirth_reset(fq_character_t *ch);

/* ---------------------------------------------------------------------------
 * fq_legacy_spend_token() — Spend one legacy token on the next free tree node.
 *
 * Phase 20: Called when BTN_A is pressed in FQ_STATE_REBIRTH.
 *
 * Scans ch->legacy_tree for the lowest unset bit (bit 0 first). If found
 * AND ch->legacy_points > 0, sets that bit and decrements legacy_points.
 *
 * No-op conditions (returns GAME_ERR_INVALID, no state change):
 *   - ch->legacy_points == 0
 *   - All 32 bits of ch->legacy_tree are set (tree full)
 *
 * NULL-safe: returns GAME_ERR_NULL_PTR if ch is NULL.
 *
 * @param ch  Character whose legacy tree to update. Must not be NULL.
 * @return    GAME_OK on success, GAME_ERR_INVALID on no-op, GAME_ERR_NULL_PTR.
 * ---------------------------------------------------------------------------*/
game_err_t fq_legacy_spend_token(fq_character_t *ch);

#endif /* FIESTAQUEST_PROGRESSION_H */
