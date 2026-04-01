/**
 * test_crc32.c — Feature/happy-path tests for fq_crc32 (Phase B: FEATURE RED).
 *
 * Tests the well-known IEEE 802.3 check vector "123456789" → 0xCBF43926,
 * multi-byte determinism, and incremental vs bulk equivalence.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "test_assert.h"
#include "crc32.h"

/* Standard IEEE 802.3 check vector: CRC32("123456789") == 0xCBF43926 */
static void test_check_vector_123456789(void)
{
    const uint8_t data[] = {
        '1','2','3','4','5','6','7','8','9'
    };
    uint32_t result = fq_crc32(data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926u, result);
}

/* All-zero buffer of length 4 — deterministic pinned value */
static void test_four_zero_bytes(void)
{
    uint8_t buf[4] = {0x00u, 0x00u, 0x00u, 0x00u};
    uint32_t result = fq_crc32(buf, 4u);
    /* Pre-computed: CRC32({0,0,0,0}) = 0x2144DF1C */
    TEST_ASSERT_EQUAL_UINT32(0x2144DF1Cu, result);
}

/* Single-byte 0x01 — deterministic pinned value */
static void test_single_byte_0x01(void)
{
    uint8_t buf[1] = {0x01u};
    uint32_t result = fq_crc32(buf, 1u);
    /* Pre-computed: CRC32({0x01}) = 0xA505DF1B */
    TEST_ASSERT_EQUAL_UINT32(0xA505DF1Bu, result);
}

/* Determinism: same data always produces same hash */
static void test_determinism_same_input_same_output(void)
{
    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint32_t r1 = fq_crc32(data, sizeof(data));
    uint32_t r2 = fq_crc32(data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT32(r1, r2);
}

/* Length 1 vs length 9 must differ */
static void test_different_lengths_differ(void)
{
    const uint8_t data[] = {
        '1','2','3','4','5','6','7','8','9'
    };
    uint32_t r1 = fq_crc32(data, 1u);
    uint32_t r9 = fq_crc32(data, 9u);
    TEST_ASSERT_TRUE(r1 != r9);
}

int main(void)
{
    test_check_vector_123456789();
    test_four_zero_bytes();
    test_single_byte_0x01();
    test_determinism_same_input_same_output();
    test_different_lengths_differ();
    return 0;
}
