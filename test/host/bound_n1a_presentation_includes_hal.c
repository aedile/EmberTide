/**
 * bound_n1a_presentation_includes_hal.c
 *
 * BOUNDARY VIOLATION TEST — must NOT compile successfully.
 *
 * This file simulates a presentation/ module attempting to include hal_*.h.
 * Compiled as a separate CMake target with WILL_FAIL TRUE.
 *
 * Per spec-challenger finding N1a:
 *   presentation/ MUST NOT depend on hal/.
 */

#include "hal_spi.h"  /* MUST cause: fatal error: hal_spi.h: No such file */

int main(void) { return 0; }
