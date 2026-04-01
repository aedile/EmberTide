/**
 * screen_home.c — FiestaQuest Presentation: Home Screen Renderer
 *
 * Renders the home/dashboard screen onto the 200x200 1-bit framebuffer.
 * Phase 18: replaced all placeholder geometry with real text and sprite calls.
 *
 * Layout (200x200 e-paper, all coordinates inclusive top-left):
 *   - Full display border:  (0,0) w=200 h=200
 *   - Name text:            fq_draw_text at (5, 3)
 *   - Level text "Lv.XX":   fq_draw_text at (130, 3)
 *   - Separator line:       y=34 (below glyph_h=30 + 1px gap)
 *   - Character sprite:     fq_blit_sprite at (84, 37) — 32×32 px
 *   - HP bar outline:       (10, 75) w=180 h=12
 *   - HP bar fill:          (11, 76) proportional, inner w=178 max
 *   - "HP" label:           fq_draw_text at (12, 76) over the bar
 *   - Separator line:       y=93
 *   - Wins text "W:XXX":    fq_draw_text at (10, 96)
 *   - Losses text "L:XXX":  fq_draw_text at (105, 96)
 *   - Separator line:       y=130
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 */

#include "screens/screen_home.h"
#include "asset_data.h"
#include <stddef.h>
#include <stdio.h>

/* HP bar geometry constants. */
#define HOME_HP_BAR_X       10
#define HOME_HP_BAR_Y       75
#define HOME_HP_BAR_W      180
#define HOME_HP_BAR_H       12
#define HOME_HP_FILL_MAX_W 178u  /**< Inner fill width at 100%. */

/* ---------------------------------------------------------------------------
 * fq_render_home
 * ---------------------------------------------------------------------------*/
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

    /* ── Character name ─────────────────────────────────────────────────── */
    fq_draw_text(fb, font, 5, 3, vm->name);

    /* ── Level "Lv.XX" ──────────────────────────────────────────────────── */
    {
        char lv_buf[8];
        snprintf(lv_buf, sizeof(lv_buf), "Lv.%u", (unsigned)vm->level);
        fq_draw_text(fb, font, 130, 3, lv_buf);
    }

    /* ── Separator under header ─────────────────────────────────────────── */
    fq_fb_draw_line(fb, 5, 34, 194, 34, 1u);

    /* ── Character sprite ───────────────────────────────────────────────── */
    {
        const fq_sprite_t *sp = fq_get_char_sprite(vm->sprite_base, 0u);
        if (sp != NULL) {
            fq_blit_sprite(fb, 84, 37, sp);
        }
    }

    /* ── HP bar outline ─────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb,
                    HOME_HP_BAR_X, HOME_HP_BAR_Y,
                    HOME_HP_BAR_W, HOME_HP_BAR_H, 1u);

    /* Fill proportional to hp_percent (integer-only, no float).
     * hp_percent is 0-100.  Clamp to 100 before multiplying to prevent
     * overflow when caller supplies out-of-range values (e.g. 255). */
    {
        uint32_t pct_clamped = (vm->hp_percent > 100u) ? 100u : (uint32_t)vm->hp_percent;
        uint32_t fill_w = pct_clamped * HOME_HP_FILL_MAX_W / 100u;
        if (fill_w > HOME_HP_FILL_MAX_W) {
            fill_w = HOME_HP_FILL_MAX_W;
        }
        if (fill_w > 0u) {
            fq_fb_fill_rect(fb,
                            HOME_HP_BAR_X + 1, HOME_HP_BAR_Y + 1,
                            (int16_t)fill_w, HOME_HP_BAR_H - 2, 1u);
        }
    }

    /* ── "HP" label (drawn last so it renders over the fill) ─────────────── */
    /* Invert a small region so "HP" text is visible on the filled bar.
     * We draw white text by using a fill_rect to clear that zone first,
     * then draw the label text. */
    fq_fb_fill_rect(fb,
                    HOME_HP_BAR_X + 1, HOME_HP_BAR_Y + 1,
                    26, HOME_HP_BAR_H - 2, 0u);  /* white patch */
    fq_draw_text(fb, font, HOME_HP_BAR_X + 2, HOME_HP_BAR_Y + 1, "HP");

    /* ── Separator ──────────────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 5, 93, 194, 93, 1u);

    /* ── Wins text "W:XXX" ──────────────────────────────────────────────── */
    {
        char w_buf[8];
        snprintf(w_buf, sizeof(w_buf), "W:%u", (unsigned)vm->wins);
        fq_draw_text(fb, font, 10, 96, w_buf);
    }

    /* ── Losses text "L:XXX" ────────────────────────────────────────────── */
    {
        char l_buf[8];
        snprintf(l_buf, sizeof(l_buf), "L:%u", (unsigned)vm->losses);
        fq_draw_text(fb, font, 105, 96, l_buf);
    }

    /* ── Bottom separator ───────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 5, 130, 194, 130, 1u);
}
