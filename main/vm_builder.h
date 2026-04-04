/**
 * vm_builder.h — FiestaQuest Application Layer: View Model Builder
 *
 * Translates raw game state (fq_character_t, fq_inventory_t) into
 * presentation-ready view model structs.
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
 *
 * Phase-9 additions: fq_vm_build_combat().
 * Phase-19.5 additions: fq_vm_build_idle().
 * Phase-19 Interactive additions:
 *   fq_vm_build_onboarding() — first-boot class carousel VM.
 *   fq_vm_build_inventory_ex() — inventory VM with equip flags + cursor param.
 *   fq_vm_build_training_session() — training VM from fq_training_session_t.
 */

#ifndef FIESTAQUEST_MAIN_VM_BUILDER_H
#define FIESTAQUEST_MAIN_VM_BUILDER_H

#include <stdint.h>
#include "types.h"
#include "view_models.h"
#include "combat.h"
#include "training_session.h"

/* ---------------------------------------------------------------------------
 * fq_vm_build_home() — Build the home screen view model from a character.
 * ---------------------------------------------------------------------------*/
void fq_vm_build_home(fq_vm_home_t *vm, const fq_character_t *ch);

/* ---------------------------------------------------------------------------
 * fq_vm_build_home_ex() — Build hp_percent from explicit hp_current/hp_max.
 * ---------------------------------------------------------------------------*/
void fq_vm_build_home_ex(fq_vm_home_t *vm, uint32_t hp_current, uint32_t hp_max);

/* ---------------------------------------------------------------------------
 * fq_vm_build_inventory() — Build the inventory screen view model.
 *
 * cursor_index and scroll_offset default to 0. equipped flags are all 0
 * (no character reference). Use fq_vm_build_inventory_ex() for full control.
 *
 * NULL-safe: returns immediately if vm or inv is NULL.
 * ---------------------------------------------------------------------------*/
void fq_vm_build_inventory(fq_vm_inventory_t *vm, const fq_inventory_t *inv);

/* ---------------------------------------------------------------------------
 * fq_vm_build_inventory_ex() — Build inventory VM with cursor, scroll, and
 * equipped flags derived from the character's equipped[] array.
 *
 * Computes item_equipped[i] = 1 if inv->items[i] appears in ch->equipped[].
 * cursor_index is clamped to [0, item_count-1].
 *
 * NULL-safe: returns immediately if vm or inv is NULL.
 * If ch is NULL: equipped flags are all 0 (same as fq_vm_build_inventory).
 *
 * @param vm      Output view model.
 * @param inv     Source inventory.
 * @param ch      Source character for equipped flags (may be NULL).
 * @param cursor  Current cursor position (clamped internally).
 * @param scroll  Scroll offset.
 * ---------------------------------------------------------------------------*/
void fq_vm_build_inventory_ex(fq_vm_inventory_t    *vm,
                               const fq_inventory_t *inv,
                               const fq_character_t *ch,
                               uint8_t               cursor,
                               uint8_t               scroll);

/* ---------------------------------------------------------------------------
 * fq_vm_build_stats() — Build the stats screen view model from a character.
 * ---------------------------------------------------------------------------*/
void fq_vm_build_stats(fq_vm_stats_t *vm, const fq_character_t *ch);

/* ---------------------------------------------------------------------------
 * fq_vm_build_combat() — Build the combat HUD view model from combat context.
 * ---------------------------------------------------------------------------*/
void fq_vm_build_combat(fq_vm_combat_t        *vm,
                        const fq_combat_ctx_t *ctx,
                        const fq_character_t  *c1,
                        const fq_character_t  *c2);

/* ---------------------------------------------------------------------------
 * fq_vm_build_idle() — Build the idle screensaver view model from a character.
 * ---------------------------------------------------------------------------*/
void fq_vm_build_idle(fq_vm_idle_t *vm, const fq_character_t *ch);

/* ---------------------------------------------------------------------------
 * fq_vm_build_onboarding() — Build the onboarding carousel view model.
 *
 * Looks up the class name and base stats for the given class_index. Populates
 * vm->class_name, vm->class_index, vm->sprite_base, and the four base stats.
 *
 * class_index is clamped to [0, FQ_CLASS_COUNT-1] before lookup.
 *
 * If ch is non-NULL, sprite_base is taken from ch->sprite_base (the character
 * may already have been partially set up). If ch is NULL, a default sprite
 * index is derived from class_index.
 *
 * NULL-safe: returns immediately if vm is NULL.
 *
 * @param vm           Output view model.
 * @param ch           Source character (may be NULL — uses class defaults).
 * @param class_index  Currently selected class [0, FQ_CLASS_COUNT-1].
 * ---------------------------------------------------------------------------*/
void fq_vm_build_onboarding(fq_vm_onboarding_t   *vm,
                             const fq_character_t *ch,
                             uint8_t               class_index);

/* ---------------------------------------------------------------------------
 * fq_vm_build_training_session() — Build training VM from a training session.
 *
 * Converts fq_training_session_t into the presentation-layer fq_vm_training_t.
 * Copies state, score, game_type, target_pos, and targets_done. Populates
 * game_name from the game_type ("Speed", "Power", or "Intel").
 *
 * NULL-safe: returns immediately if vm or ts is NULL.
 *
 * @param vm  Output view model.
 * @param ts  Source training session.
 * ---------------------------------------------------------------------------*/
void fq_vm_build_training_session(fq_vm_training_t            *vm,
                                   const fq_training_session_t *ts);

#endif /* FIESTAQUEST_MAIN_VM_BUILDER_H */
