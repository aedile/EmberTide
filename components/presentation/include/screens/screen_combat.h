/**
 * screen_combat.h — FiestaQuest Presentation: Combat HUD Screen Renderer
 *
 * Renders the split-screen combat HUD onto a 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper, all coordinates inclusive top-left):
 *   Top half    (y=0..97)  : Enemy fighter (f2) — name, HP bar, sprite area
 *   Divider line            : y=98
 *   Bottom half (y=100..199): Player fighter (f1) — name, HP bar, sprite area
 *   Round indicator         : top-right "R:XX"
 *   Action banner           : centred rect over divider when action_text != ""
 *
 * HP bar formula (integer-only, no float):
 *   int32_t bar_w = (hp_max > 0) ? ((int32_t)hp * BAR_FILL_MAX / hp_max) : 0
 *   if (bar_w < 0) bar_w = 0
 *   if (bar_w > BAR_FILL_MAX) bar_w = BAR_FILL_MAX
 *
 * NULL-safe: fq_render_combat(NULL, ...) and fq_render_combat(..., NULL)
 * are silent no-ops.
 *
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 */

#ifndef FIESTAQUEST_PRESENTATION_SCREEN_COMBAT_H
#define FIESTAQUEST_PRESENTATION_SCREEN_COMBAT_H

#include "fq_framebuffer.h"
#include "view_models.h"

/**
 * fq_render_combat — Render the combat HUD onto a framebuffer.
 *
 * Clears the framebuffer before drawing. All layout is hardcoded for 200x200.
 * Uses only fq_fb_* primitives (no text rendering — fonts are device-side).
 *
 * @param fb  Target framebuffer. NULL-safe.
 * @param vm  Combat view model (read-only). NULL-safe.
 */
void fq_render_combat(fq_fb_t *fb, const fq_vm_combat_t *vm);

#endif /* FIESTAQUEST_PRESENTATION_SCREEN_COMBAT_H */
