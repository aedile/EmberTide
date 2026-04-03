/**
 * screen_home.h — FiestaQuest Presentation: Home Screen Renderer
 *
 * Renders the home/dashboard screen onto a 200x200 1-bit framebuffer.
 *
 * Phase-19 layout (200x200 e-paper):
 *   y=0..17   : Black header bar — white character name (left) + "Lv.N" (right)
 *   y=18      : Separator line
 *   y=20..83  : 2x-scaled character sprite (64x64), horizontally centered
 *   y=86..99  : HP bar with proportional fill + "HP" label
 *   y=100..113: Win/Loss record "W:N  L:N", centered
 *   y=114     : Separator line
 *   y=116..197: Navigation menu — 4 rows × 18px:
 *                 Row 0: TRAIN
 *                 Row 1: BATTLE
 *                 Row 2: ITEMS
 *                 Row 3: STATS
 *               Highlighted row (vm->menu_index) rendered with black fill +
 *               white inverted text + ">" cursor.
 *               Normal rows rendered with plain black text.
 *
 * Button mapping (handled by app_fsm.c, not this renderer):
 *   ☀ SUN (BTN_B, GPIO18): cycles vm->menu_index via home_menu_index in ctx.
 *   ⏻ PWR (BTN_A, GPIO0):  selects current menu item.
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
 * hardcoded for the 200x200 display. Uses fq_fb_* primitives, fq_draw_text,
 * fq_draw_text_inverted, fq_draw_header_bar2, and fq_blit_sprite_2x.
 *
 * The menu_index field in vm selects which of the 4 menu rows is highlighted.
 * Out-of-range values are clamped to 0 (TRAIN) — no crash, no UB.
 *
 * @param fb  Target framebuffer. NULL-safe.
 * @param vm  Home screen view model (read-only). NULL-safe.
 */
void fq_render_home(fq_fb_t *fb, const fq_vm_home_t *vm);

#endif /* FIESTAQUEST_PRESENTATION_SCREEN_HOME_H */
