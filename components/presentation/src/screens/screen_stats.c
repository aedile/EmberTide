/**
 * screen_stats.c — FiestaQuest Presentation: Stats Screen Renderer
 *
 * Renders the character stats screen onto the 200x200 1-bit framebuffer.
 *
 * Layout:
 *   Row  4-14  : Name/level header separator line at row 15
 *   Row 16-30  : HP max bar (outline + fill)
 *   Row 32-49  : STR bar
 *   Row 51-68  : SPD bar
 *   Row 70-87  : PRC bar
 *   Row 89-106 : INT bar
 *   Row 108-120: Divider
 *   Row 121-135: XP progress bar
 *   Row 137-150: Rebirth count indicator (n dots)
 *
 * Bar fill formula (integer only, no float):
 *   stat fill = (uint32_t)stat_val * BAR_MAX_W / 255u
 *   xp fill:
 *     if xp_to_next == 0 → fill full (level 99 sentinel)
 *     else               → min(BAR_MAX_W, xp * BAR_MAX_W / xp_to_next)
 *
 * All bar widths are clamped to BAR_MAX_W to prevent OOB drawing.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG.
 */

#include "screens/screen_stats.h"
#include <stddef.h>

/* Bar geometry. */
#define STATS_BAR_X        20
#define STATS_BAR_H        10
#define STATS_BAR_MAX_W   160u   /**< Maximum inner fill width in pixels. */
#define STATS_BAR_OUTLINE_W (STATS_BAR_MAX_W + 2)

/* Row offsets for each bar (top of outline rect). */
#define STATS_HP_BAR_Y   16
#define STATS_STR_BAR_Y  32
#define STATS_SPD_BAR_Y  51
#define STATS_PRC_BAR_Y  70
#define STATS_INT_BAR_Y  89
#define STATS_XP_BAR_Y   121

/* Rebirth indicator. */
#define STATS_REBIRTH_Y       140
#define STATS_REBIRTH_DOT_W     4
#define STATS_REBIRTH_DOT_H     4
#define STATS_REBIRTH_DOT_GAP   2
#define STATS_REBIRTH_MAX_DOTS 10  /**< Max dots rendered (rebirth 0-10+). */

/* ---------------------------------------------------------------------------
 * Internal: draw a labeled bar.
 *
 * Draws an outline rect at (STATS_BAR_X, y) wide STATS_BAR_OUTLINE_W x
 * STATS_BAR_H, then fills the interior proportionally.
 * fill_pct is 0-255 mapped to 0-STATS_BAR_MAX_W pixels.
 * ---------------------------------------------------------------------------*/
static void draw_stat_bar(fq_fb_t *fb, int16_t y, uint8_t raw_val)
{
    /* Outline. */
    fq_fb_draw_rect(fb,
                    STATS_BAR_X, y,
                    (int16_t)STATS_BAR_OUTLINE_W, STATS_BAR_H, 1u);

    /* Fill: integer-only, no float. */
    uint32_t fill_w = (uint32_t)raw_val * STATS_BAR_MAX_W / 255u;
    if (fill_w > STATS_BAR_MAX_W) {
        fill_w = STATS_BAR_MAX_W;
    }
    if (fill_w > 0u) {
        fq_fb_fill_rect(fb,
                        STATS_BAR_X + 1, (int16_t)(y + 1),
                        (int16_t)fill_w, STATS_BAR_H - 2, 1u);
    }
}

/* ---------------------------------------------------------------------------
 * fq_render_stats
 * ---------------------------------------------------------------------------*/
void fq_render_stats(fq_fb_t *fb, const fq_vm_stats_t *vm)
{
    if (fb == NULL || vm == NULL) {
        return;
    }

    /* Clear to white. */
    fq_fb_clear(fb);

    /* ── Display border ─────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb, 0, 0,
                    (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* ── Header separator ────────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 5, 15, 194, 15, 1u);

    /* ── HP max bar ──────────────────────────────────────────────────────── */
    /* HP bar fill: map hp_max to a 0-255 value for display purposes.
     * We display hp_max / 512 * 255 clamped. Since hp_max is uint16_t,
     * scale: fill = min(255, hp_max * 255 / 512).
     * hp_max range: [0, 65535]. We clamp display to 512 max HP.
     * For values > 512, bar is full. */
    {
        uint32_t hp_scaled = (vm->hp_max > 512u)
                             ? 255u
                             : ((uint32_t)vm->hp_max * 255u / 512u);
        draw_stat_bar(fb, STATS_HP_BAR_Y, (uint8_t)hp_scaled);
    }

    /* ── Four core stat bars ─────────────────────────────────────────────── */
    draw_stat_bar(fb, STATS_STR_BAR_Y, vm->strength);
    draw_stat_bar(fb, STATS_SPD_BAR_Y, vm->speed);
    draw_stat_bar(fb, STATS_PRC_BAR_Y, vm->precision);
    draw_stat_bar(fb, STATS_INT_BAR_Y, vm->intelligence);

    /* ── Divider ─────────────────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 5, 108, 194, 108, 1u);

    /* ── XP progress bar ─────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb,
                    STATS_BAR_X, STATS_XP_BAR_Y,
                    (int16_t)STATS_BAR_OUTLINE_W, STATS_BAR_H, 1u);
    {
        uint32_t xp_fill_w;
        if (vm->xp_to_next == 0u) {
            /* Level 99 sentinel — show full bar. */
            xp_fill_w = STATS_BAR_MAX_W;
        } else {
            xp_fill_w = vm->xp * STATS_BAR_MAX_W / vm->xp_to_next;
            if (xp_fill_w > STATS_BAR_MAX_W) {
                xp_fill_w = STATS_BAR_MAX_W;
            }
        }
        if (xp_fill_w > 0u) {
            fq_fb_fill_rect(fb,
                            STATS_BAR_X + 1, STATS_XP_BAR_Y + 1,
                            (int16_t)xp_fill_w, STATS_BAR_H - 2, 1u);
        }
    }

    /* ── Rebirth count dots ──────────────────────────────────────────────── */
    {
        uint8_t dots = vm->rebirth_count;
        if (dots > STATS_REBIRTH_MAX_DOTS) {
            dots = STATS_REBIRTH_MAX_DOTS;
        }
        for (uint8_t d = 0u; d < dots; d++) {
            int16_t dx = (int16_t)(STATS_BAR_X +
                                   (uint16_t)d * (STATS_REBIRTH_DOT_W + STATS_REBIRTH_DOT_GAP));
            fq_fb_fill_rect(fb, dx, STATS_REBIRTH_Y,
                            STATS_REBIRTH_DOT_W, STATS_REBIRTH_DOT_H, 1u);
        }
    }
}
