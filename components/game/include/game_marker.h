/**
 * game_marker.h — FiestaQuest Game Component: Boundary Test Marker
 *
 * This header exists solely to provide a project-unique include name for
 * architectural boundary tests. The name "game_marker.h" cannot collide with
 * any system header, eliminating false-negative results in boundary tests that
 * use generic names like "types.h".
 *
 * Usage: boundary test files #include this header. If the file compiles, the
 * test CMake try_compile() detects a boundary violation (game/ is reachable
 * from a component that must not see it).
 *
 * This header is NOT for use in production code. Include types.h directly.
 */

#ifndef FIESTAQUEST_GAME_MARKER_H
#define FIESTAQUEST_GAME_MARKER_H

/* Pull in the real game types so a successful compile is meaningful. */
#include "types.h"

#endif /* FIESTAQUEST_GAME_MARKER_H */
