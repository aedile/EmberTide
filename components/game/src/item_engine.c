/**
 * item_engine.c — FiestaQuest Phase-5 Item Engine implementation.
 *
 * See item_engine.h for full API contract and architecture notes.
 *
 * Item table (8 representative items for Phase 5):
 *   001 Iron Fist     Common    PASSIVE        +1 DAMAGE_ADD  SELF
 *   003 Tough Hide    Common    ON_DEFEND      -1 DAMAGE_ADD  SELF   (NTR-B3: min floor 1)
 *   004 Lucky Coin    Common    ON_ROUND_START  probabilistic +2 DAMAGE_ADD SELF (d100≤10)
 *   104 Vampire Fang  Uncommon  ON_KILL         +5 HEAL       SELF
 *   105 Haymaker      Uncommon  ON_CRIT         DAMAGE_MULT→200 SELF
 *   109 Bandage       Uncommon  ON_ROUND_END    +1 HEAL SELF  (if HP < 50%)
 *   204 Chaos Orb     Rare      ON_ROUND_START  swap random stat (PRNG always consumed)
 *   207 Time Loop     Legendary ON_ROUND_END    snapshot HP@r3, restore@r6 (once per fight)
 *
 * PRNG discipline:
 *   Lucky Coin (004) always consumes fq_prng_range(1,100). (NTR-A1)
 *   Chaos Orb  (204) always consumes fq_prng_range(0,3).   (NTR-E2)
 *
 * Trigger ordering:
 *   fq_item_eval_trigger iterates defender fighter items first (slots 0..N-1),
 *   then attacker fighter items (slots 0..N-1). (NTR-C1, NTR-C2)
 *
 * Effect application:
 *   DAMAGE_ADD: accumulated into fighter->damage_bonus (int8_t signed),
 *               saturated to [-128, 127] (A1 fix).
 *   DAMAGE_MULT: Haymaker stores delta=100; full_mult = 100 + delta = 200.
 *                Sets fighter->damage_mult_pct to max(current, full_mult) (B2 fix).
 *   HEAL: adds to HP, clamped to hp_max.
 *   DODGE_BONUS: accumulated into fighter->dodge_bonus (uint8_t),
 *                saturated to [0, 255] (A2 fix).
 *
 * Review fixes applied:
 *   B2: Haymaker effect.value stores delta (100), not raw percentage (200).
 *       apply_effect reconstructs: full_mult = 100u + (uint8_t)effect->value.
 *       Avoids (int8_t)200u overflow that wraps to -56.
 *   A1: DAMAGE_ADD accumulation uses int16_t intermediate with saturation clamp.
 *   A2: DODGE_BONUS accumulation uses uint16_t intermediate with saturation clamp.
 *   A3: Item table lookup loop uses uint16_t counter (not uint8_t).
 *   A4: FQ_MAX_ITEM_TRIGGERS renamed to FQ_MAX_ITEM_RECURSION_DEPTH in item_engine.h.
 *
 * Constitution Priority 0: No floats, no time.h, no external entropy.
 */

#include "item_engine.h"
#include "combat.h"
#include "prng.h"
#include "progression.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Item definition table — static const, lives in flash.
 *
 * Entries must be sorted by ascending ID for binary search readability,
 * but fq_item_lookup uses a linear scan for simplicity (table is small).
 *
 * B2 fix: Haymaker effect.value = 100 (delta from 100, not raw percentage).
 *   full_mult = 100 + 100 = 200%. Previously (int8_t)200u wrapped to -56.
 * ---------------------------------------------------------------------------*/
static const fq_item_def_t s_item_table[] = {
    /* 001: Iron Fist — Common, PASSIVE, +1 DAMAGE_ADD SELF */
    {
        .id          = 1u,
        .rarity      = (uint8_t)FQ_RARITY_COMMON,
        .trigger     = (uint8_t)FQ_TRIGGER_PASSIVE,
        .condition   = { (uint8_t)FQ_COND_NONE, 0u },
        .effect      = { (uint8_t)FQ_EFFECT_DAMAGE_ADD, 1, (uint8_t)FQ_TARGET_SELF },
        ._pad        = 0u,
        .name        = "Iron Fist",
        .flavor_text = "Your knuckles ache. Good."
    },
    /* 003: Tough Hide — Common, ON_DEFEND, -1 DAMAGE_ADD SELF */
    {
        .id          = 3u,
        .rarity      = (uint8_t)FQ_RARITY_COMMON,
        .trigger     = (uint8_t)FQ_TRIGGER_ON_DEFEND,
        .condition   = { (uint8_t)FQ_COND_NONE, 0u },
        .effect      = { (uint8_t)FQ_EFFECT_DAMAGE_ADD, -1, (uint8_t)FQ_TARGET_SELF },
        ._pad        = 0u,
        .name        = "Tough Hide",
        .flavor_text = "Scales harden under fire."
    },
    /* 004: Lucky Coin — Common, ON_ROUND_START, d100≤10 → +2 DAMAGE_ADD SELF */
    {
        .id          = 4u,
        .rarity      = (uint8_t)FQ_RARITY_COMMON,
        .trigger     = (uint8_t)FQ_TRIGGER_ON_ROUND_START,
        .condition   = { (uint8_t)FQ_COND_NONE, 0u },
        .effect      = { (uint8_t)FQ_EFFECT_DAMAGE_ADD, 2, (uint8_t)FQ_TARGET_SELF },
        ._pad        = 0u,
        .name        = "Lucky Coin",
        .flavor_text = "Heads I win, tails you lose."
    },
    /* 104: Vampire Fang — Uncommon, ON_KILL, +5 HEAL SELF */
    {
        .id          = 104u,
        .rarity      = (uint8_t)FQ_RARITY_UNCOMMON,
        .trigger     = (uint8_t)FQ_TRIGGER_ON_KILL,
        .condition   = { (uint8_t)FQ_COND_NONE, 0u },
        .effect      = { (uint8_t)FQ_EFFECT_HEAL, 5, (uint8_t)FQ_TARGET_SELF },
        ._pad        = 0u,
        .name        = "Vampire Fang",
        .flavor_text = "Drink deep."
    },
    /* 105: Haymaker — Uncommon, ON_CRIT, DAMAGE_MULT→200 SELF
     *
     * B2 fix: value=100 is the DELTA from 100, not the raw percentage.
     * apply_effect reconstructs: full_mult = 100 + 100 = 200.
     * Previously (int8_t)200u wrapped to -56 due to int8_t overflow.
     */
    {
        .id          = 105u,
        .rarity      = (uint8_t)FQ_RARITY_UNCOMMON,
        .trigger     = (uint8_t)FQ_TRIGGER_ON_CRIT,
        .condition   = { (uint8_t)FQ_COND_NONE, 0u },
        .effect      = { (uint8_t)FQ_EFFECT_DAMAGE_MULT, (int8_t)100, (uint8_t)FQ_TARGET_SELF },
        ._pad        = 0u,
        .name        = "Haymaker",
        .flavor_text = "No gloves. No mercy."
    },
    /* 109: Bandage — Uncommon, ON_ROUND_END, HP_BELOW 50% → +1 HEAL SELF */
    {
        .id          = 109u,
        .rarity      = (uint8_t)FQ_RARITY_UNCOMMON,
        .trigger     = (uint8_t)FQ_TRIGGER_ON_ROUND_END,
        .condition   = { (uint8_t)FQ_COND_HP_BELOW, 50u },
        .effect      = { (uint8_t)FQ_EFFECT_HEAL, 1, (uint8_t)FQ_TARGET_SELF },
        ._pad        = 0u,
        .name        = "Bandage",
        .flavor_text = "Better than nothing."
    },
    /* 204: Chaos Orb — Rare, ON_ROUND_START, swap random effective stat */
    {
        .id          = 204u,
        .rarity      = (uint8_t)FQ_RARITY_RARE,
        .trigger     = (uint8_t)FQ_TRIGGER_ON_ROUND_START,
        .condition   = { (uint8_t)FQ_COND_NONE, 0u },
        .effect      = { (uint8_t)FQ_EFFECT_REROLL, 0, (uint8_t)FQ_TARGET_SELF },
        ._pad        = 0u,
        .name        = "Chaos Orb",
        .flavor_text = "Everything is negotiable."
    },
    /* 207: Time Loop — Legendary, ON_ROUND_END, snapshot r3 HP, restore at r6 */
    {
        .id          = 207u,
        .rarity      = (uint8_t)FQ_RARITY_LEGENDARY,
        .trigger     = (uint8_t)FQ_TRIGGER_ON_ROUND_END,
        .condition   = { (uint8_t)FQ_COND_ROUND_NUMBER, 3u },
        .effect      = { (uint8_t)FQ_EFFECT_HEAL, 0, (uint8_t)FQ_TARGET_BOTH },
        ._pad        = 0u,
        .name        = "Time Loop",
        .flavor_text = "We've been here before."
    }
};

/** Number of items in the static table. */
#define ITEM_TABLE_COUNT  (sizeof(s_item_table) / sizeof(s_item_table[0]))

/* A3 fix: assert that the table never exceeds uint16_t loop counter range. */
_Static_assert(ITEM_TABLE_COUNT <= 65535u, "Item table exceeds uint16_t loop counter range");

/* ---------------------------------------------------------------------------
 * fq_item_lookup
 * ---------------------------------------------------------------------------*/
const fq_item_def_t *fq_item_lookup(uint16_t item_id)
{
    /* ID 0 is FQ_ITEM_NONE — always NULL (NTR-F2). */
    if (item_id == 0u) {
        return NULL;
    }

    /* A3 fix: use uint16_t loop counter to avoid wrapping if table grows. */
    for (uint16_t i = 0u; i < (uint16_t)ITEM_TABLE_COUNT; i++) {
        if (s_item_table[i].id == item_id) {
            return &s_item_table[i];
        }
    }

    return NULL; /* Unknown ID → NULL (NTR-F2). */
}

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------------*/

/**
 * clamp_hp_i16() — Clamp a 32-bit HP value to [0, hp_max].
 * Uses int32_t intermediate to avoid overflow (NTR-B6).
 */
static int16_t clamp_hp_i16(int32_t v, int16_t hp_max)
{
    if (v < 0)                   return 0;
    if (v > (int32_t)hp_max)     return hp_max;
    return (int16_t)v;
}

/**
 * check_condition() — Evaluate whether an item's condition is satisfied.
 *
 * Uses int32_t intermediate for HP_BELOW percentage to prevent overflow
 * on large HP values (NTR-B6).
 *
 * @param cond    The condition to check.
 * @param fighter The fighter to check the condition against.
 * @param ctx     The combat context (for round number).
 * @return        1 if condition is met, 0 otherwise.
 */
static uint8_t check_condition(const fq_condition_t    *cond,
                               const fq_combat_fighter_t *fighter,
                               const fq_combat_ctx_t   *ctx)
{
    switch ((fq_condition_type_t)cond->type) {
        case FQ_COND_NONE:
            return 1u;

        case FQ_COND_HP_BELOW: {
            /* NTR-B6: Use int32_t intermediate to prevent overflow.
             * Condition fires when: hp * 100 < hp_max * threshold.
             * Rearranged to avoid division: hp * 100 < hp_max * threshold. */
            if (fighter->hp_max <= 0) { return 0u; }
            int32_t hp_pct_num   = (int32_t)fighter->hp * 100;
            int32_t threshold_x  = (int32_t)fighter->hp_max * (int32_t)cond->threshold;
            return (hp_pct_num < threshold_x) ? 1u : 0u;
        }

        case FQ_COND_HP_ABOVE: {
            if (fighter->hp_max <= 0) { return 0u; }
            int32_t hp_pct_num   = (int32_t)fighter->hp * 100;
            int32_t threshold_x  = (int32_t)fighter->hp_max * (int32_t)cond->threshold;
            return (hp_pct_num > threshold_x) ? 1u : 0u;
        }

        case FQ_COND_ROUND_NUMBER:
            return (ctx->current_round == (uint8_t)cond->threshold) ? 1u : 0u;

        case FQ_COND_STREAK:
            /* Not used by Phase 5 items — always passes for now. */
            return 1u;

        case FQ_COND_COUNT:
        default:
            return 0u;
    }
}

/**
 * apply_effect() — Apply an item's effect to the target fighter.
 *
 * @param effect   The effect to apply.
 * @param owner    The fighter who owns the item (for FQ_TARGET_SELF).
 * @param opponent The opposing fighter (for FQ_TARGET_OPPONENT).
 * @param ctx      Full combat context (for Chaos Orb stat swap).
 */
static void apply_effect(const fq_effect_t       *effect,
                         fq_combat_fighter_t      *owner,
                         fq_combat_fighter_t      *opponent,
                         fq_combat_ctx_t          *ctx)
{
    switch ((fq_effect_type_t)effect->type) {
        case FQ_EFFECT_DAMAGE_ADD: {
            /* A1 fix: Accumulate signed damage bonus using int16_t intermediate
             * with saturation clamp to [-128, 127] to prevent int8_t overflow. */
            int16_t sum = (int16_t)owner->damage_bonus + (int16_t)effect->value;
            if (sum > 127)  { sum = 127; }
            if (sum < -128) { sum = -128; }
            owner->damage_bonus = (int8_t)sum;
            break;
        }

        case FQ_EFFECT_DAMAGE_MULT: {
            /* B2 fix: effect->value stores the delta from 100 (not raw percentage).
             * Reconstruct full multiplier: full_mult = 100 + (uint8_t)effect->value.
             * Take the higher value (Haymaker: 200 > 150 default crit).
             *
             * Previously (int8_t)200u was stored, which wraps to -56. The delta
             * encoding avoids overflow: value=100 fits in int8_t, and
             * 100 + 100 = 200 fits in uint8_t for the final multiplier. */
            uint8_t full_mult = (uint8_t)(100u + (uint8_t)effect->value);
            if (full_mult > owner->damage_mult_pct) {
                owner->damage_mult_pct = full_mult;
            }
            break;
        }

        case FQ_EFFECT_HEAL: {
            /* Apply heal to target, clamped to hp_max. */
            fq_combat_fighter_t *target =
                (effect->target == (uint8_t)FQ_TARGET_OPPONENT) ? opponent : owner;
            int32_t new_hp = (int32_t)target->hp + (int32_t)effect->value;
            target->hp = clamp_hp_i16(new_hp, target->hp_max);
            break;
        }

        case FQ_EFFECT_DODGE_BONUS: {
            /* A2 fix: Accumulate dodge bonus using uint16_t intermediate
             * with saturation clamp to [0, 255] to prevent uint8_t overflow. */
            uint16_t sum = (uint16_t)owner->dodge_bonus
                         + (uint16_t)(uint8_t)effect->value;
            if (sum > 255u) { sum = 255u; }
            owner->dodge_bonus = (uint8_t)sum;
            break;
        }

        case FQ_EFFECT_REROLL:
            /* Handled by caller (Chaos Orb special logic requires PRNG). */
            break;

        case FQ_EFFECT_COUNT:
        default:
            break;
    }

    (void)opponent; /* Suppress unused warning if no opponent path taken. */
    (void)ctx;      /* Suppress unused warning — ctx reserved for future effects. */
}

/**
 * apply_item() — Evaluate and apply one item's effect for a given trigger.
 *
 * Handles special-case items (Lucky Coin, Chaos Orb, Time Loop) with their
 * unique PRNG and state-mutation logic. All other items go through
 * check_condition + apply_effect.
 *
 * @param def              Item definition.
 * @param trigger          Current trigger point.
 * @param owner            Fighter who owns this item.
 * @param opponent         The other fighter.
 * @param ctx              Full combat context.
 * @param owner_is_fighter1 1 if owner is ctx->f1, 0 if owner is ctx->f2.
 */
static void apply_item(const fq_item_def_t  *def,
                       fq_trigger_t          trigger,
                       fq_combat_fighter_t  *owner,
                       fq_combat_fighter_t  *opponent,
                       fq_combat_ctx_t      *ctx,
                       uint8_t               owner_is_fighter1)
{
    /* Trigger must match. */
    if ((fq_trigger_t)def->trigger != trigger) {
        return;
    }

    /* --- Special case: Lucky Coin (004) ---
     * PRNG ALWAYS consumed even if 10% check fails (NTR-A1). */
    if (def->id == 4u) {
        uint32_t roll = fq_prng_range(&ctx->rng, 1u, 100u);
        if (roll <= 10u) {
            /* A1 fix: saturated accumulation via int16_t. */
            int16_t sum = (int16_t)owner->damage_bonus + 2;
            if (sum > 127)  { sum = 127; }
            if (sum < -128) { sum = -128; }
            owner->damage_bonus = (int8_t)sum;
        }
        return;
    }

    /* --- Special case: Chaos Orb (204) ---
     * PRNG ALWAYS consumed for stat selection [0,3] (NTR-E2). */
    if (def->id == 204u) {
        uint32_t stat_idx = fq_prng_range(&ctx->rng, 0u, 3u);
        /* Swap a random effective stat between the two fighters for this round.
         * stat_idx: 0=strength, 1=speed, 2=precision, 3=intelligence. */
        uint8_t tmp;
        switch (stat_idx) {
            case 0u:
                tmp = owner->strength;
                owner->strength = opponent->strength;
                opponent->strength = tmp;
                break;
            case 1u:
                tmp = owner->speed;
                owner->speed = opponent->speed;
                opponent->speed = tmp;
                break;
            case 2u:
                tmp = owner->precision;
                owner->precision = opponent->precision;
                opponent->precision = tmp;
                break;
            case 3u:
            default:
                tmp = owner->intelligence;
                owner->intelligence = opponent->intelligence;
                opponent->intelligence = tmp;
                break;
        }
        return;
    }

    /* --- Special case: Time Loop (207) ---
     * ON_ROUND_END: snapshot HP at round 3, restore at round 6 (once per fight). */
    if (def->id == 207u) {
        if (trigger != FQ_TRIGGER_ON_ROUND_END) { return; }
        if (ctx->current_round == 3u) {
            /* Snapshot current HP of both fighters after all r3 effects (NTR-D1). */
            ctx->round_3_f1_hp = ctx->f1.hp;
            ctx->round_3_f2_hp = ctx->f2.hp;
        } else if (ctx->current_round == 6u) {
            /* Restore to round 3 snapshot, once per fight (NTR-D3). */
            if (ctx->time_loop_used == 0u) {
                ctx->f1.hp = clamp_hp_i16((int32_t)ctx->round_3_f1_hp, ctx->f1.hp_max);
                ctx->f2.hp = clamp_hp_i16((int32_t)ctx->round_3_f2_hp, ctx->f2.hp_max);
                ctx->time_loop_used = 1u;
            }
        }
        return;
    }

    /* --- General path: check condition, then apply effect. --- */
    if (!check_condition(&def->condition, owner, ctx)) {
        return;
    }

    apply_effect(&def->effect, owner, opponent, ctx);

    (void)owner_is_fighter1; /* Reserved for future use. */
}

/* ---------------------------------------------------------------------------
 * eval_fighter_items() — Evaluate all items for one fighter at a trigger point.
 *
 * Helper used by fq_item_eval_trigger to iterate a single fighter's slots.
 * ---------------------------------------------------------------------------*/
static void eval_fighter_items(fq_combat_ctx_t     *ctx,
                               fq_trigger_t         trigger,
                               fq_combat_fighter_t *owner,
                               fq_combat_fighter_t *opponent,
                               uint8_t              owner_is_f1)
{
    for (uint8_t slot = 0u; slot < owner->equipped_count; slot++) {
        uint16_t item_id = owner->equipped_items[slot];
        if (item_id == (uint16_t)FQ_ITEM_NONE) { continue; } /* BOUND-01 */
        const fq_item_def_t *def = fq_item_lookup(item_id);
        if (def == NULL) { continue; } /* NTR-F2: invalid ID is no-op */
        apply_item(def, trigger, owner, opponent, ctx, owner_is_f1);
    }
}

/* ---------------------------------------------------------------------------
 * fq_item_eval_trigger
 *
 * Trigger routing (PM-approved, NTR-C1):
 *
 * Role-specific triggers fire for only ONE fighter's items:
 *   ON_DEFEND, ON_DODGE  → defender only   (defender reacts to being attacked)
 *   ON_ATTACK, ON_CRIT   → attacker only   (attacker lands a hit)
 *   ON_KILL, ON_DEATH    → attacker only / dying fighter only
 *                          (passed via attacking_fighter from combat.c)
 *
 * Round-boundary triggers fire for BOTH fighters, defender first (NTR-C1):
 *   PASSIVE, ON_ROUND_START, ON_ROUND_END → defender first, then attacker
 * ---------------------------------------------------------------------------*/
void fq_item_eval_trigger(fq_combat_ctx_t *ctx,
                          fq_trigger_t     trigger,
                          uint8_t          attacking_fighter)
{
    /* A4 fix: constant renamed from FQ_MAX_ITEM_TRIGGERS to
     * FQ_MAX_ITEM_RECURSION_DEPTH for semantic clarity. */
    if (ctx->item_recursion_depth >= (uint8_t)FQ_MAX_ITEM_RECURSION_DEPTH) {
        return;
    }
    ctx->item_recursion_depth++;

    /* Determine attacker and defender fighter pointers. */
    fq_combat_fighter_t *attacker = (attacking_fighter == 1u) ? &ctx->f1 : &ctx->f2;
    fq_combat_fighter_t *defender = (attacking_fighter == 1u) ? &ctx->f2 : &ctx->f1;
    uint8_t attacker_is_f1 = (attacking_fighter == 1u) ? 1u : 0u;
    uint8_t defender_is_f1 = (attacking_fighter == 1u) ? 0u : 1u;

    switch (trigger) {
        /* -----------------------------------------------------------------
         * Defender-only triggers:
         *   ON_DEFEND fires when the fighter is being attacked (Tough Hide).
         *   ON_DODGE  fires when the fighter successfully dodges an attack.
         * ----------------------------------------------------------------- */
        case FQ_TRIGGER_ON_DEFEND:
        case FQ_TRIGGER_ON_DODGE:
            eval_fighter_items(ctx, trigger, defender, attacker, defender_is_f1);
            break;

        /* -----------------------------------------------------------------
         * Attacker-only triggers:
         *   ON_ATTACK fires when the attacker lands a hit.
         *   ON_CRIT   fires when the attacker scores a crit.
         *   ON_KILL   fires when the attacker KOs the defender.
         *   ON_DEATH  fires for the dying fighter (passed as attacking_fighter
         *             in combat.c — the fighter who was killed).
         * ----------------------------------------------------------------- */
        case FQ_TRIGGER_ON_ATTACK:
        case FQ_TRIGGER_ON_CRIT:
        case FQ_TRIGGER_ON_KILL:
        case FQ_TRIGGER_ON_DEATH:
            eval_fighter_items(ctx, trigger, attacker, defender, attacker_is_f1);
            break;

        /* -----------------------------------------------------------------
         * Round-boundary triggers: both fighters fire, defender first (NTR-C1).
         *   PASSIVE        — one-shot stat bonuses applied at init.
         *   ON_ROUND_START — Lucky Coin, Chaos Orb (PRNG always consumed).
         *   ON_ROUND_END   — Bandage, Time Loop.
         * ----------------------------------------------------------------- */
        case FQ_TRIGGER_PASSIVE:
        case FQ_TRIGGER_ON_ROUND_START:
        case FQ_TRIGGER_ON_ROUND_END:
        case FQ_TRIGGER_ON_LOW_HP:
        default:
            /* Defender first (NTR-C1). */
            eval_fighter_items(ctx, trigger, defender, attacker, defender_is_f1);
            /* Attacker second. */
            eval_fighter_items(ctx, trigger, attacker, defender, attacker_is_f1);
            break;
    }

    ctx->item_recursion_depth--;
}
