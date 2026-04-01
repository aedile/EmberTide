/**
 * bound_e1_game_includes_hal.c
 *
 * BOUNDARY VIOLATION TEST — must NOT compile successfully.
 *
 * This file simulates a game/ module attempting to include a hal_*.h header.
 * It is compiled as a separate target with WILL_FAIL TRUE in CMakeLists.txt.
 * A successful build (exit 0) would indicate that the CMake boundary is broken.
 *
 * Per spec-challenger finding E1 and architecture constraint:
 *   game/ MUST NOT depend on hal/.
 */

/* Attempt to include a hal header — this should cause a compile error when
 * the include path for hal is correctly excluded from the game component. */
#include "hal_gpio.h"  /* MUST cause: fatal error: hal_gpio.h: No such file */

int main(void) { return 0; }
