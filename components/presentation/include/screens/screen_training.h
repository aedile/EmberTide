/**
 * screen_training.h — FiestaQuest Presentation: Training Mini-Game Screen
 *
 * Renders the training mini-game screen onto a 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   Row  0-15  : Border + game name placeholder
 *   Row 16-80  : Mini-game activity area (state-dependent)
 *   Row 85-105 : Score bar (proportional 0-100)
 *   Row 110-130: Difficulty indicator
 *   Row 140-170: State label area (WAITING / ACTIVE / DONE)
 *
 * NULL-safe: fq_render_training(NULL, ...) and fq_render_training(..., NULL)
 * are silent no-ops.
 *
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 */

#ifndef FIESTAQUEST_PRESENTATION_SCREEN_TRAINING_H
#define FIESTAQUEST_PRESENTATION_SCREEN_TRAINING_H

#include "fq_framebuffer.h"
#include "view_models.h"

/**
 * fq_render_training — Render the training screen onto a framebuffer.
 *
 * Clears the framebuffer before drawing. All layout is hardcoded for 200x200.
 * Uses only fq_fb_* primitives.
 *
 * @param fb  Target framebuffer. NULL-safe.
 * @param vm  Training view model (read-only). NULL-safe.
 */
void fq_render_training(fq_fb_t *fb, const fq_vm_training_t *vm);

#endif /* FIESTAQUEST_PRESENTATION_SCREEN_TRAINING_H */
