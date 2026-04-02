/**
 * screen_home.c — FiestaQuest Presentation: Home Screen Renderer
 *
 * Renders the home/dashboard screen onto the 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   y=0..17   : Black header bar — white "NAME" left + "Lv.N" right
 *   y=18      : Separator line
 *   y=20..83  : 2x-scaled character sprite (64x64), centered at x=68
 *   y=86..99  : HP bar — "HP" label + proportional fill + border
 *   y=100..113: W/L record — "W:N  L:N" centered
 *   y=114     : Separator line
 *   y=116..197: Navigation menu — 4 rows, each 18px tall, with cursor ">"
 *               ☀ BTN_B cycles, ⏻ BTN_A selects
 *
 * Menu layout:
 *   Row 0 (y=116): TRAIN
 *   Row 1 (y=134): BATTLE
 *   Row 2 (y=152): ITEMS
 *   Row 3 (y=170): STATS
 *   Highlighted row: black fill_rect + white (inverted) text + ">" cursor
 *   Normal rows: plain black text
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

/** Header bar height — fits glyph_h=12 with 3px top+bottom pad. */
#define HOME_HDR_H           18

/** Sprite destination: 2x-scaled 32x32 = 64x64, centered horizontally. */
#define HOME_SPRITE_X        68   /* (200 - 64) / 2 = 68 */
#define HOME_SPRITE_Y        20

/** HP bar geometry. */
#define HOME_HP_BAR_X         4
#define HOME_HP_BAR_Y        86
#define HOME_HP_BAR_W       192
#define HOME_HP_BAR_H        14
#define HOME_HP_FILL_MAX_W  188u  /**< Inner fill pixels at 100% HP. */

/** Win/loss text row. */
#define HOME_WL_Y           100

/** Separator before menu. */
#define HOME_SEP_Y          114

/** Menu geometry: 4 rows of 18px each starting at y=116. */
#define HOME_MENU_ORIGIN_Y  116
#define HOME_MENU_ROW_H      18
#define HOME_MENU_ITEM_COUNT  4

/** Menu text indentation. */
#define HOME_MENU_CURSOR_X    4   /**< ">" cursor left edge. */
#define HOME_MENU_TEXT_X     16   /**< Item label left edge. */

/*
 * Win/loss/level buffer sizes.
 * wins/losses are uint16_t (max 65535, 5 digits) + label + NUL.
 * "W:65535  L:65535" + NUL = 18 chars → use 24 for margin.
 */
#define HOME_WL_BUF_SIZE     24
#define HOME_LV_BUF_SIZE      8

/* Menu labels in order: TRAIN(0), BATTLE(1), ITEMS(2), STATS(3). */
static const char * const s_menu_labels[HOME_MENU_ITEM_COUNT] = {
    "TRAIN",
    "BATTLE",
    "ITEMS",
    "STATS",
};

/* ── fq_render_home ──────────────────────────────────────────────────────── */

void fq_render_home(fq_fb_t *fb, const fq_vm_home_t *vm)
{
    if (fb == NULL || vm == NULL) {
        return;
    }

    const fq_font_t *font = fq_get_font_small();

    /* Clamp menu_index to valid range [0, HOME_MENU_ITEM_COUNT-1]. */
    uint8_t sel = (vm->menu_index < (uint8_t)HOME_MENU_ITEM_COUNT)
                  ? vm->menu_index
                  : 0u;

    /* Clear to white. */
    fq_fb_clear(fb);

    /* ── Header bar: black fill, white text ─────────────────────────────── */
    {
        char lv_buf[HOME_LV_BUF_SIZE];
        snprintf(lv_buf, sizeof(lv_buf), "Lv.%u", (unsigned)vm->level);
        fq_draw_header_bar2(fb, font, 0, HOME_HDR_H, vm->name, lv_buf);
    }

    /* ── Separator under header ─────────────────────────────────────────── */
    fq_fb_draw_line(fb, 0, HOME_HDR_H,
                    (int16_t)(FQ_FB_WIDTH - 1u), HOME_HDR_H, 1u);

    /* ── 2x-scaled character sprite ─────────────────────────────────────── */
    {
        const fq_sprite_t *sp = fq_get_char_sprite(vm->sprite_base, 0u);
        if (sp != NULL) {
            fq_blit_sprite_2x(fb, HOME_SPRITE_X, HOME_SPRITE_Y, sp);
        }
    }

    /* ── HP bar ──────────────────────────────────────────────────────────── */
    /* Outline. */
    fq_fb_draw_rect(fb, HOME_HP_BAR_X, HOME_HP_BAR_Y,
                    HOME_HP_BAR_W, HOME_HP_BAR_H, 1u);

    /* Proportional fill (integer math, no float). */
    {
        uint32_t pct    = (vm->hp_percent > 100u) ? 100u : (uint32_t)vm->hp_percent;
        uint32_t fill_w = pct * HOME_HP_FILL_MAX_W / 100u;
        if (fill_w > HOME_HP_FILL_MAX_W) { fill_w = HOME_HP_FILL_MAX_W; }
        if (fill_w > 0u) {
            fq_fb_fill_rect(fb,
                            HOME_HP_BAR_X + 1, HOME_HP_BAR_Y + 1,
                            (int16_t)fill_w, HOME_HP_BAR_H - 2, 1u);
        }
    }

    /* "HP" label in white patch at left of bar interior. */
    fq_fb_fill_rect(fb,
                    HOME_HP_BAR_X + 1, HOME_HP_BAR_Y + 1,
                    24, HOME_HP_BAR_H - 2, 0u);
    fq_draw_text(fb, font, HOME_HP_BAR_X + 2, HOME_HP_BAR_Y + 1, "HP");

    /* ── W/L record centered ─────────────────────────────────────────────── */
    {
        char wl_buf[HOME_WL_BUF_SIZE];
        snprintf(wl_buf, sizeof(wl_buf), "W:%-5u L:%u",
                 (unsigned)vm->wins, (unsigned)vm->losses);
        int16_t wl_w = fq_text_width(font, wl_buf);
        int16_t wl_x = (int16_t)((FQ_FB_WIDTH - wl_w) / 2);
        if (wl_x < 0) { wl_x = 0; }
        fq_draw_text(fb, font, wl_x, HOME_WL_Y, wl_buf);
    }

    /* ── Separator before menu ───────────────────────────────────────────── */
    fq_fb_draw_line(fb, 0, HOME_SEP_Y,
                    (int16_t)(FQ_FB_WIDTH - 1u), HOME_SEP_Y, 1u);

    /* ── Navigation menu ─────────────────────────────────────────────────── */
    for (uint8_t i = 0u; i < (uint8_t)HOME_MENU_ITEM_COUNT; i++) {
        int16_t row_y = (int16_t)(HOME_MENU_ORIGIN_Y + (int16_t)i * HOME_MENU_ROW_H);

        if (i == sel) {
            /* Highlighted row: black background, white text + ">" cursor. */
            fq_fb_fill_rect(fb, 2, row_y,
                            (int16_t)(FQ_FB_WIDTH - 4u), HOME_MENU_ROW_H - 2, 1u);
            fq_draw_text_inverted(fb, font, HOME_MENU_CURSOR_X, row_y + 1,
                                  ">");
            fq_draw_text_inverted(fb, font, HOME_MENU_TEXT_X, row_y + 1,
                                  s_menu_labels[i]);
        } else {
            /* Normal row: plain black text. */
            fq_draw_text(fb, font, HOME_MENU_TEXT_X, row_y + 1,
                         s_menu_labels[i]);
        }
    }
}
