/**
 * equip.h — FiestaQuest Game Engine: Inventory Equip/Unequip
 *
 * Provides fq_equip_toggle(), the canonical pure function for equipping
 * and unequipping items from fq_character_t.equipped[].
 *
 * Semantics:
 *   - equipped[5] stores item IDs. Slot value 0 = empty sentinel.
 *   - equipped_count is the MAX SLOT LIMIT (default 4, up to 5 with perk).
 *     It is NOT the current count. It is NOT modified by equip/unequip.
 *   - Current equipped count = number of non-zero entries in equipped[0..4].
 *   - The usable slot range is min(equipped_count, 5) to prevent OOB access.
 *
 * Toggle logic:
 *   UNEQUIP: If inv.items[inv_slot] is already in equipped[]:
 *     - Find the FIRST matching entry, set it to 0. Return GAME_OK.
 *   EQUIP:   If not equipped:
 *     - If current count >= usable limit: return GAME_ERR_OVERFLOW.
 *     - Find first zero slot, assign item ID. Return GAME_OK.
 *
 * Rejections:
 *   - inv_slot >= inv->count: GAME_ERR_INVALID.
 *   - item ID 0: GAME_ERR_INVALID (sentinel, cannot be equipped).
 *   - Full slots: GAME_ERR_OVERFLOW.
 *   - NULL ch or inv: GAME_ERR_NULL_PTR.
 *
 * Architecture constraint: MUST NOT include hal_*.h, presentation/,
 * or connectivity/ headers.
 *
 * Constitution Priority 0: Pure function. No malloc. No PRNG.
 *
 * Phase-19 addition.
 */

#ifndef FIESTAQUEST_GAME_EQUIP_H
#define FIESTAQUEST_GAME_EQUIP_H

#include <stdint.h>
#include "types.h"

/**
 * fq_equip_toggle() — Toggle equip/unequip for one inventory slot.
 *
 * @param ch       Character whose equipped[] array is modified.
 * @param inv      Inventory providing item IDs. Read-only.
 * @param inv_slot Inventory slot index [0, inv->count - 1].
 * @return         GAME_OK on success.
 *                 GAME_ERR_NULL_PTR if ch or inv is NULL.
 *                 GAME_ERR_INVALID if inv_slot >= inv->count or item ID == 0.
 *                 GAME_ERR_OVERFLOW if all slots full (equip attempt only).
 */
game_err_t fq_equip_toggle(fq_character_t       *ch,
                            const fq_inventory_t *inv,
                            uint8_t               inv_slot);

#endif /* FIESTAQUEST_GAME_EQUIP_H */
