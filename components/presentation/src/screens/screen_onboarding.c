/**
 * screen_onboarding.c — FiestaQuest Presentation: First-Boot Onboarding Screen
 *
 * Renders the character creation class selection carousel on the 200x200
 * 1-bit e-paper framebuffer.
 *
 * Layout:
 *   y=0..33   : Black header bar — white "NEW GAME" text
 *   y=36      : Inner border line
 *   y=40..73  : Class sprite (32x32, centered at x=84)
 *   y=78..93  : Class name, centered
 *   y=100..164: Stat bars — 4 rows: STR, SPD, PRC, INT
 *               Each row: label at x=5, bar at x=35..165 (130px wide)
 *               Bar fill: stat/255 * 130 (integer, no float)
 *   y=166..199: Black footer — "[PWR] Cycle  [SUN] OK"
 *
 * Constitution Priority 0: no float, no malloc, no PRNG.
 */

#include "screens/screen_onboarding.h"
#include "asset_data.h"
#include "sprite_util.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ── Layout constants ────────────────────────────────────────────────────── */

#define OB_HDR_H        34   /**< Header bar height. */
#define OB_SPRITE_Y     40   /**< Sprite top y. */
#define OB_NAME_Y       80   /**< Class name text y_param. */
#define OB_STATS_Y0    106   /**< First stat bar y (STR). */
#define OB_STAT_ROW_H   15   /**< Vertical spacing between stat bars. */
#define OB_STAT_LABEL_X  5   /**< Stat label left x. */
#define OB_STAT_BAR_X   40   /**< Stat bar left x. */
#define OB_STAT_BAR_W  130   /**< Stat bar outer width. */
#define OB_STAT_BAR_H    8   /**< Stat bar height. */
#define OB_FOOTER_Y    166   /**< Footer bar top y. */
#define OB_FOOTER_H     34   /**< Footer bar height. */

/* ── Stat labels ─────────────────────────────────────────────────────────── */
static const char * const k_stat_labels[4u] = { "STR", "SPD", "PRC", "INT" };

/* ── fq_render_onboarding ────────────────────────────────────────────────── */

void fq_render_onboarding(fq_fb_t *fb, const fq_vm_onboarding_t *vm)
{
    if (fb == NULL || vm == NULL) {
        return;
    }

    const fq_font_t *font = fq_get_font_small();

    fq_fb_clear(fb);

    /* ── Display border ─────────────────────────────────────────────────── */
    fq_fb_draw_rect(fb, 0, 0,
                    (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* ── Header bar: "NEW GAME" ─────────────────────────────────────────── */
    fq_draw_header_bar(fb, font, 0, OB_HDR_H, "NEW GAME");

    /* ── Class sprite (32x32, centered) ─────────────────────────────────── */
    {
        uint8_t sprite_id = vm->sprite_base;
        const fq_sprite_t *spr = fq_get_char_sprite(sprite_id, 0u);
        if (spr != NULL) {
            int16_t sx = (int16_t)((FQ_FB_WIDTH - 32u) / 2u); /* = 84 */
            fq_blit_sprite(fb, sx, OB_SPRITE_Y, spr);
        }
    }

    /* ── Class name, centered ───────────────────────────────────────────── */
    {
        /* class_name is at most 15 chars + null, safely fits on stack. */
        char name_buf[16];
        strncpy(name_buf, vm->class_name, 15u);
        name_buf[15] = '\0';
        int16_t w = fq_text_width(font, name_buf);
        int16_t x = (int16_t)((FQ_FB_WIDTH - (uint16_t)w) / 2u);
        if (x < 0) { x = 0; }
        fq_draw_text(fb, font, x, OB_NAME_Y, name_buf);
    }

    /* ── Stat bars (STR / SPD / PRC / INT) ──────────────────────────────── */
    {
        const uint8_t stats[4u] = {
            vm->strength,
            vm->speed,
            vm->precision,
            vm->intelligence
        };

        for (uint8_t i = 0u; i < 4u; i++) {
            int16_t row_y = (int16_t)(OB_STATS_Y0 + (int16_t)i * OB_STAT_ROW_H);

            /* Label. */
            fq_draw_text(fb, font, OB_STAT_LABEL_X, row_y, k_stat_labels[i]);

            /* Bar outline. */
            fq_fb_draw_rect(fb,
                            OB_STAT_BAR_X, row_y,
                            OB_STAT_BAR_W, OB_STAT_BAR_H, 1u);

            /* Bar fill: stat * (OB_STAT_BAR_W - 2) / 255. */
            uint32_t fill = (uint32_t)stats[i] * (uint32_t)(OB_STAT_BAR_W - 2u) / 255u;
            if (fill > (uint32_t)(OB_STAT_BAR_W - 2u)) {
                fill = (uint32_t)(OB_STAT_BAR_W - 2u);
            }
            if (fill > 0u) {
                fq_fb_fill_rect(fb,
                                OB_STAT_BAR_X + 1, row_y + 1,
                                (int16_t)fill, OB_STAT_BAR_H - 2, 1u);
            }
        }
    }

    /* ── Footer bar: nav hints ─────────────────────────────────────────── */
    fq_draw_header_bar(fb, font, OB_FOOTER_Y, OB_FOOTER_H,
                       "[SUN]Cycle [PWR]OK");
}
