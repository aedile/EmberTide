/**
 * equip.c — FiestaQuest Inventory Equip/Unequip Implementation
 *
 * Constitution Priority 0: No float, no malloc, no PRNG.
 * Pure function: result depends only on inputs. No global mutable state.
 *
 * Duplicate item handling:
 *   When the same item ID appears in multiple inventory slots, each slot
 *   is treated as a distinct physical item. The toggle decision uses a
 *   per-slot-relative count:
 *
 *     copies_in_inv_up_to_slot = count of item_id in inv->items[0..inv_slot]
 *     copies_in_equipped       = count of item_id in ch->equipped[0..4]
 *
 *   If copies_in_equipped < copies_in_inv_up_to_slot:
 *     This slot's item is NOT equipped → EQUIP (add one copy).
 *   Else:
 *     This slot's item IS equipped → UNEQUIP (remove one copy).
 *
 * This ensures that equipping item 42 from slot 0 and then from slot 1
 * results in two copies of 42 in equipped[], and unequipping from slot 0
 * removes exactly one copy.
 */

#include "equip.h"

/* ---------------------------------------------------------------------------
 * fq_equip_toggle
 * ---------------------------------------------------------------------------*/
game_err_t fq_equip_toggle(fq_character_t       *ch,
                            const fq_inventory_t *inv,
                            uint8_t               inv_slot)
{
    if (ch == NULL || inv == NULL) {
        return GAME_ERR_NULL_PTR;
    }

    /* Validate slot range. */
    if (inv_slot >= inv->count) {
        return GAME_ERR_INVALID;
    }

    uint16_t item_id = inv->items[inv_slot];

    /* Reject item ID 0 (empty sentinel). */
    if (item_id == 0u) {
        return GAME_ERR_INVALID;
    }

    /* Clamp usable slots to the physical array size.
     * equipped_count is the max limit (default 4, max 5).
     * Values > 5 are corrupt — clamp to 5 to prevent OOB. */
    uint8_t max_slots = ch->equipped_count;
    if (max_slots > 5u) {
        max_slots = 5u;
    }

    /* ── Count copies of item_id in inv->items[0..inv_slot] (inclusive). ── */
    uint8_t copies_in_inv = 0u;
    for (uint8_t i = 0u; i <= inv_slot; i++) {
        if (inv->items[i] == item_id) {
            copies_in_inv++;
        }
    }

    /* ── Count copies of item_id currently equipped. ─────────────────────── */
    uint8_t copies_equipped = 0u;
    for (uint8_t i = 0u; i < 5u; i++) {
        if (ch->equipped[i] == item_id) {
            copies_equipped++;
        }
    }

    /* ── Decision: equip or unequip? ────────────────────────────────────── */
    if (copies_equipped < copies_in_inv) {
        /* This slot's item is NOT yet represented → EQUIP path. */

        /* Check overall slot capacity. */
        uint8_t current_total = 0u;
        for (uint8_t i = 0u; i < 5u; i++) {
            if (ch->equipped[i] != 0u) {
                current_total++;
            }
        }

        if (current_total >= max_slots) {
            return GAME_ERR_OVERFLOW;
        }

        /* Find first empty slot and fill it. */
        for (uint8_t i = 0u; i < 5u; i++) {
            if (ch->equipped[i] == 0u) {
                ch->equipped[i] = item_id;
                return GAME_OK;
            }
        }

        /* Unreachable: current_total < max_slots <= 5 guarantees a zero slot. */
        return GAME_ERR_OVERFLOW;

    } else {
        /* copies_equipped >= copies_in_inv → this slot is already equipped.
         * UNEQUIP: remove the FIRST occurrence of item_id. */
        for (uint8_t i = 0u; i < 5u; i++) {
            if (ch->equipped[i] == item_id) {
                ch->equipped[i] = 0u;
                return GAME_OK;
            }
        }

        /* Item was expected to be in equipped[] but wasn't — shouldn't happen. */
        return GAME_ERR_INVALID;
    }
}
