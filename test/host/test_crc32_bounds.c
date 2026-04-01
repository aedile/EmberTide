/**
 * test_crc32_bounds.c — Bound/negative tests for fq_crc32 (Phase A: BOUND RED).
 *
 * Covers: NULL pointer guard, empty buffer, single-byte vectors, endianness
 * canary, and NULL+SIZE_MAX (null guard must fire before loop).
 *
 * Rule 22: BOUND RED → FEATURE RED → GREEN → REFACTOR.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "test_assert.h"
#include "crc32.h"

/* CRC32-NT-4: NULL pointer — must return 0, not crash. */
static void test_null_pointer_returns_zero(void)
{
    uint32_t result = fq_crc32(NULL, 100u);
    TEST_ASSERT_EQUAL_UINT32(0u, result);
}

/* CRC32-NT-4 (SIZE_MAX variant): NULL + SIZE_MAX — null guard fires first. */
static void test_null_with_size_max_returns_zero(void)
{
    uint32_t result = fq_crc32(NULL, SIZE_MAX);
    TEST_ASSERT_EQUAL_UINT32(0u, result);
}

/* Empty buffer: len=0 — must return 0x00000000. */
static void test_empty_buffer_returns_zero(void)
{
    uint8_t buf[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint32_t result = fq_crc32(buf, 0u);
    TEST_ASSERT_EQUAL_UINT32(0x00000000u, result);
}

/* CRC32-NT-2: Single byte 0x00 — must equal 0xD202EF8D. */
static void test_single_byte_0x00(void)
{
    uint8_t buf[1] = {0x00};
    uint32_t result = fq_crc32(buf, 1u);
    TEST_ASSERT_EQUAL_UINT32(0xD202EF8Du, result);
}

/* CRC32-NT-2: Single byte 0xFF — must equal 0xFF000000. */
static void test_single_byte_0xFF(void)
{
    uint8_t buf[1] = {0xFF};
    uint32_t result = fq_crc32(buf, 1u);
    TEST_ASSERT_EQUAL_UINT32(0xFF000000u, result);
}

/* CRC32-NT-3: Endianness canary.
 * Hash the 4 bytes of 0x12345678 in little-endian order (bytes: 0x78, 0x56,
 * 0x34, 0x12) and assert the exact result. This pins the byte-processing order
 * so that cross-device endianness mismatches are caught immediately. */
static void test_endianness_canary(void)
{
    /* Little-endian representation of 0x12345678 */
    uint8_t le_bytes[4] = {0x78u, 0x56u, 0x34u, 0x12u};
    uint32_t result = fq_crc32(le_bytes, 4u);
    /* Expected: 0xAF6D87D2 (pre-computed with Python reference implementation) */
    TEST_ASSERT_EQUAL_UINT32(0xAF6D87D2u, result);
}

int main(void)
{
    test_null_pointer_returns_zero();
    test_null_with_size_max_returns_zero();
    test_empty_buffer_returns_zero();
    test_single_byte_0x00();
    test_single_byte_0xFF();
    test_endianness_canary();
    return 0;
}
