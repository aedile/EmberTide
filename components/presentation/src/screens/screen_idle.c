/**
 * screen_idle.c — FiestaQuest Presentation: Idle Screensaver Renderer
 *
 * Renders the idle screensaver onto the 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   y=10..105  : 3x-scaled character sprite (96x96), centered at x=52
 *                (200 - 96) / 2 = 52
 *   y=112      : "EmberTide" in title font, centered
 *   y=155      : "NAME  Lv.N" in small font, centered
 *
 * Architecture: idle is a render-layer overlay, NOT an FSM state.
 * The caller (app_main.c) sets s_idle_active; any button press clears it.
 * This renderer is pure — it receives a view model and writes pixels only.
 *
 * Frame 0 of sprite_base is always used (no walk animation during idle).
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 */

#include "screens/screen_idle.h"
#include "asset_data.h"
#include "sprite_util.h"
#include <stddef.h>
#include <stdio.h>

/* ── Layout constants ────────────────────────────────────────────────────── */

/**
 * Sprite: 3x-scaled 32x32 = 96x96, centered horizontally.
 * x = (200 - 96) / 2 = 52
 */
#define IDLE_SPRITE_X    52
#define IDLE_SPRITE_Y    10

/** "EmberTide" title font baseline. */
#define IDLE_TITLE_Y    112

/** Character name + level status line. */
#define IDLE_STATUS_Y   155

/** Status line buffer: "NAME  Lv.255" + NUL. */
#define IDLE_STATUS_BUF  24

/* ── fq_render_idle ──────────────────────────────────────────────────────── */

void fq_render_idle(fq_fb_t *fb, const fq_vm_idle_t *vm)
{
    if (fb == NULL || vm == NULL) {
        return;
    }

    const fq_font_t *title_font = fq_get_font_title();
    const fq_font_t *small_font = fq_get_font_small();

    /* Clear to white. */
    fq_fb_clear(fb);

    /* ── 3x-scaled character sprite, centered ─────────────────────────── */
    {
        const fq_sprite_t *sp = fq_get_char_sprite(vm->sprite_base, 0u);
        if (sp != NULL) {
            fq_blit_sprite_3x(fb, IDLE_SPRITE_X, IDLE_SPRITE_Y, sp);
        }
    }

    /* ── "EmberTide" title, centered ─────────────────────────────────── */
    {
        static const char s_title[] = "EmberTide";
        int16_t tw = fq_text_width(title_font, s_title);
        int16_t tx = (int16_t)((FQ_FB_WIDTH - tw) / 2);
        if (tx < 0) { tx = 0; }
        fq_draw_text(fb, title_font, tx, IDLE_TITLE_Y, s_title);
    }

    /* ── Character name + level, centered ────────────────────────────── */
    {
        char status[IDLE_STATUS_BUF];
        snprintf(status, sizeof(status), "%s  Lv.%u",
                 vm->name, (unsigned)vm->level);
        int16_t sw = fq_text_width(small_font, status);
        int16_t sx = (int16_t)((FQ_FB_WIDTH - sw) / 2);
        if (sx < 0) { sx = 0; }
        fq_draw_text(fb, small_font, sx, IDLE_STATUS_Y, status);
    }
}
