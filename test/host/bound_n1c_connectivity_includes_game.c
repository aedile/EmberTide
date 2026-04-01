/**
 * bound_n1c_connectivity_includes_game.c
 *
 * BOUNDARY VIOLATION TEST — must NOT compile successfully.
 *
 * This file simulates a connectivity/ module attempting to include a game/
 * header. It uses the project-unique "game_marker.h" (not the generic
 * "types.h") so the test cannot produce a false-negative by accidentally
 * resolving against a system header with the same name.
 *
 * Per spec-challenger finding N1c:
 *   connectivity/ MUST NOT depend on game/ headers.
 *
 * Per review finding B5:
 *   Changed from #include "types.h" to #include "game_marker.h" to prevent
 *   false-negative results if a system types.h resolves first.
 */

#include "game_marker.h"  /* MUST cause: fatal error: game_marker.h: No such file */

int main(void) { return 0; }
