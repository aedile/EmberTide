/**
 * bound_n1b_game_includes_presentation.c
 *
 * BOUNDARY VIOLATION TEST — must NOT compile successfully.
 *
 * This file simulates a game/ module attempting to include a presentation
 * header (view_models.h). Compiled with WILL_FAIL TRUE.
 *
 * Per spec-challenger finding N1b:
 *   game/ MUST NOT depend on presentation/.
 */

#include "view_models.h"  /* MUST cause: fatal error: view_models.h: No such file */

int main(void) { return 0; }
