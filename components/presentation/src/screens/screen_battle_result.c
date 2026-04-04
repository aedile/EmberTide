/**
 * screen_battle_result.c — FiestaQuest Presentation: Battle Result Screen Renderer
 *
 * Renders the post-combat result screen onto the 200x200 1-bit e-paper framebuffer.
 *
 * Layout (200x200 px, 1-bit):
 *   y=0..33    : Header bar (black) — "YOU WIN!" or "YOU LOSE" in white inverted text
 *   y=34..97   : Winner's sprite (2x, 64x64) centered
 *   y=100..113 : "Winner: <name>" centered, small font
 *   y=116..129 : "XP: +NNNN" centered
 *   y=132..145 : "Rounds: N" centered
 *   y=166..199 : Footer bar — "[A] Continue" hint
 *
 * NULL-safe: both fb and vm NULL checks at entry.
 * Constitution Priority 0: no float, no malloc, no PRNG.
 */

#include "screens/screen_battle_result.h"
#include "asset_data.h"
#include "sprite_util.h"
#include "fq_text.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/* ── Layout constants ────────────────────────────────────────────────────── */
#define RESULT_HEADER_H     34   /**< Header bar height. */
#define RESULT_SPRITE_X     68   /**< Sprite x: (200-64)/2 = 68 */
#define RESULT_SPRITE_Y     36   /**< Sprite y: just below header. */
#define RESULT_LINE1_Y     102   /**< Winner name row y. */
#define RESULT_LINE2_Y     116   /**< XP earned row y. */
#define RESULT_LINE3_Y     130   /**< Rounds survived row y. */
#define RESULT_FOOTER_Y    166   /**< Footer bar top y. */
#define RESULT_FOOTER_H     34   /**< Footer bar height. */

/* ── fq_render_battle_result ─────────────────────────────────────────────── */

void fq_render_battle_result(fq_fb_t                     *fb,
                              const fq_vm_battle_result_t *vm)
{
    if (fb == NULL || vm == NULL) {
        return;
    }

    const fq_font_t *font_small  = fq_get_font_small();

    fq_fb_clear(fb);

    /* ── Outer border ────────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb, 0, 0, (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* ── Header bar: "YOU WIN!" or "YOU LOSE" ────────────────────────────── */
    {
        const char *header_text = vm->you_won ? "YOU WIN!" : "YOU LOSE";
        fq_draw_header_bar(fb, font_small, 0, RESULT_HEADER_H, header_text);
    }

    /* ── Winner sprite (class sprite 0, frame 0) centered ───────────────── */
    {
        const fq_sprite_t *spr = fq_get_char_sprite(
            (uint8_t)vm->player_sprite_base, 0u);
        if (spr != NULL) {
            fq_blit_sprite_2x(fb, RESULT_SPRITE_X, RESULT_SPRITE_Y, spr);
        }
    }

    /* ── Text lines ─────────────────────────────────────────────────────── */

    /* Winner name */
    if (vm->winner_name[0] != '\0') {
        char line[24];
        snprintf(line, sizeof(line), "Winner: %.12s", vm->winner_name);
        int16_t w = fq_text_width(font_small, line);
        int16_t x = (int16_t)((200 - w) / 2);
        fq_draw_text(fb, font_small, x, RESULT_LINE1_Y, line);
    }

    /* XP earned */
    {
        char line[24];
        snprintf(line, sizeof(line), "XP: +%u", (unsigned)vm->xp_earned);
        int16_t w = fq_text_width(font_small, line);
        int16_t x = (int16_t)((200 - w) / 2);
        fq_draw_text(fb, font_small, x, RESULT_LINE2_Y, line);
    }

    /* Rounds survived */
    {
        char line[24];
        snprintf(line, sizeof(line), "Rounds: %u", (unsigned)vm->rounds_survived);
        int16_t w = fq_text_width(font_small, line);
        int16_t x = (int16_t)((200 - w) / 2);
        fq_draw_text(fb, font_small, x, RESULT_LINE3_Y, line);
    }

    /* ── Horizontal divider above footer ─────────────────────────────────── */
    fq_fb_draw_line(fb, 1, (int16_t)(RESULT_FOOTER_Y - 2),
                    (int16_t)(FQ_FB_WIDTH - 2u), (int16_t)(RESULT_FOOTER_Y - 2), 1u);

    /* ── Footer bar with button hint ─────────────────────────────────────── */
    {
        const char *hint = vm->is_dead ? "[A] Rebirth" : "[A] Home";
        fq_draw_header_bar(fb, font_small,
                           RESULT_FOOTER_Y, RESULT_FOOTER_H, hint);
    }
}
