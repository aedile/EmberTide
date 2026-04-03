/**
 * ui_widgets.c — FiestaQuest Presentation Layer: Reusable UI Widgets
 *
 * Implements word-wrapping dialogue box overlay.
 *
 * Dialogue box layout (overlays y=80..199 on the 200x200 display):
 *   Outer rect:  (2, 80) → (197, 197)
 *   Inner rect:  (4, 82) → (195, 195)  (2px inset ornate double border)
 *   Title bar:   y=82..115 — BLACK fill with WHITE text (glyph_h=30, 2px pad)
 *   Title/body divider: y=116
 *   Body area:   y=118..197 — up to 4 lines × 11px each
 *   Buttons:     y=183..195 — "YES" and "NO" bordered buttons (show_yes_no=1)
 *
 * Word-wrap algorithm (no malloc, stack-only):
 *   - '\n' forces a line break.
 *   - Lines split at last space within 20 characters.
 *   - Force-break at 20 chars if no space found.
 *   - Max 4 lines; 5th+ lines appended as "..." on line 4.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 */

#include "ui_widgets.h"
#include "asset_data.h"
#include "sprite_util.h"
#include <stddef.h>
#include <string.h>

/* ── Dialogue box geometry ───────────────────────────────────────────────── */

#define DLG_OUTER_X      2
#define DLG_OUTER_Y     80
#define DLG_OUTER_W    196
#define DLG_OUTER_H    118

#define DLG_INNER_INSET  2
#define DLG_INNER_X     (DLG_OUTER_X + DLG_INNER_INSET)
#define DLG_INNER_Y     (DLG_OUTER_Y + DLG_INNER_INSET)
#define DLG_INNER_W     (DLG_OUTER_W - 2 * DLG_INNER_INSET)
#define DLG_INNER_H     (DLG_OUTER_H - 2 * DLG_INNER_INSET)

/** Title bar (black fill, white text): from inner top, glyph_h=30 + 4px pad. */
#define DLG_TITLE_BAR_H  34
#define DLG_TITLE_Y      (DLG_INNER_Y + 2)

/** Divider between title and body. */
#define DLG_DIVIDER_Y    (DLG_INNER_Y + DLG_TITLE_BAR_H)

/** Body area. */
#define DLG_BODY_Y       (DLG_DIVIDER_Y + 2)
#define DLG_CONTENT_X    (DLG_INNER_X + 3)
#define DLG_LINE_H       11
#define DLG_MAX_LINES     4

/** YES/NO button area. */
#define DLG_BTN_Y        (DLG_OUTER_Y + DLG_OUTER_H - 16)
#define DLG_BTN_H        12
#define DLG_YES_X        (DLG_CONTENT_X)
#define DLG_YES_W        44
#define DLG_NO_X         (DLG_CONTENT_X + 60)
#define DLG_NO_W         34

/** Max characters per wrapped line. */
#define DLG_WRAP_COLS    20

/* ── Internal word-wrap helper ───────────────────────────────────────────── */

static void wrap_lines(const char  *body,
                       const char  *out_ptrs[DLG_MAX_LINES],
                       int          out_lens[DLG_MAX_LINES],
                       int         *out_count,
                       int         *truncated)
{
    *out_count = 0;
    *truncated = 0;

    if (body == NULL) { return; }

    const char *p   = body;
    size_t      rem = strnlen(p, 512u);

    while (rem > 0u && *out_count < DLG_MAX_LINES) {
        if (*p == '\n') {
            out_ptrs[*out_count] = p;
            out_lens[*out_count] = 0;
            (*out_count)++;
            p++;
            rem--;
            continue;
        }

        size_t line_max = (rem < (size_t)DLG_WRAP_COLS) ? rem : (size_t)DLG_WRAP_COLS;
        size_t break_at = line_max;
        int    found_nl = 0;

        for (size_t i = 0u; i < line_max; i++) {
            if (p[i] == '\n') {
                break_at = i;
                found_nl = 1;
                break;
            }
        }

        if (!found_nl && rem > line_max) {
            size_t last_space = (size_t)DLG_WRAP_COLS;
            for (size_t i = line_max; i > 0u; i--) {
                if (p[i - 1u] == ' ') {
                    last_space = i - 1u;
                    break;
                }
            }
            break_at = last_space;
        }

        out_ptrs[*out_count] = p;
        out_lens[*out_count] = (int)break_at;
        (*out_count)++;

        size_t advance = break_at;
        if (advance < rem) { advance++; }
        p   += advance;
        rem -= advance;
    }

    if (rem > 0u && *p != '\0') { *truncated = 1; }
}

/* ── fq_render_dialogue ───────────────────────────────────────────────────── */

void fq_render_dialogue(fq_fb_t    *fb,
                        const char *title,
                        const char *body,
                        uint8_t     show_yes_no)
{
    if (fb == NULL) { return; }

    const fq_font_t *font = fq_get_font_small();

    /* ── Erase dialogue region (white fill) ─────────────────────────────── */
    fq_fb_fill_rect(fb,
                    DLG_OUTER_X, DLG_OUTER_Y,
                    DLG_OUTER_W, DLG_OUTER_H, 0u);

    /* ── Ornate double-rect border ──────────────────────────────────────── */
    fq_fb_draw_rect(fb,
                    DLG_OUTER_X, DLG_OUTER_Y,
                    DLG_OUTER_W, DLG_OUTER_H, 1u);
    fq_fb_draw_rect(fb,
                    DLG_INNER_X, DLG_INNER_Y,
                    DLG_INNER_W, DLG_INNER_H, 1u);

    /* ── Title: black bar with white text ───────────────────────────────── */
    if (title != NULL && title[0] != '\0') {
        /* Fill title area black (inside inner border). */
        fq_fb_fill_rect(fb,
                        DLG_INNER_X + 1, DLG_INNER_Y + 1,
                        DLG_INNER_W - 2, DLG_TITLE_BAR_H - 1, 1u);
        /* White text. */
        fq_draw_text_inverted(fb, font,
                              DLG_CONTENT_X, DLG_TITLE_Y, title);
    }

    /* ── Title / body divider ───────────────────────────────────────────── */
    fq_fb_draw_line(fb,
                    DLG_INNER_X + 1, DLG_DIVIDER_Y,
                    DLG_INNER_X + DLG_INNER_W - 2, DLG_DIVIDER_Y, 1u);

    /* ── Body text lines ────────────────────────────────────────────────── */
    {
        const char *line_ptrs[DLG_MAX_LINES];
        int         line_lens[DLG_MAX_LINES];
        int         line_count = 0;
        int         truncated  = 0;

        char line_buf[DLG_WRAP_COLS + 4];

        wrap_lines(body, line_ptrs, line_lens, &line_count, &truncated);

        for (int i = 0; i < line_count; i++) {
            int line_y = DLG_BODY_Y + i * DLG_LINE_H;
            int len    = line_lens[i];
            if (len < 0) { len = 0; }

            int copy_len = len;
            if (copy_len > DLG_WRAP_COLS) { copy_len = DLG_WRAP_COLS; }

            if (truncated && i == DLG_MAX_LINES - 1) {
                int abbrev = copy_len - 3;
                if (abbrev < 0) { abbrev = 0; }
                for (int k = 0; k < abbrev; k++) { line_buf[k] = line_ptrs[i][k]; }
                line_buf[abbrev + 0] = '.';
                line_buf[abbrev + 1] = '.';
                line_buf[abbrev + 2] = '.';
                line_buf[abbrev + 3] = '\0';
            } else {
                for (int k = 0; k < copy_len; k++) { line_buf[k] = line_ptrs[i][k]; }
                line_buf[copy_len] = '\0';
            }

            fq_draw_text(fb, font, DLG_CONTENT_X, (int16_t)line_y, line_buf);
        }
    }

    /* ── YES / NO buttons ───────────────────────────────────────────────── */
    if (show_yes_no != 0u) {
        fq_fb_draw_rect(fb, DLG_YES_X, DLG_BTN_Y, DLG_YES_W, DLG_BTN_H, 1u);
        fq_draw_text(fb, font, (int16_t)(DLG_YES_X + 3), (int16_t)(DLG_BTN_Y + 1), "YES");

        fq_fb_draw_rect(fb, DLG_NO_X, DLG_BTN_Y, DLG_NO_W, DLG_BTN_H, 1u);
        fq_draw_text(fb, font, (int16_t)(DLG_NO_X + 3), (int16_t)(DLG_BTN_Y + 1), "NO");
    }
}
