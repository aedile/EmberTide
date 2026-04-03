/**
 * vm_builder.c — FiestaQuest Application Layer: View Model Builder
 *
 * Translates fq_character_t / fq_inventory_t / fq_combat_ctx_t game state
 * into view model structs consumed by the presentation layer.
 *
 * Constitution Priority 0 compliance:
 *   - No floating point. HP percent uses integer-only formula.
 *   - No malloc in fast path.
 *   - No PRNG calls.
 *   - All input pointers NULL-checked before use.
 *
 * Architecture constraint: this file is the ONLY module that includes both
 * game/types.h (via vm_builder.h → types.h) and presentation/view_models.h.
 *
 * Phase-9: fq_vm_build_combat() added.
 * Phase-19.5: fq_vm_build_idle() added.
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
 * hp_percent = min(100u, hp_clamped * 100u / hp_max)
 *
 * Overflow guard: if hp_current > hp_max we clamp BEFORE multiplying so
 * that hp_clamped <= hp_max and hp_clamped * 100u <= UINT32_MAX for any
 * hp_max representable in uint16_t (max 65535 * 100 = 6,553,500 < 2^32,
 * comfortably within uint32_t range).
 *
 * This function is safe for uint16_t-range inputs (hp_max up to 65535).
 * For theoretical uint32_t inputs above ~42,949,672 (UINT32_MAX / 100),
 * the multiply clamped * 100u would overflow uint32_t. That case is
 * unreachable in production because hp_max originates from fq_character_t
 * which declares hp_max as uint16_t.
 *
 * Guards:
 *   - hp_max == 0 → return 0 (divide-by-zero protection).
 *   - hp_current > hp_max → clamp to hp_max before multiply.
 * ---------------------------------------------------------------------------*/
static uint8_t calc_hp_percent(uint32_t hp_current, uint32_t hp_max)
{
    if (hp_max == 0u) {
        return 0u;
    }
    /* Clamp-before-multiply: eliminates overflow for arbitrarily large input. */
    uint32_t clamped = (hp_current > hp_max) ? hp_max : hp_current;
    return (uint8_t)(clamped * 100u / hp_max);
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

    /* anim_frame zeroed — application layer (app_main.c) sets it from timer. */
    vm->anim_frame = 0u;
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

    /* Clamp count to the view model capacity (32 slots). */
    uint8_t count = (inv->count > 32u) ? 32u : inv->count;

    vm->item_count    = count;
    vm->cursor_index  = 0u;
    vm->scroll_offset = 0u;
    vm->equipped_count = 0u;

    for (uint8_t i = 0u; i < count; i++) {
        const fq_item_def_t *def = fq_item_lookup(inv->items[i]);
        if (def != NULL) {
            strncpy(vm->item_names[i], def->name, 15u);
            vm->item_names[i][15] = '\0';
            vm->item_rarities[i]  = def->rarity;
        } else {
            /* Unknown or null item → empty name, rarity 0. */
            vm->item_names[i][0] = '\0';
            vm->item_rarities[i] = 0u;
        }
    }

    /* Zero out remaining (unused) slots. */
    for (uint8_t i = count; i < 32u; i++) {
        vm->item_names[i][0] = '\0';
        vm->item_rarities[i] = 0u;
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

    vm->level         = ch->level;
    vm->strength      = ch->strength;
    vm->speed         = ch->speed;
    vm->precision     = ch->precision;
    vm->intelligence  = ch->intelligence;
    vm->hp_max        = ch->hp_max;
    vm->xp            = ch->xp;
    vm->rebirth_count = ch->rebirth_count;

    /* Compute XP required to reach next level (frozen formula, pure function). */
    vm->xp_to_next = fq_calc_xp_to_next(ch->level);
}

/* ---------------------------------------------------------------------------
 * fq_vm_build_combat
 *
 * Builds the combat HUD view model from a live fq_combat_ctx_t snapshot and
 * the two character records. HP values come from ctx->f1/f2 (int16_t —
 * matches fq_vm_combat_t field types exactly, no conversion hazard).
 *
 * action_text is always zeroed — the caller (event loop) fills it from the
 * latest round result after calling this function.
 *
 * NULL-safe: any NULL argument → immediate return without modifying vm.
 * ---------------------------------------------------------------------------*/
void fq_vm_build_combat(fq_vm_combat_t        *vm,
                        const fq_combat_ctx_t *ctx,
                        const fq_character_t  *c1,
                        const fq_character_t  *c2)
{
    if (vm == NULL || ctx == NULL || c1 == NULL || c2 == NULL) {
        return;
    }

    /* Fighter 1 (player) */
    strncpy(vm->f1_name, c1->name, 12u);
    vm->f1_name[12]  = '\0';
    vm->f1_hp        = ctx->f1.hp;
    vm->f1_hp_max    = ctx->f1.hp_max;
    vm->f1_class_id  = c1->class_id;

    /* Fighter 2 (enemy) */
    strncpy(vm->f2_name, c2->name, 12u);
    vm->f2_name[12]  = '\0';
    vm->f2_hp        = ctx->f2.hp;
    vm->f2_hp_max    = ctx->f2.hp_max;
    vm->f2_class_id  = c2->class_id;

    /* Round and outcome */
    vm->round    = ctx->current_round;
    vm->finished = ctx->finished;
    vm->winner   = ctx->winner;

    /* action_text cleared — caller populates from round result. */
    vm->action_text[0] = '\0';
}

/* ---------------------------------------------------------------------------
 * fq_vm_build_idle
 *
 * Builds the idle screensaver view model from a character snapshot.
 * Copies sprite_base, name (max 12 chars, null-terminated), and level.
 *
 * NULL-safe: returns immediately if vm or ch is NULL.
 * ---------------------------------------------------------------------------*/
void fq_vm_build_idle(fq_vm_idle_t *vm, const fq_character_t *ch)
{
    if (vm == NULL || ch == NULL) {
        return;
    }

    vm->sprite_base = ch->sprite_base;
    vm->level       = ch->level;

    strncpy(vm->name, ch->name, 12u);
    vm->name[12] = '\0';
}
