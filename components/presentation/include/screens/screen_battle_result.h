/**
 * screen_battle_result.h — FiestaQuest Presentation: Battle Result Screen Renderer
 *
 * Renders the post-combat result screen onto the 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   y=0..39   : Header bar (black) — "YOU WIN!" or "YOU LOSE" centered in white
 *   y=40..99  : Winner sprite (2x, centered), or loser sprite
 *   y=100..119: "XP: +NNN" centered in small font
 *   y=120..139: "Rounds: N" centered
 *   y=140..159: Winner name centered
 *   y=170..199: Footer — "[A] Continue" hint
 *
 * NULL-safe: fq_render_battle_result(NULL, ...) is a silent no-op.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG.
 * Architecture constraint: MUST NOT include hal_*.h or game/ headers directly.
 */

#ifndef FIESTAQUEST_SCREEN_BATTLE_RESULT_H
#define FIESTAQUEST_SCREEN_BATTLE_RESULT_H

#include "fq_framebuffer.h"
#include "view_models.h"

/**
 * fq_render_battle_result() — Render the battle result screen.
 *
 * @param fb  Framebuffer to draw into. NULL-safe (no-op if NULL).
 * @param vm  View model. NULL-safe (no-op if NULL).
 */
void fq_render_battle_result(fq_fb_t                     *fb,
                              const fq_vm_battle_result_t *vm);

#endif /* FIESTAQUEST_SCREEN_BATTLE_RESULT_H */
