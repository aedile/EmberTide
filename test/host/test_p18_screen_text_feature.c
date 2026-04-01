/**
 * test_p18_screen_text_feature.c — Phase 18 Screen Text Wiring: Feature Tests
 *
 * Rule 22 (FEATURE RED): Happy-path tests that prove each screen renderer
 * produces real text and sprite pixels in the correct screen regions.
 *
 * Tests:
 *   F01 — home screen: name text region has non-zero pixels.
 *   F02 — home screen: HP bar region still has non-zero pixels (bar preserved).
 *   F03 — home screen: character sprite region has non-zero pixels.
 *   F04 — combat screen: f2_name text region has non-zero pixels.
 *   F05 — combat screen: f1_name text region has non-zero pixels.
 *   F06 — stats screen: stat label "STR" region has non-zero pixels.
 *   F07 — stats screen: XP bar region has non-zero pixels (bar fill preserved).
 *   F08 — inventory screen: "INVENTORY" header text has non-zero pixels.
 *   F09 — inventory screen: tooltip item name text has non-zero pixels.
 *   F10 — training screen: game_name text has non-zero pixels.
 *   F11 — training screen: state label "DONE" text has non-zero pixels.
 *   F12 — dialogue: title text has non-zero pixels in title band.
 *   F13 — dialogue: body text has non-zero pixels in body area.
 *
 * All tests use specific-value assertions on individual fb byte values or
 * non-zero pixel counts in bounded screen regions (Constitution Priority 4).
 *
 * Architecture: presentation layer only — no hal headers, no game headers.
 * Constitution Priority 0: no float arithmetic.
 */

#include "test_assert.h"
#include "fq_framebuffer.h"
#include "fq_sprite.h"
#include "fq_text.h"
#include "asset_data.h"

#include "screens/screen_home.h"
#include "screens/screen_combat.h"
#include "screens/screen_stats.h"
#include "screens/screen_inventory.h"
#include "screens/screen_training.h"
#include "ui_widgets.h"

#include "view_models.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ── Helper: count non-zero bytes in a pixel rectangle ─────────────────────── */
static uint32_t count_nonzero_bytes_in_region(const fq_fb_t *fb,
                                               uint32_t x_byte_start,
                                               uint32_t x_byte_end,
                                               uint32_t y_start,
                                               uint32_t y_end)
{
    uint32_t count = 0u;
    for (uint32_t row = y_start; row <= y_end; row++) {
        for (uint32_t col = x_byte_start; col <= x_byte_end; col++) {
            uint32_t idx = row * (uint32_t)FQ_FB_STRIDE + col;
            if (idx < (uint32_t)FQ_FB_SIZE && fb->pixels[idx] != 0x00u) {
                count++;
            }
        }
    }
    return count;
}

/* ── F01: home — name text region has non-zero pixels ───────────────────────── */
static void test_home_name_text_pixels_nonzero(void)
{
    fq_fb_t      fb;
    fq_vm_home_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.name, "Ember", sizeof(vm.name) - 1u);
    vm.level       = 7u;
    vm.hp_percent  = 75u;
    vm.sprite_base = 0u;
    vm.wins        = 3u;
    vm.losses      = 1u;

    fq_render_home(&fb, &vm);

    /*
     * Name is drawn at x=5, y=3 with glyph_h=30.
     * Region: rows 3-32, cols 0-12 (first 13 bytes covers 104 pixels — more
     * than enough for "Ember" at ~8px average advance).
     */
    uint32_t nz = count_nonzero_bytes_in_region(&fb, 0u, 12u, 3u, 32u);
    TEST_ASSERT_TRUE(nz > 0u);
}

/* ── F02: home — HP bar region still produces pixels ──────────────────────── */
static void test_home_hp_bar_pixels_nonzero(void)
{
    fq_fb_t      fb;
    fq_vm_home_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.name, "Test", sizeof(vm.name) - 1u);
    vm.hp_percent = 60u;

    fq_render_home(&fb, &vm);

    /*
     * HP bar outline is at y=115, width=180, height=12.
     * HP label "HP" is at x=10, y=115+1=116, inside the bar region.
     * The bar outline itself at row 115 must have non-zero pixels.
     */
    uint32_t nz = count_nonzero_bytes_in_region(&fb, 1u, 22u, 115u, 127u);
    TEST_ASSERT_TRUE(nz > 0u);
}

/* ── F03: home — character sprite region has non-zero pixels ──────────────── */
static void test_home_sprite_pixels_nonzero(void)
{
    fq_fb_t      fb;
    fq_vm_home_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.name, "Ember", sizeof(vm.name) - 1u);
    vm.hp_percent  = 50u;
    vm.sprite_base = 0u;   /* first character (Dark Knight) */

    fq_render_home(&fb, &vm);

    /*
     * Character sprite is blitted at x=84, y=30 (32x32 pixels).
     * x=84 → byte col = 84/8 = 10 (with bit offset).
     * Region: rows 30-61, cols 10-13 (32px wide = 4 bytes, starting at x=84).
     */
    uint32_t nz = count_nonzero_bytes_in_region(&fb, 10u, 14u, 30u, 61u);
    TEST_ASSERT_TRUE(nz > 0u);
}

/* ── F04: combat — f2 name text region has non-zero pixels ───────────────── */
static void test_combat_f2_name_text_nonzero(void)
{
    fq_fb_t        fb;
    fq_vm_combat_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.f1_name, "Ember",  sizeof(vm.f1_name)  - 1u);
    strncpy(vm.f2_name, "Shadow", sizeof(vm.f2_name)  - 1u);
    vm.f1_hp     = 75;
    vm.f1_hp_max = 100;
    vm.f2_hp     = 40;
    vm.f2_hp_max = 80;
    vm.round     = 3u;

    fq_render_combat(&fb, &vm);

    /*
     * f2_name is drawn in the enemy zone top band (y=5, glyph_h=30).
     * Region: rows 5-34, first 12 bytes (covers "Shadow" at ~8px/char).
     */
    uint32_t nz = count_nonzero_bytes_in_region(&fb, 0u, 12u, 5u, 34u);
    TEST_ASSERT_TRUE(nz > 0u);
}

/* ── F05: combat — f1 name text region has non-zero pixels ───────────────── */
static void test_combat_f1_name_text_nonzero(void)
{
    fq_fb_t        fb;
    fq_vm_combat_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.f1_name, "Ember",  sizeof(vm.f1_name)  - 1u);
    strncpy(vm.f2_name, "Shadow", sizeof(vm.f2_name)  - 1u);
    vm.f1_hp     = 75;
    vm.f1_hp_max = 100;
    vm.f2_hp     = 40;
    vm.f2_hp_max = 80;
    vm.round     = 3u;

    fq_render_combat(&fb, &vm);

    /*
     * f1_name is drawn in the player zone (y=135, glyph_h=30).
     * Region: rows 135-164, first 12 bytes.
     */
    uint32_t nz = count_nonzero_bytes_in_region(&fb, 0u, 12u, 135u, 164u);
    TEST_ASSERT_TRUE(nz > 0u);
}

/* ── F06: stats — stat label region has non-zero pixels ─────────────────────
 *
 * "STR" is drawn at x=5 (left margin), y=STATS_STR_BAR_Y=32.
 * glyph_h=30, so rows 32-61 contain the label.
 */
static void test_stats_label_text_nonzero(void)
{
    fq_fb_t       fb;
    fq_vm_stats_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.name, "Tide", sizeof(vm.name) - 1u);
    vm.level      = 12u;
    vm.strength   = 80u;
    vm.speed      = 60u;
    vm.precision  = 45u;
    vm.intelligence = 55u;
    vm.hp_max     = 350u;
    vm.xp         = 1100u;
    vm.xp_to_next = 5000u;

    fq_render_stats(&fb, &vm);

    /*
     * "STR" drawn at x=5, y=32: rows 32-61, first 2 bytes (pixels 0-15).
     */
    uint32_t nz = count_nonzero_bytes_in_region(&fb, 0u, 2u, 32u, 61u);
    TEST_ASSERT_TRUE(nz > 0u);
}

/* ── F07: stats — XP bar region has non-zero pixels ──────────────────────── */
static void test_stats_xp_bar_nonzero(void)
{
    fq_fb_t       fb;
    fq_vm_stats_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.name, "Tide", sizeof(vm.name) - 1u);
    vm.level      = 12u;
    vm.xp         = 1100u;
    vm.xp_to_next = 5000u;

    fq_render_stats(&fb, &vm);

    /*
     * XP bar drawn at y=121 (STATS_XP_BAR_Y), height=10, x=20, width=162.
     * x=20 → byte col = 2. Scan cols 2-22, rows 121-130.
     */
    uint32_t nz = count_nonzero_bytes_in_region(&fb, 2u, 22u, 121u, 130u);
    TEST_ASSERT_TRUE(nz > 0u);
}

/* ── F08: inventory — INVENTORY header text has non-zero pixels ────────────── */
static void test_inventory_header_text_nonzero(void)
{
    fq_fb_t           fb;
    fq_vm_inventory_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    vm.item_count = 3u;
    strncpy(vm.item_names[0], "Iron Fist",   sizeof(vm.item_names[0]) - 1u);
    strncpy(vm.item_names[1], "Tough Hide",  sizeof(vm.item_names[1]) - 1u);
    strncpy(vm.item_names[2], "Lucky Coin",  sizeof(vm.item_names[2]) - 1u);
    vm.cursor_index = 1u;

    fq_render_inventory(&fb, &vm);

    /*
     * "INVENTORY" header drawn at x=5, y=1 (glyph_h=30 → rows 1-30).
     * First 13 bytes (covers "INVENTORY" ~9 chars * ~8px/char = ~72px = 9 bytes).
     */
    uint32_t nz = count_nonzero_bytes_in_region(&fb, 0u, 12u, 1u, 30u);
    TEST_ASSERT_TRUE(nz > 0u);
}

/* ── F09: inventory — tooltip item name has non-zero pixels ─────────────────── */
static void test_inventory_tooltip_text_nonzero(void)
{
    fq_fb_t           fb;
    fq_vm_inventory_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    vm.item_count = 3u;
    strncpy(vm.item_names[0], "Iron Fist",   sizeof(vm.item_names[0]) - 1u);
    strncpy(vm.item_names[1], "Tough Hide",  sizeof(vm.item_names[1]) - 1u);
    strncpy(vm.item_names[2], "Lucky Coin",  sizeof(vm.item_names[2]) - 1u);
    vm.item_rarities[0] = 1u;
    vm.item_rarities[1] = 2u;
    vm.item_rarities[2] = 3u;
    vm.cursor_index = 0u;

    fq_render_inventory(&fb, &vm);

    /*
     * Tooltip text drawn at y=152, glyph_h=30 → rows 152-181.
     * "Iron Fist" → first 14 bytes.
     */
    uint32_t nz = count_nonzero_bytes_in_region(&fb, 0u, 13u, 152u, 181u);
    TEST_ASSERT_TRUE(nz > 0u);
}

/* ── F10: training — game_name text has non-zero pixels ─────────────────────── */
static void test_training_game_name_text_nonzero(void)
{
    fq_fb_t          fb;
    fq_vm_training_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.game_name, "Speed", sizeof(vm.game_name) - 1u);
    vm.score      = 70u;
    vm.difficulty = 3u;
    vm.state      = 1u;

    fq_render_training(&fb, &vm);

    /*
     * game_name text drawn at x=12, y=5 (glyph_h=30 → rows 5-34).
     * "Speed" → first 8 bytes starting from col 1.
     */
    uint32_t nz = count_nonzero_bytes_in_region(&fb, 1u, 9u, 5u, 34u);
    TEST_ASSERT_TRUE(nz > 0u);
}

/* ── F11: training — state label text has non-zero pixels ───────────────────── */
static void test_training_state_label_text_nonzero(void)
{
    fq_fb_t          fb;
    fq_vm_training_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.game_name, "Speed", sizeof(vm.game_name) - 1u);
    vm.score      = 70u;
    vm.difficulty = 3u;
    vm.state      = 2u;  /* DONE */

    fq_render_training(&fb, &vm);

    /*
     * State label "DONE" drawn at x=12, y=145 (glyph_h=30 → rows 145-174).
     * "DONE" → first 8 bytes from col 1.
     */
    uint32_t nz = count_nonzero_bytes_in_region(&fb, 1u, 8u, 145u, 174u);
    TEST_ASSERT_TRUE(nz > 0u);
}

/* ── F12: dialogue — title text has non-zero pixels in title band ─────────── */
static void test_dialogue_title_text_nonzero(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_render_dialogue(&fb, "REBIRTH",
                       "Your character has fallen.",
                       1u);

    /*
     * Title band: y=124, glyph_h=30 → rows 124-153.
     * "REBIRTH" drawn at DLG_CONTENT_X=9, rows 124-153.
     * First 12 bytes of the row (covers 96 pixels — enough for "REBIRTH").
     */
    uint32_t nz = count_nonzero_bytes_in_region(&fb, 1u, 12u, 124u, 153u);
    TEST_ASSERT_TRUE(nz > 0u);
}

/* ── F13: dialogue — body text has non-zero pixels ──────────────────────────── */
static void test_dialogue_body_text_nonzero(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_render_dialogue(&fb, "REBIRTH",
                       "Your character has fallen.",
                       0u);

    /*
     * Body area: DLG_BODY_Y = DLG_DIVIDER_Y+2.
     * DLG_DIVIDER_Y = DLG_TITLE_Y + DLG_TITLE_H + 1 = 124+12+1 = 137.
     * DLG_BODY_Y = 139. glyph_h=30 → rows 139-168.
     * "Your character..." → first 20 bytes.
     */
    uint32_t nz = count_nonzero_bytes_in_region(&fb, 1u, 20u, 139u, 168u);
    TEST_ASSERT_TRUE(nz > 0u);
}

/* ── main ──────────────────────────────────────────────────────────────────── */
int main(void)
{
    test_home_name_text_pixels_nonzero();
    test_home_hp_bar_pixels_nonzero();
    test_home_sprite_pixels_nonzero();
    test_combat_f2_name_text_nonzero();
    test_combat_f1_name_text_nonzero();
    test_stats_label_text_nonzero();
    test_stats_xp_bar_nonzero();
    test_inventory_header_text_nonzero();
    test_inventory_tooltip_text_nonzero();
    test_training_game_name_text_nonzero();
    test_training_state_label_text_nonzero();
    test_dialogue_title_text_nonzero();
    test_dialogue_body_text_nonzero();

    printf("test_p18_screen_text_feature: ALL PASS\n");
    return 0;
}
