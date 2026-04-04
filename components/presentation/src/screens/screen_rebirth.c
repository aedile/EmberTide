/**
 * screen_rebirth.c — FiestaQuest Presentation: Rebirth Screen Renderer
 *
 * Renders the permadeath rebirth screen onto the 200x200 1-bit e-paper framebuffer.
 *
 * Layout (200x200 px, 1-bit):
 *   y=0..29    : Header bar — "REBIRTH" in white on black
 *   y=32..47   : "LVL NN -> 1" centered
 *   y=50..63   : "STR NN->NN  SPD NN->NN"
 *   y=66..79   : "PRC NN->NN  INT NN->NN"
 *   y=82..95   : "Tokens: N  +N earned"
 *   y=98..111  : Legacy tree: up to 8 filled/empty circles in a row
 *   y=168..199 : Footer — "[A] Token  [B] Confirm" hint
 *
 * NULL-safe: both fb and vm NULL checks at entry.
 * Constitution Priority 0: no float, no malloc, no PRNG.
 */

#include "screens/screen_rebirth.h"
#include "asset_data.h"
#include "sprite_util.h"
#include "fq_text.h"
#include "fq_framebuffer.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/* ── Layout constants ────────────────────────────────────────────────────── */
#define REBIRTH_HEADER_H    30   /**< Header bar height. */
#define REBIRTH_LINE1_Y     34   /**< Level line y. */
#define REBIRTH_LINE2_Y     50   /**< STR/SPD stat comparison y. */
#define REBIRTH_LINE3_Y     66   /**< PRC/INT stat comparison y. */
#define REBIRTH_LINE4_Y     82   /**< Tokens line y. */
#define REBIRTH_TREE_Y     100   /**< Legacy tree circles top y. */
#define REBIRTH_TREE_CIRCLE_R  6 /**< Circle radius (pixels). */
#define REBIRTH_TREE_GAP    20   /**< Circle center spacing. */
#define REBIRTH_FOOTER_Y   168   /**< Footer bar top y. */
#define REBIRTH_FOOTER_H    32   /**< Footer bar height. */

/* ── Helper: draw a small circle (8x8) using rect approximation ─────────── */
static void draw_circle_cell(fq_fb_t *fb, int16_t cx, int16_t cy,
                              uint8_t filled)
{
    int16_t r = REBIRTH_TREE_CIRCLE_R;
    int16_t x = (int16_t)(cx - r);
    int16_t y = (int16_t)(cy - r);
    int16_t d = (int16_t)(r * 2);

    /* Draw a rounded square approximation: outer outline. */
    fq_fb_draw_rect(fb, x, y, d, d, 1u);

    /* Fill if unlocked. */
    if (filled) {
        fq_fb_fill_rect(fb,
                        (int16_t)(x + 1), (int16_t)(y + 1),
                        (int16_t)(d - 2), (int16_t)(d - 2),
                        1u);
    }
}

/* ── fq_render_rebirth ───────────────────────────────────────────────────── */

void fq_render_rebirth(fq_fb_t              *fb,
                        const fq_vm_rebirth_t *vm)
{
    if (fb == NULL || vm == NULL) {
        return;
    }

    const fq_font_t *font_small = fq_get_font_small();

    fq_fb_clear(fb);

    /* ── Outer border ────────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb, 0, 0, (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* ── Header bar: "REBIRTH" ───────────────────────────────────────────── */
    fq_draw_header_bar(fb, font_small, 0, REBIRTH_HEADER_H, "REBIRTH");

    /* ── Level line ─────────────────────────────────────────────────────── */
    {
        char line[24];
        snprintf(line, sizeof(line), "LVL %u -> 1",
                 (unsigned)vm->old_level);
        int16_t w = fq_text_width(font_small, line);
        int16_t x = (int16_t)((200 - w) / 2);
        fq_draw_text(fb, font_small, x, REBIRTH_LINE1_Y, line);
    }

    /* ── Stat comparison: STR / SPD ─────────────────────────────────────── */
    {
        char line[40];
        snprintf(line, sizeof(line), "STR:%u->%u SPD:%u->%u",
                 (unsigned)vm->old_stats[0], (unsigned)vm->new_stats[0],
                 (unsigned)vm->old_stats[1], (unsigned)vm->new_stats[1]);
        int16_t w = fq_text_width(font_small, line);
        int16_t x = (int16_t)((200 - w) / 2);
        fq_draw_text(fb, font_small, x, REBIRTH_LINE2_Y, line);
    }

    /* ── Stat comparison: PRC / INT ─────────────────────────────────────── */
    {
        char line[40];
        snprintf(line, sizeof(line), "PRC:%u->%u INT:%u->%u",
                 (unsigned)vm->old_stats[2], (unsigned)vm->new_stats[2],
                 (unsigned)vm->old_stats[3], (unsigned)vm->new_stats[3]);
        int16_t w = fq_text_width(font_small, line);
        int16_t x = (int16_t)((200 - w) / 2);
        fq_draw_text(fb, font_small, x, REBIRTH_LINE3_Y, line);
    }

    /* ── Tokens line ─────────────────────────────────────────────────────── */
    {
        char line[32];
        snprintf(line, sizeof(line), "Tokens:%u +%u",
                 (unsigned)vm->tokens_available,
                 (unsigned)vm->tokens_earned);
        int16_t w = fq_text_width(font_small, line);
        int16_t x = (int16_t)((200 - w) / 2);
        fq_draw_text(fb, font_small, x, REBIRTH_LINE4_Y, line);
    }

    /* ── Legacy tree circles (up to 8 visible) ───────────────────────────── */
    {
        /* Center the 8-circle row: total width = 8 * REBIRTH_TREE_GAP = 160px,
         * starting x center = (200 - 160) / 2 + REBIRTH_TREE_GAP/2 = 30. */
        int16_t start_cx = 30;
        int16_t cy = (int16_t)(REBIRTH_TREE_Y + REBIRTH_TREE_CIRCLE_R);

        for (uint8_t i = 0u; i < 8u; i++) {
            int16_t cx = (int16_t)(start_cx + i * REBIRTH_TREE_GAP);
            uint8_t filled = (vm->legacy_tree & (1u << i)) ? 1u : 0u;
            draw_circle_cell(fb, cx, cy, filled);
        }

        /* If more than 8 nodes are unlocked, show a "..." indicator. */
        if (vm->next_node > 8u || (vm->legacy_tree >> 8u) != 0u) {
            fq_draw_text(fb, font_small,
                         (int16_t)(start_cx + 8 * REBIRTH_TREE_GAP + 2),
                         (int16_t)(REBIRTH_TREE_Y),
                         "...");
        }
    }

    /* ── Horizontal divider above footer ─────────────────────────────────── */
    fq_fb_draw_line(fb, 1, (int16_t)(REBIRTH_FOOTER_Y - 2),
                    (int16_t)(FQ_FB_WIDTH - 2u),
                    (int16_t)(REBIRTH_FOOTER_Y - 2), 1u);

    /* ── Footer bar with button hints ───────────────────────────────────── */
    fq_draw_header_bar(fb, font_small,
                       REBIRTH_FOOTER_Y, REBIRTH_FOOTER_H,
                       "[A]Token [B]Confirm");
}
