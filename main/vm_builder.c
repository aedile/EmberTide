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
 * Phase-19 Interactive:
 *   fq_vm_build_onboarding(), fq_vm_build_inventory_ex(),
 *   fq_vm_build_training_session() added.
 */

#include "vm_builder.h"

#include <string.h>
#include <stdint.h>

/* item_engine.h provides fq_item_lookup(). */
#include "item_engine.h"

/* progression.h provides fq_calc_xp_to_next(). */
#include "progression.h"

/* character.h provides fq_character_create() base stat table access.
 * We use an internal class-name table here — no dependency on character.c
 * internals; the names are display strings only. */
#include "character.h"

/* ---------------------------------------------------------------------------
 * Internal helper: hp_percent formula.
 * ---------------------------------------------------------------------------*/
static uint8_t calc_hp_percent(uint32_t hp_current, uint32_t hp_max)
{
    if (hp_max == 0u) {
        return 0u;
    }
    uint32_t clamped = (hp_current > hp_max) ? hp_max : hp_current;
    return (uint8_t)(clamped * 100u / hp_max);
}

/* ---------------------------------------------------------------------------
 * Internal: class name lookup table.
 * Must stay in sync with fq_class_t enum order.
 * ---------------------------------------------------------------------------*/
static const char * const k_class_names[5u] = {
    "Bruiser",    /* FQ_CLASS_BRUISER   = 0 */
    "Trickster",  /* FQ_CLASS_TRICKSTER = 1 */
    "Hex",        /* FQ_CLASS_HEX       = 2 */
    "Warden",     /* FQ_CLASS_WARDEN    = 3 */
    "Wildcard"    /* FQ_CLASS_WILDCARD  = 4 */
};

/* ---------------------------------------------------------------------------
 * Internal: class base stats table (mirrors character.c k_class_base).
 *
 * [strength, speed, precision, intelligence]
 * ---------------------------------------------------------------------------*/
static const uint8_t k_class_base_stats[5u][4u] = {
    /* Bruiser   */ { 20u, 10u,  8u,  5u },
    /* Trickster */ {  8u, 20u, 15u,  8u },
    /* Hex       */ {  5u,  8u, 12u, 20u },
    /* Warden    */ { 15u, 15u,  8u, 12u },
    /* Wildcard  */ { 10u, 10u, 10u, 10u }
};

/* ---------------------------------------------------------------------------
 * fq_vm_build_home
 * ---------------------------------------------------------------------------*/
void fq_vm_build_home(fq_vm_home_t *vm, const fq_character_t *ch)
{
    if (vm == NULL || ch == NULL) {
        return;
    }

    strncpy(vm->name, ch->name, 12u);
    vm->name[12] = '\0';

    vm->class_id    = ch->class_id;
    vm->level       = ch->level;
    vm->sprite_base = ch->sprite_base;
    vm->wins        = ch->wins;
    vm->losses      = ch->losses;

    vm->hp_percent = calc_hp_percent((uint32_t)ch->hp_max, (uint32_t)ch->hp_max);
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
 * Internal: populate a single inventory slot in the VM.
 * ---------------------------------------------------------------------------*/
static void fill_inv_slot(fq_vm_inventory_t *vm, uint8_t i, uint16_t item_id)
{
    const fq_item_def_t *def = fq_item_lookup(item_id);
    if (def != NULL) {
        strncpy(vm->item_names[i], def->name, 15u);
        vm->item_names[i][15] = '\0';
        vm->item_rarities[i]  = def->rarity;
    } else {
        vm->item_names[i][0] = '\0';
        vm->item_rarities[i] = 0u;
    }
    vm->item_equipped[i] = 0u;
}

/* ---------------------------------------------------------------------------
 * fq_vm_build_inventory
 * ---------------------------------------------------------------------------*/
void fq_vm_build_inventory(fq_vm_inventory_t *vm, const fq_inventory_t *inv)
{
    fq_vm_build_inventory_ex(vm, inv, NULL, 0u, 0u);
}

/* ---------------------------------------------------------------------------
 * fq_vm_build_inventory_ex
 * ---------------------------------------------------------------------------*/
void fq_vm_build_inventory_ex(fq_vm_inventory_t    *vm,
                               const fq_inventory_t *inv,
                               const fq_character_t *ch,
                               uint8_t               cursor,
                               uint8_t               scroll)
{
    if (vm == NULL || inv == NULL) {
        return;
    }

    uint8_t count = (inv->count > 32u) ? 32u : inv->count;

    vm->item_count   = count;
    vm->scroll_offset = scroll;

    /* Clamp cursor to valid range. */
    if (count > 0u) {
        vm->cursor_index = (cursor >= count) ? (uint8_t)(count - 1u) : cursor;
    } else {
        vm->cursor_index = 0u;
    }

    /* Count max equipped slots for display. */
    vm->equipped_count = (ch != NULL) ? ch->equipped_count : 0u;

    /* Fill item slots. */
    for (uint8_t i = 0u; i < count; i++) {
        fill_inv_slot(vm, i, inv->items[i]);
    }

    /* Zero remaining slots. */
    for (uint8_t i = count; i < 32u; i++) {
        vm->item_names[i][0]  = '\0';
        vm->item_rarities[i]  = 0u;
        vm->item_equipped[i]  = 0u;
    }

    /* Compute equipped flags: cross-reference inv->items[i] with ch->equipped[]. */
    if (ch != NULL) {
        for (uint8_t i = 0u; i < count; i++) {
            uint16_t item_id = inv->items[i];
            if (item_id == 0u) {
                vm->item_equipped[i] = 0u;
                continue;
            }
            /* Scan equipped[0..4] for a match. */
            uint8_t found = 0u;
            for (uint8_t j = 0u; j < 5u; j++) {
                if (ch->equipped[j] == item_id) {
                    found = 1u;
                    break;
                }
            }
            vm->item_equipped[i] = found;
        }
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
    vm->xp_to_next    = fq_calc_xp_to_next(ch->level);
}

/* ---------------------------------------------------------------------------
 * fq_vm_build_combat
 * ---------------------------------------------------------------------------*/
void fq_vm_build_combat(fq_vm_combat_t        *vm,
                        const fq_combat_ctx_t *ctx,
                        const fq_character_t  *c1,
                        const fq_character_t  *c2)
{
    if (vm == NULL || ctx == NULL || c1 == NULL || c2 == NULL) {
        return;
    }

    strncpy(vm->f1_name, c1->name, 12u);
    vm->f1_name[12]  = '\0';
    vm->f1_hp        = ctx->f1.hp;
    vm->f1_hp_max    = ctx->f1.hp_max;
    vm->f1_class_id  = c1->class_id;

    strncpy(vm->f2_name, c2->name, 12u);
    vm->f2_name[12]  = '\0';
    vm->f2_hp        = ctx->f2.hp;
    vm->f2_hp_max    = ctx->f2.hp_max;
    vm->f2_class_id  = c2->class_id;

    vm->round    = ctx->current_round;
    vm->finished = ctx->finished;
    vm->winner   = ctx->winner;

    vm->action_text[0] = '\0';
}

/* ---------------------------------------------------------------------------
 * fq_vm_build_idle
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

/* ---------------------------------------------------------------------------
 * fq_vm_build_onboarding
 * ---------------------------------------------------------------------------*/
void fq_vm_build_onboarding(fq_vm_onboarding_t   *vm,
                             const fq_character_t *ch,
                             uint8_t               class_index)
{
    if (vm == NULL) {
        return;
    }

    /* Clamp class_index to valid range. */
    if (class_index >= 5u) {
        class_index = 0u;
    }

    vm->class_index = class_index;

    /* Class name. */
    strncpy(vm->class_name, k_class_names[class_index], 15u);
    vm->class_name[15] = '\0';

    /* Base stats from frozen table. */
    vm->strength     = k_class_base_stats[class_index][0u];
    vm->speed        = k_class_base_stats[class_index][1u];
    vm->precision    = k_class_base_stats[class_index][2u];
    vm->intelligence = k_class_base_stats[class_index][3u];

    /* Sprite base: use character's value if available, else derive from class. */
    if (ch != NULL) {
        vm->sprite_base = ch->sprite_base;
    } else {
        vm->sprite_base = (uint8_t)(class_index * 4u); /* 4 sprites per class */
    }
}

/* ---------------------------------------------------------------------------
 * fq_vm_build_training_session
 * ---------------------------------------------------------------------------*/
void fq_vm_build_training_session(fq_vm_training_t            *vm,
                                   const fq_training_session_t *ts)
{
    if (vm == NULL || ts == NULL) {
        return;
    }

    static const char * const k_game_names[3u] = { "Speed", "Power", "Intel" };

    uint8_t gtype = ts->game_type;
    if (gtype >= 3u) { gtype = 0u; }

    strncpy(vm->game_name, k_game_names[gtype], 15u);
    vm->game_name[15] = '\0';

    vm->state        = ts->state;
    vm->score        = (ts->score > 100u) ? 100u : ts->score;
    vm->target_pos   = ts->target_pos;
    vm->targets_done = ts->targets_done;
    vm->game_type    = gtype;

    /* difficulty: 0 for now (session doesn't carry level info).
     * In a full integration, this would derive from the character level. */
    vm->difficulty = 0u;
}
