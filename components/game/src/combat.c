/**
 * combat.c — FiestaQuest Phase-4 Combat Engine implementation.
 *
 * See combat.h for full API contract, PRNG call ordering, and
 * Constitution compliance notes.
 *
 * Design decisions (PM-approved):
 *   - HP clamped to INT16_MAX in init (defense-in-depth for uint16_t hp_max).
 *   - Simultaneous KO is impossible: attacker's KO ends the fight immediately.
 *   - Round limit (FQ_MAX_ROUNDS=12): winner by HP percentage, ties → F2.
 *   - Reroll evaluation order: F1 (first) then F2 (second), per Appendix A.
 *   - Lucky Star: always consumes 2 PRNG calls; bonus damage deferred to Phase 5.
 *   - Conditional PRNG calls (crit, reroll) only advance PRNG when taken.
 *
 * Constitution Priority 0: No floats, no time.h, no external entropy.
 * This module uses fq_prng_t exclusively for all random decisions.
 */

#include "combat.h"
#include "progression.h"
#include <string.h>
#include <limits.h>

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------------*/

/**
 * clamp_i16() — Clamp a signed 32-bit value to the int16_t range.
 *
 * Used for HP bookkeeping: HP must stay in [0, INT16_MAX].
 * Lower bound is always 0 (dead) unless a different floor is needed.
 */
static int16_t clamp_i16_to_hp(int32_t v)
{
    if (v < 0) return 0;
    if (v > (int32_t)INT16_MAX) return (int16_t)INT16_MAX;
    return (int16_t)v;
}

/**
 * clamp_i8_damage() — Clamp a positive damage value to [0, INT8_MAX].
 *
 * N9: Crit damage could theoretically exceed INT8_MAX if stats were extreme
 * (in practice max is 25 << 127). This clamp is defense-in-depth.
 */
static int8_t clamp_i8_damage(int32_t dmg)
{
    if (dmg < 0) return 0;
    if (dmg > (int32_t)INT8_MAX) return (int8_t)INT8_MAX;
    return (int8_t)dmg;
}

/**
 * copy_fighter() — Copy relevant stats from fq_character_t into
 * fq_combat_fighter_t and initialise derived fields.
 *
 * HP is clamped: hp = min(hp_max, INT16_MAX). N5 defense-in-depth.
 * reroll_charges = fq_effective_stat(intelligence) / 4.
 */
static void copy_fighter(fq_combat_fighter_t *f, const fq_character_t *c)
{
    /* HP clamp: hp_max is uint16_t; INT16_MAX = 32767.
     * In practice hp_max never exceeds ~1000, but the clamp is mandatory. */
    int16_t clamped_hp = (c->hp_max > (uint16_t)INT16_MAX)
                         ? (int16_t)INT16_MAX
                         : (int16_t)c->hp_max;

    f->hp             = clamped_hp;
    f->hp_max         = clamped_hp;
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
    /* N1: NULL guard on all three pointers. */
    if ((ctx == NULL) || (c1 == NULL) || (c2 == NULL)) {
        return GAME_ERR_NULL_PTR;
    }

    /* Zero the context for deterministic initial state. */
    memset(ctx, 0, sizeof(*ctx));

    /* Copy fighter stats. */
    copy_fighter(&ctx->f1, c1);
    copy_fighter(&ctx->f2, c2);

    /* N3: If either fighter has zero HP after copy, the fight is already over. */
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

    /* Initialise PRNG. fq_prng_init handles seed=0 → state=1. */
    fq_prng_init(&ctx->rng, seed);

    /* Roll initiative: PRNG calls 1 and 2.
     * initiative_total = roll[0..99] + effective_speed
     * Ties: F2 (defender) wins. */
    uint32_t eff_spd1 = (uint32_t)fq_effective_stat(c1->speed);
    uint32_t eff_spd2 = (uint32_t)fq_effective_stat(c2->speed);

    uint32_t init1 = fq_prng_range(&ctx->rng, 0u, 99u) + eff_spd1;
    uint32_t init2 = fq_prng_range(&ctx->rng, 0u, 99u) + eff_spd2;

    /* Ties → F2 (defender). Strictly greater required for F1 to go first. */
    ctx->first_attacker = (init1 > init2) ? 1u : 2u;

    /* Start at round 1. */
    ctx->current_round = 1u;
    ctx->finished      = 0u;
    ctx->winner        = 0u;

    return GAME_OK;
}

/* ---------------------------------------------------------------------------
 * resolve_attack() — Internal: resolve one attacker→responder attack.
 *
 * Mutates:
 *   - responder->hp (damage applied)
 *   - attacker->reroll_charges (decremented if reroll used)
 *
 * Returns outcome fields to be stored in fq_round_result_t:
 *   out_hit, out_crit, out_rerolled, out_damage_dealt
 *
 * PRNG call ordering (per Appendix A):
 *   1. Attack roll: fq_prng_range(1, 6)
 *   2. Dodge roll:  fq_prng_range(1, 100)
 *   3. [Conditional] Reroll: fq_prng_range(1, 6) — only if atk_roll <= 2 AND charges > 0
 *   4. [Conditional] Crit:   fq_prng_range(1, 100) — only if hit
 * ---------------------------------------------------------------------------*/
static void resolve_attack(fq_prng_t           *rng,
                           fq_combat_fighter_t *attacker,
                           fq_combat_fighter_t *responder,
                           uint8_t             *out_hit,
                           uint8_t             *out_crit,
                           uint8_t             *out_rerolled,
                           int8_t              *out_damage_dealt)
{
    /* Step 1: Attack roll d6. */
    uint32_t atk_roll = fq_prng_range(rng, 1u, 6u);

    /* Step 2: Dodge check.
     * dodge_chance = eff_speed_responder * 2 - eff_precision_attacker / 2
     * Clamped to [5, 75]. */
    uint32_t dodge_roll = fq_prng_range(rng, 1u, 100u);

    int32_t eff_spd_resp  = (int32_t)fq_effective_stat(responder->speed);
    int32_t eff_prec_atk  = (int32_t)fq_effective_stat(attacker->precision);
    int32_t dodge_chance  = eff_spd_resp * 2 - eff_prec_atk / 2;
    if (dodge_chance < 5)  dodge_chance = 5;
    if (dodge_chance > 75) dodge_chance = 75;

    /* Dodge succeeds if dodge_roll <= dodge_chance → miss. */
    int hit = ((int32_t)dodge_roll > dodge_chance) ? 1 : 0;

    /* Step 3 (conditional): Reroll if atk_roll <= 2 AND attacker has charges. */
    int rerolled = 0;
    if ((atk_roll <= 2u) && (attacker->reroll_charges > 0u)) {
        uint32_t reroll = fq_prng_range(rng, 1u, 6u);
        if (reroll > atk_roll) {
            atk_roll = reroll;
        }
        attacker->reroll_charges--;
        rerolled = 1;
    }

    /* Step 4 (conditional): Damage and crit, only if hit. */
    int32_t damage = 0;
    int crit = 0;
    if (hit) {
        int32_t eff_str = (int32_t)fq_effective_stat(attacker->strength);
        damage = (int32_t)atk_roll + eff_str / 2;

        /* Crit check: fq_prng_range(1, 100) <= eff_precision * 5, clamped to 50%. */
        uint32_t crit_roll  = fq_prng_range(rng, 1u, 100u);
        uint32_t crit_thresh = (uint32_t)fq_effective_stat(attacker->precision) * 5u;
        if (crit_thresh > 50u) crit_thresh = 50u;

        if (crit_roll <= crit_thresh) {
            /* Integer 3/2 multiply: damage * 3 / 2. */
            damage = damage * 3 / 2;
            crit = 1;
        }

        /* N9: Clamp damage to [0, INT8_MAX]. */
        if (damage > (int32_t)INT8_MAX) damage = (int32_t)INT8_MAX;
        if (damage < 0) damage = 0;

        /* Apply damage to responder. Clamp HP to [0, ...]. */
        int32_t new_hp = (int32_t)responder->hp - damage;
        responder->hp = clamp_i16_to_hp(new_hp);
    }

    *out_hit          = (uint8_t)hit;
    *out_crit         = (uint8_t)crit;
    *out_rerolled     = (uint8_t)rerolled;
    *out_damage_dealt = clamp_i8_damage(damage);
}

/* ---------------------------------------------------------------------------
 * fq_combat_step
 * ---------------------------------------------------------------------------*/
fq_round_result_t fq_combat_step(fq_combat_ctx_t *ctx)
{
    fq_round_result_t result;
    memset(&result, 0, sizeof(result));

    /* N14: If already finished, return a NO-OP result without advancing PRNG. */
    if (ctx->finished) {
        result.finished = 1u;
        result.winner   = ctx->winner;
        result.f1_hp    = ctx->f1.hp;
        result.f2_hp    = ctx->f2.hp;
        result.round    = ctx->current_round;
        return result;
    }

    result.round = ctx->current_round;

    /* Determine attacker and responder for attack 1. */
    fq_combat_fighter_t *first_ftr  = (ctx->first_attacker == 1u) ? &ctx->f1 : &ctx->f2;
    fq_combat_fighter_t *second_ftr = (ctx->first_attacker == 1u) ? &ctx->f2 : &ctx->f1;

    /* Convenience references for result fields (always F1/F2 perspective). */
    /* Attack 1: first_ftr → second_ftr */
    uint8_t a1_hit, a1_crit, a1_rerolled;
    int8_t  a1_damage;
    resolve_attack(&ctx->rng, first_ftr, second_ftr,
                   &a1_hit, &a1_crit, &a1_rerolled, &a1_damage);

    /* Map to result fields: first_ftr is f1 if first_attacker==1, else f2. */
    if (ctx->first_attacker == 1u) {
        result.f1_hit          = a1_hit;
        result.f1_crit         = a1_crit;
        result.f1_rerolled     = a1_rerolled;
        result.f1_damage_dealt = a1_damage;
    } else {
        result.f2_hit          = a1_hit;
        result.f2_crit         = a1_crit;
        result.f2_rerolled     = a1_rerolled;
        result.f2_damage_dealt = a1_damage;
    }

    /* N6: Check for KO after attack 1. If responder is dead, fight ends now.
     * The second attacker does NOT get to counter-attack (first kill wins). */
    if (second_ftr->hp <= 0) {
        ctx->finished = 1u;
        ctx->winner   = ctx->first_attacker;
        /* Lucky Star calls are consumed EVEN after KO per Appendix A invariant.
         * Both peers advance PRNG the same way regardless of fight outcome. */
        fq_prng_range(&ctx->rng, 1u, 100u); /* LS F1 */
        fq_prng_range(&ctx->rng, 1u, 100u); /* LS F2 */
        goto done;
    }

    /* Attack 2: second_ftr → first_ftr */
    {
        uint8_t a2_hit, a2_crit, a2_rerolled;
        int8_t  a2_damage;
        resolve_attack(&ctx->rng, second_ftr, first_ftr,
                       &a2_hit, &a2_crit, &a2_rerolled, &a2_damage);

        if (ctx->first_attacker == 1u) {
            result.f2_hit          = a2_hit;
            result.f2_crit         = a2_crit;
            result.f2_rerolled     = a2_rerolled;
            result.f2_damage_dealt = a2_damage;
        } else {
            result.f1_hit          = a2_hit;
            result.f1_crit         = a2_crit;
            result.f1_rerolled     = a2_rerolled;
            result.f1_damage_dealt = a2_damage;
        }
    }

    /* Check for KO after attack 2. */
    if (first_ftr->hp <= 0) {
        ctx->finished = 1u;
        /* Winner is the second attacker (opposite of first_attacker). */
        ctx->winner = (ctx->first_attacker == 1u) ? 2u : 1u;
        /* Lucky Star consumed even after KO (PRNG sync invariant). */
        fq_prng_range(&ctx->rng, 1u, 100u); /* LS F1 */
        fq_prng_range(&ctx->rng, 1u, 100u); /* LS F2 */
        goto done;
    }

    /* N11: Lucky Star — always consume 2 PRNG calls (Appendix A invariant).
     * Phase 4: bonus damage deferred to Phase 5 (no perk ownership check yet).
     * ADVISORY ADV-P4-01: Lucky Star bonus damage not wired — Phase 5 item engine
     * must hook here to check perk ownership and apply bonus d6 on trigger. */
    {
        uint32_t ls1 = fq_prng_range(&ctx->rng, 1u, 100u);
        uint32_t ls2 = fq_prng_range(&ctx->rng, 1u, 100u);
        /* Phase 4: trigger threshold is 5 but bonus never fires (no perk check). */
        result.f1_lucky_star = (ls1 <= 5u) ? 0u : 0u; /* Always 0 — Phase 5 TODO */
        result.f2_lucky_star = (ls2 <= 5u) ? 0u : 0u; /* Always 0 — Phase 5 TODO */
    }

    /* N8: Overtime — fires AFTER both attacks, BEFORE round-limit check.
     * Round > FQ_OVERTIME_THRESHOLD (8): overtime_damage = round - threshold.
     * Applied to BOTH fighters. HP clamped to 0. */
    if (ctx->current_round > FQ_OVERTIME_THRESHOLD) {
        uint8_t ot_dmg = (uint8_t)(ctx->current_round - FQ_OVERTIME_THRESHOLD);
        result.overtime_damage = ot_dmg;

        int32_t f1_new = (int32_t)ctx->f1.hp - (int32_t)ot_dmg;
        int32_t f2_new = (int32_t)ctx->f2.hp - (int32_t)ot_dmg;
        ctx->f1.hp = clamp_i16_to_hp(f1_new);
        ctx->f2.hp = clamp_i16_to_hp(f2_new);

        /* After overtime, check for double KO or single KO. */
        if (ctx->f1.hp <= 0 || ctx->f2.hp <= 0) {
            ctx->finished = 1u;
            if (ctx->f1.hp > 0) {
                ctx->winner = 1u;
            } else if (ctx->f2.hp > 0) {
                ctx->winner = 2u;
            } else {
                /* Both KO'd by overtime: higher HP% wins, ties → F2. */
                uint32_t f1pct = ((uint32_t)ctx->f1.hp * 100u)
                                 / (uint32_t)(ctx->f1.hp_max > 0 ? ctx->f1.hp_max : 1);
                uint32_t f2pct = ((uint32_t)ctx->f2.hp * 100u)
                                 / (uint32_t)(ctx->f2.hp_max > 0 ? ctx->f2.hp_max : 1);
                ctx->winner = (f1pct > f2pct) ? 1u : 2u;
            }
            goto done;
        }
    }

    /* N17: Round limit — after FQ_MAX_ROUNDS, fight ends.
     * Winner = higher HP percentage. Ties → F2 (defender). */
    if (ctx->current_round >= FQ_MAX_ROUNDS) {
        ctx->finished = 1u;
        uint32_t f1pct = ((uint32_t)ctx->f1.hp * 100u)
                         / (uint32_t)(ctx->f1.hp_max > 0 ? ctx->f1.hp_max : 1);
        uint32_t f2pct = ((uint32_t)ctx->f2.hp * 100u)
                         / (uint32_t)(ctx->f2.hp_max > 0 ? ctx->f2.hp_max : 1);
        ctx->winner = (f1pct > f2pct) ? 1u : 2u;
        goto done;
    }

done:
    /* Record final HP in result. */
    result.f1_hp   = ctx->f1.hp;
    result.f2_hp   = ctx->f2.hp;
    result.finished = ctx->finished;
    result.winner   = ctx->winner;

    /* Advance round counter (even if fight just ended — marks which round ended it). */
    ctx->current_round++;

    return result;
}
