/**
 * screen_onboarding.h — FiestaQuest Presentation: First-Boot Onboarding Screen
 *
 * Renders the character creation carousel onto the 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   y=0..33   : Black header bar — white "NEW GAME" text
 *   y=40..80  : Class sprite (32x32 at 1x, centered)
 *   y=85..100 : Class name text, centered
 *   y=105..170: Stat summary (STR / SPD / PRC / INT bars)
 *   y=166..199: Black footer bar — "[PWR] Cycle  [SUN] Confirm"
 *
 * NULL-safe: fq_render_onboarding(NULL, ...) is a silent no-op.
 *
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 *
 * Phase-19 addition.
 */

#ifndef FIESTAQUEST_PRESENTATION_SCREEN_ONBOARDING_H
#define FIESTAQUEST_PRESENTATION_SCREEN_ONBOARDING_H

#include "fq_framebuffer.h"
#include "view_models.h"

/**
 * fq_render_onboarding — Render the onboarding class selection screen.
 *
 * @param fb  Target framebuffer. NULL-safe.
 * @param vm  Onboarding view model (read-only). NULL-safe.
 */
void fq_render_onboarding(fq_fb_t *fb, const fq_vm_onboarding_t *vm);

#endif /* FIESTAQUEST_PRESENTATION_SCREEN_ONBOARDING_H */
