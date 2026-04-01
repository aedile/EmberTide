/**
 * vm_builder.c — FiestaQuest Application Layer: View Model Builder
 *
 * Translates fq_character_t / fq_inventory_t game state into view model
 * structs consumed by the presentation layer.
 *
 * Constitution Priority 0 compliance:
 *   - No floating point. HP percent uses integer-only formula.
 *   - No malloc in fast path.
 *   - No PRNG calls.
 *   - All input pointers NULL-checked before use.
 *
 * Architecture constraint: this file is the ONLY module that includes both
 * game/types.h (via vm_builder.h → types.h) and presentation/view_models.h.
 */

#include "vm_builder.h"

#include <string.h>
#include <stdint.h>

/* item_engine.h provides fq_item_lookup(). */
#include "item_engine.h"

/* progression.h provides fq_calc_xp_to_next(). */
#include "progression.h"

/* ---------------------------------------------------------------------------
 * Internal helper: hp_percent formula.
 *
 * hp_percent = min(100u, (hp_current * 100u) / hp_max)
 *
 * Guards:
 *   - hp_max == 0 → return 0 (no divide-by-zero).
 *   - Result > 100 → clamp to 100 (hp_current > hp_max edge case).
 *
 * Uses uint32_t arithmetic to prevent overflow when hp_current is large.
 * ---------------------------------------------------------------------------*/
static uint8_t calc_hp_percent(uint32_t hp_current, uint32_t hp_max)
{
    if (hp_max == 0u) {
        return 0u;
    }
    uint32_t pct = (hp_current * 100u) / hp_max;
    if (pct > 100u) {
        pct = 100u;
    }
    return (uint8_t)pct;
}

/* ---------------------------------------------------------------------------
 * fq_vm_build_home
 * ---------------------------------------------------------------------------*/
void fq_vm_build_home(fq_vm_home_t *vm, const fq_character_t *ch)
{
    if (vm == NULL || ch == NULL) {
        return;
    }

    /* Copy name with guaranteed null termination (source is 12 bytes,
     * vm->name is 13 bytes — last byte is always the terminator). */
    strncpy(vm->name, ch->name, 12u);
    vm->name[12] = '\0';

    vm->class_id    = ch->class_id;
    vm->level       = ch->level;
    vm->sprite_base = ch->sprite_base;
    vm->wins        = ch->wins;
    vm->losses      = ch->losses;

    /* Home screen always shows the character at full HP (hp_current == hp_max). */
    vm->hp_percent = calc_hp_percent((uint32_t)ch->hp_max, (uint32_t)ch->hp_max);
}

/* ---------------------------------------------------------------------------
 * fq_vm_build_home_ex
 * ---------------------------------------------------------------------------*/
void fq_vm_build_home_ex(fq_vm_home_t *vm, uint32_t hp_current, uint32_t hp_max)
{
    if (vm == NULL) {
        return;
    }
    vm->hp_percent = calc_hp_percent(hp_current, hp_max);
}

/* ---------------------------------------------------------------------------
 * fq_vm_build_inventory
 * ---------------------------------------------------------------------------*/
void fq_vm_build_inventory(fq_vm_inventory_t *vm, const fq_inventory_t *inv)
{
    if (vm == NULL || inv == NULL) {
        return;
    }

    /* Clamp count to the view model capacity. */
    uint8_t count = inv->count;
    if (count > 32u) {
        count = 32u;
    }
    vm->item_count    = count;
    vm->cursor_index  = 0u;
    vm->scroll_offset = 0u;
    vm->equipped_count = 0u;

    for (uint8_t i = 0u; i < count; i++) {
        const fq_item_def_t *def = fq_item_lookup(inv->items[i]);
        if (def != NULL) {
            strncpy(vm->item_names[i], def->name, 15u);
            vm->item_names[i][15]  = '\0';
            vm->item_rarities[i]   = def->rarity;
        } else {
            /* Unknown item: empty name, rarity 0. */
            vm->item_names[i][0]  = '\0';
            vm->item_rarities[i]  = 0u;
        }
    }

    /* Zero out remaining slots. */
    for (uint8_t i = count; i < 32u; i++) {
        vm->item_names[i][0]  = '\0';
        vm->item_rarities[i]  = 0u;
    }
}

/* ---------------------------------------------------------------------------
 * fq_vm_build_stats
 * ---------------------------------------------------------------------------*/
void fq_vm_build_stats(fq_vm_stats_t *vm, const fq_character_t *ch)
{
    if (vm == NULL || ch == NULL) {
        return;
    }

    strncpy(vm->name, ch->name, 12u);
    vm->name[12] = '\0';

    vm->level        = ch->level;
    vm->strength     = ch->strength;
    vm->speed        = ch->speed;
    vm->precision    = ch->precision;
    vm->intelligence = ch->intelligence;
    vm->hp_max       = ch->hp_max;
    vm->xp           = ch->xp;
    vm->rebirth_count = ch->rebirth_count;

    /* Compute XP required to reach next level using the frozen formula. */
    vm->xp_to_next = fq_calc_xp_to_next(ch->level);
}
