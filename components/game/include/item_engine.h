/**
 * item_engine.h — FiestaQuest Phase-5 Item Engine Public API.
 *
 * Implements the pluggable item evaluation loop invoked by combat.c at
 * predefined lifecycle trigger points during the combat round stepper.
 *
 * Architecture constraints:
 *   - This module lives in components/game/ — pure logic, NO HAL dependencies.
 *   - All math is integer-only. No floating point.
 *   - PRNG calls are ALWAYS consumed when an item uses them, even if the
 *     probabilistic check fails. This is mandatory for BLE sync (NTR-A1).
 *   - Recursion depth is capped at FQ_MAX_ITEM_RECURSION_DEPTH per trigger
 *     boundary (prevents ON_HEAL → HEAL → ON_HEAL infinite chains).
 *
 * Trigger ordering (PM-approved spec decision):
 *   Defender items resolve before attacker items, in slot order (0→equipped_count-1).
 *
 * Effect ordering (PM-approved spec decision):
 *   DAMAGE_ADD applied first, then DAMAGE_MULT.
 *   Final: (base + sum(DAMAGE_ADD)) * damage_mult_pct / 100
 *   Minimum damage: 1 (floor enforced in combat.c integration).
 *
 * Haymaker encoding (B2 fix):
 *   FQ_EFFECT_DAMAGE_MULT stores the delta from 100, not the raw percentage.
 *   Haymaker effect.value = 100 means 100 + 100 = 200% (x2 damage).
 *   apply_effect reconstructs: full_mult = 100 + (uint8_t)effect->value.
 *   This avoids int8_t overflow that (int8_t)200u causes.
 *
 * Item database lookup:
 *   Items are resolved by ID via a static item definition table. The table is
 *   const, lives in flash, and is never modified at runtime.
 *
 * Constitution Priority 0: This file MUST NOT import any hal_*.h headers.
 */

#ifndef FIESTAQUEST_ITEM_ENGINE_H
#define FIESTAQUEST_ITEM_ENGINE_H

#include <stdint.h>
#include "types.h"
#include "combat.h"

/* ---------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------------*/

/** Empty slot sentinel — item ID 0 is reserved as "no item". */
#define FQ_ITEM_NONE                   0u

/**
 * Maximum recursion depth per trigger boundary.
 * Prevents ON_HEAL→HEAL→ON_HEAL infinite chains.
 * After fq_item_eval_trigger is entered, item_recursion_depth is checked and
 * incremented. If already >= FQ_MAX_ITEM_RECURSION_DEPTH, returns immediately.
 *
 * Renamed from FQ_MAX_ITEM_TRIGGERS (A4 review fix: semantically clearer name).
 */
#define FQ_MAX_ITEM_RECURSION_DEPTH    1u

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/**
 * fq_item_lookup() — Look up an item definition by ID.
 *
 * Performs a linear scan of the static item table for a matching ID.
 * Unknown IDs return NULL (safe no-op, NTR-F2).
 * ID 0 (FQ_ITEM_NONE) always returns NULL.
 *
 * @param item_id  Item ID to look up.
 * @return         Const pointer to the item definition, or NULL if not found.
 */
const fq_item_def_t *fq_item_lookup(uint16_t item_id);

/**
 * fq_item_eval_trigger() — Evaluate all equipped items for a trigger point.
 *
 * Called by combat.c at appropriate lifecycle boundaries. Iterates equipped
 * items in resolver order: defender items first (slots 0→equipped_count-1),
 * then attacker items (slots 0→equipped_count-1).
 *
 * For ON_DEFEND, ON_ATTACK, ON_CRIT, ON_DODGE, ON_KILL triggers:
 *   attacking_fighter identifies who is the attacker (1 or 2).
 *   Defender items fire first.
 *
 * For PASSIVE, ON_ROUND_START, ON_ROUND_END, ON_DEATH triggers:
 *   Both fighters' items are evaluated; attacker-based ordering uses
 *   attacking_fighter to determine which fighter's items are "defender" vs
 *   "attacker" for symmetry. For round-boundary triggers, attacking_fighter
 *   still determines ordering but both fighters may have items fire.
 *
 * Items with trigger != the current trigger are silently skipped.
 * Items with ID == FQ_ITEM_NONE (0) are skipped (NTR-F2).
 * Slots beyond equipped_count are not evaluated (NTR-F3).
 *
 * Recursion guard: if ctx->item_recursion_depth >= FQ_MAX_ITEM_RECURSION_DEPTH,
 * returns immediately without evaluating any items.
 *
 * @param ctx                Initialized combat context. Must not be NULL.
 * @param trigger            The trigger point firing at this lifecycle boundary.
 * @param attacking_fighter  1 if F1 is the current attacker, 2 if F2.
 */
void fq_item_eval_trigger(fq_combat_ctx_t *ctx,
                          fq_trigger_t     trigger,
                          uint8_t          attacking_fighter);

#endif /* FIESTAQUEST_ITEM_ENGINE_H */
