/**
 * combat.c — FiestaQuest Phase-4/5 Combat Engine implementation.
 *
 * See combat.h for full API contract, PRNG call ordering, and
 * Constitution compliance notes.
 *
 * Design decisions (PM-approved, B1–B5 rework applied):
 *   - B1: Initiative = d6 + eff_speed/3 (was d100 + full eff_speed).
 *   - B2: Precision tier table maps raw d6 → adjusted roll before damage calc.
 *   - B3: Dodge clamp upper bound is 40 (was 75) per design doc Section 2.4.
 *   - B4: Crit determined from raw_roll >= crit_threshold; no separate PRNG call.
 *         crit_threshold = 6 - (precision_tier / 2).
 *   - B5: Two reroll conditions per fighter per round:
 *         (1) Self-reroll if own raw_roll <= 2 and charges > 0 (keep higher).
 *         (2) Defensive reroll if opponent's attack roll >= 5 and own charges > 0
 *             (keep LOWER for opponent). Evaluated after attacker 2 rolls.
 *   - HP clamped to INT16_MAX in init (defense-in-depth for uint16_t hp_max).
 *   - Simultaneous KO is impossible: first attacker's KO ends the fight immediately.
 *   - Round limit (FQ_MAX_ROUNDS=12): winner by HP percentage, ties → F2.
 *   - Lucky Star: always consumes 2 PRNG calls (d20); bonus damage deferred to Phase 5.
 *   - Conditional PRNG calls only advance PRNG when the branch is taken.
 *
 * Phase 5 additions:
 *   - fq_item_eval_trigger() integrated at lifecycle trigger points.
 *   - Per-round item state reset at ON_ROUND_START (damage_bonus, damage_mult_pct,
 *     dodge_bonus). PASSIVE items evaluated once during fq_combat_init.
 *   - Damage formula: final = (base + damage_bonus) * damage_mult_pct / 100.
 *     Minimum damage floor of 1 enforced after multiplier (NTR-B3).
 *   - Time Loop snapshot fields initialized to 0 in fq_combat_init.
 *
 * Constitution Priority 0: No floats, no time.h, no external entropy.
 * All random decisions use fq_prng_t exclusively.
 */

#include "combat.h"
#include "item_engine.h"
#include "progression.h"
#include <string.h>
#include <limits.h>

/* ---------------------------------------------------------------------------
 * Precision tier lookup table (B2).
 *
 * precision_tier = min(eff_precision / 5, 3)
 *
 * After rolling raw d6 (1-6), look up adjusted_roll = PRECISION_TABLE[tier][raw-1].
 * Tier 0: no adjustment. Tier 3: tight distribution (3-5 range).
 *
 * Crit uses the ORIGINAL raw_roll, not adjusted_roll.
 * ---------------------------------------------------------------------------*/
static const uint8_t PRECISION_TABLE[4][6] = {
    {1u, 2u, 3u, 4u, 5u, 6u},  /* Tier 0: no change */
    {2u, 2u, 3u, 4u, 5u, 6u},  /* Tier 1: floor raised */
    {2u, 3u, 3u, 4u, 5u, 5u},  /* Tier 2: compressed toward middle */
    {3u, 3u, 4u, 4u, 5u, 5u},  /* Tier 3: tight distribution */
};

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------------*/

/** Clamp a signed 32-bit value to [0, INT16_MAX] for HP bookkeeping. */
static int16_t clamp_hp(int32_t v)
{
    if (v < 0)                  return 0;
    if (v > (int32_t)INT16_MAX) return (int16_t)INT16_MAX;
    return (int16_t)v;
}

/** Clamp a damage value to [1, INT8_MAX]. Enforces minimum damage floor of 1
 *  per NTR-B3 (Tough Hide cannot reduce damage below 1). Zero is only returned
 *  when dmg_before_floor is 0 and no hit occurred — this function is only
 *  called on hits, so the floor is always active.
 *  Use clamp_damage_miss() for misses. */
static int8_t clamp_damage_hit(int32_t dmg)
{
    if (dmg < 1)                 return 1;  /* NTR-B3: minimum floor of 1 */
    if (dmg > (int32_t)INT8_MAX) return (int8_t)INT8_MAX;
    return (int8_t)dmg;
}

/** Clamp a damage value to [0, INT8_MAX] for misses (no floor). */
static int8_t clamp_damage_miss(int32_t dmg)
{
    if (dmg < 0)                 return 0;
    if (dmg > (int32_t)INT8_MAX) return (int8_t)INT8_MAX;
    return (int8_t)dmg;
}

/**
 * hp_winner() — Resolve the winner by HP percentage (round-limit tiebreaker).
 *
 * Returns 1 if F1 has strictly higher HP%, 2 otherwise (including ties, which
 * go to F2 as the defender). Avoids division by zero via hp_max guard.
 */
static uint8_t hp_winner(const fq_combat_fighter_t *f1, const fq_combat_fighter_t *f2)
{
    uint32_t max1 = (f1->hp_max > 0) ? (uint32_t)f1->hp_max : 1u;
    uint32_t max2 = (f2->hp_max > 0) ? (uint32_t)f2->hp_max : 1u;
    uint32_t pct1 = (uint32_t)f1->hp * 100u / max1;
    uint32_t pct2 = (uint32_t)f2->hp * 100u / max2;
    return (pct1 > pct2) ? 1u : 2u; /* ties → F2 */
}

/**
 * copy_fighter() — Populate fq_combat_fighter_t from fq_character_t.
 *
 * Phase 5: copies equipped item IDs and count.
 * Initializes per-round item state: damage_bonus=0, damage_mult_pct=100,
 * dodge_bonus=0.
 *
 * HP is clamped to INT16_MAX (N5 defense-in-depth: hp_max is uint16_t).
 * reroll_charges = fq_effective_stat(intelligence) / 4.
 */
static void copy_fighter(fq_combat_fighter_t *f, const fq_character_t *c)
{
    int16_t clamped = (c->hp_max > (uint16_t)INT16_MAX)
                      ? (int16_t)INT16_MAX
                      : (int16_t)c->hp_max;

    f->hp             = clamped;
    f->hp_max         = clamped;
    f->strength       = c->strength;
    f->speed          = c->speed;
    f->precision      = c->precision;
    f->intelligence   = c->intelligence;
    f->class_id       = c->class_id;
    f->reroll_charges = fq_effective_stat(c->intelligence) / 4u;

    /* Phase 5: copy equipped item IDs. */
    f->equipped_count = c->equipped_count;
    for (uint8_t i = 0u; i < 5u; i++) {
        f->equipped_items[i] = c->equipped[i];
    }

    /* Phase 5: initialize per-round item state. */
    f->damage_bonus   = 0;
    f->damage_mult_pct = 100u;
    f->dodge_bonus    = 0u;
}

/**
 * reset_per_round_item_state() — Reset per-round item accumulators.
 *
 * Called at the start of each round (before ON_ROUND_START triggers fire).
 * This ensures DAMAGE_ADD and DAMAGE_MULT bonuses from the previous round
 * do not carry over.
 *
 * Note: PASSIVE bonuses are applied once during init, NOT here.
 * The damage_bonus from PASSIVE items is re-applied each round start
 * by running PASSIVE triggers again is NOT the design — instead,
 * PASSIVE items use a separate accumulator in future phases.
 *
 * For Phase 5: damage_bonus resets each round; PASSIVE items were one-shot
 * at init and added a permanent stat adjustment (Iron Fist effectively adds
 * to strength for the fight, not a per-round accumulator).
 *
 * CORRECTION per spec: Iron Fist is PASSIVE +1 damage_bonus, applied each
 * round at ON_ROUND_START (re-evaluated). damage_bonus resets to 0 here,
 * then PASSIVE trigger re-fires at ON_ROUND_START loop. But the spec says
 * PASSIVE is "one-time stat modifiers" applied during init.
 *
 * PM decision: PASSIVE trigger fires ONCE at init. The damage_bonus it sets
 * persists for the fight (it's a flat stat bonus). We do NOT reset it per
 * round. Only per-round items (Lucky Coin, Tough Hide, etc.) modify
 * damage_bonus temporarily. Reset only non-PASSIVE accumulators.
 *
 * Implementation: reset damage_bonus to the fighter's passive_damage_base
 * (the sum of all PASSIVE DAMAGE_ADD items, computed once at init).
 * For simplicity in Phase 5: fighters store passive_damage_base = 0 and
 * we apply PASSIVE items through a separate init-time call.
 *
 * FINAL DECISION: Reset damage_bonus to 0 each round AND re-apply PASSIVE
 * items via a PASSIVE trigger eval at round start. This is the cleanest model.
 */
static void reset_per_round_item_state(fq_combat_fighter_t *f)
{
    f->damage_bonus   = 0;
    f->damage_mult_pct = 100u;
    f->dodge_bonus    = 0u;
}

/* ---------------------------------------------------------------------------
 * fq_combat_init
 * ---------------------------------------------------------------------------*/
game_err_t fq_combat_init(fq_combat_ctx_t      *ctx,
                           const fq_character_t *c1,
                           const fq_character_t *c2,
                           uint32_t              seed)
{
    /* N1: NULL guard. */
    if ((ctx == NULL) || (c1 == NULL) || (c2 == NULL)) {
        return GAME_ERR_NULL_PTR;
    }

    /* Zero context for a deterministic initial state.
     * This also initializes round_3_f1_hp=0, round_3_f2_hp=0,
     * time_loop_used=0, item_recursion_depth=0 (NTR-D1). */
    memset(ctx, 0, sizeof(*ctx));

    copy_fighter(&ctx->f1, c1);
    copy_fighter(&ctx->f2, c2);

    /* N3: Detect dead fighters at init (hp_max == 0 → hp == 0). */
    if (ctx->f1.hp <= 0 && ctx->f2.hp <= 0) {
        ctx->finished = 1u;
        ctx->winner   = 0u; /* draw */
        return GAME_OK;
    }
    if (ctx->f1.hp <= 0) {
        ctx->finished = 1u;
        ctx->winner   = 2u;
        return GAME_OK;
    }
    if (ctx->f2.hp <= 0) {
        ctx->finished = 1u;
        ctx->winner   = 1u;
        return GAME_OK;
    }

    /* fq_prng_init guards against seed == 0. */
    fq_prng_init(&ctx->rng, seed);

    /* B1 Initiative: d6 + eff_speed/3.
     * F1 must strictly exceed F2 to go first; ties give initiative to F2 (defender). */
    uint32_t init1 = fq_prng_range(&ctx->rng, 1u, 6u)
                     + (uint32_t)(fq_effective_stat(c1->speed) / 3);
    uint32_t init2 = fq_prng_range(&ctx->rng, 1u, 6u)
                     + (uint32_t)(fq_effective_stat(c2->speed) / 3);

    ctx->first_attacker = (init1 > init2) ? 1u : 2u;
    ctx->current_round  = 1u;

    /* Phase 5: Apply PASSIVE item triggers once at init.
     * Uses first_attacker as the attacking_fighter parameter (arbitrary for
     * PASSIVE — both fighters' items are evaluated). */
    fq_item_eval_trigger(ctx, FQ_TRIGGER_PASSIVE, ctx->first_attacker);

    return GAME_OK;
}

/* ---------------------------------------------------------------------------
 * consume_lucky_star() — Advance PRNG for Lucky Star phase.
 *
 * Always consumes exactly 2 calls (A1 fix: d20, not d100). Both BLE peers'
 * PRNG streams stay synchronized regardless of perk ownership.
 * ---------------------------------------------------------------------------*/
static void consume_lucky_star(fq_prng_t *rng)
{
    fq_prng_range(rng, 1u, 20u); /* F1 Lucky Star check */
    fq_prng_range(rng, 1u, 20u); /* F2 Lucky Star check */
}

/* ---------------------------------------------------------------------------
 * fq_combat_step
 *
 * PRNG order per round (Phase 4 base, unchanged):
 *   1. [ON_ROUND_START item triggers — Lucky Coin d100, Chaos Orb d4]
 *   2. Attacker 1 d6 attack roll
 *   3. [Conditional] Attacker 1 self-reroll d6  (if raw_roll<=2 and charges>0)
 *   4. Dodge roll for attack 1 d100
 *   5. Attacker 2 d6 attack roll
 *   6. [Conditional] Attacker 2 self-reroll d6  (if raw_roll<=2 and charges>0)
 *   7. [Conditional] Defensive reroll d6         (if atk2_raw>=5 and atk1 charges>0)
 *   8. Dodge roll for attack 2 d100
 *   9. Lucky Star F1 d20
 *  10. Lucky Star F2 d20
 *
 * Item PRNG calls at ON_ROUND_START fire BEFORE step 2 (NTR-A4).
 * NTR-A2: No-item fight produces same PRNG state as Phase 4 baseline
 *         (item triggers with no items make no PRNG calls).
 * ---------------------------------------------------------------------------*/
fq_round_result_t fq_combat_step(fq_combat_ctx_t *ctx)
{
    fq_round_result_t result;
    memset(&result, 0, sizeof(result));

    /* N14: NO-OP if already finished. PRNG state is not advanced. */
    if (ctx->finished) {
        result.finished = 1u;
        result.winner   = ctx->winner;
        result.f1_hp    = ctx->f1.hp;
        result.f2_hp    = ctx->f2.hp;
        result.round    = ctx->current_round;
        return result;
    }

    result.round = ctx->current_round;

    /* --- Phase 5: Reset per-round item state and re-apply PASSIVE bonuses --- */
    reset_per_round_item_state(&ctx->f1);
    reset_per_round_item_state(&ctx->f2);
    /* Re-apply PASSIVE items (Iron Fist etc.) each round.
     * PASSIVE is the only trigger that re-fires every round via this path. */
    fq_item_eval_trigger(ctx, FQ_TRIGGER_PASSIVE, ctx->first_attacker);

    /* --- Phase 5: ON_ROUND_START triggers (Lucky Coin, Chaos Orb) ---
     * These fire BEFORE attacks (NTR-A4). PRNG consumed even if no items. */
    fq_item_eval_trigger(ctx, FQ_TRIGGER_ON_ROUND_START, ctx->first_attacker);

    /* Pointer aliases: first/second are initiative-ordered, f1/f2 are fixed. */
    fq_combat_fighter_t *first  = (ctx->first_attacker == 1u) ? &ctx->f1 : &ctx->f2;
    fq_combat_fighter_t *second = (ctx->first_attacker == 1u) ? &ctx->f2 : &ctx->f1;

    /* ------------------------------------------------------------------
     * Attack 1: first → second
     * ------------------------------------------------------------------ */
    {
        /* Phase 5: ON_DEFEND triggers for the defender (second), before attack. */
        fq_item_eval_trigger(ctx, FQ_TRIGGER_ON_DEFEND, ctx->first_attacker);

        /* Step 1: d6 attack roll. */
        uint32_t raw1 = fq_prng_range(&ctx->rng, 1u, 6u);

        /* Step 2: Self-reroll — only if raw1 <= 2 and attacker has charges. */
        uint8_t rr1 = 0u;
        if ((raw1 <= 2u) && (first->reroll_charges > 0u)) {
            uint32_t rr = fq_prng_range(&ctx->rng, 1u, 6u);
            if (rr > raw1) { raw1 = rr; }
            first->reroll_charges--;
            rr1 = 1u;
        }

        /* Step 3: Dodge roll d100.
         * B3: dodge_chance clamped to [5, 40]. */
        uint32_t dodge1 = fq_prng_range(&ctx->rng, 1u, 100u);
        int32_t  dc1    = (int32_t)fq_effective_stat(second->speed) * 2
                        - (int32_t)fq_effective_stat(first->precision) / 2;
        if (dc1 < 5)  dc1 = 5;
        if (dc1 > 40) dc1 = 40; /* B3 fix */

        /* Phase 5: Apply dodge_bonus from items (e.g., future dodge items). */
        dc1 = dc1 + (int32_t)second->dodge_bonus;
        if (dc1 > 40) dc1 = 40; /* Re-clamp after bonus */

        int      hit1   = ((int32_t)dodge1 > dc1);

        /* Phase 5: ON_DODGE trigger for defender if dodge succeeded. */
        if (!hit1) {
            fq_item_eval_trigger(ctx, FQ_TRIGGER_ON_DODGE, ctx->first_attacker);
        }

        /* B2: Precision tier and adjusted roll. */
        int     eff_prec1 = fq_effective_stat(first->precision);
        int     tier1     = eff_prec1 / 5;
        if (tier1 > 3) { tier1 = 3; }
        uint8_t adj1 = PRECISION_TABLE[tier1][raw1 - 1u];

        /* B4: Crit from original raw_roll, no separate PRNG call.
         * crit_threshold = 6 - (precision_tier / 2). */
        int crit_thresh1 = 6 - (tier1 / 2);
        int crit1        = ((int)raw1 >= crit_thresh1);

        int32_t damage1 = 0;
        if (hit1) {
            /* Phase 5: ON_ATTACK trigger for attacker. */
            fq_item_eval_trigger(ctx, FQ_TRIGGER_ON_ATTACK, ctx->first_attacker);

            /* Phase 5: ON_CRIT trigger if crit fires. */
            if (crit1) {
                fq_item_eval_trigger(ctx, FQ_TRIGGER_ON_CRIT, ctx->first_attacker);
            }

            /* Phase 5 damage formula:
             * base = adj1 + eff_strength/2
             * with_add = base + first->damage_bonus
             * Phase 4 crit was: damage * 3/2
             * Phase 5 crit is controlled by damage_mult_pct:
             *   - Default crit: damage_mult_pct = 150 (applied by combat, not items)
             *   - Haymaker: damage_mult_pct = 200 (set by ON_CRIT item trigger)
             *   - No crit: damage_mult_pct = 100 (reset at round start)
             */
            int32_t base1 = (int32_t)adj1
                          + (int32_t)fq_effective_stat(first->strength) / 2;

            /* Apply DAMAGE_ADD from items. */
            int32_t with_add1 = base1 + (int32_t)first->damage_bonus;

            /* Apply crit multiplier (Phase 5: item-controlled). */
            int32_t mult1 = crit1
                ? (int32_t)first->damage_mult_pct  /* item may have set 200 */
                : 100;
            if (crit1 && first->damage_mult_pct == 100u) {
                mult1 = 150; /* Default crit multiplier when no Haymaker. */
            }
            damage1 = with_add1 * mult1 / 100;

            /* NTR-B3: Minimum damage floor of 1 on hits. */
            if (damage1 < 1) { damage1 = 1; }

            second->hp = clamp_hp((int32_t)second->hp - damage1);
        }

        /* Write result fields (always in F1/F2 absolute terms). */
        if (ctx->first_attacker == 1u) {
            result.f1_hit          = (uint8_t)hit1;
            result.f1_crit         = (uint8_t)crit1;
            result.f1_rerolled     = rr1;
            result.f1_damage_dealt = hit1 ? clamp_damage_hit(damage1)
                                          : clamp_damage_miss(0);
        } else {
            result.f2_hit          = (uint8_t)hit1;
            result.f2_crit         = (uint8_t)crit1;
            result.f2_rerolled     = rr1;
            result.f2_damage_dealt = hit1 ? clamp_damage_hit(damage1)
                                          : clamp_damage_miss(0);
        }
    }

    /* N6: KO check after attack 1 — no counter-attack. */
    if (second->hp <= 0) {
        /* Phase 5: ON_KILL trigger for the attacker. */
        fq_item_eval_trigger(ctx, FQ_TRIGGER_ON_KILL, ctx->first_attacker);
        /* Phase 5: ON_DEATH trigger for the KO'd fighter. */
        fq_item_eval_trigger(ctx, FQ_TRIGGER_ON_DEATH,
                             (ctx->first_attacker == 1u) ? 2u : 1u);

        ctx->finished = 1u;
        ctx->winner   = ctx->first_attacker;
        consume_lucky_star(&ctx->rng); /* PRNG sync invariant — always consumed */
        goto done;
    }

    /* ------------------------------------------------------------------
     * Attack 2: second → first
     * ------------------------------------------------------------------ */
    {
        /* Phase 5: ON_DEFEND triggers for the defender (first), before attack 2.
         * attacking_fighter for attack 2 is the opposite of first_attacker. */
        uint8_t atk2 = (ctx->first_attacker == 1u) ? 2u : 1u;
        fq_item_eval_trigger(ctx, FQ_TRIGGER_ON_DEFEND, atk2);

        /* Step 4: d6 attack roll. */
        uint32_t raw2 = fq_prng_range(&ctx->rng, 1u, 6u);

        /* Step 5: Attacker 2 self-reroll. */
        uint8_t rr2 = 0u;
        if ((raw2 <= 2u) && (second->reroll_charges > 0u)) {
            uint32_t rr = fq_prng_range(&ctx->rng, 1u, 6u);
            if (rr > raw2) { raw2 = rr; }
            second->reroll_charges--;
            rr2 = 1u;
        }

        /* Step 6: B5 Defensive reroll by first (attacker 1 defending).
         * If second's raw roll >= 5 AND first still has charges, first
         * defensively rerolls second's attack, keeping the LOWER value. */
        uint8_t def_rr1 = 0u;
        if ((raw2 >= 5u) && (first->reroll_charges > 0u)) {
            uint32_t rr = fq_prng_range(&ctx->rng, 1u, 6u);
            if (rr < raw2) { raw2 = rr; }
            first->reroll_charges--;
            def_rr1 = 1u;
        }

        /* Step 7: Dodge roll d100.
         * B3: dodge_chance clamped to [5, 40]. */
        uint32_t dodge2 = fq_prng_range(&ctx->rng, 1u, 100u);
        int32_t  dc2    = (int32_t)fq_effective_stat(first->speed) * 2
                        - (int32_t)fq_effective_stat(second->precision) / 2;
        if (dc2 < 5)  dc2 = 5;
        if (dc2 > 40) dc2 = 40; /* B3 fix */

        /* Phase 5: Apply dodge_bonus. */
        dc2 = dc2 + (int32_t)first->dodge_bonus;
        if (dc2 > 40) dc2 = 40;

        int      hit2   = ((int32_t)dodge2 > dc2);

        /* Phase 5: ON_DODGE trigger for defender (first) if dodge succeeded. */
        if (!hit2) {
            fq_item_eval_trigger(ctx, FQ_TRIGGER_ON_DODGE, atk2);
        }

        /* B2: Precision tier and adjusted roll. */
        int     eff_prec2 = fq_effective_stat(second->precision);
        int     tier2     = eff_prec2 / 5;
        if (tier2 > 3) { tier2 = 3; }
        uint8_t adj2 = PRECISION_TABLE[tier2][raw2 - 1u];

        /* B4: Crit from original raw_roll, no separate PRNG call. */
        int crit_thresh2 = 6 - (tier2 / 2);
        int crit2        = ((int)raw2 >= crit_thresh2);

        int32_t damage2 = 0;
        if (hit2) {
            /* Phase 5: ON_ATTACK trigger for attacker 2. */
            fq_item_eval_trigger(ctx, FQ_TRIGGER_ON_ATTACK, atk2);

            /* Phase 5: ON_CRIT trigger for attacker 2 if crit fires. */
            if (crit2) {
                fq_item_eval_trigger(ctx, FQ_TRIGGER_ON_CRIT, atk2);
            }

            int32_t base2 = (int32_t)adj2
                          + (int32_t)fq_effective_stat(second->strength) / 2;

            /* Apply DAMAGE_ADD from items. */
            int32_t with_add2 = base2 + (int32_t)second->damage_bonus;

            /* Apply crit multiplier. */
            int32_t mult2 = crit2
                ? (int32_t)second->damage_mult_pct
                : 100;
            if (crit2 && second->damage_mult_pct == 100u) {
                mult2 = 150;
            }
            damage2 = with_add2 * mult2 / 100;

            /* NTR-B3: minimum floor. */
            if (damage2 < 1) { damage2 = 1; }

            first->hp = clamp_hp((int32_t)first->hp - damage2);
        }

        /* Write result fields. Merge reroll flags (self + defensive). */
        if (ctx->first_attacker == 1u) {
            result.f2_hit          = (uint8_t)hit2;
            result.f2_crit         = (uint8_t)crit2;
            result.f2_rerolled     = rr2;
            result.f2_damage_dealt = hit2 ? clamp_damage_hit(damage2)
                                          : clamp_damage_miss(0);
            /* def_rr1: F1 (first) used a defensive reroll — merge with f1_rerolled. */
            result.f1_rerolled    |= def_rr1;
        } else {
            result.f1_hit          = (uint8_t)hit2;
            result.f1_crit         = (uint8_t)crit2;
            result.f1_rerolled     = rr2;
            result.f1_damage_dealt = hit2 ? clamp_damage_hit(damage2)
                                          : clamp_damage_miss(0);
            /* def_rr1: F2 (first) used a defensive reroll — merge with f2_rerolled. */
            result.f2_rerolled    |= def_rr1;
        }
    }

    /* KO check after attack 2. */
    if (first->hp <= 0) {
        uint8_t atk2 = (ctx->first_attacker == 1u) ? 2u : 1u;
        /* Phase 5: ON_KILL for attacker 2, ON_DEATH for the KO'd fighter (first). */
        fq_item_eval_trigger(ctx, FQ_TRIGGER_ON_KILL, atk2);
        fq_item_eval_trigger(ctx, FQ_TRIGGER_ON_DEATH, ctx->first_attacker);

        ctx->finished = 1u;
        ctx->winner   = (ctx->first_attacker == 1u) ? 2u : 1u;
        consume_lucky_star(&ctx->rng); /* PRNG sync invariant */
        goto done;
    }

    /* --- Lucky Star phase (N11): always 2 PRNG calls (d20) --------------- */
    consume_lucky_star(&ctx->rng);
    /* f1_lucky_star and f2_lucky_star remain 0 (Phase 5 TODO). */

    /* --- Overtime (N8): fires after both attacks, round > threshold ------- */
    if (ctx->current_round > FQ_OVERTIME_THRESHOLD) {
        uint8_t ot = (uint8_t)(ctx->current_round - FQ_OVERTIME_THRESHOLD);
        result.overtime_damage = ot;
        ctx->f1.hp = clamp_hp((int32_t)ctx->f1.hp - (int32_t)ot);
        ctx->f2.hp = clamp_hp((int32_t)ctx->f2.hp - (int32_t)ot);

        if (ctx->f1.hp <= 0 || ctx->f2.hp <= 0) {
            ctx->finished = 1u;
            /* If only one fighter was KO'd, they lose. Double KO → hp% tiebreak. */
            if (ctx->f1.hp > 0) {
                ctx->winner = 1u;
            } else if (ctx->f2.hp > 0) {
                ctx->winner = 2u;
            } else {
                ctx->winner = hp_winner(&ctx->f1, &ctx->f2);
            }
            goto done;
        }
    }

    /* --- Phase 5: ON_ROUND_END triggers (Bandage, Time Loop) -------------- */
    fq_item_eval_trigger(ctx, FQ_TRIGGER_ON_ROUND_END, ctx->first_attacker);

    /* --- Round limit (N17): winner by HP percentage ----------------------- */
    if (ctx->current_round >= FQ_MAX_ROUNDS) {
        ctx->finished = 1u;
        ctx->winner   = hp_winner(&ctx->f1, &ctx->f2);
        goto done;
    }

done:
    result.f1_hp    = ctx->f1.hp;
    result.f2_hp    = ctx->f2.hp;
    result.finished = ctx->finished;
    result.winner   = ctx->winner;
    ctx->current_round++;

    return result;
}
