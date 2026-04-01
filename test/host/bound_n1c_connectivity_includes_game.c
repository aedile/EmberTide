/**
 * bound_n1c_connectivity_includes_game.c
 *
 * BOUNDARY VIOLATION TEST — must NOT compile successfully.
 *
 * This file simulates a connectivity/ module attempting to include a game/
 * header (types.h). Compiled with WILL_FAIL TRUE.
 *
 * Per spec-challenger finding N1c:
 *   connectivity/ MUST NOT depend on game/ headers.
 */

#include "types.h"  /* MUST cause: fatal error: types.h: No such file */

int main(void) { return 0; }
