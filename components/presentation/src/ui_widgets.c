/**
 * ui_widgets.c — FiestaQuest Presentation Layer: Reusable UI Widgets
 *
 * Implements word-wrapping dialogue box overlay.
 * Phase 18: wired real font rendering via fq_get_font_small().
 *
 * Dialogue box layout (overlays y=120..199 on the 200x200 display):
 *   Outer rect:  (2, 120) → (197, 197)
 *   Inner rect:  (4, 122) → (195, 195)  (2px gap ornate border)
 *   Title band:  y=124..153  (glyph_h=30)
 *   Title divider: y=155
 *   Body area:   y=157..196  (up to 4 lines × 10px each + 1px gap)
 *   Buttons:     y=184..194  (only when show_yes_no=1)
 *
 * Word-wrap algorithm (no malloc, stack-only):
 *   - Input string is scanned with strnlen-safe pointer arithmetic.
 *   - '\n' forces a line break.
 *   - Lines are split at the last space within 20 characters (narrowed from 25
 *     to account for real font advance widths averaging ~8px/char on a 186px
 *     usable body width).
 *   - If no space found within 20 characters, force-break at position 20.
 *   - Max 4 lines total. Line 4 suffix "..." is applied if input exceeds 4.
 *
 * Constitution Priority 0: no float, no malloc, no PRNG calls.
 */

#include "ui_widgets.h"
#include "asset_data.h"
#include <stddef.h>
#include <string.h>

/* ── Dialogue box geometry ───────────────────────────────────────────────── */

#define DLG_OUTER_X      2
#define DLG_OUTER_Y    120
#define DLG_OUTER_W    196
#define DLG_OUTER_H     78

#define DLG_INNER_INSET  2      /**< Border inset for ornate double-rect. */
#define DLG_INNER_X     (DLG_OUTER_X + DLG_INNER_INSET)
#define DLG_INNER_Y     (DLG_OUTER_Y + DLG_INNER_INSET)
#define DLG_INNER_W     (DLG_OUTER_W - 2 * DLG_INNER_INSET)
#define DLG_INNER_H     (DLG_OUTER_H - 2 * DLG_INNER_INSET)

/** X margin for content inside the inner border. */
#define DLG_CONTENT_X   (DLG_INNER_X + 3)
/** Usable content width. */
#define DLG_CONTENT_W   (DLG_INNER_W - 6)

/** Title band top y. */
#define DLG_TITLE_Y     (DLG_INNER_Y + 2)
/** Title band height = font glyph_h (30). */
#define DLG_TITLE_H     30

/** Divider line y (between title and body). */
#define DLG_DIVIDER_Y   (DLG_TITLE_Y + DLG_TITLE_H + 1)

/** Body area start y. */
#define DLG_BODY_Y      (DLG_DIVIDER_Y + 2)
/** Body line height (including gap) — keep compact to fit 4 lines. */
#define DLG_LINE_H      10
/** Maximum body lines before truncation. */
#define DLG_MAX_LINES    4

/** YES/NO button area y. */
#define DLG_BTN_Y       (DLG_OUTER_Y + DLG_OUTER_H - 14)
#define DLG_BTN_H       10
#define DLG_YES_X       (DLG_CONTENT_X)
#define DLG_YES_W       40
#define DLG_NO_X        (DLG_CONTENT_X + 50)
#define DLG_NO_W        30

/** Max characters per wrapped line (tightened for ~8px/char average). */
#define DLG_WRAP_COLS   20

/* ── Internal word-wrap helper ───────────────────────────────────────────── */

/**
 * wrap_lines — Split body text into up to DLG_MAX_LINES wrapped lines.
 *
 * Stores pointers and lengths into out_ptrs[]/out_lens[].
 * 'truncated' is set to 1 if the input had more lines than DLG_MAX_LINES.
 *
 * Operates entirely on the original string — no copies, no malloc.
 * Safe for NULL input (treats as empty).
 */
static void wrap_lines(const char  *body,
                       const char  *out_ptrs[DLG_MAX_LINES],
                       int          out_lens[DLG_MAX_LINES],
                       int         *out_count,
                       int         *truncated)
{
    *out_count = 0;
    *truncated = 0;

    if (body == NULL) {
        return;
    }

    const char *p   = body;
    size_t      rem = strnlen(p, 512u);  /* bounded scan of the body */

    while (rem > 0u && *out_count < DLG_MAX_LINES) {
        /* Handle explicit newline at position 0 */
        if (*p == '\n') {
            out_ptrs[*out_count] = p;
            out_lens[*out_count] = 0;
            (*out_count)++;
            p++;
            rem--;
            continue;
        }

        /* Determine this line's extent */
        size_t  line_max = (rem < (size_t)DLG_WRAP_COLS) ? rem : (size_t)DLG_WRAP_COLS;
        size_t  break_at = line_max;
        int     found_nl = 0;

        /* Search for '\n' or last space within line_max */
        for (size_t i = 0u; i < line_max; i++) {
            if (p[i] == '\n') {
                break_at = i;
                found_nl = 1;
                break;
            }
        }

        if (!found_nl) {
            /* If there is still more text beyond line_max, try to break at space */
            if (rem > line_max) {
                size_t last_space = (size_t)DLG_WRAP_COLS; /* default: force-break */
                for (size_t i = line_max; i > 0u; i--) {
                    if (p[i - 1u] == ' ') {
                        last_space = i - 1u;
                        break;
                    }
                }
                break_at = last_space;
            }
        }

        out_ptrs[*out_count] = p;
        out_lens[*out_count] = (int)break_at;
        (*out_count)++;

        /* Advance past the line and the break character ('\n' or ' ') */
        size_t advance = break_at;
        if (advance < rem) {
            /* Skip the break character itself */
            advance++;
        }
        p   += advance;
        rem -= advance;
    }

    /* Check if there is still unconsumed input */
    if (rem > 0u && *p != '\0') {
        *truncated = 1;
    }
}

/* ── fq_render_dialogue ───────────────────────────────────────────────────── */

void fq_render_dialogue(fq_fb_t    *fb,
                        const char *title,
                        const char *body,
                        uint8_t     show_yes_no)
{
    if (fb == NULL) {
        return;
    }

    const fq_font_t *font = fq_get_font_small();

    /* ── Erase dialogue region (fill white) ─────────────────────────────── */
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

    /* ── Title text ─────────────────────────────────────────────────────── */
    if (title != NULL && title[0] != '\0') {
        fq_draw_text(fb, font, DLG_CONTENT_X, DLG_TITLE_Y, title);
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

        /* Stack buffer for rendering each line (null-terminated copy). */
        char line_buf[DLG_WRAP_COLS + 4];  /* +4 for "..." and null */

        wrap_lines(body, line_ptrs, line_lens, &line_count, &truncated);

        for (int i = 0; i < line_count; i++) {
            int line_y = DLG_BODY_Y + i * DLG_LINE_H;
            int len    = line_lens[i];
            if (len < 0) {
                len = 0;
            }

            /* Build null-terminated string for this line. */
            int copy_len = len;
            if (copy_len > DLG_WRAP_COLS) {
                copy_len = DLG_WRAP_COLS;
            }

            /* Append "..." on last line if truncated. */
            if (truncated && i == DLG_MAX_LINES - 1) {
                int abbrev = copy_len - 3;
                if (abbrev < 0) {
                    abbrev = 0;
                }
                for (int k = 0; k < abbrev; k++) {
                    line_buf[k] = line_ptrs[i][k];
                }
                line_buf[abbrev + 0] = '.';
                line_buf[abbrev + 1] = '.';
                line_buf[abbrev + 2] = '.';
                line_buf[abbrev + 3] = '\0';
            } else {
                for (int k = 0; k < copy_len; k++) {
                    line_buf[k] = line_ptrs[i][k];
                }
                line_buf[copy_len] = '\0';
            }

            fq_draw_text(fb, font, DLG_CONTENT_X, (int16_t)line_y, line_buf);
        }
    }

    /* ── YES / NO buttons ───────────────────────────────────────────────── */
    if (show_yes_no != 0u) {
        fq_fb_draw_rect(fb,
                        DLG_YES_X, DLG_BTN_Y,
                        DLG_YES_W, DLG_BTN_H, 1u);
        fq_draw_text(fb, font, DLG_YES_X + 2, DLG_BTN_Y, "YES");

        fq_fb_draw_rect(fb,
                        DLG_NO_X, DLG_BTN_Y,
                        DLG_NO_W, DLG_BTN_H, 1u);
        fq_draw_text(fb, font, DLG_NO_X + 2, DLG_BTN_Y, "NO");
    }
}
