/**
 * ui_widgets.h — FiestaQuest Presentation Layer: Reusable UI Widgets
 *
 * Provides reusable widget primitives that overlay an existing framebuffer.
 * Widgets do NOT clear the framebuffer — they draw on top of whatever
 * content is already present.
 *
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 *
 * Phase-9 additions: fq_render_dialogue — word-wrapping dialogue box overlay.
 */

#ifndef FIESTAQUEST_PRESENTATION_UI_WIDGETS_H
#define FIESTAQUEST_PRESENTATION_UI_WIDGETS_H

#include "fq_framebuffer.h"
#include <stdint.h>

/**
 * fq_render_dialogue — Render a word-wrapping dialogue box on the framebuffer.
 *
 * Overlays the bottom ~80 pixels of the display (y=120..199) with a dialogue
 * box containing a title and body text. An ornate double-rect border is drawn
 * (outer rect + inner rect with 2px gap).
 *
 * Word-wrap rules:
 *   - Max ~25 characters per line (based on ~8px avg char width for 200px).
 *   - Breaks on spaces when possible; force-breaks at 25 chars if no space.
 *   - Embedded '\n' characters are treated as explicit line breaks.
 *   - Maximum 4 visible body lines; any overflow is truncated with "...".
 *
 * Title is rendered as an ALL-CAPS placeholder rect in the title band.
 * Body lines are rendered as horizontal rule placeholders (no font rendering —
 * fonts are device-side).
 *
 * @param fb         Target framebuffer. NULL-safe: silently returns.
 * @param title      Dialogue title. NULL-safe: treated as empty string.
 * @param body       Dialogue body text. NULL-safe: treated as empty string.
 * @param show_yes_no  1 = render [YES] and [NO] button placeholders.
 *                     0 = no buttons rendered.
 */
void fq_render_dialogue(fq_fb_t    *fb,
                        const char *title,
                        const char *body,
                        uint8_t     show_yes_no);

#endif /* FIESTAQUEST_PRESENTATION_UI_WIDGETS_H */
