/**
 * test_sanity.c
 *
 * Phase B feature test: sanity check.
 *
 * Verifies that the host test framework is wired correctly:
 *   - The executable compiles without ESP-IDF dependencies.
 *   - Basic C assertions hold (1 == 1).
 *   - Exit 0 means "green" in CTest.
 *
 * Per spec Item 2 acceptance criteria: a dummy test executes and passes.
 */

#include <stdio.h>
#include <assert.h>
#include <stdint.h>

int main(void)
{
    /* Specific value assertion (required per Constitution Priority 4). */
    uint32_t a = 1u;
    uint32_t b = 1u;
    assert(a == b);

    /* Arithmetic sanity — no floating point allowed anywhere. */
    uint32_t sum = 100u + 200u;
    assert(sum == 300u);

    /* Bit-width sanity. */
    _Static_assert(sizeof(uint8_t)  == 1u, "uint8_t must be 1 byte");
    _Static_assert(sizeof(uint16_t) == 2u, "uint16_t must be 2 bytes");
    _Static_assert(sizeof(uint32_t) == 4u, "uint32_t must be 4 bytes");

    printf("test_sanity: PASS\n");
    return 0;
}
