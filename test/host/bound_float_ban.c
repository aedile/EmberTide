/**
 * bound_float_ban.c — Float ban boundary test (DET-NT-5).
 *
 * BOUNDARY VIOLATION TEST — must NOT compile successfully.
 *
 * This file attempts to use floating-point math by calling sin(1.0) WITHOUT
 * including <math.h>. Constitution Priority 0 forbids ALL floating-point
 * operations in component code. This boundary test proves the compiler/CMake
 * setup enforces the ban.
 *
 * Expected outcome: compilation FAILS because sin() is called without a
 * declaration. Under -Wall -Werror -Wimplicit-function-declaration, an
 * undeclared function call is a hard error, causing the compile to fail as
 * required. check_boundary.sh exits 0 when compile fails.
 *
 * Mechanism: sin() is intentionally NOT declared (no #include <math.h>).
 * The -Wimplicit-function-declaration flag (enforced via -Wall -Werror in
 * check_boundary.sh and the assert_compile_fails CMake macro) promotes the
 * implicit declaration warning to a hard error, guaranteeing the file cannot
 * compile successfully. This proves that float-using code is rejected at the
 * toolchain level before it could ever reach a game component.
 */

/* Intentionally calling undeclared function to force -Wimplicit-function-declaration */
int main(void)
{
    /* sin is not declared — under -Wall -Werror -Wimplicit-function-declaration
     * this is a hard compile error, proving the float ban is enforced. */
    double result = sin(1.0);
    (void)result;
    return 0;
}
