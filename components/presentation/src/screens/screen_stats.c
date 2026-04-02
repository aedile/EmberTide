/**
 * screen_stats.c — FiestaQuest Presentation: Stats Screen Renderer
 *
 * Renders the character stats screen onto the 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   y=0..33   : Black header bar — white "NAME" left, "Lv.N" right
 *   y=36..49  : STR bar with label (14px, wider fill zone)
 *   y=52..65  : SPD bar
 *   y=68..81  : PRC bar
 *   y=84..97  : INT bar
 *   y=100     : Separator line
 *   y=104..117: HP bar
 *   y=120     : Separator line
 *   y=124..137: XP bar
 *   y=140     : XP numeric text param — visible at y≈149..161 (font off_y=9)
 *   y=153     : Deaths text param — visible at y≈162..174 (font off_y=9)
 *   y=182..199: Black footer bar — white "[PWR] Back" only (no Deaths collision)
 *
 * Font note: FONT_REGS_12 has glyph_h=30 with off_y=9, so visible glyph
 * content appears at (y_param + 9) to (y_param + 21).  All text y coordinates
 * in this file follow that convention.
 *
 * Bar fill formula (integer-only):
 *   stat fill = (uint32_t)stat_val * BAR_FILL_W / 255u
 *   xp fill:
 *     xp_to_next == 0 → full (level 99 sentinel)
 *     else            → min(BAR_FILL_W, xp * BAR_FILL_W / xp_to_next)
 *
 * Constitution Priority 0: no float, no malloc, no PRNG.
 */

#include "screens/screen_stats.h"
#include "asset_data.h"
#include "sprite_util.h"
#include <stddef.h>
#include <stdio.h>

/* ── Layout constants ────────────────────────────────────────────────────── */

#define STATS_HDR_H           34  /**< Header bar height. */

/** Stat bar geometry — label left of bar, bar spans right portion. */
#define STATS_LABEL_X          5
#define STATS_BAR_LABEL_W     40  /**< Space reserved for label text. */
#define STATS_BAR_X           45  /**< Bar outline left edge. */
#define STATS_BAR_OUTLINE_W  148  /**< Bar outline width. */
#define STATS_BAR_ROW_H       14  /**< Each bar's drawn height. */
#define STATS_BAR_FILL_MAX   146u /**< Inner fill pixels at max value. */

/** Row y-offsets (top of each bar outline). */
#define STATS_STR_Y           36
#define STATS_SPD_Y           52
#define STATS_PRC_Y           68
#define STATS_INT_Y           84

#define STATS_SEP1_Y         100
#define STATS_HP_Y           104
#define STATS_SEP2_Y         120
#define STATS_XP_Y           124

/**
 * XP numbers y parameter — drawn at (STATS_XP_Y + STATS_BAR_ROW_H + 2) = 140.
 * Visible glyph content at y=149..161 (font off_y=9, 12px visible height).
 */
#define STATS_XP_NUM_Y  (STATS_XP_Y + STATS_BAR_ROW_H + 2)  /* = 140 */

/**
 * Deaths text y parameter — in the content area, below XP numbers.
 * y=153: visible glyph at y=162..174 (off_y=9 → 153+9=162).
 * XP text visible ends at y=161 → 1px gap before Deaths visible starts.
 * Footer starts at y=182 → 8px gap after Deaths visible ends (y=174).
 */
#define STATS_DEATHS_Y       153

/**
 * Footer y — shows nav hint only; Deaths is in the content area above.
 * Smaller bar height (18px) to fit within remaining display space.
 * y=182 + h=18 → exactly fills to y=200 (display bottom).
 */
#define STATS_FOOTER_Y       176
#define STATS_FOOTER_H        24

/* ── Internal: draw a labeled stat bar ──────────────────────────────────── */

/**
 * draw_stat_bar — Render a label + outlined bar with proportional fill.
 *
 * @param fill_val  0-255 mapped to [0, STATS_BAR_FILL_MAX].
 */
static void draw_stat_bar(fq_fb_t *fb, const fq_font_t *font,
                           int16_t y, uint8_t fill_val,
                           const char *label)
{
    /* Label text left of bar. */
    fq_draw_text(fb, font, STATS_LABEL_X, y, label);

    /* Bar outline. */
    fq_fb_draw_rect(fb, STATS_BAR_X, y,
                    STATS_BAR_OUTLINE_W, STATS_BAR_ROW_H, 1u);

    /* Integer fill — no float. */
    uint32_t fill_w = (uint32_t)fill_val * STATS_BAR_FILL_MAX / 255u;
    if (fill_w > STATS_BAR_FILL_MAX) { fill_w = STATS_BAR_FILL_MAX; }
    if (fill_w > 0u) {
        fq_fb_fill_rect(fb,
                        STATS_BAR_X + 1, (int16_t)(y + 1),
                        (int16_t)fill_w, STATS_BAR_ROW_H - 2, 1u);
    }
}

/* ── fq_render_stats ─────────────────────────────────────────────────────── */

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

    /* ── Header bar: name + level ───────────────────────────────────────── */
    {
        char lv_buf[8];
        snprintf(lv_buf, sizeof(lv_buf), "Lv.%u", (unsigned)vm->level);
        fq_draw_header_bar2(fb, font, 0, STATS_HDR_H, vm->name, lv_buf);
    }

    /* ── Four core stat bars ────────────────────────────────────────────── */
    draw_stat_bar(fb, font, STATS_STR_Y, vm->strength,     "STR");
    draw_stat_bar(fb, font, STATS_SPD_Y, vm->speed,        "SPD");
    draw_stat_bar(fb, font, STATS_PRC_Y, vm->precision,    "PRC");
    draw_stat_bar(fb, font, STATS_INT_Y, vm->intelligence, "INT");

    /* ── Separator ──────────────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 1, STATS_SEP1_Y,
                    (int16_t)(FQ_FB_WIDTH - 1u), STATS_SEP1_Y, 1u);

    /* ── HP bar (scaled from hp_max, max meaningful value = 512). ───────── */
    {
        uint8_t hp_scaled = (vm->hp_max >= 512u)
                            ? 255u
                            : (uint8_t)((uint32_t)vm->hp_max * 255u / 512u);
        draw_stat_bar(fb, font, STATS_HP_Y, hp_scaled, "HP");
    }

    /* ── Separator ──────────────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 1, STATS_SEP2_Y,
                    (int16_t)(FQ_FB_WIDTH - 1u), STATS_SEP2_Y, 1u);

    /* ── XP bar ─────────────────────────────────────────────────────────── */
    fq_draw_text(fb, font, STATS_LABEL_X, STATS_XP_Y, "XP");
    fq_fb_draw_rect(fb, STATS_BAR_X, STATS_XP_Y,
                    STATS_BAR_OUTLINE_W, STATS_BAR_ROW_H, 1u);
    {
        uint32_t xp_fill;
        if (vm->xp_to_next == 0u) {
            xp_fill = STATS_BAR_FILL_MAX;  /* Level 99 sentinel — full bar. */
        } else {
            xp_fill = vm->xp * STATS_BAR_FILL_MAX / vm->xp_to_next;
            if (xp_fill > STATS_BAR_FILL_MAX) { xp_fill = STATS_BAR_FILL_MAX; }
        }
        if (xp_fill > 0u) {
            fq_fb_fill_rect(fb,
                            STATS_BAR_X + 1, (int16_t)(STATS_XP_Y + 1),
                            (int16_t)xp_fill, STATS_BAR_ROW_H - 2, 1u);
        }
    }

    /* ── XP numbers ─────────────────────────────────────────────────────── */
    /* y_param=140: visible glyph at y≈149..161 (FONT_REGS_12 off_y=9). */
    {
        char xp_buf[24];
        snprintf(xp_buf, sizeof(xp_buf), "%lu/%lu",
                 (unsigned long)vm->xp,
                 (unsigned long)vm->xp_to_next);
        fq_draw_text(fb, font, STATS_LABEL_X, STATS_XP_NUM_Y, xp_buf);
    }

    /* ── Deaths count — in content area, NOT in the footer bar ──────────── */
    /* y_param=153: visible glyph at y≈162..174 (off_y=9).                  */
    /* Sits between XP text (visible end y≈161) and footer (y=182).         */
    {
        char d_buf[16];
        snprintf(d_buf, sizeof(d_buf), "Deaths: %u", (unsigned)vm->rebirth_count);
        fq_draw_text(fb, font, STATS_LABEL_X, STATS_DEATHS_Y, d_buf);
    }

    /* ── Footer bar: nav hint ONLY — Deaths is in content area above ────── */
    /* Footer height=18px fits glyph_h=12 visible content with margins.     */
    fq_draw_header_bar(fb, font, STATS_FOOTER_Y, STATS_FOOTER_H, "[PWR] Back");
}
