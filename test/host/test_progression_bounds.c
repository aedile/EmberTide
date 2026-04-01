/**
 * test_progression_bounds.c — Bound/negative tests for fq_effective_stat
 * (Phase A: BOUND RED).
 *
 * Covers: monotonicity of all 256 entries, boundary pins (raw=0, raw=1,
 * raw=255), purity (idempotent), and out-of-bounds cast safety.
 *
 * Rule 22: BOUND RED → FEATURE RED → GREEN → REFACTOR.
 */

#include <stdint.h>
#include <inttypes.h>
#include "test_assert.h"
#include "progression.h"

/* STAT-NT-1: Monotonicity — table[i] >= table[i-1] for all i in 1..255. */
static void test_monotonicity_all_256(void)
{
    for (int i = 1; i < 256; i++) {
        uint8_t prev = fq_effective_stat((uint8_t)(i - 1));
        uint8_t curr = fq_effective_stat((uint8_t)i);
        TEST_ASSERT_TRUE(curr >= prev);
    }
}

/* STAT-NT-2: Pin raw=255 — must equal 23 (formula ceiling for uint8_t domain). */
static void test_pin_raw_255(void)
{
    uint8_t result = fq_effective_stat(255u);
    TEST_ASSERT_EQUAL_UINT8(23u, result);
}

/* STAT-NT-3: Pin raw=1 — must be non-zero (logarithm of 2 > 0). */
static void test_pin_raw_1_nonzero(void)
{
    uint8_t result = fq_effective_stat(1u);
    TEST_ASSERT_EQUAL_UINT8(3u, result);
}

/* raw=0 boundary — must equal 0 (ln(0+1) == 0). */
static void test_pin_raw_0_is_zero(void)
{
    uint8_t result = fq_effective_stat(0u);
    TEST_ASSERT_EQUAL_UINT8(0u, result);
}

/* STAT-NT-4: Purity — same input always yields same output (idempotent). */
static void test_purity_same_result_on_repeat(void)
{
    for (int raw = 0; raw < 256; raw++) {
        uint8_t r1 = fq_effective_stat((uint8_t)raw);
        uint8_t r2 = fq_effective_stat((uint8_t)raw);
        TEST_ASSERT_EQUAL_UINT8(r1, r2);
    }
}

/* Out-of-bounds cast safety: casting (int8_t)-1 to uint8_t yields 255.
 * Verify that fq_effective_stat(255) hits the last valid element without
 * any memory violation (this is a run-time observable safety check). */
static void test_negative_cast_to_255_is_safe(void)
{
    int8_t negative_one = -1;
    uint8_t cast_result = (uint8_t)negative_one; /* Must be 255 */
    TEST_ASSERT_EQUAL_UINT8(255u, cast_result);

    uint8_t stat = fq_effective_stat(cast_result);
    /* Must equal the pinned value at 255 */
    TEST_ASSERT_EQUAL_UINT8(23u, stat);
}

int main(void)
{
    test_pin_raw_0_is_zero();
    test_pin_raw_1_nonzero();
    test_pin_raw_255();
    test_monotonicity_all_256();
    test_purity_same_result_on_repeat();
    test_negative_cast_to_255_is_safe();
    return 0;
}
