/**
 * name_gen.c — FiestaQuest Two-Part Creature Name Generator
 *
 * Word tables are frozen constants — never modified at runtime.
 * Each prefix is 1-5 chars. Each suffix is 1-5 chars.
 * The longest possible combination is 5+5 = 10 chars + null = 11 bytes,
 * which fits the 12-byte name field.
 *
 * Prefix table: 16 entries, each <= 5 chars.
 * Suffix table: 16 entries, each <= 5 chars.
 *
 * PRNG usage: exactly 2 calls to fq_prng_next() — one for prefix index,
 * one for suffix index. PRNG stream advances by exactly 2 per name.
 *
 * Constitution Priority 0: No float, no malloc, no time/entropy.
 */

#include "name_gen.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Frozen word tables.
 *
 * Prefix: evocative first syllables, each <= 5 chars.
 * Suffix: creature-flavoured endings, each <= 5 chars.
 * Combined max = 5 + 5 = 10 chars (null at position 10).
 *
 * 16 entries each → index via (prng % 16) — no modulo-bias concern
 * for display purposes (names are cosmetic, not game-logic deterministic).
 * ---------------------------------------------------------------------------*/

#define NAME_TABLE_SIZE   16u
#define MAX_WORD_LEN       5u  /**< max chars per word (no null included). */

static const char * const k_prefixes[NAME_TABLE_SIZE] = {
    "Brim", "Cind", "Dusk", "Emba", "Flar",
    "Glow", "Haze", "Igni", "Jolt", "Krag",
    "Lava", "Murk", "Nyte", "Orin", "Pyre",
    "Quor"
};

static const char * const k_suffixes[NAME_TABLE_SIZE] = {
    "ax",   "bel",  "cor",  "dox",  "era",
    "fin",  "gal",  "hox",  "ira",  "jin",
    "kel",  "lix",  "mor",  "nex",  "ora",
    "pax"
};

/* Validate table entries fit within MAX_WORD_LEN at compile time via
 * a _Static_assert that would be impractical for each string. Instead,
 * the bound test loop (test_p19_interactive_bounds.c, TEST 2) validates
 * the 1000-name output guarantee, which transitively validates the tables. */

/* ---------------------------------------------------------------------------
 * fq_generate_name
 * ---------------------------------------------------------------------------*/
game_err_t fq_generate_name(fq_prng_t *rng, char *out, size_t max_len)
{
    if (rng == NULL || out == NULL) {
        return GAME_ERR_NULL_PTR;
    }
    if (max_len == 0u) {
        return GAME_ERR_INVALID;
    }

    /* Draw two indices via PRNG. Exactly 2 fq_prng_next() calls. */
    uint32_t pri = fq_prng_next(rng) % NAME_TABLE_SIZE;
    uint32_t sui = fq_prng_next(rng) % NAME_TABLE_SIZE;

    const char *prefix = k_prefixes[pri];
    const char *suffix = k_suffixes[sui];

    /* Compute lengths safely. */
    size_t plen = strnlen(prefix, MAX_WORD_LEN);
    size_t slen = strnlen(suffix, MAX_WORD_LEN);
    size_t total = plen + slen; /* max 10, fits in uint8_t */

    /* Clamp total to max_len - 1 (leave room for null terminator). */
    if (total >= max_len) {
        total = max_len - 1u;
        /* Recalculate slen so it fits. */
        if (plen >= total) {
            plen  = total;
            slen  = 0u;
        } else {
            slen = total - plen;
        }
    }

    /* Copy prefix. */
    size_t i = 0u;
    for (size_t k = 0u; k < plen && i < max_len - 1u; k++, i++) {
        out[i] = prefix[k];
    }

    /* Append suffix. */
    for (size_t k = 0u; k < slen && i < max_len - 1u; k++, i++) {
        out[i] = suffix[k];
    }

    /* Null-terminate — always, at every reachable position. */
    out[i] = '\0';

    /* Belt-and-suspenders: force null at max_len - 1 regardless. */
    out[max_len - 1u] = '\0';

    return GAME_OK;
}
