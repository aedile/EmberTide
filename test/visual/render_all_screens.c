/**
 * render_all_screens.c
 *
 * Visual regression harness — Phase 9 updated version.
 *
 * Phase 9 additions: scene_combat.png, scene_dialogue.png, scene_training.png.
 *
 * B1 (review): Replaced raw uint8_t framebuffer[5000] with fq_fb_t from
 * fq_framebuffer.h. Uses fq_fb_clear/fq_fb_fill instead of raw memset.
 * Passes framebuffer.pixels to the PNG writer. Local constants (FB_WIDTH_PX
 * etc.) removed; FQ_FB_WIDTH, FQ_FB_HEIGHT, FQ_FB_STRIDE, FQ_FB_SIZE used.
 *
 * B1 visual demo: In addition to output/blank.png (all-black), writes
 * output/fb_test.png — a framebuffer with:
 *   - A border rect (full display outline)
 *   - An X drawn from corner to corner using fq_fb_draw_line
 * This exercises fq_fb_draw_line and fq_fb_draw_rect in the visual harness.
 *
 * Per spec-challenger N3: the return value of stbi_write_png() is checked.
 * Exit code is non-zero if the write fails.
 *
 * Per review advisory A2: the failure path test uses NULL as the filepath.
 *
 * The output/ directory is created if it does not exist (POSIX mkdir).
 */

/* STB single-header implementation — define EXACTLY ONCE in this translation
 * unit. All other files that include stb_image_write.h must NOT define this. */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendors/stb_image_write.h"

#include "fq_framebuffer.h"
#include "fq_text.h"
#include "screens/screen_home.h"
#include "screens/screen_inventory.h"
#include "screens/screen_stats.h"
#include "screens/screen_combat.h"
#include "screens/screen_training.h"
#include "screens/screen_idle.h"
#include "screens/screen_onboarding.h"
#include "ui_widgets.h"
#include "vm_builder.h"
#include "types.h"
#include "asset_data.h"
#include "sprite_util.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* ---------------------------------------------------------------------------
 * ensure_output_dir
 *
 * Creates the output/ directory relative to the current working directory.
 * Returns 0 on success, -1 on failure.
 * ---------------------------------------------------------------------------
 */
static int ensure_output_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        /* Path exists — verify it is a directory. */
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }
        fprintf(stderr, "render_all_screens: '%s' exists but is not a directory\n",
                path);
        return -1;
    }
    /* Directory does not exist — create it. */
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "render_all_screens: cannot create '%s': %s\n",
                path, strerror(errno));
        return -1;
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * expand_1bit_to_grayscale_row
 *
 * Converts one row of 1-bit packed data to a 1-byte-per-pixel grayscale row
 * suitable for stbi_write_png (which expects byte-aligned channel data).
 *
 * Bit convention: 1 = black (0x00), 0 = white (0xFF).
 * This matches typical e-paper display convention.
 *
 * packed_row : pointer to FQ_FB_STRIDE packed bytes for one row.
 * out_row    : caller-allocated buffer of FQ_FB_WIDTH bytes.
 * ---------------------------------------------------------------------------
 */
static void expand_1bit_to_grayscale_row(const uint8_t *packed_row,
                                          uint8_t       *out_row)
{
    for (uint32_t x = 0; x < FQ_FB_WIDTH; x++) {
        uint32_t byte_idx = x / 8u;
        uint32_t bit_idx  = 7u - (x % 8u);  /* MSB first */
        uint8_t  bit      = (packed_row[byte_idx] >> bit_idx) & 0x01u;
        /* 1-bit = black (0x00), 0-bit = white (0xFF) */
        out_row[x] = bit ? 0x00u : 0xFFu;
    }
}

/* ---------------------------------------------------------------------------
 * write_framebuffer_png
 *
 * Expands the 1-bit framebuffer to grayscale and writes a PNG file.
 * Takes a pointer to fq_fb_t and uses fb->pixels.
 *
 * Returns 0 on success, -1 on failure.
 * ---------------------------------------------------------------------------
 */
static int write_framebuffer_png(const fq_fb_t *fb,
                                  const char    *filepath)
{
    /* Temporary row buffer — one grayscale byte per pixel. */
    uint8_t row_gray[FQ_FB_WIDTH];

    /* stbi_write_png expects a flat RGBA or grayscale byte array.
     * We build a full grayscale image buffer (FQ_FB_WIDTH * FQ_FB_HEIGHT bytes). */
    uint8_t *gray_image = (uint8_t *)malloc(FQ_FB_WIDTH * FQ_FB_HEIGHT);
    if (!gray_image) {
        fprintf(stderr, "render_all_screens: malloc failed for gray_image\n");
        return -1;
    }

    for (uint32_t y = 0; y < FQ_FB_HEIGHT; y++) {
        const uint8_t *src_row = fb->pixels + (y * FQ_FB_STRIDE);
        expand_1bit_to_grayscale_row(src_row, row_gray);
        memcpy(gray_image + (y * FQ_FB_WIDTH), row_gray, FQ_FB_WIDTH);
    }

    /* N3: Check stbi_write_png return value — 0 = failure. */
    int result = stbi_write_png(
        filepath,
        (int)FQ_FB_WIDTH,
        (int)FQ_FB_HEIGHT,
        1,                  /* 1 channel = grayscale */
        gray_image,
        (int)FQ_FB_WIDTH    /* stride in bytes for the grayscale buffer */
    );

    free(gray_image);

    if (result == 0) {
        fprintf(stderr, "render_all_screens: stbi_write_png failed for '%s'\n",
                filepath);
        return -1;
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * render_title_screen
 *
 * Mirrors the title screen layout from app_main.c render_title_screen().
 * Called here so the visual harness can produce scene_title.png for review.
 *
 * Layout (200x200 px, 1-bit e-paper):
 *   y=0..3    4-px thick outer border
 *   y=4..51   Solid black title band (48px tall) with "EmberTide" as white
 *             inverted text, centered — FONT_SCRIPT_36 with corrected advance
 *             widths renders "EmberTide" in ~160px (was ~99px broken kerning).
 *   y=52      horizontal separator
 *   y=60      Dark Knight 2x sprite (64x64), centered
 *   y=130     horizontal separator
 *   y=145     "Press Any Button" centered, small font (FONT_REGS_12)
 *   y=196..199 bottom 4-px thick border
 *
 * Title design: solid black band at top with white script text — maximum
 * visual contrast on e-paper. Corrected advance widths eliminate inter-
 * character gaps by zeroing off_x and using w+2 as advance.
 *
 * Button note: either button (SUN/GPIO18 = BTN_B, PWR/GPIO0 = BTN_A)
 * advances the title screen.  The prompt says "Press Any Button".
 * ---------------------------------------------------------------------------
 */
static void render_title_screen(fq_fb_t *fb)
{
    const fq_font_t   *font_title = fq_get_font_title();
    const fq_font_t   *font_small = fq_get_font_small();
    const fq_sprite_t *spr        = fq_get_char_sprite(0u, 0u); /* Dark Knight, frame 0 */

    /* Outer 4-px thick border */
    fq_fb_fill_rect(fb,   0,   0, 200,   4, 1u); /* top    */
    fq_fb_fill_rect(fb,   0, 196, 200,   4, 1u); /* bottom */
    fq_fb_fill_rect(fb,   0,   4,   4, 192, 1u); /* left   */
    fq_fb_fill_rect(fb, 196,   4,   4, 192, 1u); /* right  */

    /* Solid black title band y=4..51 (48px tall).
     * "EmberTide" rendered as white inverted text, vertically centered in band.
     * FONT_SCRIPT_36: glyph_h=30, so vertical center = (48-30)/2 = 9px margin.
     * Render at y=4+9=13.
     * Width = 160px (corrected advances); center at x=(200-160)/2=20. */
    fq_fb_fill_rect(fb, 4, 4, 192, 48, 1u);
    {
        static const char title_str[] = "EmberTide";
        int16_t w = fq_text_width(font_title, title_str);
        int16_t x = (int16_t)((200 - w) / 2);
        fq_draw_text_inverted(fb, font_title, x, 13, title_str);
    }

    /* Horizontal separator at y=52 */
    fq_fb_draw_line(fb, 12, 52, 187, 52, 1u);

    /* Dark Knight sprite 2x (64x64), centered at x=68, top at y=60 */
    if (spr != NULL) {
        int16_t sprite_x = (int16_t)((200 - 64) / 2);
        fq_blit_sprite_2x(fb, sprite_x, 60, spr);
    }

    /* Horizontal separator at y=130 */
    fq_fb_draw_line(fb, 12, 130, 187, 130, 1u);

    /* "Press Any Button" centered at y=145, small font */
    {
        static const char prompt_str[] = "Press Any Button";
        int16_t w = fq_text_width(font_small, prompt_str);
        int16_t x = (int16_t)((200 - w) / 2);
        fq_draw_text(fb, font_small, x, 145, prompt_str);
    }
}

/* ---------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------
 */
int main(void)
{
    /* B1: Static fq_fb_t — no malloc in the fast path. */
    static fq_fb_t framebuffer;

    /* Ensure the output directory exists. */
    if (ensure_output_dir("output") != 0) {
        return EXIT_FAILURE;
    }

    /* -----------------------------------------------------------------------
     * Screen 1: blank.png — all-black (fq_fb_fill with color=1).
     * ----------------------------------------------------------------------- */
    fq_fb_fill(&framebuffer, 1u);

    printf("render_all_screens: writing output/blank.png ...\n");
    if (write_framebuffer_png(&framebuffer, "output/blank.png") != 0) {
        return EXIT_FAILURE;
    }
    printf("render_all_screens: output/blank.png written successfully "
           "(%u x %u, 1-bit, %u bytes framebuffer)\n",
           (unsigned)FQ_FB_WIDTH, (unsigned)FQ_FB_HEIGHT, (unsigned)FQ_FB_SIZE);

    /* -----------------------------------------------------------------------
     * Screen 2: fb_test.png — visual demo: X from corners + border rect.
     *
     * Demonstrates fq_fb_draw_line and fq_fb_draw_rect on a white background.
     *   - Border rect: full display outline (0,0) to (199,199)
     *   - Diagonal 1:  top-left (0,0) to bottom-right (199,199)
     *   - Diagonal 2:  top-right (199,0) to bottom-left (0,199)
     * ----------------------------------------------------------------------- */
    fq_fb_clear(&framebuffer);

    /* Border rect: outline of the full 200x200 display. */
    fq_fb_draw_rect(&framebuffer, 0, 0,
                    (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

    /* Diagonal: top-left → bottom-right. */
    fq_fb_draw_line(&framebuffer, 0, 0, (int16_t)(FQ_FB_WIDTH - 1u),
                    (int16_t)(FQ_FB_HEIGHT - 1u), 1u);

    /* Diagonal: top-right → bottom-left. */
    fq_fb_draw_line(&framebuffer, (int16_t)(FQ_FB_WIDTH - 1u), 0,
                    0, (int16_t)(FQ_FB_HEIGHT - 1u), 1u);

    printf("render_all_screens: writing output/fb_test.png ...\n");
    if (write_framebuffer_png(&framebuffer, "output/fb_test.png") != 0) {
        return EXIT_FAILURE;
    }
    printf("render_all_screens: output/fb_test.png written successfully "
           "(border rect + X from corners)\n");

    /* -----------------------------------------------------------------------
     * Failure path test — pass NULL as the filepath.
     *
     * stbi_write_png(NULL, ...) returns 0 without attempting any I/O.
     * ----------------------------------------------------------------------- */
    printf("render_all_screens: testing stbi_write_png failure path (NULL filepath) ...\n");
    int bad_result = stbi_write_png(
        NULL,
        (int)FQ_FB_WIDTH,
        (int)FQ_FB_HEIGHT,
        1,
        framebuffer.pixels,
        (int)FQ_FB_STRIDE
    );
    if (bad_result != 0) {
        fprintf(stderr,
                "render_all_screens: FAIL — expected stbi_write_png to return 0 "
                "for NULL filepath, got %d\n", bad_result);
        return EXIT_FAILURE;
    }
    printf("render_all_screens: stbi_write_png correctly returned 0 for NULL filepath\n");

    /* -----------------------------------------------------------------------
     * Screen 3: scene_title.png — EmberTide title screen.
     * ----------------------------------------------------------------------- */
    fq_fb_clear(&framebuffer);
    render_title_screen(&framebuffer);

    printf("render_all_screens: writing output/scene_title.png ...\n");
    if (write_framebuffer_png(&framebuffer, "output/scene_title.png") != 0) {
        return EXIT_FAILURE;
    }
    printf("render_all_screens: output/scene_title.png written successfully\n");

    /* -----------------------------------------------------------------------
     * Screen 4: scene_home.png — home screen with Ember, level 7.
     * ----------------------------------------------------------------------- */
    {
        fq_character_t ch;
        fq_vm_home_t   vm_home;
        memset(&ch,      0, sizeof(ch));
        memset(&vm_home, 0, sizeof(vm_home));

        strncpy(ch.name, "Ember", sizeof(ch.name) - 1u);
        ch.level  = 7u;
        ch.wins   = 3u;
        ch.losses = 1u;
        ch.hp_max = 100u;

        fq_vm_build_home(&vm_home, &ch);
        vm_home.menu_index = 0u;  /* TRAIN highlighted by default */
        fq_render_home(&framebuffer, &vm_home);

        printf("render_all_screens: writing output/scene_home.png ...\n");
        if (write_framebuffer_png(&framebuffer, "output/scene_home.png") != 0) {
            return EXIT_FAILURE;
        }
        printf("render_all_screens: output/scene_home.png written successfully\n");
    }

    /* -----------------------------------------------------------------------
     * Screen 5: scene_inventory.png — inventory with 5 items, cursor at 2.
     * ----------------------------------------------------------------------- */
    {
        fq_inventory_t    inv;
        fq_vm_inventory_t vm_inv;
        memset(&inv,    0, sizeof(inv));
        memset(&vm_inv, 0, sizeof(vm_inv));

        inv.count    = 5u;
        inv.items[0] = 1u;   /* Iron Fist */
        inv.items[1] = 3u;   /* Tough Hide */
        inv.items[2] = 4u;   /* Lucky Coin */
        inv.items[3] = 104u; /* Vampire Fang */
        inv.items[4] = 105u; /* Haymaker */

        fq_vm_build_inventory(&vm_inv, &inv);
        vm_inv.cursor_index = 2u;  /* cursor on Lucky Coin */

        fq_render_inventory(&framebuffer, &vm_inv);

        printf("render_all_screens: writing output/scene_inventory.png ...\n");
        if (write_framebuffer_png(&framebuffer, "output/scene_inventory.png") != 0) {
            return EXIT_FAILURE;
        }
        printf("render_all_screens: output/scene_inventory.png written successfully\n");
    }

    /* -----------------------------------------------------------------------
     * Screen 6: scene_stats.png — stats screen for Tide, level 12, rebirth 2.
     * ----------------------------------------------------------------------- */
    {
        fq_character_t ch;
        fq_vm_stats_t  vm_stats;
        memset(&ch,       0, sizeof(ch));
        memset(&vm_stats, 0, sizeof(vm_stats));

        strncpy(ch.name, "Tide", sizeof(ch.name) - 1u);
        ch.level         = 12u;
        ch.strength      = 80u;
        ch.speed         = 60u;
        ch.precision     = 45u;
        ch.intelligence  = 55u;
        ch.hp_max        = 350u;
        ch.xp            = 1100u;
        ch.rebirth_count = 2u;

        fq_vm_build_stats(&vm_stats, &ch);
        fq_render_stats(&framebuffer, &vm_stats);

        printf("render_all_screens: writing output/scene_stats.png ...\n");
        if (write_framebuffer_png(&framebuffer, "output/scene_stats.png") != 0) {
            return EXIT_FAILURE;
        }
        printf("render_all_screens: output/scene_stats.png written successfully\n");
    }

    /* -----------------------------------------------------------------------
     * Screen 7: scene_combat.png — Round 3, Ember 75/100 HP vs Shadow 40/80.
     * action_text = "Cleave! -15"
     * ----------------------------------------------------------------------- */
    {
        fq_vm_combat_t vm_combat;
        memset(&vm_combat, 0, sizeof(vm_combat));

        strncpy(vm_combat.f1_name, "Ember",  sizeof(vm_combat.f1_name)  - 1u);
        strncpy(vm_combat.f2_name, "Shadow", sizeof(vm_combat.f2_name)  - 1u);
        vm_combat.f1_hp     = 75;
        vm_combat.f1_hp_max = 100;
        vm_combat.f2_hp     = 40;
        vm_combat.f2_hp_max = 80;
        vm_combat.round     = 3u;
        vm_combat.f1_class_id = 0u;  /* BRUISER */
        vm_combat.f2_class_id = 1u;  /* TRICKSTER */
        strncpy(vm_combat.action_text, "Cleave! -15",
                sizeof(vm_combat.action_text) - 1u);

        fq_render_combat(&framebuffer, &vm_combat);

        printf("render_all_screens: writing output/scene_combat.png ...\n");
        if (write_framebuffer_png(&framebuffer, "output/scene_combat.png") != 0) {
            return EXIT_FAILURE;
        }
        printf("render_all_screens: output/scene_combat.png written successfully\n");
    }

    /* -----------------------------------------------------------------------
     * Screen 8: scene_dialogue.png — REBIRTH dialogue with YES/NO buttons.
     * ----------------------------------------------------------------------- */
    {
        fq_fb_clear(&framebuffer);
        /* Draw a simple background first (home screen geometry) so the
         * dialogue overlay has content beneath it. */
        fq_fb_draw_rect(&framebuffer, 0, 0,
                        (int16_t)FQ_FB_WIDTH, (int16_t)FQ_FB_HEIGHT, 1u);

        fq_render_dialogue(&framebuffer, "REBIRTH",
                           "Your character has fallen. "
                           "Would you like to be reborn?",
                           1u);

        printf("render_all_screens: writing output/scene_dialogue.png ...\n");
        if (write_framebuffer_png(&framebuffer, "output/scene_dialogue.png") != 0) {
            return EXIT_FAILURE;
        }
        printf("render_all_screens: output/scene_dialogue.png written successfully\n");
    }

    /* -----------------------------------------------------------------------
     * Screen 9: scene_training.png — Speed game, score=70, state=2 (done).
     * ----------------------------------------------------------------------- */
    {
        fq_vm_training_t vm_training;
        memset(&vm_training, 0, sizeof(vm_training));

        strncpy(vm_training.game_name, "Speed",
                sizeof(vm_training.game_name) - 1u);
        vm_training.score      = 70u;
        vm_training.difficulty = 3u;
        vm_training.state      = 2u;  /* done */

        fq_render_training(&framebuffer, &vm_training);

        printf("render_all_screens: writing output/scene_training.png ...\n");
        if (write_framebuffer_png(&framebuffer, "output/scene_training.png") != 0) {
            return EXIT_FAILURE;
        }
        printf("render_all_screens: output/scene_training.png written successfully\n");
    }

    /* -----------------------------------------------------------------------
     * Screen 10: scene_idle.png — Idle screensaver, Ember at level 5.
     *
     * BLOCKER 5 fix: screen_idle was not rendered in the visual harness.
     * fq_render_idle() clears the framebuffer before drawing, so we just
     * call it directly with a valid fq_vm_idle_t.
     * ----------------------------------------------------------------------- */
    {
        fq_vm_idle_t vm_idle;
        memset(&vm_idle, 0, sizeof(vm_idle));
        vm_idle.sprite_base = 0u;   /* Dark Knight character */
        vm_idle.level       = 5u;
        strncpy(vm_idle.name, "Ember", sizeof(vm_idle.name) - 1u);

        fq_fb_clear(&framebuffer);
        fq_render_idle(&framebuffer, &vm_idle);

        printf("render_all_screens: writing output/scene_idle.png ...\n");
        if (write_framebuffer_png(&framebuffer, "output/scene_idle.png") != 0) {
            return EXIT_FAILURE;
        }
        printf("render_all_screens: output/scene_idle.png written successfully\n");
    }

    /* -----------------------------------------------------------------------
     * Screen 11: scene_onboarding.png — Warrior class, class index 0.
     * ----------------------------------------------------------------------- */
    {
        fq_vm_onboarding_t vm_onboard;
        memset(&vm_onboard, 0, sizeof(vm_onboard));
        strncpy(vm_onboard.class_name, "Warrior", sizeof(vm_onboard.class_name) - 1u);
        vm_onboard.class_index   = 0u;
        vm_onboard.sprite_base   = 0u;
        vm_onboard.strength      = 80u;
        vm_onboard.speed         = 40u;
        vm_onboard.precision     = 30u;
        vm_onboard.intelligence  = 20u;

        fq_render_onboarding(&framebuffer, &vm_onboard);

        printf("render_all_screens: writing output/scene_onboarding.png ...\n");
        if (write_framebuffer_png(&framebuffer, "output/scene_onboarding.png") != 0) {
            return EXIT_FAILURE;
        }
        printf("render_all_screens: output/scene_onboarding.png written successfully\n");
    }

    printf("render_all_screens: ALL SCREENS OK\n");
    return EXIT_SUCCESS;
}
