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
    /* Replace zero seed with 1 to prevent the zero fixed-point deadlock.
     * xorshift32(0) == 0 forever — this guard is the first line of defense. */
    rng->state = (seed != 0u) ? seed : 1u;
}

uint32_t fq_prng_next(fq_prng_t *rng)
{
    /* Defensive second line: if state arrives as 0 (e.g., from a memset-to-zero
     * struct that was never passed through fq_prng_init), force it to 1 before
     * applying the shift sequence. This guarantees the return value is non-zero
     * for all inputs, satisfying the PRNG-NT-6 never-returns-zero contract. */
    if (rng->state == 0u) {
        rng->state = 1u;
    }

    /* Frozen xorshift32 — shifts MUST NOT change without updating all
     * pinned sequence literals in test_prng.c and the cross-device protocol. */
    uint32_t x = rng->state;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x <<  5u;
    rng->state = x;
    return x;
}

uint32_t fq_prng_range(fq_prng_t *rng, uint32_t min, uint32_t max)
{
    /* Defensive: invalid range — return min without consuming entropy. */
    if (min >= max) {
        return min;
    }

    uint32_t span = max - min + 1u;

    /* Overflow guard: max==UINT32_MAX, min==0 gives span==0.
     * Return the raw next value (full 32-bit range) to avoid division by zero. */
    if (span == 0u) {
        return fq_prng_next(rng);
    }

    /* Intentional modulo bias — accepted per architecture doc v2. */
    return min + (fq_prng_next(rng) % span);
}
