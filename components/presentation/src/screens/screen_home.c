/**
 * screen_home.c — FiestaQuest Presentation: Home Screen Renderer
 *
 * Renders the home/dashboard screen onto the 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   y=0..33   : Black header bar — white text "NAME" left + "Lv.N" right
 *   y=34      : Separator line
 *   y=36..99  : 2x-scaled character sprite (64x64), centered at x=68
 *   y=102..115: HP bar (outline + proportional fill) + "HP" label
 *   y=118     : Separator line
 *   y=120..133: Win/Loss stats text "W:N  L:N"
 *   y=166..199: Black footer bar — white text "W:N" left + "L:N" right
 *
 * HP bar formula (integer-only, no float):
 *   fill_w = hp_percent (0-100) * HOME_HP_FILL_MAX_W / 100
 *   Clamped to [0, HOME_HP_FILL_MAX_W].
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 */

#include "screens/screen_home.h"
#include "asset_data.h"
#include "sprite_util.h"
#include <stddef.h>
#include <stdio.h>

/* ── Layout constants ────────────────────────────────────────────────────── */

/** Header and footer bar height (fits glyph_h=30 with 2px top pad). */
#define HOME_BAR_H           34

/** Sprite destination: 2x-scaled 32x32 = 64x64, centered horizontally. */
#define HOME_SPRITE_X        68   /* (200 - 64) / 2 = 68 */
#define HOME_SPRITE_Y        36

/** HP bar geometry. */
#define HOME_HP_BAR_X        10
#define HOME_HP_BAR_Y       102
#define HOME_HP_BAR_W       180
#define HOME_HP_BAR_H        14
#define HOME_HP_FILL_MAX_W  178u  /**< Inner fill pixels at 100% HP. */

/** Stats text row. */
#define HOME_STATS_Y        120

/** Footer bar top y. */
#define HOME_FOOTER_Y       166

/*
 * Win/loss buffer size: "W: " (3) + uint16 max "65535" (5) + NUL (1) = 9.
 * Use 10 for comfortable margin and to satisfy -Wformat-truncation.
 */
#define HOME_WL_BUF_SIZE     10

/* ── fq_render_home ──────────────────────────────────────────────────────── */

void fq_render_home(fq_fb_t *fb, const fq_vm_home_t *vm)
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

    /* ── Header bar: black fill, white text ─────────────────────────────── */
    {
        char lv_buf[8];
        snprintf(lv_buf, sizeof(lv_buf), "Lv.%u", (unsigned)vm->level);
        fq_draw_header_bar2(fb, font, 0, HOME_BAR_H, vm->name, lv_buf);
    }

    /* ── Separator under header ─────────────────────────────────────────── */
    fq_fb_draw_line(fb, 1, HOME_BAR_H, (int16_t)(FQ_FB_WIDTH - 1u), HOME_BAR_H, 1u);

    /* ── 2x-scaled character sprite ─────────────────────────────────────── */
    {
        const fq_sprite_t *sp = fq_get_char_sprite(vm->sprite_base, 0u);
        if (sp != NULL) {
            fq_blit_sprite_2x(fb, HOME_SPRITE_X, HOME_SPRITE_Y, sp);
        }
    }

    /* ── HP bar (bordered panel) ────────────────────────────────────────── */
    /* Outline. */
    fq_fb_draw_rect(fb,
                    HOME_HP_BAR_X, HOME_HP_BAR_Y,
                    HOME_HP_BAR_W, HOME_HP_BAR_H, 1u);

    /* Proportional fill (integer math, no float). */
    {
        uint32_t pct = (vm->hp_percent > 100u) ? 100u : (uint32_t)vm->hp_percent;
        uint32_t fill_w = pct * HOME_HP_FILL_MAX_W / 100u;
        if (fill_w > HOME_HP_FILL_MAX_W) { fill_w = HOME_HP_FILL_MAX_W; }
        if (fill_w > 0u) {
            fq_fb_fill_rect(fb,
                            HOME_HP_BAR_X + 1, HOME_HP_BAR_Y + 1,
                            (int16_t)fill_w, HOME_HP_BAR_H - 2, 1u);
        }
    }

    /* "HP" label — white patch then text. */
    fq_fb_fill_rect(fb,
                    HOME_HP_BAR_X + 1, HOME_HP_BAR_Y + 1,
                    28, HOME_HP_BAR_H - 2, 0u);
    fq_draw_text(fb, font, HOME_HP_BAR_X + 2, HOME_HP_BAR_Y + 1, "HP");

    /* ── Separator ──────────────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 1, HOME_STATS_Y - 2,
                    (int16_t)(FQ_FB_WIDTH - 1u), HOME_STATS_Y - 2, 1u);

    /* ── Win / Loss stats text ──────────────────────────────────────────── */
    {
        char w_buf[HOME_WL_BUF_SIZE];
        char l_buf[HOME_WL_BUF_SIZE];
        snprintf(w_buf, sizeof(w_buf), "W: %u", (unsigned)vm->wins);
        snprintf(l_buf, sizeof(l_buf), "L: %u", (unsigned)vm->losses);
        fq_draw_text(fb, font, 10, HOME_STATS_Y, w_buf);
        fq_draw_text(fb, font, 110, HOME_STATS_Y, l_buf);
    }

    /* ── Footer bar: black fill, white text ─────────────────────────────── */
    {
        char w_buf[HOME_WL_BUF_SIZE];
        char l_buf[HOME_WL_BUF_SIZE];
        snprintf(w_buf, sizeof(w_buf), "W: %u", (unsigned)vm->wins);
        snprintf(l_buf, sizeof(l_buf), "L: %u", (unsigned)vm->losses);
        fq_draw_header_bar2(fb, font, HOME_FOOTER_Y, HOME_BAR_H, w_buf, l_buf);
    }
}
