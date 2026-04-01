/**
 * bound_float_ban.c — Float ban boundary test (DET-NT-5).
 *
 * BOUNDARY VIOLATION TEST — must NOT compile successfully.
 *
 * This file attempts to use floating-point math (<math.h> and sin(1.0)).
 * Constitution Priority 0 forbids ALL floating-point operations in component
 * code. This boundary test proves the compiler/CMake setup enforces the ban.
 *
 * Expected outcome: compilation FAILS (no <math.h> reachable OR -Werror=float
 * flags cause failure). check_boundary.sh exits 0 when compile fails.
 *
 * Strategy: Declare sin() without including <math.h>, then call it with a
 * double literal. The -Wall -Werror flags will cause "implicit declaration" or
 * the -Wdouble-promotion / float usage warning to become an error, failing the
 * compile as required.
 */

/* Intentional: include math.h to pull in floating-point functions.
 * The -Wfloat-conversion -Werror pair (or the explicit -mno-sse float-disable
 * on some embedded toolchains) must reject this. For the host test harness,
 * we rely on the fact that this file is compiled WITHOUT the game include
 * paths and WITH -Wdouble-promotion -Werror (set in check_boundary.sh via
 * the BOUNDARY_FLOAT_BAN_CFLAGS environment variable).
 *
 * The definitive enforcement: this file is compiled by check_boundary.sh
 * with the additional flag: -Werror=double-promotion -pedantic-errors
 * AND we use an implicit declaration of sin() (without math.h include) to
 * trigger a -Wimplicit-function-declaration error under -Wall -Werror. */

/* Intentionally calling undeclared function to force -Wimplicit-function-declaration */
int main(void)
{
    /* sin is not declared — under -Wall -Werror this is a hard error */
    double result = sin(1.0);
    (void)result;
    return 0;
}
