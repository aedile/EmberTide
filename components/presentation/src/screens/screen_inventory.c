/**
 * screen_inventory.c — FiestaQuest Presentation: Inventory Grid Renderer
 *
 * Renders a 4-column item grid onto the 200x200 1-bit framebuffer.
 * Phase 18: replaced placeholder geometry with real text and item sprites.
 *
 * Grid layout (4 columns x up to 4 visible rows, each cell 50x35):
 *   Header band:     rows 0-9 ("INVENTORY" header text at x=5, y=1)
 *   Grid origin Y:   10
 *   Cell width:      50 px  (4 columns → 200 px total)
 *   Cell height:     35 px
 *   Grid rows visible: 4
 *
 * Item sprite (24×24) is blitted at (cx+13, cy+5) — centred in the 50×35 cell.
 *
 * Tooltip area (rows 152-181):
 *   Separator at row 150.
 *   Selected item name drawn via fq_draw_text at (5, 152).
 *   Rarity label ("Rarity: X") drawn below at (5, 155+glyph_h).
 *
 * Constitution Priority 0: no float, no malloc, no PRNG.
 */

#include "screens/screen_inventory.h"
#include "asset_data.h"
#include <stddef.h>
#include <stdio.h>

/* Grid geometry constants. */
#define INV_GRID_ORIGIN_Y  10
#define INV_CELL_W         50
#define INV_CELL_H         35
#define INV_GRID_COLS       4
#define INV_VISIBLE_ROWS    4

/* Item sprite blit offset within cell (centres 24×24 sprite in 50×35 cell). */
#define INV_SPRITE_OFFSET_X 13
#define INV_SPRITE_OFFSET_Y  5

/* Tooltip area. */
#define INV_TOOLTIP_Y  152

/* Header text y-position (glyph_h=30 → rows 1-30). */
#define INV_HEADER_Y    1

/* ---------------------------------------------------------------------------
 * fq_render_inventory
 * ---------------------------------------------------------------------------*/
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

    /* ── "INVENTORY" header text ─────────────────────────────────────────── */
    fq_draw_text(fb, font, 5, INV_HEADER_Y, "INVENTORY");

    /* ── Header separator ────────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 0, INV_GRID_ORIGIN_Y - 1,
                    (int16_t)(FQ_FB_WIDTH - 1u), INV_GRID_ORIGIN_Y - 1, 1u);

    /* ── Empty inventory guard ──────────────────────────────────────────── */
    if (vm->item_count == 0u) {
        /* Draw an X in the grid area to indicate "no items". */
        fq_fb_draw_line(fb, 5, INV_GRID_ORIGIN_Y,
                        (int16_t)(FQ_FB_WIDTH - 5),
                        INV_GRID_ORIGIN_Y + INV_CELL_H * INV_VISIBLE_ROWS, 1u);
        fq_fb_draw_line(fb, (int16_t)(FQ_FB_WIDTH - 5), INV_GRID_ORIGIN_Y,
                        5,
                        INV_GRID_ORIGIN_Y + INV_CELL_H * INV_VISIBLE_ROWS, 1u);
        return;
    }

    /* ── Clamp cursor ────────────────────────────────────────────────────── */
    uint8_t clamped_cursor = vm->cursor_index;
    if (clamped_cursor >= vm->item_count) {
        clamped_cursor = (uint8_t)(vm->item_count - 1u);
    }

    /* ── Clamp scroll_offset ─────────────────────────────────────────────── */
    uint8_t scroll = vm->scroll_offset;
    if (scroll > 0u && scroll >= vm->item_count) {
        scroll = (uint8_t)(vm->item_count - 1u);
    }

    /* ── Draw grid cells ─────────────────────────────────────────────────── */
    for (uint8_t idx = scroll; idx < vm->item_count; idx++) {
        uint8_t vis_idx = (uint8_t)(idx - scroll);
        uint8_t col     = vis_idx % (uint8_t)INV_GRID_COLS;
        uint8_t row     = vis_idx / (uint8_t)INV_GRID_COLS;

        /* Only render cells that fit within the visible rows. */
        if (row >= (uint8_t)INV_VISIBLE_ROWS) {
            break;
        }

        int16_t cx = (int16_t)((uint16_t)col * INV_CELL_W);
        int16_t cy = (int16_t)(INV_GRID_ORIGIN_Y + (uint16_t)row * INV_CELL_H);

        /* Cell outline. */
        fq_fb_draw_rect(fb, cx, cy, INV_CELL_W, INV_CELL_H, 1u);

        /* Item sprite — use rarity as a proxy for item visual variety.
         * item_id modulo SPRITE_ITEM_TABLE size (16) maps to available sprites. */
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

        /* Rarity dot at top-right of cell. */
        if (vm->item_rarities[idx] > 0u) {
            fq_fb_fill_rect(fb,
                            (int16_t)(cx + INV_CELL_W - 4),
                            (int16_t)(cy + 2),
                            2, 2, 1u);
        }
    }

    /* ── Draw cursor highlight ───────────────────────────────────────────── */
    if (clamped_cursor < vm->item_count) {
        uint8_t vis_idx = (clamped_cursor >= scroll)
                          ? (uint8_t)(clamped_cursor - scroll)
                          : 0u;
        uint8_t col     = vis_idx % (uint8_t)INV_GRID_COLS;
        uint8_t row     = vis_idx / (uint8_t)INV_GRID_COLS;

        if (row < (uint8_t)INV_VISIBLE_ROWS) {
            int16_t cx = (int16_t)((uint16_t)col * INV_CELL_W);
            int16_t cy = (int16_t)(INV_GRID_ORIGIN_Y + (uint16_t)row * INV_CELL_H);

            /* Thick cursor: two nested rects (outer + inner outline). */
            fq_fb_draw_rect(fb, cx, cy, INV_CELL_W, INV_CELL_H, 1u);
            fq_fb_draw_rect(fb, (int16_t)(cx + 1), (int16_t)(cy + 1),
                            INV_CELL_W - 2, INV_CELL_H - 2, 1u);
        }
    }

    /* ── Tooltip area separator ──────────────────────────────────────────── */
    fq_fb_draw_line(fb, 0, INV_TOOLTIP_Y - 2,
                    (int16_t)(FQ_FB_WIDTH - 1u), INV_TOOLTIP_Y - 2, 1u);

    /* ── Tooltip: selected item name ─────────────────────────────────────── */
    if (clamped_cursor < vm->item_count) {
        const char *item_name = vm->item_names[clamped_cursor];
        fq_draw_text(fb, font, 5, INV_TOOLTIP_Y, item_name);
    }
}
