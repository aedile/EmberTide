/**
 * legacy.h — FiestaQuest Rebirth & Legacy Tree System.
 *
 * Defines the 32-bit bitmask legacy tree with 16 named nodes (bits 0-15).
 * Bits 16-31 are reserved and silently ignored by all functions.
 *
 * Tier layout (4 nodes per tier):
 *   Tier 1 (bits 0- 3): No prerequisites.
 *   Tier 2 (bits 4- 7): Require >= 1 Tier-1 node.
 *   Tier 3 (bits 8-11): Require >= 2 Tier-2 nodes.
 *   Tier 4 (bits 12-15): Require >= 2 Tier-3 nodes.
 *
 * Rebirth stat retention (applied to gained stats above class base):
 *   No retention perk:  50% of gains retained (floor division).
 *   SOFT_LANDING (T2):  60% of gains retained.
 *   PHOENIX_FLAME (T3): 75% of gains retained.
 *   When multiple perks apply, max() is used.
 *   Stats never drop below class base.
 *
 * Class base stats (from design doc Section 7):
 *   Bruiser:   STR=3, SPD=0, PRC=0, INT=0, HP=60
 *   Trickster: STR=0, SPD=3, PRC=1, INT=0, HP=40
 *   Hex:       STR=0, SPD=0, PRC=1, INT=3, HP=45
 *   Warden:    STR=1, SPD=1, PRC=0, INT=1, HP=55
 *   Wildcard:  STR=1, SPD=1, PRC=1, INT=0, HP=50
 *
 * Architecture constraint: this header MUST NOT include hal_*.h,
 * presentation/, or connectivity/ headers.
 */

#ifndef FIESTAQUEST_LEGACY_H
#define FIESTAQUEST_LEGACY_H

#include <stdint.h>
#include "types.h"
#include "prng.h"

/* ---------------------------------------------------------------------------
 * Legacy node bitmask constants.
 * Bits 0-15 are defined. Bits 16-31 are reserved (silently ignored).
 * ---------------------------------------------------------------------------*/

/* Tier 1 — no prerequisites */
#define FQ_LEGACY_THICK_SKIN_1  (1u << 0)  /**< +5 HP on rebirth. */
#define FQ_LEGACY_THICK_SKIN_2  (1u << 1)  /**< +5 HP on rebirth. */
#define FQ_LEGACY_KEEN_EYE      (1u << 2)  /**< +2 precision base. */
#define FQ_LEGACY_QUICK_FEET    (1u << 3)  /**< +2 speed base. */

/* Tier 2 — require >= 1 Tier-1 node */
#define FQ_LEGACY_IRON_WILL     (1u << 4)  /**< +3 intelligence base. */
#define FQ_LEGACY_SCAVENGER     (1u << 5)  /**< 5th equipment slot. */
#define FQ_LEGACY_SOFT_LANDING  (1u << 6)  /**< 60% stat retention on rebirth. */
#define FQ_LEGACY_DEEP_POCKETS  (1u << 7)  /**< +8 inventory slots (deferred). */

/* Tier 3 — require >= 2 Tier-2 nodes */
#define FQ_LEGACY_PHOENIX_FLAME (1u << 8)  /**< 75% stat retention on rebirth. */
#define FQ_LEGACY_VETERAN_MARK  (1u << 9)  /**< +10 HP on rebirth. */
#define FQ_LEGACY_BATTLE_SCARS  (1u << 10) /**< +3 strength base. */
#define FQ_LEGACY_SIXTH_SENSE   (1u << 11) /**< +3 speed base. */

/* Tier 4 — require >= 2 Tier-3 nodes */
#define FQ_LEGACY_MASTER_MIND   (1u << 12) /**< +5 intelligence base. */
#define FQ_LEGACY_DIAMOND_SKIN  (1u << 13) /**< +15 HP on rebirth. */
#define FQ_LEGACY_GODSPEED      (1u << 14) /**< +5 speed base. */
#define FQ_LEGACY_BERSERKER     (1u << 15) /**< +5 strength base. */

/* Reserved: bits 16-31 are undefined; fq_legacy_apply_bonuses() ignores them. */
#define FQ_LEGACY_RESERVED_MASK (0xFFFF0000u)

/* ---------------------------------------------------------------------------
 * fq_legacy_tier() — Return the tier index for a node index.
 *
 * Returns 0 for nodes 0-3 (T1), 1 for 4-7 (T2), 2 for 8-11 (T3),
 * 3 for 12-15 (T4). For node_index >= 16, returns 0 (defensive).
 *
 * @param node_index  Node index in [0, 15].
 * @return            Tier index in [0, 3].
 * ---------------------------------------------------------------------------*/
uint8_t fq_legacy_tier(uint8_t node_index);

/* ---------------------------------------------------------------------------
 * fq_legacy_count_tier() — Count the number of set bits in a given tier.
 *
 * Counts bits [tier*4, tier*4+3] that are set in the tree bitmask.
 *
 * @param tree  Legacy tree bitmask (ch->legacy_tree).
 * @param tier  Tier index in [0, 3].
 * @return      Count of set bits in that tier [0, 4].
 * ---------------------------------------------------------------------------*/
uint8_t fq_legacy_count_tier(uint32_t tree, uint8_t tier);

/* ---------------------------------------------------------------------------
 * fq_legacy_unlock_node() — Spend one legacy point to unlock a node.
 *
 * Preconditions (returns GAME_ERR_INVALID if violated):
 *   1. ch->legacy_points >= 1 (insufficient points → reject).
 *   2. Node not already set in ch->legacy_tree (re-unlock → reject).
 *   3. Tier prerequisites satisfied:
 *      - T2 node: at least 1 T1 node set.
 *      - T3 node: at least 2 T2 nodes set.
 *      - T4 node: at least 2 T3 nodes set.
 *      - T1 node: no prerequisites.
 *
 * On success:
 *   - Sets the bit for node_index in ch->legacy_tree.
 *   - Decrements ch->legacy_points by 1.
 *
 * @param ch          Pointer to the character. Must not be NULL.
 * @param node_index  Node index in [0, 15].
 * @return            GAME_OK, GAME_ERR_NULL_PTR, or GAME_ERR_INVALID.
 * ---------------------------------------------------------------------------*/
game_err_t fq_legacy_unlock_node(fq_character_t *ch, uint8_t node_index);

/* ---------------------------------------------------------------------------
 * fq_rebirth() — Execute rebirth: reset character with stat penalties.
 *
 * Preconditions (returns error if violated):
 *   - ch != NULL and rng != NULL → GAME_ERR_NULL_PTR.
 *   - ch->is_dead == 1 → GAME_ERR_INVALID if alive.
 *
 * Rebirth procedure:
 *   1. Determine retention rate: 50% (none), 60% (SOFT_LANDING), 75%
 *      (PHOENIX_FLAME). Take the maximum of all applicable rates.
 *   2. For each stat: new_stat = class_base + floor((stat - class_base)
 *      * rate / 100). Floor at class_base (never below).
 *   3. Increment rebirth_count (saturate at 255).
 *   4. Calculate rebirth tokens and add to legacy_points (saturate at 255).
 *   5. Set is_dead = 0.
 *   6. If class is WILDCARD: reroll wildcard_passive = rng result in [0, 3].
 *   7. hp_max is NOT recalculated here — caller must invoke
 *      fq_legacy_apply_bonuses() afterwards to apply any HP bonuses.
 *      Base HP is restored to the class base value.
 *
 * @param ch   Pointer to the (dead) character. Must not be NULL.
 * @param rng  Pointer to an initialized PRNG. Must not be NULL.
 * @return     GAME_OK, GAME_ERR_NULL_PTR, or GAME_ERR_INVALID.
 * ---------------------------------------------------------------------------*/
game_err_t fq_rebirth(fq_character_t *ch, fq_prng_t *rng);

/* ---------------------------------------------------------------------------
 * fq_legacy_apply_bonuses() — Apply all legacy tree bonuses to a character.
 *
 * Reads ch->legacy_tree and applies stat/HP bonuses for each set bit.
 * Reserved bits (16-31) are silently ignored.
 * Stats saturate at 255. hp_max saturates at UINT16_MAX.
 *
 * Bonus table:
 *   THICK_SKIN_1  → +5 hp_max
 *   THICK_SKIN_2  → +5 hp_max
 *   KEEN_EYE      → +2 precision
 *   QUICK_FEET    → +2 speed
 *   IRON_WILL     → +3 intelligence
 *   SCAVENGER     → (sets equipped_count to 5 if < 5)
 *   SOFT_LANDING  → (retention perk, no stat bonus)
 *   DEEP_POCKETS  → (deferred, no stat bonus)
 *   PHOENIX_FLAME → (retention perk, no stat bonus)
 *   VETERAN_MARK  → +10 hp_max
 *   BATTLE_SCARS  → +3 strength
 *   SIXTH_SENSE   → +3 speed
 *   MASTER_MIND   → +5 intelligence
 *   DIAMOND_SKIN  → +15 hp_max
 *   GODSPEED      → +5 speed
 *   BERSERKER     → +5 strength
 *
 * @param ch  Pointer to the character. If NULL, does nothing.
 * ---------------------------------------------------------------------------*/
void fq_legacy_apply_bonuses(fq_character_t *ch);

/* ---------------------------------------------------------------------------
 * fq_calc_rebirth_tokens() — Calculate rebirth tokens earned.
 *
 * Formula: (level / 10) + (wins / 100), saturated to uint8_t max (255).
 *
 * @param level  Character level at time of death [0, 255].
 * @param wins   Total wins accumulated [0, 65535].
 * @return       Token count in [0, 255].
 * ---------------------------------------------------------------------------*/
uint8_t fq_calc_rebirth_tokens(uint8_t level, uint16_t wins);

#endif /* FIESTAQUEST_LEGACY_H */
