/**
 * screen_inventory.c — FiestaQuest Presentation: Inventory Grid Renderer
 *
 * Renders a 4-column item grid onto the 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   y=0..33  : Black header bar — white "INVENTORY" text
 *   y=35..154: Item grid — 4 columns × 3 visible rows, each cell 50×40px
 *              Item sprites (24×24) centred in each cell at (+13, +8)
 *              Cursor cell drawn with thick (double) border
 *   y=155    : Separator line
 *   y=166..199: Black footer bar — white selected-item name text
 *
 * Cell geometry:
 *   Width  : 50px (4 × 50 = 200px, flush to display edges)
 *   Height : 40px (3 × 40 = 120px for the grid zone)
 *   Sprite : 24×24, blitted at (cx + 13, cy + 8) — centre in cell
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

#define INV_BAR_H          34  /**< Header / footer bar height. */

#define INV_GRID_ORIGIN_Y  35  /**< Grid top-left y. */
#define INV_CELL_W         50  /**< Cell width (4 × 50 = 200). */
#define INV_CELL_H         40  /**< Cell height. */
#define INV_GRID_COLS       4
#define INV_VISIBLE_ROWS    3  /**< 3 rows × 40 = 120px grid zone. */

/** Item sprite blit offset within cell (centres 24×24 sprite in 50×40 cell). */
#define INV_SPRITE_OFFSET_X  13
#define INV_SPRITE_OFFSET_Y   8

/** Footer y — item name tooltip. */
#define INV_FOOTER_Y  166

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
    fq_draw_header_bar(fb, font, 0, INV_BAR_H, "INVENTORY");

    /* ── Empty inventory guard ──────────────────────────────────────────── */
    if (vm->item_count == 0u) {
        /* Centred "empty" indicator lines in the grid zone. */
        int16_t mid_y = (int16_t)(INV_GRID_ORIGIN_Y + INV_CELL_H * INV_VISIBLE_ROWS / 2);
        fq_fb_draw_line(fb, 5, (int16_t)(mid_y - 1),
                        (int16_t)(FQ_FB_WIDTH - 5u), (int16_t)(mid_y - 1), 1u);
        fq_fb_draw_line(fb, 5, (int16_t)(mid_y + 1),
                        (int16_t)(FQ_FB_WIDTH - 5u), (int16_t)(mid_y + 1), 1u);
        fq_draw_header_bar(fb, font, INV_FOOTER_Y, INV_BAR_H, "Empty");
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

    /* ── Separator above footer ──────────────────────────────────────────── */
    fq_fb_draw_line(fb, 0, (int16_t)(INV_FOOTER_Y - 1),
                    (int16_t)(FQ_FB_WIDTH - 1u), (int16_t)(INV_FOOTER_Y - 1), 1u);

    /* ── Footer bar: selected item name ─────────────────────────────────── */
    if (clamped_cursor < vm->item_count) {
        fq_draw_header_bar(fb, font, INV_FOOTER_Y, INV_BAR_H,
                           vm->item_names[clamped_cursor]);
    }
}
