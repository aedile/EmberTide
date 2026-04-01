/**
 * screen_home.h — FiestaQuest Presentation: Home Screen Renderer
 *
 * Renders the home/dashboard screen onto a 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   Row  0-15  : Character name and level (top label area)
 *   Row 16-135 : Sprite placeholder rectangle (centered, 80x80)
 *   Row 136-151: HP bar (filled proportional to hp_percent)
 *   Row 155-170: Wins / Losses stats
 *   Row 175-195: Screen label / class name
 *
 * Input: fq_vm_home_t — pure data, no game/ dependency.
 * Output: pixels written to fq_fb_t.
 *
 * NULL-safe: fq_render_home(NULL, ...) and fq_render_home(..., NULL) are
 * silent no-ops.
 *
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 */

#ifndef FIESTAQUEST_PRESENTATION_SCREEN_HOME_H
#define FIESTAQUEST_PRESENTATION_SCREEN_HOME_H

#include "fq_framebuffer.h"
#include "view_models.h"

/**
 * fq_render_home — Render the home screen onto a framebuffer.
 *
 * Clears the framebuffer before drawing. All layout coordinates are
 * hardcoded for the 200x200 display. Uses only fq_fb_* primitives (no
 * text rendering — fonts are a device-side concern not yet wired for host).
 *
 * @param fb  Target framebuffer. NULL-safe.
 * @param vm  Home screen view model (read-only). NULL-safe.
 */
void fq_render_home(fq_fb_t *fb, const fq_vm_home_t *vm);

#endif /* FIESTAQUEST_PRESENTATION_SCREEN_HOME_H */
