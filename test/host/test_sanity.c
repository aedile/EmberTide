/**
 * test_sanity.c
 *
 * Phase 1 feature test: sanity check for the host test framework.
 *
 * Verifies:
 *   - The executable compiles without ESP-IDF dependencies.
 *   - Basic arithmetic holds (no floating-point anywhere).
 *   - Bit-width sizes are correct for the target ABI.
 *   - All game_err_t enum values have the exact expected integer values
 *     (A1: prevents silent renumbering breaking the save/wire format).
 *   - Exit 0 means "green" in CTest.
 *
 * Per spec Item 2 acceptance criteria: a dummy test executes and passes.
 * Per review advisory A1: game_err_t values are explicitly asserted.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include "test_assert.h"
#include "types.h"

int main(void)
{
    /* --- Arithmetic sanity (no floating point allowed anywhere) --- */
    uint32_t a = 1u;
    uint32_t b = 1u;
    TEST_ASSERT_EQUAL_UINT32(1u, a);
    TEST_ASSERT_EQUAL_UINT32(1u, b);
    TEST_ASSERT_TRUE(a == b);

    uint32_t sum = 100u + 200u;
    TEST_ASSERT_EQUAL_UINT32(300u, sum);

    /* --- Bit-width sanity --- */
    /* Note: _Static_assert fires at compile time; no runtime macro needed. */
    _Static_assert(sizeof(uint8_t)  == 1u, "uint8_t must be 1 byte");
    _Static_assert(sizeof(uint16_t) == 2u, "uint16_t must be 2 bytes");
    _Static_assert(sizeof(uint32_t) == 4u, "uint32_t must be 4 bytes");

    TEST_ASSERT_EQUAL_UINT8(1u,  (uint8_t)sizeof(uint8_t));
    TEST_ASSERT_EQUAL_UINT8(2u,  (uint8_t)sizeof(uint16_t));
    TEST_ASSERT_EQUAL_UINT8(4u,  (uint8_t)sizeof(uint32_t));

    /* --- A1: game_err_t enum values must match the wire/save format --- */
    TEST_ASSERT_EQUAL_INT(0, (int)GAME_OK);
    TEST_ASSERT_EQUAL_INT(1, (int)GAME_ERR_NULL_PTR);
    TEST_ASSERT_EQUAL_INT(2, (int)GAME_ERR_OVERFLOW);
    TEST_ASSERT_EQUAL_INT(3, (int)GAME_ERR_UNDERFLOW);
    TEST_ASSERT_EQUAL_INT(4, (int)GAME_ERR_DIV_ZERO);
    TEST_ASSERT_EQUAL_INT(5, (int)GAME_ERR_INVALID);

    printf("test_sanity: PASS\n");
    return 0;
}
