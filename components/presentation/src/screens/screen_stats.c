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
 *   y=140..169: XP numeric text ("XP: NNNN/NNNN") — glyph_h=30
 *   y=166..199: Black footer bar — white "Deaths: N"
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

#define STATS_BAR_H           34  /**< Header / footer bar height. */

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

/** Footer y. */
#define STATS_FOOTER_Y       166

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
        fq_draw_header_bar2(fb, font, 0, STATS_BAR_H, vm->name, lv_buf);
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
    {
        char xp_buf[24];
        snprintf(xp_buf, sizeof(xp_buf), "%lu/%lu",
                 (unsigned long)vm->xp,
                 (unsigned long)vm->xp_to_next);
        /* Draw below XP bar — text starts at bar bottom + 2px gap. */
        fq_draw_text(fb, font, STATS_LABEL_X,
                     (int16_t)(STATS_XP_Y + STATS_BAR_ROW_H + 2), xp_buf);
    }

    /* ── Footer bar: Deaths count ───────────────────────────────────────── */
    {
        char d_buf[16];
        snprintf(d_buf, sizeof(d_buf), "Deaths: %u", (unsigned)vm->rebirth_count);
        fq_draw_header_bar(fb, font, STATS_FOOTER_Y, STATS_BAR_H, d_buf);
    }
}
