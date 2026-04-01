/**
 * vm_builder.h — FiestaQuest Application Layer: View Model Builder
 *
 * Translates raw game state (fq_character_t, fq_inventory_t) into
 * presentation-ready view model structs (fq_vm_home_t, fq_vm_inventory_t,
 * fq_vm_stats_t).
 *
 * Architecture placement: main/ (application layer).
 *   - This is the ONLY module that includes BOTH game/types.h AND
 *     presentation/view_models.h.
 *   - Screen renderers (presentation/) NEVER include game/ headers.
 *   - game/ modules NEVER include presentation/ headers.
 *
 * All functions are NULL-safe: passing NULL for any pointer is a safe no-op.
 * No floating point. No malloc. Integer-only HP formula with div-zero guard.
 *
 * Host-compilable: no hal_*.h included.
 */

#ifndef FIESTAQUEST_MAIN_VM_BUILDER_H
#define FIESTAQUEST_MAIN_VM_BUILDER_H

#include <stdint.h>
#include "types.h"
#include "view_models.h"

/* ---------------------------------------------------------------------------
 * fq_vm_build_home() — Build the home screen view model from a character.
 *
 * Populates vm with the character's name (null-terminated, max 12 chars),
 * class_id, level, sprite_base, wins, losses, and hp_percent=100 (home
 * screen always shows full HP — combat HP is not persisted in the save).
 *
 * NULL-safe: returns immediately if vm or ch is NULL.
 *
 * @param vm  Output view model. NULL-safe.
 * @param ch  Source character. NULL-safe.
 * ---------------------------------------------------------------------------*/
void fq_vm_build_home(fq_vm_home_t *vm, const fq_character_t *ch);

/* ---------------------------------------------------------------------------
 * fq_vm_build_home_ex() — Build hp_percent from explicit hp_current/hp_max.
 *
 * Used for unit testing the HP percent formula independently of a character.
 * hp_percent = min(100, hp_current * 100 / hp_max)
 * If hp_max == 0, hp_percent = 0 (no divide-by-zero).
 *
 * @param vm          Output view model. NULL-safe: returns if NULL.
 * @param hp_current  Current HP value.
 * @param hp_max      Maximum HP value (0 → hp_percent = 0).
 * ---------------------------------------------------------------------------*/
void fq_vm_build_home_ex(fq_vm_home_t *vm, uint32_t hp_current, uint32_t hp_max);

/* ---------------------------------------------------------------------------
 * fq_vm_build_inventory() — Build the inventory screen view model.
 *
 * For each slot in inv->items[0..inv->count-1], looks up the item definition
 * via fq_item_lookup(). If found, copies the name (max 15 chars, null-
 * terminated) and rarity into the view model. Unknown items get empty name
 * and rarity 0.
 *
 * NULL-safe: returns immediately if vm or inv is NULL.
 *
 * @param vm   Output view model. NULL-safe.
 * @param inv  Source inventory. NULL-safe.
 * ---------------------------------------------------------------------------*/
void fq_vm_build_inventory(fq_vm_inventory_t *vm, const fq_inventory_t *inv);

/* ---------------------------------------------------------------------------
 * fq_vm_build_stats() — Build the stats screen view model from a character.
 *
 * Copies all stat fields and computes xp_to_next via fq_calc_xp_to_next().
 *
 * NULL-safe: returns immediately if vm or ch is NULL.
 *
 * @param vm  Output view model. NULL-safe.
 * @param ch  Source character. NULL-safe.
 * ---------------------------------------------------------------------------*/
void fq_vm_build_stats(fq_vm_stats_t *vm, const fq_character_t *ch);

#endif /* FIESTAQUEST_MAIN_VM_BUILDER_H */
