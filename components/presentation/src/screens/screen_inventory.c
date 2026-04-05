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
 *               Equipped cell: small "E" marker at top-left of cell
 *   y=146     : Grid bottom separator line
 *   y=148..171: Bordered tooltip panel — selected item name
 *   y=174     : Footer separator line
 *   y=177..199: Black footer bar — "[SUN] Cycle  [PWR] Equip  ²×[SUN] Back"
 *
 * Phase-19 additions:
 *   - item_equipped[cursor] check: draws inverted "E" marker on equipped cells.
 *   - "FULL" overlay text shown when equipped_count == max and cursor item
 *     is not equipped (caller sets this via vm->equipped_count when all slots
 *     are full). Actually the renderer shows a "FULL" message by checking
 *     how many equipped flags are set vs equipped_count.
 *
 * NULL-safe: fq_render_inventory(NULL, ...) is a silent no-op.
 * Constitution Priority 0: no float, no malloc, no PRNG.
 */

#include "screens/screen_inventory.h"
#include "asset_data.h"
#include "sprite_util.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* ── Grid geometry constants ─────────────────────────────────────────────── */

#define INV_HDR_H          34  /**< Header bar height. */

#define INV_GRID_ORIGIN_Y  35  /**< Grid top-left y. */
#define INV_CELL_W         50  /**< Cell width (4 × 50 = 200). */
#define INV_CELL_H         37  /**< Cell height. */
#define INV_GRID_COLS       4
#define INV_VISIBLE_ROWS    3

#define INV_SPRITE_OFFSET_X  13
#define INV_SPRITE_OFFSET_Y   6

#define INV_GRID_SEP_Y     146

#define INV_TOOLTIP_Y      148
#define INV_TOOLTIP_H       24
#define INV_TOOLTIP_TEXT_Y  149

#define INV_FOOTER_SEP_Y   174
#define INV_FOOTER_Y       177
#define INV_FOOTER_H        23

/* ── fq_render_inventory ─────────────────────────────────────────────────── */

void fq_render_inventory(fq_fb_t *fb, const fq_vm_inventory_t *vm)
{
    if (fb == NULL || vm == NULL) {
        return;
    }

    const fq_font_t *font = fq_get_font_small();

    fq_fb_clear(fb);

    /* ── Display border ─────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb, 0, 0,
                    (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* ── Header bar: "INVENTORY" ────────────────────────────────────────── */
    fq_draw_header_bar(fb, font, 0, INV_HDR_H, "INVENTORY");

    /* ── Empty inventory guard ──────────────────────────────────────────── */
    if (vm->item_count == 0u) {
        int16_t mid_y = (int16_t)(INV_GRID_ORIGIN_Y
                                   + INV_CELL_H * INV_VISIBLE_ROWS / 2);
        fq_fb_draw_line(fb, 5, (int16_t)(mid_y - 1),
                        (int16_t)(FQ_FB_WIDTH - 5u), (int16_t)(mid_y - 1), 1u);
        fq_fb_draw_line(fb, 5, (int16_t)(mid_y + 1),
                        (int16_t)(FQ_FB_WIDTH - 5u), (int16_t)(mid_y + 1), 1u);
        fq_fb_draw_rect(fb, 4, INV_TOOLTIP_Y,
                        (int16_t)(FQ_FB_WIDTH - 8u), INV_TOOLTIP_H, 1u);
        fq_draw_text(fb, font, 8, INV_TOOLTIP_TEXT_Y, "Empty");
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

    /* ── Count currently equipped items for FULL detection ─────────────── */
    uint8_t eq_count = 0u;
    for (uint8_t i = 0u; i < vm->item_count && i < 32u; i++) {
        if (vm->item_equipped[i]) { eq_count++; }
    }
    /* max slots from vm->equipped_count (the MAX SLOT LIMIT). Clamp to 5. */
    uint8_t max_slots = (vm->equipped_count > 5u) ? 5u : vm->equipped_count;
    uint8_t slots_full = (eq_count >= max_slots && max_slots > 0u) ? 1u : 0u;

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

        /* Item sprite. */
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
        if (idx < 32u && vm->item_rarities[idx] > 0u) {
            fq_fb_fill_rect(fb,
                            (int16_t)(cx + INV_CELL_W - 5),
                            (int16_t)(cy + 3),
                            3, 3, 1u);
        }

        /* Equipped indicator: inverted small "E" marker at top-left. */
        if (idx < 32u && vm->item_equipped[idx]) {
            /* Draw a small filled 7x7 square at top-left as equipped marker. */
            fq_fb_fill_rect(fb,
                            (int16_t)(cx + 1),
                            (int16_t)(cy + 1),
                            7, 7, 1u);
            /* Draw "E" in white (color 0) on the black marker. */
            fq_draw_text(fb, font, (int16_t)(cx + 2), (int16_t)(cy - 6), "E");
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

            fq_fb_draw_rect(fb, cx, cy, INV_CELL_W, INV_CELL_H, 1u);
            fq_fb_draw_rect(fb, (int16_t)(cx + 2), (int16_t)(cy + 2),
                            INV_CELL_W - 4, INV_CELL_H - 4, 1u);
        }
    }

    /* ── Grid bottom separator ───────────────────────────────────────────── */
    fq_fb_draw_line(fb, 0, INV_GRID_SEP_Y,
                    (int16_t)(FQ_FB_WIDTH - 1u), INV_GRID_SEP_Y, 1u);

    /* ── Tooltip panel ──────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb, 4, INV_TOOLTIP_Y,
                    (int16_t)(FQ_FB_WIDTH - 8u), INV_TOOLTIP_H, 1u);
    if (clamped_cursor < vm->item_count) {
        /* Show "FULL" if slots are all taken and cursor item is not equipped. */
        if (slots_full && clamped_cursor < 32u &&
            !vm->item_equipped[clamped_cursor]) {
            fq_draw_text(fb, font, 8, INV_TOOLTIP_TEXT_Y, "FULL");
        } else {
            fq_draw_text(fb, font, 8, INV_TOOLTIP_TEXT_Y,
                         vm->item_names[clamped_cursor]);
        }
    }

    /* ── Footer separator ────────────────────────────────────────────────── */
    fq_fb_draw_line(fb, 0, INV_FOOTER_SEP_Y,
                    (int16_t)(FQ_FB_WIDTH - 1u), INV_FOOTER_SEP_Y, 1u);

    /* ── Footer bar: 2×[SUN]=back hint ─────────────────────────────────── */
    fq_draw_header_bar(fb, font, INV_FOOTER_Y, INV_FOOTER_H,
                       "[PWR]Cyc [SUN]Eq 2x[PWR]Back");
}
