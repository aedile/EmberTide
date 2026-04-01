/**
 * test_progression.c — Feature/happy-path tests for fq_effective_stat
 * (Phase B: FEATURE RED).
 *
 * Tests exact pinned values at key raw inputs from the frozen 256-entry
 * lookup table: raw=0→0, raw=1→3, raw=10→10, raw=50→16, raw=100→19,
 * raw=200→22, raw=255→23.
 *
 * Formula used to generate the table (offline only, NOT in C code):
 *   round(10 * ln(raw + 1) / ln(11))
 */

#include <stdint.h>
#include "test_assert.h"
#include "progression.h"

static void test_raw_0_is_0(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, fq_effective_stat(0u));
}

static void test_raw_1_is_3(void)
{
    TEST_ASSERT_EQUAL_UINT8(3u, fq_effective_stat(1u));
}

static void test_raw_10_is_10(void)
{
    TEST_ASSERT_EQUAL_UINT8(10u, fq_effective_stat(10u));
}

static void test_raw_50_is_16(void)
{
    TEST_ASSERT_EQUAL_UINT8(16u, fq_effective_stat(50u));
}

static void test_raw_100_is_19(void)
{
    TEST_ASSERT_EQUAL_UINT8(19u, fq_effective_stat(100u));
}

static void test_raw_200_is_22(void)
{
    TEST_ASSERT_EQUAL_UINT8(22u, fq_effective_stat(200u));
}

static void test_raw_255_is_23(void)
{
    TEST_ASSERT_EQUAL_UINT8(23u, fq_effective_stat(255u));
}

/* Result must always be <= 23 (logarithm is bounded for uint8_t input) */
static void test_max_effective_is_23(void)
{
    for (int i = 0; i < 256; i++) {
        uint8_t v = fq_effective_stat((uint8_t)i);
        TEST_ASSERT_TRUE(v <= 23u);
    }
}

int main(void)
{
    test_raw_0_is_0();
    test_raw_1_is_3();
    test_raw_10_is_10();
    test_raw_50_is_16();
    test_raw_100_is_19();
    test_raw_200_is_22();
    test_raw_255_is_23();
    test_max_effective_is_23();
    return 0;
}
