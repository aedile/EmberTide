/**
 * view_models.h — FiestaQuest Presentation Layer: Screen View Models
 *
 * All screen-specific view model structs live here. View models are pure
 * data carriers constructed by the application layer from game state. Screen
 * renderers receive read-only view model pointers — they NEVER include
 * game/types.h directly.
 *
 * Architecture constraint: presentation/ MUST NOT include hal_*.h or
 * game/ headers (other than through the application layer constructor).
 *
 * HOST-COMPILABLE: this file compiles on the host for visual test harness.
 */

#ifndef FIESTAQUEST_PRESENTATION_VIEW_MODELS_H
#define FIESTAQUEST_PRESENTATION_VIEW_MODELS_H

#include <stdint.h>

/* Placeholder: expanded in later phases. */

/** Generic screen view model placeholder. */
typedef struct {
    uint8_t screen_id; /**< Which screen to render. */
} screen_view_model_t;

#endif /* FIESTAQUEST_PRESENTATION_VIEW_MODELS_H */
