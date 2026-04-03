/**
 * screen_inventory.c — FiestaQuest Presentation: Inventory Grid Renderer
 *
 * Renders a 4-column item grid onto the 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   y=0..33   : Black header bar — white "INVENTORY" text
 *   y=35..145 : Item grid — 4 columns × 3 visible rows, each cell 50×37px
 *               Item sprites (24×24) centred in each cell at (+13, +6)
 *               Cursor cell drawn with thick (double) border
 *   y=146     : Grid bottom separator line
 *   y=148..171: Bordered tooltip panel — selected item name in content area
 *               Text y_param=149: visible at y≈158..170 (font off_y=9)
 *   y=174     : Footer separator line
 *   y=177..199: Black footer bar — white "[PWR] Back" only
 *               Text at y=179: visible at y≈188..200 (font off_y=9)
 *
 * Cell geometry:
 *   Width  : 50px (4 × 50 = 200px, flush to display edges)
 *   Height : 37px (3 × 37 = 111px for the grid zone)
 *   Sprite : 24×24, blitted at (cx + 13, cy + 6) — centre in cell
 *
 * Font note: FONT_REGS_12 has glyph_h=30 with off_y=9, so visible glyph
 * content appears at (y_param + 9) to (y_param + 21).
 *
 * NULL-safe: fq_render_inventory(NULL, ...) is a silent no-op.
 * Constitution Priority 0: no float, no malloc, no PRNG.
 */

#include "screens/screen_inventory.h"
#include "asset_data.h"
#include "sprite_util.h"
#include <stddef.h>
#include <stdio.h>

/* ── Grid geometry constants ─────────────────────────────────────────────── */

#define INV_HDR_H          34  /**< Header bar height. */

#define INV_GRID_ORIGIN_Y  35  /**< Grid top-left y. */
#define INV_CELL_W         50  /**< Cell width (4 × 50 = 200). */
#define INV_CELL_H         37  /**< Cell height (3 × 37 = 111px grid zone). */
#define INV_GRID_COLS       4
#define INV_VISIBLE_ROWS    3  /**< 3 rows × 37 = 111px grid zone. */

/** Item sprite blit offset within cell (centres 24×24 sprite in 50×37 cell). */
#define INV_SPRITE_OFFSET_X  13
#define INV_SPRITE_OFFSET_Y   6

/**
 * Grid bottom separator — immediately below the grid zone.
 * y = INV_GRID_ORIGIN_Y + INV_VISIBLE_ROWS * INV_CELL_H = 35 + 111 = 146.
 */
#define INV_GRID_SEP_Y     146

/**
 * Tooltip bordered panel: selected item name displayed in the content area.
 * Panel top y=148, height=24px → panel bottom y=171.
 * Text y_param=149: visible at y=158..170 (off_y=9), fits inside panel.
 */
#define INV_TOOLTIP_Y      148  /**< Panel top. */
#define INV_TOOLTIP_H       24  /**< Panel height — contains 12px visible text. */
#define INV_TOOLTIP_TEXT_Y  149 /**< Text y_param inside the tooltip box. */

/**
 * Footer separator and bar.
 * Separator at y=174, footer at y=177 (height=23 fills to y=200).
 * Footer text at y=179: visible at y=188..200, fully within display.
 */
#define INV_FOOTER_SEP_Y   174  /**< Separator line above footer. */
#define INV_FOOTER_Y       177  /**< Footer bar top. */
#define INV_FOOTER_H        23  /**< Footer bar height — fills y=177..199. */

/* ── fq_render_inventory ─────────────────────────────────────────────────── */

void fq_render_inventory(fq_fb_t *fb, const fq_vm_inventory_t *vm)
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

    /* ── Header bar: "INVENTORY" ────────────────────────────────────────── */
    fq_draw_header_bar(fb, font, 0, INV_HDR_H, "INVENTORY");

    /* ── Empty inventory guard ──────────────────────────────────────────── */
    if (vm->item_count == 0u) {
        /* Centred "empty" indicator lines in the grid zone. */
        int16_t mid_y = (int16_t)(INV_GRID_ORIGIN_Y
                                   + INV_CELL_H * INV_VISIBLE_ROWS / 2);
        fq_fb_draw_line(fb, 5, (int16_t)(mid_y - 1),
                        (int16_t)(FQ_FB_WIDTH - 5u), (int16_t)(mid_y - 1), 1u);
        fq_fb_draw_line(fb, 5, (int16_t)(mid_y + 1),
                        (int16_t)(FQ_FB_WIDTH - 5u), (int16_t)(mid_y + 1), 1u);
        /* Tooltip: "Empty" */
        fq_fb_draw_rect(fb, 4, INV_TOOLTIP_Y,
                        (int16_t)(FQ_FB_WIDTH - 8u), INV_TOOLTIP_H, 1u);
        fq_draw_text(fb, font, 8, INV_TOOLTIP_TEXT_Y, "Empty");
        /* Footer: nav hint only */
        fq_draw_header_bar(fb, font, INV_FOOTER_Y, INV_FOOTER_H, "[PWR] Back");
        return;
    }

    /* ── Clamp cursor & scroll ──────────────────────────────────────────── */
    uint8_t clamped_cursor = vm->cursor_index;
    if (clamped_cursor >= vm->item_count) {
        clamped_cursor = (uint8_t)(vm->item_count - 1u);
    }

    uint8_t scroll = vm->scroll_offset;
    if (scroll > 0u && scroll >= vm->item_count) {
        scroll = (uint8_t)(vm->item_count - 1u);
    }

    /* ── Draw grid cells ─────────────────────────────────────────────────── */
    for (uint8_t idx = scroll; idx < vm->item_count; idx++) {
        uint8_t vis_idx = (uint8_t)(idx - scroll);
        uint8_t col     = vis_idx % (uint8_t)INV_GRID_COLS;
        uint8_t row     = vis_idx / (uint8_t)INV_GRID_COLS;

        if (row >= (uint8_t)INV_VISIBLE_ROWS) { break; }

        int16_t cx = (int16_t)((uint16_t)col * INV_CELL_W);
        int16_t cy = (int16_t)(INV_GRID_ORIGIN_Y + (uint16_t)row * INV_CELL_H);

        /* Cell outline. */
        fq_fb_draw_rect(fb, cx, cy, INV_CELL_W, INV_CELL_H, 1u);

        /* Item sprite — map item index to available sprites (mod 16). */
        {
            uint8_t sprite_id = (uint8_t)(idx % 16u);
            const fq_sprite_t *sp = fq_get_item_sprite(sprite_id);
            if (sp != NULL) {
                fq_blit_sprite(fb,
                               (int16_t)(cx + INV_SPRITE_OFFSET_X),
                               (int16_t)(cy + INV_SPRITE_OFFSET_Y),
                               sp);
            }
        }

        /* Rarity dot: small filled square at top-right of cell. */
        if (vm->item_rarities[idx] > 0u) {
            fq_fb_fill_rect(fb,
                            (int16_t)(cx + INV_CELL_W - 5),
                            (int16_t)(cy + 3),
                            3, 3, 1u);
        }
    }

    /* ── Cursor highlight (double-thick border) ──────────────────────────── */
    if (clamped_cursor < vm->item_count) {
        uint8_t vis_idx = (clamped_cursor >= scroll)
                          ? (uint8_t)(clamped_cursor - scroll)
                          : 0u;
        uint8_t col = vis_idx % (uint8_t)INV_GRID_COLS;
        uint8_t row = vis_idx / (uint8_t)INV_GRID_COLS;

        if (row < (uint8_t)INV_VISIBLE_ROWS) {
            int16_t cx = (int16_t)((uint16_t)col * INV_CELL_W);
            int16_t cy = (int16_t)(INV_GRID_ORIGIN_Y + (uint16_t)row * INV_CELL_H);

            /* Outer cursor rect (already drawn above as cell outline). */
            fq_fb_draw_rect(fb, cx, cy, INV_CELL_W, INV_CELL_H, 1u);
            /* Inner inset for double-border effect. */
            fq_fb_draw_rect(fb, (int16_t)(cx + 2), (int16_t)(cy + 2),
                            INV_CELL_W - 4, INV_CELL_H - 4, 1u);
        }
    }

    /* ── Grid bottom separator ───────────────────────────────────────────── */
    fq_fb_draw_line(fb, 0, INV_GRID_SEP_Y,
                    (int16_t)(FQ_FB_WIDTH - 1u), INV_GRID_SEP_Y, 1u);

    /* ── Tooltip panel: selected item name in content area ──────────────── */
    /* Bordered rectangle. Text at y_param=149: visible glyph at y=158..170. */
    fq_fb_draw_rect(fb, 4, INV_TOOLTIP_Y,
                    (int16_t)(FQ_FB_WIDTH - 8u), INV_TOOLTIP_H, 1u);
    if (clamped_cursor < vm->item_count) {
        fq_draw_text(fb, font, 8, INV_TOOLTIP_TEXT_Y,
                     vm->item_names[clamped_cursor]);
    }

    /* ── Footer separator ────────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 0, INV_FOOTER_SEP_Y,
                    (int16_t)(FQ_FB_WIDTH - 1u), INV_FOOTER_SEP_Y, 1u);

    /* ── Footer bar: nav hint ONLY — no item name collision ─────────────── */
    fq_draw_header_bar(fb, font, INV_FOOTER_Y, INV_FOOTER_H, "[PWR] Back");
}
