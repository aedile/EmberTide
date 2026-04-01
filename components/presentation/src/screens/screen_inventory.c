/**
 * screen_inventory.c — FiestaQuest Presentation: Inventory Grid Renderer
 *
 * Renders a 4-column item grid onto the 200x200 1-bit framebuffer.
 *
 * Grid layout (4 columns x up to 8 rows, each cell 50x35):
 *   Header row height:  10 px (rows 0-9)
 *   Grid origin Y:      10
 *   Cell width:         50 px  (4 columns → 200 px total)
 *   Cell height:        35 px
 *   Grid rows visible:  4 (rows 0-3 → y offsets 10..149)
 *
 * Cursor:
 *   clamped_cursor = (item_count > 0) ? min(cursor_index, item_count-1) : 0
 *   cell_col = clamped_cursor % 4
 *   cell_row = (clamped_cursor - scroll_offset) / 4  (scroll_offset in items)
 *   cursor rect drawn only if clamped_cursor < item_count
 *
 * Tooltip area (rows 152-175):
 *   Selected item name drawn as a horizontal rule + inner rect placeholder.
 *   Empty slot: bottom panel cleared (already cleared by fq_fb_clear).
 *
 * Constitution Priority 0: no float, no malloc, no PRNG.
 */

#include "screens/screen_inventory.h"
#include <stddef.h>

/* Grid geometry constants. */
#define INV_GRID_ORIGIN_Y  10
#define INV_CELL_W         50
#define INV_CELL_H         35
#define INV_GRID_COLS       4
#define INV_VISIBLE_ROWS    4

/* Tooltip area. */
#define INV_TOOLTIP_Y  152
#define INV_TOOLTIP_H   22

/* ---------------------------------------------------------------------------
 * fq_render_inventory
 * ---------------------------------------------------------------------------*/
void fq_render_inventory(fq_fb_t *fb, const fq_vm_inventory_t *vm)
{
    if (fb == NULL || vm == NULL) {
        return;
    }

    /* Clear to white. */
    fq_fb_clear(fb);

    /* ── Display border ─────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb, 0, 0,
                    (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* ── Header separator ────────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 0, INV_GRID_ORIGIN_Y - 1,
                    (int16_t)(FQ_FB_WIDTH - 1u), INV_GRID_ORIGIN_Y - 1, 1u);

    /* ── Empty inventory guard ──────────────────────────────────────────── */
    if (vm->item_count == 0u) {
        /* Draw an X in the grid area to indicate "no items". */
        fq_fb_draw_line(fb, 5, INV_GRID_ORIGIN_Y,
                        (int16_t)(FQ_FB_WIDTH - 5), INV_GRID_ORIGIN_Y + INV_CELL_H * INV_VISIBLE_ROWS, 1u);
        fq_fb_draw_line(fb, (int16_t)(FQ_FB_WIDTH - 5), INV_GRID_ORIGIN_Y,
                        5, INV_GRID_ORIGIN_Y + INV_CELL_H * INV_VISIBLE_ROWS, 1u);
        return;
    }

    /* ── Clamp cursor ────────────────────────────────────────────────────── */
    uint8_t clamped_cursor = vm->cursor_index;
    if (clamped_cursor >= vm->item_count) {
        clamped_cursor = (uint8_t)(vm->item_count - 1u);
    }

    /* ── Clamp scroll_offset ─────────────────────────────────────────────── */
    uint8_t scroll = vm->scroll_offset;
    /* scroll_offset is in "items" (not rows). Max meaningful: item_count - 1. */
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

        /* Filled indicator: draw a small filled rect for the rarity level. */
        if (vm->item_rarities[idx] > 0u) {
            /* 2x2 filled rarity dot at top-right of cell. */
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

    /* Tooltip placeholder rect (item name would be rendered here). */
    fq_fb_draw_rect(fb, 5, INV_TOOLTIP_Y, FQ_FB_WIDTH - 10, INV_TOOLTIP_H, 1u);
}
