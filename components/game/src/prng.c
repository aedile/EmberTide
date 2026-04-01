/**
 * prng.c — FiestaQuest Frozen XOR-shift PRNG implementation.
 *
 * See prng.h for contract, algorithm reference, and usage notes.
 *
 * Constitution Priority 0: No floating point. No <time.h>. No external entropy.
 */

#include "prng.h"

void fq_prng_init(fq_prng_t *rng, uint32_t seed)
{
    rng->state = (seed != 0u) ? seed : 1u;
}

uint32_t fq_prng_next(fq_prng_t *rng)
{
    /* Defensive guard: if state is somehow 0 (e.g., uninitialized struct),
     * force to 1 before applying the shift sequence. This prevents the
     * zero fixed-point deadlock and guarantees the return value is non-zero. */
    if (rng->state == 0u) {
        rng->state = 1u;
    }

    uint32_t x = rng->state;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    rng->state = x;
    return x;
}

uint32_t fq_prng_range(fq_prng_t *rng, uint32_t min, uint32_t max)
{
    /* Defensive: min > max — return min without advancing state */
    if (min > max) {
        return min;
    }

    /* min == max — return constant without advancing state */
    if (min == max) {
        return min;
    }

    uint32_t span = max - min + 1u;

    /* span == 0 means max - min + 1 overflowed (i.e., max==UINT32_MAX, min==0).
     * Return the raw next value directly (full 32-bit range, no modulo). */
    if (span == 0u) {
        return fq_prng_next(rng);
    }

    return min + (fq_prng_next(rng) % span);
}
