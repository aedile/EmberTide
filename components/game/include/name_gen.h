/**
 * name_gen.h — FiestaQuest Game Engine: Two-Part Name Generator
 *
 * Generates deterministic two-part creature names using a frozen PRNG.
 * Names are composed of a prefix word and a suffix word drawn from two
 * frozen word tables. The generator produces names that are:
 *   - <= 11 characters (fits name[12] field in fq_character_t)
 *   - Always non-empty
 *   - Always null-terminated
 *
 * Architecture constraint: this header MUST NOT include hal_*.h,
 * presentation/, or connectivity/ headers.
 *
 * Constitution Priority 0: Pure function. No malloc. No time/entropy.
 * All randomness must come from the caller-provided fq_prng_t.
 *
 * Phase-19 addition.
 */

#ifndef FIESTAQUEST_GAME_NAME_GEN_H
#define FIESTAQUEST_GAME_NAME_GEN_H

#include <stddef.h>
#include "types.h"
#include "prng.h"

/**
 * fq_generate_name() — Generate a two-part creature name.
 *
 * Picks one word from the prefix table and one from the suffix table using
 * the provided PRNG. Concatenates them into @p out. The combined length is
 * guaranteed <= 11 characters. The output is always null-terminated.
 *
 * If @p max_len == 0: returns GAME_ERR_INVALID, @p out is unchanged.
 * If @p rng == NULL or @p out == NULL: returns GAME_ERR_NULL_PTR.
 *
 * @param rng      Seeded PRNG (advances state by exactly 2 calls).
 * @param out      Output buffer. Must be at least @p max_len bytes.
 * @param max_len  Buffer size including null terminator (12 recommended).
 * @return         GAME_OK, GAME_ERR_NULL_PTR, or GAME_ERR_INVALID.
 */
game_err_t fq_generate_name(fq_prng_t *rng, char *out, size_t max_len);

#endif /* FIESTAQUEST_GAME_NAME_GEN_H */
