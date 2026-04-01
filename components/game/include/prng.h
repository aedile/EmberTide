/**
 * prng.h — FiestaQuest Frozen XOR-shift PRNG
 *
 * Implements a deterministic 32-bit XOR-shift pseudo-random number generator
 * used exclusively inside the game engine for combat and item resolution.
 *
 * Constitution Priority 0: This PRNG is the ONLY entropy source permitted
 * inside components/game/. No <time.h>, no hardware RNG, no external entropy.
 *
 * Algorithm: xorshift32 with shifts <<13, >>17, <<5.
 * Reference: G. Marsaglia, "Xorshift RNGs" (2003).
 *
 * Properties:
 *   - Period: 2^32 - 1 (never visits state 0)
 *   - Fully deterministic given an initial seed
 *   - State is plain struct — trivially serializable for save/resume and
 *     BLE synchronization
 *   - Modulo bias is intentional and documented (see fq_prng_range)
 */

#ifndef FIESTAQUEST_PRNG_H
#define FIESTAQUEST_PRNG_H

#include <stdint.h>

/**
 * fq_prng_t — PRNG state container.
 *
 * Contains a single 32-bit state word. State must NEVER be 0 — the XOR-shift
 * algorithm degenerates to the zero fixed-point if state reaches 0.
 * fq_prng_init() enforces this invariant; fq_prng_next() re-enforces it as a
 * defensive guard.
 */
typedef struct {
    uint32_t state; /**< Current PRNG state. Must always be non-zero. */
} fq_prng_t;

/**
 * fq_prng_init() — Initialize the PRNG with a seed.
 *
 * @param rng   Pointer to the fq_prng_t to initialize. Must not be NULL.
 * @param seed  Initial seed value. If seed == 0, state is set to 1 to avoid
 *              the zero-state deadlock.
 */
void fq_prng_init(fq_prng_t *rng, uint32_t seed);

/**
 * fq_prng_next() — Advance the PRNG and return the next value.
 *
 * Applies the frozen xorshift32 steps: state ^= state<<13;
 * state ^= state>>17; state ^= state<<5.
 *
 * Defensive guard: if state is 0 on entry (e.g., uninitialized struct via
 * memset to zero), state is forced to 1 before the shift sequence. This
 * prevents the zero-fixed-point deadlock and ensures the function never
 * returns 0.
 *
 * @param rng  Pointer to an initialized fq_prng_t. Must not be NULL.
 * @return     Next pseudo-random 32-bit value (guaranteed non-zero).
 */
uint32_t fq_prng_next(fq_prng_t *rng);

/**
 * fq_prng_range() — Return a value in [min, max] inclusive.
 *
 * Uses modulo reduction: min + (fq_prng_next() % (max - min + 1)).
 * Modulo bias is intentional and accepted per architecture doc v2.
 *
 * Edge cases:
 *   - min > max : returns min (defensive, no state advancement).
 *   - min == max: returns min (no state advancement).
 *   - max == UINT32_MAX and min == 0: span overflows to 0; returns
 *     fq_prng_next() directly (full 32-bit range, no modulo).
 *
 * @param rng  Pointer to an initialized fq_prng_t.
 * @param min  Lower bound (inclusive).
 * @param max  Upper bound (inclusive).
 * @return     Value in [min, max].
 */
uint32_t fq_prng_range(fq_prng_t *rng, uint32_t min, uint32_t max);

#endif /* FIESTAQUEST_PRNG_H */
