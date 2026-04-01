/**
 * types.h — FiestaQuest Game Engine: Shared Value Types
 *
 * Frozen neutral value objects consumed by multiple modules.
 * No business logic lives here — pure data carriers only.
 *
 * Architecture constraint: this header is the ONLY game header that
 * presentation/ may reference (via view_models.h adapters, never directly).
 *
 * Dependency rule: this file MUST NOT include any hal_*.h, presentation/,
 * or connectivity/ headers.
 */

#ifndef FIESTAQUEST_GAME_TYPES_H
#define FIESTAQUEST_GAME_TYPES_H

#include <stdint.h>

/* Placeholder: expanded in later phases. */

/** Error codes returned by all game module functions. */
typedef enum {
    GAME_OK            = 0,
    GAME_ERR_NULL_PTR  = 1,
    GAME_ERR_OVERFLOW  = 2,
    GAME_ERR_UNDERFLOW = 3,
    GAME_ERR_DIV_ZERO  = 4,
    GAME_ERR_INVALID   = 5
} game_err_t;

#endif /* FIESTAQUEST_GAME_TYPES_H */
