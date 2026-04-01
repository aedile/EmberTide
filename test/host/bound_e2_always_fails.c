/**
 * bound_e2_always_fails.c
 *
 * FAILURE ESCALATION TEST — this test MUST fail (WILL_FAIL TRUE).
 *
 * Purpose: prove that CTest returns a non-zero exit code when a test
 * executable exits non-zero. If this test were to "pass" (exit 0), the
 * CI pipeline would be reporting false greens and must be considered broken.
 *
 * Per spec Item 2 negative test requirement (E2):
 *   CTest failure escalation — a failing ctest MUST return non-zero.
 *
 * Uses explicit return 1 (not assert) so ctest sees a non-zero exit code
 * rather than a signal, ensuring WILL_FAIL TRUE works on all platforms.
 */

#include <stdio.h>
#include <stdint.h>

int main(void)
{
    printf("bound_e2: intentional failure — proving ctest catches non-zero exit\n");

    /* Specific value assertion style: compute something wrong on purpose. */
    uint32_t expected = 1u;
    uint32_t actual   = 2u;  /* deliberately wrong */

    if (actual != expected) {
        printf("bound_e2: FAIL — expected %u got %u (intentional)\n",
               expected, actual);
        /* Return non-zero so CTest WILL_FAIL TRUE marks this as "passed". */
        return 1;
    }

    /* If we somehow reach here, the test has a logic error itself. */
    printf("bound_e2: ERROR — should have returned 1 above\n");
    return 2;
}
