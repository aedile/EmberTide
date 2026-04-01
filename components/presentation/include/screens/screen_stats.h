/**
 * screen_stats.h — FiestaQuest Presentation: Stats Screen Renderer
 *
 * Renders the character stats screen onto a 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   Row  0-14  : Name and level header
 *   Row 15-24  : HP max bar
 *   Row 25-99  : Four stat bars (STR, SPD, PRC, INT) — 18px each with label
 *   Row 100-129: XP progress bar
 *   Row 130-144: Rebirth count
 *
 * Stat bars are proportional to raw value / 255 mapped to a 160-pixel wide bar.
 * Bar fill uses integer arithmetic: fill_w = (uint32_t)stat_val * 160u / 255u
 *
 * XP bar fill:
 *   If xp_to_next == 0: bar is full (level 99 sentinel).
 *   Else: fill_w = min(160, xp * 160 / xp_to_next)
 *
 * NULL-safe: both pointer arguments are checked before use.
 *
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 */

#ifndef FIESTAQUEST_PRESENTATION_SCREEN_STATS_H
#define FIESTAQUEST_PRESENTATION_SCREEN_STATS_H

#include "fq_framebuffer.h"
#include "view_models.h"

/**
 * fq_render_stats — Render the character stats screen onto a framebuffer.
 *
 * @param fb  Target framebuffer. NULL-safe.
 * @param vm  Stats view model (read-only). NULL-safe.
 */
void fq_render_stats(fq_fb_t *fb, const fq_vm_stats_t *vm);

#endif /* FIESTAQUEST_PRESENTATION_SCREEN_STATS_H */
