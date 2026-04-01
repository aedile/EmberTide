/**
 * character.h — FiestaQuest Game Engine: Character Creation
 *
 * Provides fq_character_create(), the canonical factory for initialising a
 * new fq_character_t from scratch.  The factory:
 *   1. Zeroes the struct (no garbage from the call-site stack frame).
 *   2. Sets class_id, id, and name (strncpy with guaranteed null-terminator).
 *   3. Assigns base stats from the class base table (identical to legacy.c).
 *   4. Calls fq_legacy_apply_bonuses() to wire any pre-set legacy_tree bits.
 *   5. Calculates initial hp_max = base_hp + (strength * 2) + legacy bonuses.
 *
 * Architecture constraint: this header MUST NOT include hal_*.h,
 * presentation/, or connectivity/ headers.
 *
 * Constitution Priority 0: No floating point, no external entropy.
 */

#ifndef FIESTAQUEST_GAME_CHARACTER_H
#define FIESTAQUEST_GAME_CHARACTER_H

#include <stdint.h>
#include "types.h"

/**
 * fq_character_create() — Initialise a character from a class template.
 *
 * Zeroes *ch completely, then sets:
 *   - ch->id          = id
 *   - ch->class_id    = (uint8_t)class_id
 *   - ch->name        = strncpy(name, 11 chars max + null terminator)
 *   - ch->strength    = class base strength
 *   - ch->speed       = class base speed
 *   - ch->precision   = class base precision
 *   - ch->intelligence = class base intelligence
 *   - ch->equipped_count = 4  (default slot count)
 *   - ch->hp_max      = base_hp + (strength * 2)
 * Then calls fq_legacy_apply_bonuses(ch) to apply any pre-set legacy bits
 * (normally 0 at creation, so this is a no-op for fresh characters).
 *
 * @param ch        Pointer to the character to initialise. Must not be NULL.
 * @param class_id  Character class. Must be < FQ_CLASS_COUNT.
 * @param id        Unique character ID (caller-assigned, e.g. from esp_random).
 * @param name      Display name string. NULL is treated as "". Truncated to
 *                  11 characters to fit the name[12] field.
 *
 * @return  GAME_OK           — success.
 *          GAME_ERR_NULL_PTR — ch is NULL.
 *          GAME_ERR_INVALID  — class_id >= FQ_CLASS_COUNT.
 */
game_err_t fq_character_create(fq_character_t *ch,
                                fq_class_t      class_id,
                                uint32_t        id,
                                const char     *name);

#endif /* FIESTAQUEST_GAME_CHARACTER_H */
