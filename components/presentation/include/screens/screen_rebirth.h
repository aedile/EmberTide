/**
 * screen_rebirth.h — FiestaQuest Presentation: Rebirth Screen Renderer
 *
 * Renders the permadeath rebirth screen onto the 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   y=0..29   : Header bar (black) — "REBIRTH" centered in white
 *   y=32..59  : Stat comparison: "STR: OLD -> NEW  SPD: OLD -> NEW"
 *   y=62..89  : "PRC: OLD -> NEW  INT: OLD -> NEW"
 *   y=92..109 : "Level: OLD -> 1"
 *   y=112..129: "Tokens: N  Earned: +N"
 *   y=132..175: Legacy tree — row of 8 circles (filled=unlocked, empty=locked)
 *               with overflow indicator if >8 nodes
 *   y=176..199: Footer — "[A] Spend Token  [B] Confirm"
 *
 * NULL-safe: fq_render_rebirth(NULL, ...) is a silent no-op.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG.
 * Architecture constraint: MUST NOT include hal_*.h or game/ headers directly.
 */

#ifndef FIESTAQUEST_SCREEN_REBIRTH_H
#define FIESTAQUEST_SCREEN_REBIRTH_H

#include "fq_framebuffer.h"
#include "view_models.h"

/**
 * fq_render_rebirth() — Render the rebirth screen.
 *
 * @param fb  Framebuffer to draw into. NULL-safe (no-op if NULL).
 * @param vm  View model. NULL-safe (no-op if NULL).
 */
void fq_render_rebirth(fq_fb_t              *fb,
                        const fq_vm_rebirth_t *vm);

#endif /* FIESTAQUEST_SCREEN_REBIRTH_H */
