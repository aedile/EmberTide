/**
 * character.c — FiestaQuest Game Engine: Character Creation
 *
 * Implements fq_character_create(). See character.h for the full contract.
 *
 * Class base stat table is duplicated (not shared) from legacy.c to keep the
 * two modules independently compilable. Both tables are identical; any future
 * divergence would be a bug and will be caught by the host tests.
 *
 * Constitution Priority 0: No floating point, no external entropy.
 * No malloc — ch is caller-allocated (stack or static).
 */

#include <string.h>
#include "character.h"
#include "legacy.h"

/* ---------------------------------------------------------------------------
 * Class base stat table.
 * Indices: [class][0]=STR, [1]=SPD, [2]=PRC, [3]=INT, [4]=HP_BASE
 *
 * | Class     | STR | SPD | PRC | INT | HP |
 * |-----------|-----|-----|-----|-----|-----|
 * | Bruiser   |  3  |  0  |  0  |  0  | 60 |
 * | Trickster |  0  |  3  |  1  |  0  | 40 |
 * | Hex       |  0  |  0  |  1  |  3  | 45 |
 * | Warden    |  1  |  1  |  0  |  1  | 55 |
 * | Wildcard  |  1  |  1  |  1  |  0  | 50 |
 * ---------------------------------------------------------------------------*/
static const uint8_t k_class_base[FQ_CLASS_COUNT][5] = {
    /* FQ_CLASS_BRUISER   */ {  3u,  0u,  0u,  0u, 60u },
    /* FQ_CLASS_TRICKSTER */ {  0u,  3u,  1u,  0u, 40u },
    /* FQ_CLASS_HEX       */ {  0u,  0u,  1u,  3u, 45u },
    /* FQ_CLASS_WARDEN    */ {  1u,  1u,  0u,  1u, 55u },
    /* FQ_CLASS_WILDCARD  */ {  1u,  1u,  1u,  0u, 50u }
};

/* ---------------------------------------------------------------------------
 * fq_character_create — see character.h for full contract.
 * ---------------------------------------------------------------------------*/
game_err_t fq_character_create(fq_character_t *ch,
                                fq_class_t      class_id,
                                uint32_t        id,
                                const char     *name)
{
    if (ch == NULL) {
        return GAME_ERR_NULL_PTR;
    }
    if ((uint8_t)class_id >= (uint8_t)FQ_CLASS_COUNT) {
        return GAME_ERR_INVALID;
    }

    /* 1. Zero the entire struct to eliminate uninitialised-memory hazards. */
    memset(ch, 0, sizeof(fq_character_t));

    /* 2. Set identity fields. */
    ch->id       = id;
    ch->class_id = (uint8_t)class_id;

    /* 3. Copy name with guaranteed null-termination.
     *    name[12] — keep 11 chars + '\0'. */
    if (name != NULL) {
        strncpy(ch->name, name, sizeof(ch->name) - 1u);
        /* strncpy pads with '\0' if src is shorter; the last byte is always
         * zero because we passed sizeof-1 and the struct was zeroed above. */
    }
    /* If name == NULL: name field remains all-zero from memset. */

    /* 4. Set base stats from class table. */
    ch->strength     = k_class_base[(uint8_t)class_id][0];
    ch->speed        = k_class_base[(uint8_t)class_id][1];
    ch->precision    = k_class_base[(uint8_t)class_id][2];
    ch->intelligence = k_class_base[(uint8_t)class_id][3];

    /* 5. Default equipment slot count = 4. */
    ch->equipped_count = 4u;

    /* 6. Initial hp_max = base_hp + (strength * 2).
     *    Promote to uint32_t for the multiply to prevent any 8-bit overflow,
     *    then clamp to uint16_t. Max possible: HP=60 + STR=255*2 = 570 < 65535.
     *    In practice base STR <= 3 so hp_max is always tiny, but we keep the
     *    guard for correctness if legacy bonuses push strength up later. */
    uint32_t hp_max32 = (uint32_t)k_class_base[(uint8_t)class_id][4]
                      + (uint32_t)ch->strength * 2u;
    ch->hp_max = (uint16_t)(hp_max32 > 65535u ? 65535u : hp_max32);

    /* 7. Apply legacy bonuses for any pre-set legacy_tree bits.
     *    For a freshly created character legacy_tree == 0 so this is a no-op.
     *    Callers who pre-populate legacy_tree before calling this function
     *    (e.g. loading a rebirth save) will have their bonuses applied here. */
    fq_legacy_apply_bonuses(ch);

    return GAME_OK;
}
