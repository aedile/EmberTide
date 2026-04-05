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
 *   y=116..179: Navigation menu — 4 rows, each 16px tall, with cursor "> " prefix
 *               ☀ BTN_B cycles, ⏻ BTN_A selects
 *   y=178     : Nav hint y parameter — "[PWR] Move  [SUN] OK" visible ~y=187-199
 *
 * Menu layout (row_h=16, 4 rows from y=116 to y=180):
 *   Row 0 (y=116): TRAIN
 *   Row 1 (y=132): BATTLE
 *   Row 2 (y=148): ITEMS
 *   Row 3 (y=164): STATS
 *   Highlighted row: black fill_rect extending 1px above row baseline
 *                    + white (inverted) text with extra left indent + ">  " cursor
 *   Normal rows: plain black text with same indent
 *
 * Font note: FONT_REGS_12 has glyph_h=30 but off_y=9 for most glyphs,
 * so visible content appears at (y + 9) to (y + 21) — 12px of visible text.
 * All y coordinates in this file follow that convention.
 *
 * HP bar formula (integer-only, no float):
 *   fill_w = hp_percent (0-100) * HOME_HP_FILL_MAX_W / 100
 *   Clamped to [0, HOME_HP_FILL_MAX_W].
 *
 * Phase-19.5: vm->anim_frame selects the sprite frame for walk animation.
 * anim_frame is clamped to [0, HOME_ANIM_FRAME_MAX] defensively — the
 * application layer should only supply 0 or 2, but we guard either way.
 * If fq_get_char_sprite() returns NULL for the requested frame, the blit
 * is skipped (no crash).
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

/**
 * Menu geometry: 4 rows of 16px each starting at y=116.
 * 4 × 16 = 64px → menu occupies y=116..180.
 * Row spacing reduced from 18→16 to free up room for the nav hint below.
 */
#define HOME_MENU_ORIGIN_Y  102
#define HOME_MENU_ROW_H      16
#define HOME_MENU_ITEM_COUNT  4

/**
 * Menu text indentation.
 * cursor_x: left edge of the ">  " cursor arrow.
 * text_x  : left edge of the item label (extra 4px vs previous 16px baseline).
 */
#define HOME_MENU_CURSOR_X    4   /**< ">" cursor left edge. */
#define HOME_MENU_TEXT_X     20   /**< Item label left edge — 4px extra indent. */

/**
 * Highlight bar padding: extends 1px above the row baseline for visual breathing
 * room.  Total highlight height = HOME_MENU_ROW_H + HOME_MENU_HL_EXTRA.
 */
#define HOME_MENU_HL_EXTRA    1   /**< Extra pixels above row_y for highlight. */

/**
 * Nav hint y parameter.
 * FONT_REGS_12 has off_y=9, so visible content appears at y+9.
 * y=178 → visible glyph at y=187..199 — fully within the 200px display.
 * Last menu row (STATS) text visible at y=173..185, leaving a 2px gap.
 */
#define HOME_NAV_HINT_Y     178

/*
 * Win/loss/level buffer sizes.
 * wins/losses are uint16_t (max 65535, 5 digits) + label + NUL.
 * "W:65535  L:65535" + NUL = 18 chars → use 24 for margin.
 */
#define HOME_WL_BUF_SIZE     24
#define HOME_LV_BUF_SIZE      8

/**
 * Animation frame clamp: valid sprite frames are 0-7 for a standard 8-frame
 * sprite sheet. Values >= HOME_ANIM_FRAME_MAX are clamped to 0 (frame 0 is
 * always the safe default).
 */
#define HOME_ANIM_FRAME_MAX   8u

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

    /* Clamp anim_frame to valid range [0, HOME_ANIM_FRAME_MAX-1].
     * The renderer accepts any value; out-of-range falls back to frame 0.
     * fq_get_char_sprite() returns NULL for missing frames — blit is skipped. */
    uint8_t frame = (vm->anim_frame < (uint8_t)HOME_ANIM_FRAME_MAX)
                    ? vm->anim_frame
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

    /* ── 2x-scaled character sprite, frame selected by anim_frame ───────── */
    {
        const fq_sprite_t *sp = fq_get_char_sprite(vm->sprite_base, (uint32_t)frame);
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

    /* ── Separator before menu ───────────────────────────────────────────── */
    fq_fb_draw_line(fb, 0, (int16_t)(HOME_HP_BAR_Y + HOME_HP_BAR_H + 4),
                    (int16_t)(FQ_FB_WIDTH - 1u),
                    (int16_t)(HOME_HP_BAR_Y + HOME_HP_BAR_H + 4), 1u);

    /* ── Navigation menu ─────────────────────────────────────────────────── */
    for (uint8_t i = 0u; i < (uint8_t)HOME_MENU_ITEM_COUNT; i++) {
        int16_t row_y = (int16_t)(HOME_MENU_ORIGIN_Y + (int16_t)i * HOME_MENU_ROW_H);

        if (i == sel) {
            /* Highlighted row: ">" cursor + label, with a thick border box
             * around the entire row for clear visual selection. */
            fq_draw_text(fb, font, HOME_MENU_CURSOR_X, row_y, ">");
            fq_draw_text(fb, font, HOME_MENU_TEXT_X, row_y,
                         s_menu_labels[i]);
            /* Thick selection box around this row. */
            int16_t box_y = (int16_t)(row_y - 1);
            int16_t box_h = (int16_t)(HOME_MENU_ROW_H + 2);
            fq_fb_draw_rect(fb, 2, box_y,
                            (int16_t)(FQ_FB_WIDTH - 4u), box_h, 1u);
            fq_fb_draw_rect(fb, 3, (int16_t)(box_y + 1),
                            (int16_t)(FQ_FB_WIDTH - 6u), (int16_t)(box_h - 2), 1u);
        } else {
            /* Normal row: plain black text with matching indent. */
            fq_draw_text(fb, font, HOME_MENU_TEXT_X, row_y,
                         s_menu_labels[i]);
        }
    }

    /* ── Nav hint below menu: tells user what buttons do ────────────────── */
    /*
     * Rendered at HOME_NAV_HINT_Y=178.  With FONT_REGS_12 off_y=9, the
     * visible glyph content appears at y=187..199 — fully within the display.
     */
    {
        static const char s_nav_hint[] = "PWR Move  SUN OK";
        int16_t hint_w = fq_text_width(font, s_nav_hint);
        int16_t hint_x = (int16_t)((FQ_FB_WIDTH - hint_w) / 2);
        if (hint_x < 0) { hint_x = 0; }
        fq_draw_text(fb, font, hint_x, HOME_NAV_HINT_Y, s_nav_hint);
    }
}
