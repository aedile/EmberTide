/**
 * combat.c — FiestaQuest Phase-4 Combat Engine implementation.
 *
 * See combat.h for full API contract, PRNG call ordering, and
 * Constitution compliance notes.
 *
 * Design decisions (PM-approved):
 *   - HP clamped to INT16_MAX in init (defense-in-depth for uint16_t hp_max).
 *   - Simultaneous KO is impossible: first attacker's KO ends the fight immediately.
 *   - Round limit (FQ_MAX_ROUNDS=12): winner by HP percentage, ties → F2.
 *   - Reroll evaluation order: first attacker then second, per Appendix A.
 *   - Lucky Star: always consumes 2 PRNG calls; bonus damage deferred to Phase 5.
 *   - Conditional PRNG calls (crit, reroll) only advance PRNG when the branch is taken.
 *
 * Constitution Priority 0: No floats, no time.h, no external entropy.
 * All random decisions use fq_prng_t exclusively.
 */

#include "combat.h"
#include "progression.h"
#include <string.h>
#include <limits.h>

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

    /* Initiative: two PRNG calls. Total = roll[0..99] + effective_speed.
     * F1 must strictly exceed F2 to go first; ties give initiative to F2 (defender). */
    uint32_t init1 = fq_prng_range(&ctx->rng, 0u, 99u)
                     + (uint32_t)fq_effective_stat(c1->speed);
    uint32_t init2 = fq_prng_range(&ctx->rng, 0u, 99u)
                     + (uint32_t)fq_effective_stat(c2->speed);

    ctx->first_attacker = (init1 > init2) ? 1u : 2u;
    ctx->current_round  = 1u;

    return GAME_OK;
}

/* ---------------------------------------------------------------------------
 * resolve_attack() — Resolve a single attacker→responder attack.
 *
 * Mutates responder->hp and (conditionally) attacker->reroll_charges.
 * PRNG call ordering per Appendix A:
 *   1. Attack roll:              fq_prng_range(1, 6)         — always
 *   2. Dodge roll:               fq_prng_range(1, 100)       — always
 *   3. Reroll (conditional):     fq_prng_range(1, 6)         — if atk_roll <= 2 AND charges > 0
 *   4. Crit check (conditional): fq_prng_range(1, 100)       — if hit
 * ---------------------------------------------------------------------------*/
static void resolve_attack(fq_prng_t           *rng,
                           fq_combat_fighter_t *attacker,
                           fq_combat_fighter_t *responder,
                           uint8_t             *out_hit,
                           uint8_t             *out_crit,
                           uint8_t             *out_rerolled,
                           int8_t              *out_damage_dealt)
{
    /* 1. Attack roll d6. */
    uint32_t atk = fq_prng_range(rng, 1u, 6u);

    /* 2. Dodge check: dodge_chance = eff_spd_resp*2 − eff_prec_atk/2, clamped [5, 75]. */
    uint32_t dodge_roll = fq_prng_range(rng, 1u, 100u);
    int32_t  dc = (int32_t)fq_effective_stat(responder->speed) * 2
                - (int32_t)fq_effective_stat(attacker->precision) / 2;
    if (dc < 5)  dc = 5;
    if (dc > 75) dc = 75;
    int hit = ((int32_t)dodge_roll > dc);

    /* 3. Conditional reroll: only if atk <= 2 and attacker has charges. */
    int rerolled = 0;
    if ((atk <= 2u) && (attacker->reroll_charges > 0u)) {
        uint32_t rr = fq_prng_range(rng, 1u, 6u);
        if (rr > atk) atk = rr;
        attacker->reroll_charges--;
        rerolled = 1;
    }

    /* 4. Damage and crit — only if the attack landed. */
    int32_t damage = 0;
    int     crit   = 0;
    if (hit) {
        damage = (int32_t)atk + (int32_t)fq_effective_stat(attacker->strength) / 2;

        uint32_t crit_roll   = fq_prng_range(rng, 1u, 100u);
        uint32_t crit_thresh = (uint32_t)fq_effective_stat(attacker->precision) * 5u;
        if (crit_thresh > 50u) crit_thresh = 50u;

        if (crit_roll <= crit_thresh) {
            damage = damage * 3 / 2; /* integer 50% bonus */
            crit   = 1;
        }

        /* N9: clamp damage to [0, INT8_MAX]. */
        if (damage > (int32_t)INT8_MAX) damage = (int32_t)INT8_MAX;
        if (damage < 0)                 damage = 0;

        responder->hp = clamp_hp((int32_t)responder->hp - damage);
    }

    *out_hit          = (uint8_t)hit;
    *out_crit         = (uint8_t)crit;
    *out_rerolled     = (uint8_t)rerolled;
    *out_damage_dealt = clamp_damage(damage);
}

/* ---------------------------------------------------------------------------
 * consume_lucky_star() — Advance PRNG for Lucky Star phase.
 *
 * Always consumes exactly 2 calls (N11 / Appendix A invariant). Phase 4:
 * bonus damage is not wired (no perk ownership system yet). The calls keep
 * both BLE peers' PRNG streams synchronized regardless of perk ownership.
 *
 * ADVISORY ADV-P4-01: Phase 5 item engine must hook here to check perk
 * ownership and apply bonus d6 on trigger when ls_roll <= 5.
 * ---------------------------------------------------------------------------*/
static void consume_lucky_star(fq_prng_t *rng)
{
    fq_prng_range(rng, 1u, 100u); /* F1 Lucky Star check */
    fq_prng_range(rng, 1u, 100u); /* F2 Lucky Star check */
}

/* ---------------------------------------------------------------------------
 * fq_combat_step
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

    /* --- Attack 1: first → second ---------------------------------------- */
    {
        uint8_t hit, crit, rr;
        int8_t  dmg;
        resolve_attack(&ctx->rng, first, second, &hit, &crit, &rr, &dmg);

        /* Write into the always-F1/F2 result fields. */
        if (ctx->first_attacker == 1u) {
            result.f1_hit = hit; result.f1_crit = crit;
            result.f1_rerolled = rr; result.f1_damage_dealt = dmg;
        } else {
            result.f2_hit = hit; result.f2_crit = crit;
            result.f2_rerolled = rr; result.f2_damage_dealt = dmg;
        }
    }

    /* N6: KO check after attack 1 — no counter-attack. */
    if (second->hp <= 0) {
        ctx->finished = 1u;
        ctx->winner   = ctx->first_attacker;
        consume_lucky_star(&ctx->rng); /* PRNG sync invariant — always consumed */
        goto done;
    }

    /* --- Attack 2: second → first ---------------------------------------- */
    {
        uint8_t hit, crit, rr;
        int8_t  dmg;
        resolve_attack(&ctx->rng, second, first, &hit, &crit, &rr, &dmg);

        if (ctx->first_attacker == 1u) {
            result.f2_hit = hit; result.f2_crit = crit;
            result.f2_rerolled = rr; result.f2_damage_dealt = dmg;
        } else {
            result.f1_hit = hit; result.f1_crit = crit;
            result.f1_rerolled = rr; result.f1_damage_dealt = dmg;
        }
    }

    /* KO check after attack 2. */
    if (first->hp <= 0) {
        ctx->finished = 1u;
        ctx->winner   = (ctx->first_attacker == 1u) ? 2u : 1u;
        consume_lucky_star(&ctx->rng); /* PRNG sync invariant */
        goto done;
    }

    /* --- Lucky Star phase (N11): always 2 PRNG calls ---------------------- */
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
