/**
 * screen_inventory.h — FiestaQuest Presentation: Inventory Screen Renderer
 *
 * Renders the inventory grid screen onto a 200x200 1-bit framebuffer.
 *
 * Layout (200x200 e-paper):
 *   Row  0-9   : "INVENTORY" header label area
 *   Row 10-149 : 4x4 item grid (40x35 pixels per cell, 4 columns x 4 rows)
 *   Row 150-175: Selected item tooltip / name panel
 *   Row 176-199: Empty slot / count info
 *
 * Grid cell computation:
 *   col = cursor_index % 4
 *   row = cursor_index / 4
 *   x   = col * 50
 *   y   = 10 + row * 35
 *
 * Cursor clamped: if cursor_index >= item_count, no cursor is drawn.
 * Empty inventory: draws "No items" placeholder (empty grid, no cursor).
 *
 * NULL-safe: both pointer arguments are checked before use.
 *
 * HOST-COMPILABLE — no hal_*.h, no game/ headers.
 */

#ifndef FIESTAQUEST_PRESENTATION_SCREEN_INVENTORY_H
#define FIESTAQUEST_PRESENTATION_SCREEN_INVENTORY_H

#include "fq_framebuffer.h"
#include "view_models.h"

/**
 * fq_render_inventory — Render the inventory grid screen onto a framebuffer.
 *
 * @param fb  Target framebuffer. NULL-safe.
 * @param vm  Inventory view model (read-only). NULL-safe.
 */
void fq_render_inventory(fq_fb_t *fb, const fq_vm_inventory_t *vm);

#endif /* FIESTAQUEST_PRESENTATION_SCREEN_INVENTORY_H */
