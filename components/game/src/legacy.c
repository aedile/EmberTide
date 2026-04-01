/**
 * legacy.c — FiestaQuest Rebirth & Legacy Tree System implementation.
 *
 * See legacy.h for contract, tier layout, bonus table, and rebirth
 * stat retention formula.
 *
 * Constitution Priority 0: No floating point. No <time.h>. No external entropy.
 *
 * Retention formula (integer arithmetic, floor division):
 *   new_stat = class_base + ((stat - class_base) * rate_pct) / 100
 *   where rate_pct = 50 (none), 60 (SOFT_LANDING), 75 (PHOENIX_FLAME).
 *   Take max() of all applicable rates. Floor at class_base.
 */

#include "legacy.h"

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------------*/

/** Saturating uint8_t addition. */
static uint8_t sat8_add(uint8_t a, uint8_t b)
{
    uint32_t s = (uint32_t)a + (uint32_t)b;
    return (uint8_t)(s > 255u ? 255u : s);
}

/** Saturating uint16_t addition. */
static uint16_t sat16_add(uint16_t a, uint16_t b)
{
    uint32_t s = (uint32_t)a + (uint32_t)b;
    return (uint16_t)(s > 65535u ? 65535u : s);
}

/**
 * k_class_base — Class base stats.
 * Layout: [FQ_CLASS_COUNT][5]  where indices are {STR, SPD, PRC, INT, HP}.
 *
 * | Class     | STR | SPD | PRC | INT | HP |
 * |-----------|-----|-----|-----|-----|-----|
 * | Bruiser   |  3  |  0  |  0  |  0  | 60 |
 * | Trickster |  0  |  3  |  1  |  0  | 40 |
 * | Hex       |  0  |  0  |  1  |  3  | 45 |
 * | Warden    |  1  |  1  |  0  |  1  | 55 |
 * | Wildcard  |  1  |  1  |  1  |  0  | 50 |
 */
static const uint8_t k_class_base[FQ_CLASS_COUNT][5] = {
    /* FQ_CLASS_BRUISER   */ {  3u,  0u,  0u,  0u, 60u },
    /* FQ_CLASS_TRICKSTER */ {  0u,  3u,  1u,  0u, 40u },
    /* FQ_CLASS_HEX       */ {  0u,  0u,  1u,  3u, 45u },
    /* FQ_CLASS_WARDEN    */ {  1u,  1u,  0u,  1u, 55u },
    /* FQ_CLASS_WILDCARD  */ {  1u,  1u,  1u,  0u, 50u }
};

/**
 * retain_stat() — Apply retention rate to a single stat.
 *
 * new_stat = base + floor((current - base) * rate_pct / 100).
 * If current <= base, returns base (never below class base).
 *
 * @param current   Current stat value.
 * @param base      Class base value for this stat.
 * @param rate_pct  Retention percentage (50, 60, or 75).
 * @return          Retained stat value >= base.
 */
static uint8_t retain_stat(uint8_t current, uint8_t base, uint8_t rate_pct)
{
    if (current <= base) {
        return base;
    }
    uint32_t gained   = (uint32_t)(current - base);
    uint32_t retained = (gained * (uint32_t)rate_pct) / 100u;
    uint32_t result   = (uint32_t)base + retained;
    return (uint8_t)(result > 255u ? 255u : result);
}

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

uint8_t fq_legacy_tier(uint8_t node_index)
{
    if (node_index >= 16u) {
        return 0u; /* Defensive: reserved nodes map to tier 0. */
    }
    return (uint8_t)(node_index / 4u);
}

uint8_t fq_legacy_count_tier(uint32_t tree, uint8_t tier)
{
    if (tier > 3u) {
        return 0u;
    }
    uint8_t  base_bit = (uint8_t)(tier * 4u);
    uint8_t  count    = 0u;
    uint8_t  i;
    for (i = 0u; i < 4u; i++) {
        if ((tree & (1u << (base_bit + i))) != 0u) {
            count++;
        }
    }
    return count;
}

game_err_t fq_legacy_unlock_node(fq_character_t *ch, uint8_t node_index)
{
    if (ch == NULL) {
        return GAME_ERR_NULL_PTR;
    }
    if (node_index >= 16u) {
        return GAME_ERR_INVALID;
    }

    /* Must have at least one legacy point to spend. */
    if (ch->legacy_points == 0u) {
        return GAME_ERR_INVALID;
    }

    uint32_t node_bit = (1u << node_index);

    /* Node already unlocked → reject. */
    if ((ch->legacy_tree & node_bit) != 0u) {
        return GAME_ERR_INVALID;
    }

    /* Check tier prerequisites. */
    uint8_t tier = fq_legacy_tier(node_index);
    if (tier == 1u) {
        /* T2: require >= 1 T1 node. */
        if (fq_legacy_count_tier(ch->legacy_tree, 0u) < 1u) {
            return GAME_ERR_INVALID;
        }
    } else if (tier == 2u) {
        /* T3: require >= 2 T2 nodes. */
        if (fq_legacy_count_tier(ch->legacy_tree, 1u) < 2u) {
            return GAME_ERR_INVALID;
        }
    } else if (tier == 3u) {
        /* T4: require >= 2 T3 nodes. */
        if (fq_legacy_count_tier(ch->legacy_tree, 2u) < 2u) {
            return GAME_ERR_INVALID;
        }
    }
    /* tier == 0 (T1): no prerequisites. */

    /* Unlock the node and spend one point. */
    ch->legacy_tree |= node_bit;
    ch->legacy_points--;

    return GAME_OK;
}

uint8_t fq_calc_rebirth_tokens(uint8_t level, uint16_t wins)
{
    uint32_t tokens = (uint32_t)(level / 10u) + (uint32_t)(wins / 100u);
    return (uint8_t)(tokens > 255u ? 255u : tokens);
}

game_err_t fq_rebirth(fq_character_t *ch, fq_prng_t *rng)
{
    if (ch == NULL || rng == NULL) {
        return GAME_ERR_NULL_PTR;
    }
    if (ch->is_dead == 0u) {
        return GAME_ERR_INVALID;
    }

    /* Determine class base values. */
    uint8_t cls = ch->class_id;
    if (cls >= (uint8_t)FQ_CLASS_COUNT) {
        cls = (uint8_t)FQ_CLASS_BRUISER; /* Defensive fallback. */
    }

    uint8_t base_str = k_class_base[cls][0];
    uint8_t base_spd = k_class_base[cls][1];
    uint8_t base_prc = k_class_base[cls][2];
    uint8_t base_int = k_class_base[cls][3];
    uint8_t base_hp  = k_class_base[cls][4];

    /* Determine retention rate (take max of applicable perks). */
    uint8_t rate = 50u; /* Default: 50% retention. */
    if ((ch->legacy_tree & FQ_LEGACY_SOFT_LANDING) != 0u) {
        if (60u > rate) {
            rate = 60u;
        }
    }
    if ((ch->legacy_tree & FQ_LEGACY_PHOENIX_FLAME) != 0u) {
        if (75u > rate) {
            rate = 75u;
        }
    }

    /* Apply retention to each stat. */
    ch->strength     = retain_stat(ch->strength,     base_str, rate);
    ch->speed        = retain_stat(ch->speed,         base_spd, rate);
    ch->precision    = retain_stat(ch->precision,     base_prc, rate);
    ch->intelligence = retain_stat(ch->intelligence,  base_int, rate);

    /* Restore base HP. */
    ch->hp_max = (uint16_t)base_hp;

    /* Increment rebirth_count (saturate at 255). */
    if (ch->rebirth_count < 255u) {
        ch->rebirth_count++;
    }

    /* Calculate and add rebirth tokens (saturate legacy_points at 255). */
    uint8_t tokens = fq_calc_rebirth_tokens(ch->level, ch->wins);
    ch->legacy_points = sat8_add(ch->legacy_points, tokens);

    /* Clear dead flag. */
    ch->is_dead = 0u;

    /* Wildcard passive reroll using the PRNG. */
    if (ch->class_id == (uint8_t)FQ_CLASS_WILDCARD) {
        ch->wildcard_passive = (uint8_t)(fq_prng_range(rng, 0u, 3u));
    }

    return GAME_OK;
}

void fq_legacy_apply_bonuses(fq_character_t *ch)
{
    if (ch == NULL) {
        return;
    }

    uint32_t tree = ch->legacy_tree;

    /* Process only defined bits (0-15); silently ignore reserved bits 16-31. */

    /* Tier 1 bonuses. */
    if ((tree & FQ_LEGACY_THICK_SKIN_1) != 0u) {
        ch->hp_max = sat16_add(ch->hp_max, 5u);
    }
    if ((tree & FQ_LEGACY_THICK_SKIN_2) != 0u) {
        ch->hp_max = sat16_add(ch->hp_max, 5u);
    }
    if ((tree & FQ_LEGACY_KEEN_EYE) != 0u) {
        ch->precision = sat8_add(ch->precision, 2u);
    }
    if ((tree & FQ_LEGACY_QUICK_FEET) != 0u) {
        ch->speed = sat8_add(ch->speed, 2u);
    }

    /* Tier 2 bonuses. */
    if ((tree & FQ_LEGACY_IRON_WILL) != 0u) {
        ch->intelligence = sat8_add(ch->intelligence, 3u);
    }
    if ((tree & FQ_LEGACY_SCAVENGER) != 0u) {
        /* Grant 5th equipment slot if not already at 5. */
        if (ch->equipped_count < 5u) {
            ch->equipped_count = 5u;
        }
    }
    /* FQ_LEGACY_SOFT_LANDING: retention perk — no stat bonus. */
    /* FQ_LEGACY_DEEP_POCKETS: deferred — no stat bonus. */

    /* Tier 3 bonuses. */
    /* FQ_LEGACY_PHOENIX_FLAME: retention perk — no stat bonus. */
    if ((tree & FQ_LEGACY_VETERAN_MARK) != 0u) {
        ch->hp_max = sat16_add(ch->hp_max, 10u);
    }
    if ((tree & FQ_LEGACY_BATTLE_SCARS) != 0u) {
        ch->strength = sat8_add(ch->strength, 3u);
    }
    if ((tree & FQ_LEGACY_SIXTH_SENSE) != 0u) {
        ch->speed = sat8_add(ch->speed, 3u);
    }

    /* Tier 4 bonuses. */
    if ((tree & FQ_LEGACY_MASTER_MIND) != 0u) {
        ch->intelligence = sat8_add(ch->intelligence, 5u);
    }
    if ((tree & FQ_LEGACY_DIAMOND_SKIN) != 0u) {
        ch->hp_max = sat16_add(ch->hp_max, 15u);
    }
    if ((tree & FQ_LEGACY_GODSPEED) != 0u) {
        ch->speed = sat8_add(ch->speed, 5u);
    }
    if ((tree & FQ_LEGACY_BERSERKER) != 0u) {
        ch->strength = sat8_add(ch->strength, 5u);
    }
}
