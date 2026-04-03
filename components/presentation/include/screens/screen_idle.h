/**
 * screen_idle.h — FiestaQuest Presentation: Idle Screensaver Renderer
 *
 * Renders the idle screensaver onto a 200x200 1-bit framebuffer.
 *
 * Architecture Decision (Phase-19.5):
 *   Idle is a render-layer OVERLAY, NOT an FSM state.
 *   app_main.c maintains an s_idle_active flag. When set, fq_render_idle()
 *   is called instead of the current state's renderer. On any button press,
 *   s_idle_active is cleared and the normal renderer resumes with the
 *   underlying FSM state preserved.
 *
 * Layout (200x200 e-paper):
 *   y=10..105  : 3x-scaled character sprite (96x96), centered at x=52
 *   y=112      : "EmberTide" in title font (FONT_SCRIPT_24), centered
 *   y=155      : character name + " Lv.N" in small font, centered
 *
 * Input: fq_vm_idle_t — pure data, no game/ dependency.
 * Output: pixels written to fq_fb_t.
 *
 * NULL-safe: fq_render_idle(NULL, ...) and fq_render_idle(..., NULL)
 * are silent no-ops.
 *
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 */

#ifndef FIESTAQUEST_PRESENTATION_SCREEN_IDLE_H
#define FIESTAQUEST_PRESENTATION_SCREEN_IDLE_H

#include "fq_framebuffer.h"
#include "view_models.h"

/**
 * fq_render_idle — Render the idle screensaver onto a framebuffer.
 *
 * Clears the framebuffer before drawing. Uses fq_blit_sprite_3x for the
 * 96x96 character sprite and fq_draw_text for title and status lines.
 *
 * Frame 0 of sprite_base is always used on the idle screen (no animation).
 * The sprite is centered horizontally: x = (200 - 96) / 2 = 52.
 *
 * @param fb  Target framebuffer. NULL-safe.
 * @param vm  Idle screen view model (read-only). NULL-safe.
 */
void fq_render_idle(fq_fb_t *fb, const fq_vm_idle_t *vm);

#endif /* FIESTAQUEST_PRESENTATION_SCREEN_IDLE_H */
