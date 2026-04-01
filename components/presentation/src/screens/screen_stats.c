/**
 * screen_stats.c — FiestaQuest Presentation: Stats Screen Renderer
 *
 * Renders the character stats screen onto the 200x200 1-bit framebuffer.
 * Phase 18: replaced placeholder geometry with real text and labeled bars.
 *
 * Layout:
 *   Row  3-32  : Name + "Lv.XX" header text (glyph_h=30)
 *   Row 33     : Header separator line
 *   Row 38-47  : HP bar (outline + fill) + "HP" label
 *   Row 52-61  : STR bar + "STR" label left of bar
 *   Row 66-75  : SPD bar + "SPD" label
 *   Row 80-89  : PRC bar + "PRC" label
 *   Row 94-103 : INT bar + "INT" label
 *   Row 108    : Divider
 *   Row 113-122: XP progress bar + "XP" label
 *   Row 126-135: XP "XXXX/XXXX" text
 *   Row 140-149: Rebirth count dots
 *   Row 152-181: "Deaths: X" rebirth text
 *
 * Bar fill formula (integer only, no float):
 *   stat fill = (uint32_t)stat_val * BAR_FILL_W / 255u
 *   xp fill:
 *     if xp_to_next == 0 → fill full (level 99 sentinel)
 *     else               → min(BAR_FILL_W, xp * BAR_FILL_W / xp_to_next)
 *
 * All bar widths are clamped to BAR_FILL_W to prevent OOB drawing.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG.
 */

#include "screens/screen_stats.h"
#include "asset_data.h"
#include <stddef.h>
#include <stdio.h>

/* Bar geometry. */
#define STATS_LABEL_X        5   /**< X position for bar labels (left margin). */
#define STATS_BAR_X         40   /**< X start of bar outline (after label). */
#define STATS_BAR_H         10
#define STATS_BAR_MAX_W    150u  /**< Maximum inner fill width in pixels. */
#define STATS_BAR_OUTLINE_W ((int16_t)(STATS_BAR_MAX_W + 2))

/* Row offsets for each bar (top of outline rect). */
#define STATS_HP_BAR_Y   38
#define STATS_STR_BAR_Y  52
#define STATS_SPD_BAR_Y  66
#define STATS_PRC_BAR_Y  80
#define STATS_INT_BAR_Y  94
#define STATS_XP_BAR_Y  113

/* Rebirth indicator. */
#define STATS_REBIRTH_Y       140
#define STATS_REBIRTH_DOT_W     4
#define STATS_REBIRTH_DOT_H     4
#define STATS_REBIRTH_DOT_GAP   2
#define STATS_REBIRTH_MAX_DOTS 10  /**< Max dots rendered (rebirth 0-10+). */

/* ---------------------------------------------------------------------------
 * Internal: draw a labeled bar.
 *
 * Draws the label text at (STATS_LABEL_X, y), then an outline rect at
 * (STATS_BAR_X, y) wide STATS_BAR_OUTLINE_W x STATS_BAR_H, then fills the
 * interior proportionally.
 * fill_val is 0-255 mapped to 0-STATS_BAR_MAX_W pixels.
 * ---------------------------------------------------------------------------*/
static void draw_labeled_stat_bar(fq_fb_t *fb, const fq_font_t *font,
                                   int16_t y, uint8_t raw_val,
                                   const char *label)
{
    /* Label text left of bar. */
    fq_draw_text(fb, font, STATS_LABEL_X, y, label);

    /* Bar outline. */
    fq_fb_draw_rect(fb,
                    STATS_BAR_X, y,
                    STATS_BAR_OUTLINE_W, STATS_BAR_H, 1u);

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

    const fq_font_t *font = fq_get_font_small();

    /* Clear to white. */
    fq_fb_clear(fb);

    /* ── Display border ─────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb, 0, 0,
                    (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* ── Name + level header ─────────────────────────────────────────────── */
    fq_draw_text(fb, font, 5, 3, vm->name);
    {
        char lv_buf[8];
        snprintf(lv_buf, sizeof(lv_buf), "Lv.%u", (unsigned)vm->level);
        fq_draw_text(fb, font, 130, 3, lv_buf);
    }

    /* ── Header separator ────────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 5, 33, 194, 33, 1u);

    /* ── HP max bar ──────────────────────────────────────────────────────── */
    {
        uint32_t hp_scaled = (vm->hp_max > 512u)
                             ? 255u
                             : ((uint32_t)vm->hp_max * 255u / 512u);
        draw_labeled_stat_bar(fb, font, STATS_HP_BAR_Y,
                              (uint8_t)hp_scaled, "HP");
    }

    /* ── Four core stat bars ─────────────────────────────────────────────── */
    draw_labeled_stat_bar(fb, font, STATS_STR_BAR_Y, vm->strength,     "STR");
    draw_labeled_stat_bar(fb, font, STATS_SPD_BAR_Y, vm->speed,        "SPD");
    draw_labeled_stat_bar(fb, font, STATS_PRC_BAR_Y, vm->precision,    "PRC");
    draw_labeled_stat_bar(fb, font, STATS_INT_BAR_Y, vm->intelligence, "INT");

    /* ── Divider ─────────────────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 5, 108, 194, 108, 1u);

    /* ── XP progress bar ─────────────────────────────────────────────────── */
    fq_draw_text(fb, font, STATS_LABEL_X, STATS_XP_BAR_Y, "XP");
    fq_fb_draw_rect(fb,
                    STATS_BAR_X, STATS_XP_BAR_Y,
                    STATS_BAR_OUTLINE_W, STATS_BAR_H, 1u);
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

    /* ── XP numbers "XXXX / XXXX" ───────────────────────────────────────── */
    {
        char xp_buf[24];
        snprintf(xp_buf, sizeof(xp_buf), "%lu/%lu",
                 (unsigned long)vm->xp,
                 (unsigned long)vm->xp_to_next);
        fq_draw_text(fb, font, 5, 126, xp_buf);
    }

    /* ── Rebirth count dots ──────────────────────────────────────────────── */
    {
        uint8_t dots = vm->rebirth_count;
        if (dots > STATS_REBIRTH_MAX_DOTS) {
            dots = STATS_REBIRTH_MAX_DOTS;
        }
        for (uint8_t d = 0u; d < dots; d++) {
            int16_t dx = (int16_t)(5 +
                                   (uint16_t)d * (STATS_REBIRTH_DOT_W + STATS_REBIRTH_DOT_GAP));
            fq_fb_fill_rect(fb, dx, STATS_REBIRTH_Y,
                            STATS_REBIRTH_DOT_W, STATS_REBIRTH_DOT_H, 1u);
        }
    }

    /* ── "Deaths: X" rebirth text ────────────────────────────────────────── */
    {
        char d_buf[16];
        snprintf(d_buf, sizeof(d_buf), "Deaths: %u", (unsigned)vm->rebirth_count);
        fq_draw_text(fb, font, 5, 152, d_buf);
    }
}
