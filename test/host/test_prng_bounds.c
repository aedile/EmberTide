/**
 * test_prng_bounds.c — Bound/negative tests for fq_prng (Phase A: BOUND RED).
 *
 * These tests MUST compile and run as bound constraint checks BEFORE any feature
 * tests. They cover: zero-seed deadlock, min>max defensive return, min==max
 * identity, full-range overflow guard, never-returns-zero, state serialization.
 *
 * Rule 22: BOUND RED → FEATURE RED → GREEN → REFACTOR.
 */

#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "test_assert.h"
#include "prng.h"

/* PRNG-NT-2: seed=0 must be treated as seed=1 and produce non-zero output. */
static void test_zero_seed_not_deadlocked(void)
{
    fq_prng_t rng;
    fq_prng_init(&rng, 0);
    /* State must have been forced to 1 — never 0 */
    TEST_ASSERT_EQUAL_UINT32(1u, rng.state);
    /* First value must equal xorshift32(1) — deterministic and non-zero */
    uint32_t v = fq_prng_next(&rng);
    TEST_ASSERT_EQUAL_UINT32(0x00042021u, v);
}

/* PRNG-NT-1: min > max — defensive return of min. */
static void test_range_min_greater_than_max(void)
{
    fq_prng_t rng;
    fq_prng_init(&rng, 1);
    uint32_t result = fq_prng_range(&rng, 10u, 5u);
    TEST_ASSERT_EQUAL_UINT32(10u, result);
}

/* PRNG-NT-2 (range): min == max — must return exactly min. */
static void test_range_min_equals_max(void)
{
    fq_prng_t rng;
    fq_prng_init(&rng, 1);
    uint32_t result = fq_prng_range(&rng, 7u, 7u);
    TEST_ASSERT_EQUAL_UINT32(7u, result);
}

/* PRNG-NT-3: max - min + 1 overflows to 0 (full range) — must return
 * fq_prng_next() directly, not divide by zero. */
static void test_range_full_uint32_no_div_zero(void)
{
    fq_prng_t rng;
    fq_prng_init(&rng, 1);
    /* min=0, max=UINT32_MAX: span = UINT32_MAX - 0 + 1 = 0 (overflow) */
    uint32_t result = fq_prng_range(&rng, 0u, UINT32_MAX);
    /* Any 32-bit value is valid — just must not hang or trap */
    (void)result;
    /* Second call must produce a different deterministic value */
    fq_prng_init(&rng, 1);
    uint32_t r1 = fq_prng_range(&rng, 0u, UINT32_MAX);
    fq_prng_init(&rng, 1);
    uint32_t r2 = fq_prng_range(&rng, 0u, UINT32_MAX);
    /* Must be deterministic: same seed → same result */
    TEST_ASSERT_EQUAL_UINT32(r1, r2);
}

/* PRNG-NT-6: 100,000 iterations from seed=1 must NEVER produce 0. */
static void test_never_returns_zero(void)
{
    fq_prng_t rng;
    fq_prng_init(&rng, 1);
    for (uint32_t i = 0u; i < 100000u; i++) {
        uint32_t v = fq_prng_next(&rng);
        TEST_ASSERT_TRUE(v != 0u);
    }
}

/* PRNG-NT-4: State serialization round-trip.
 * Run 50 values, capture state, run 10 more, restore state, run 10 more,
 * assert identical sequence. */
static void test_state_serialization_round_trip(void)
{
    fq_prng_t rng;
    fq_prng_init(&rng, 42u);

    /* Advance 50 steps */
    for (int i = 0; i < 50; i++) {
        fq_prng_next(&rng);
    }

    /* Snapshot state */
    fq_prng_t saved;
    memcpy(&saved, &rng, sizeof(fq_prng_t));

    /* Collect 10 values from original */
    uint32_t original[10];
    for (int i = 0; i < 10; i++) {
        original[i] = fq_prng_next(&rng);
    }

    /* Restore from snapshot and collect 10 values */
    uint32_t restored[10];
    for (int i = 0; i < 10; i++) {
        restored[i] = fq_prng_next(&saved);
    }

    /* Both sequences must be identical */
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_UINT32(original[i], restored[i]);
    }
}

/* PRNG-NT-6 (DET-NT-6 variant): Uninitialized struct behavior documentation.
 * Calling fq_prng_next() on a zero-memset struct should not crash; the
 * function's internal guard against state==0 must fire. */
static void test_uninit_struct_zero_guard(void)
{
    fq_prng_t rng;
    memset(&rng, 0x00, sizeof(fq_prng_t));
    /* state is 0 — next() must handle this gracefully (state→1 or similar) */
    uint32_t v = fq_prng_next(&rng);
    /* Must not return 0 — the guard must have activated */
    TEST_ASSERT_TRUE(v != 0u);
}

/* PRNG-NT-7: Distribution fairness — no catastrophic modulo bias.
 *
 * Seed=1, call fq_prng_range(&rng, 0u, 10u) 100,000 times.
 * Count occurrences in 11 buckets (values 0-10).
 * Fair share = 100,000 / 11 ≈ 9090.
 * Assert each bucket falls in [8090, 10090] (fair share ± ~10%).
 * A catastrophic bias (e.g., one value appearing 50%+ of the time due to a
 * modulo bug) would be caught immediately; minor statistical modulo bias
 * (accepted per architecture doc v2) passes this check easily. */
static void test_range_distribution_no_catastrophic_bias(void)
{
    fq_prng_t rng;
    fq_prng_init(&rng, 1u);

    uint32_t buckets[11];
    for (int i = 0; i < 11; i++) {
        buckets[i] = 0u;
    }

    for (uint32_t i = 0u; i < 100000u; i++) {
        uint32_t val = fq_prng_range(&rng, 0u, 10u);
        buckets[val]++;
    }

    for (int i = 0; i < 11; i++) {
        TEST_ASSERT_TRUE(buckets[i] >= 8090u);
        TEST_ASSERT_TRUE(buckets[i] <= 10090u);
    }
}

int main(void)
{
    test_zero_seed_not_deadlocked();
    test_range_min_greater_than_max();
    test_range_min_equals_max();
    test_range_full_uint32_no_div_zero();
    test_never_returns_zero();
    test_state_serialization_round_trip();
    test_uninit_struct_zero_guard();
    test_range_distribution_no_catastrophic_bias();
    return 0;
}
