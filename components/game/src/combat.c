/**
 * combat.c — FiestaQuest Phase-4 Combat Engine implementation.
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
 * Constitution Priority 0: No floats, no time.h, no external entropy.
 * All random decisions use fq_prng_t exclusively.
 */

#include "combat.h"
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

/** Clamp a damage value to [0, INT8_MAX]. Defense-in-depth against overflow. */
static int8_t clamp_damage(int32_t dmg)
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

    /* Zero context for a deterministic initial state. */
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

    return GAME_OK;
}

/* ---------------------------------------------------------------------------
 * consume_lucky_star() — Advance PRNG for Lucky Star phase.
 *
 * Always consumes exactly 2 calls (A1 fix: d20, not d100). Both BLE peers'
 * PRNG streams stay synchronized regardless of perk ownership.
 *
 * ADVISORY ADV-P4-01: Phase 5 item engine must hook here to check perk
 * ownership and apply bonus attack when ls_roll == 1 (5% of d20).
 * ---------------------------------------------------------------------------*/
static void consume_lucky_star(fq_prng_t *rng)
{
    fq_prng_range(rng, 1u, 20u); /* F1 Lucky Star check */
    fq_prng_range(rng, 1u, 20u); /* F2 Lucky Star check */
}

/* ---------------------------------------------------------------------------
 * fq_combat_step
 *
 * PRNG order per round:
 *   1. Attacker 1 d6 attack roll
 *   2. [Conditional] Attacker 1 self-reroll d6  (if raw_roll<=2 and charges>0)
 *   3. Dodge roll for attack 1 d100
 *   4. Attacker 2 d6 attack roll
 *   5. [Conditional] Attacker 2 self-reroll d6  (if raw_roll<=2 and charges>0)
 *   6. [Conditional] Defensive reroll d6         (if atk2_raw>=5 and atk1 charges>0)
 *   7. Dodge roll for attack 2 d100
 *   8. Lucky Star F1 d20
 *   9. Lucky Star F2 d20
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

    /* Pointer aliases: first/second are initiative-ordered, f1/f2 are fixed. */
    fq_combat_fighter_t *first  = (ctx->first_attacker == 1u) ? &ctx->f1 : &ctx->f2;
    fq_combat_fighter_t *second = (ctx->first_attacker == 1u) ? &ctx->f2 : &ctx->f1;

    /* ------------------------------------------------------------------
     * Attack 1: first → second
     * ------------------------------------------------------------------ */
    {
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
        int      hit1   = ((int32_t)dodge1 > dc1);

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
            damage1 = (int32_t)adj1
                    + (int32_t)fq_effective_stat(first->strength) / 2;
            if (crit1) {
                damage1 = damage1 * 3 / 2; /* integer 50% bonus */
            }
            second->hp = clamp_hp((int32_t)second->hp - damage1);
        }

        /* Write result fields (always in F1/F2 absolute terms). */
        if (ctx->first_attacker == 1u) {
            result.f1_hit          = (uint8_t)hit1;
            result.f1_crit         = (uint8_t)crit1;
            result.f1_rerolled     = rr1;
            result.f1_damage_dealt = clamp_damage(damage1);
        } else {
            result.f2_hit          = (uint8_t)hit1;
            result.f2_crit         = (uint8_t)crit1;
            result.f2_rerolled     = rr1;
            result.f2_damage_dealt = clamp_damage(damage1);
        }
    }

    /* N6: KO check after attack 1 — no counter-attack. */
    if (second->hp <= 0) {
        ctx->finished = 1u;
        ctx->winner   = ctx->first_attacker;
        consume_lucky_star(&ctx->rng); /* PRNG sync invariant — always consumed */
        goto done;
    }

    /* ------------------------------------------------------------------
     * Attack 2: second → first
     * ------------------------------------------------------------------ */
    {
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
        int      hit2   = ((int32_t)dodge2 > dc2);

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
            damage2 = (int32_t)adj2
                    + (int32_t)fq_effective_stat(second->strength) / 2;
            if (crit2) {
                damage2 = damage2 * 3 / 2;
            }
            first->hp = clamp_hp((int32_t)first->hp - damage2);
        }

        /* Write result fields. Merge reroll flags (self + defensive). */
        if (ctx->first_attacker == 1u) {
            result.f2_hit          = (uint8_t)hit2;
            result.f2_crit         = (uint8_t)crit2;
            result.f2_rerolled     = rr2;
            result.f2_damage_dealt = clamp_damage(damage2);
            /* def_rr1: F1 (first) used a defensive reroll — merge with f1_rerolled. */
            result.f1_rerolled    |= def_rr1;
        } else {
            result.f1_hit          = (uint8_t)hit2;
            result.f1_crit         = (uint8_t)crit2;
            result.f1_rerolled     = rr2;
            result.f1_damage_dealt = clamp_damage(damage2);
            /* def_rr1: F2 (first) used a defensive reroll — merge with f2_rerolled. */
            result.f2_rerolled    |= def_rr1;
        }
    }

    /* KO check after attack 2. */
    if (first->hp <= 0) {
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
