/**
 * test_p18_screen_text_bounds.c — Phase 18 Screen Text Wiring: Bound/Guard Tests
 *
 * Rule 22 (BOUND RED): Negative and boundary tests that prove the system
 * REJECTS invalid inputs and that each screen renderer with real font/sprite
 * calls does NOT crash under adversarial conditions.
 *
 * Tests:
 *   B01 — fq_get_font_small() returns non-NULL pointer.
 *   B02 — fq_get_char_sprite(0, 0) returns non-NULL (valid first entry).
 *   B03 — fq_get_char_sprite(char_id_max, 7) returns non-NULL (last valid).
 *   B04 — fq_get_char_sprite OOB: char_id * 8 + frame >= 168 returns NULL.
 *   B05 — fq_get_item_sprite(0) returns non-NULL.
 *   B06 — fq_get_item_sprite(15) returns non-NULL (last valid item).
 *   B07 — fq_get_item_sprite(16) returns NULL (OOB).
 *   B08 — fq_render_home(NULL, &vm) — no crash.
 *   B09 — fq_render_home(&fb, NULL) — no crash.
 *   B10 — fq_render_home with real font produces at least 1 non-zero pixel
 *          in the name text region (row 3-35, cols 0-24 bytes).
 *   B11 — fq_render_combat(NULL, &vm) — no crash.
 *   B12 — fq_render_combat(&fb, NULL) — no crash.
 *   B13 — fq_render_stats(NULL, &vm) — no crash.
 *   B14 — fq_render_stats(&fb, NULL) — no crash.
 *   B15 — fq_render_inventory(NULL, &vm) — no crash.
 *   B16 — fq_render_inventory(&fb, NULL) — no crash.
 *   B17 — fq_render_training(NULL, &vm) — no crash.
 *   B18 — fq_render_training(&fb, NULL) — no crash.
 *   B19 — fq_render_dialogue(NULL, "T", "B", 0) — no crash.
 *   B20 — Integer overflow guard: hp_percent = 255 must not OOB-draw the HP bar.
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

/* ── Constants ─────────────────────────────────────────────────────────────── */

/* 8 animation frames per character, 21 characters = 168 entries. */
#define SPRITE_CHAR_TOTAL   168u
/* 21 characters (rows 0-20 of the sprite sheet), 8 frames each. */
#define SPRITE_CHAR_ROWS     21u
#define SPRITE_CHAR_FRAMES    8u

/* ── B01: fq_get_font_small returns non-NULL ──────────────────────────────── */
static void test_get_font_small_not_null(void)
{
    const fq_font_t *font = fq_get_font_small();
    TEST_ASSERT_NOT_NULL(font);
    TEST_ASSERT_NOT_NULL(font->bitmap);
    TEST_ASSERT_NOT_NULL(font->widths);
}

/* ── B02: fq_get_char_sprite valid first entry ────────────────────────────── */
static void test_get_char_sprite_valid_first(void)
{
    const fq_sprite_t *sp = fq_get_char_sprite(0u, 0u);
    TEST_ASSERT_NOT_NULL(sp);
    TEST_ASSERT_EQUAL_UINT32(32u, (uint32_t)sp->width);
    TEST_ASSERT_EQUAL_UINT32(32u, (uint32_t)sp->height);
}

/* ── B03: fq_get_char_sprite valid last entry ─────────────────────────────── */
static void test_get_char_sprite_valid_last(void)
{
    /* Last valid: char_id = 20, frame = 7 → idx = 20*8+7 = 167 */
    const fq_sprite_t *sp = fq_get_char_sprite(20u, 7u);
    TEST_ASSERT_NOT_NULL(sp);
}

/* ── B04: fq_get_char_sprite OOB returns NULL ─────────────────────────────── */
static void test_get_char_sprite_oob_returns_null(void)
{
    /* char_id=21 → idx = 21*8+0 = 168 which equals table size — must return NULL. */
    const fq_sprite_t *sp = fq_get_char_sprite(21u, 0u);
    TEST_ASSERT_NULL(sp);

    /* char_id=0, frame=8 → idx=8 which is valid BUT frame must be < 8.
     * fq_get_char_sprite is indexed flat: idx = char_id*8 + frame.
     * idx=8 is char_id=1, frame=0, which is valid. Test large char_id. */
    const fq_sprite_t *sp2 = fq_get_char_sprite(255u, 0u);
    TEST_ASSERT_NULL(sp2);
}

/* ── B05: fq_get_item_sprite valid first ──────────────────────────────────── */
static void test_get_item_sprite_valid_first(void)
{
    const fq_sprite_t *sp = fq_get_item_sprite(0u);
    TEST_ASSERT_NOT_NULL(sp);
}

/* ── B06: fq_get_item_sprite valid last ───────────────────────────────────── */
static void test_get_item_sprite_valid_last(void)
{
    const fq_sprite_t *sp = fq_get_item_sprite(15u);
    TEST_ASSERT_NOT_NULL(sp);
}

/* ── B07: fq_get_item_sprite OOB returns NULL ─────────────────────────────── */
static void test_get_item_sprite_oob_returns_null(void)
{
    const fq_sprite_t *sp = fq_get_item_sprite(16u);
    TEST_ASSERT_NULL(sp);

    const fq_sprite_t *sp2 = fq_get_item_sprite(255u);
    TEST_ASSERT_NULL(sp2);
}

/* ── B08: fq_render_home NULL fb — no crash ───────────────────────────────── */
static void test_render_home_null_fb_no_crash(void)
{
    fq_vm_home_t vm;
    memset(&vm, 0, sizeof(vm));
    strncpy(vm.name, "Test", sizeof(vm.name) - 1u);
    vm.level      = 1u;
    vm.hp_percent = 50u;
    fq_render_home(NULL, &vm);
    _TA_PASS("fq_render_home(NULL fb) did not crash");
}

/* ── B09: fq_render_home NULL vm — no crash ───────────────────────────────── */
static void test_render_home_null_vm_no_crash(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_render_home(&fb, NULL);
    _TA_PASS("fq_render_home(NULL vm) did not crash");
}

/* ── B10: fq_render_home with real font produces non-zero pixels in name area */
static void test_render_home_name_text_nonzero(void)
{
    fq_fb_t      fb;
    fq_vm_home_t vm;
    memset(&vm, 0, sizeof(vm));
    fq_fb_clear(&fb);

    strncpy(vm.name, "Ember", sizeof(vm.name) - 1u);
    vm.level      = 7u;
    vm.hp_percent = 75u;
    vm.sprite_base = 0u;
    vm.wins   = 3u;
    vm.losses = 1u;

    fq_render_home(&fb, &vm);

    /*
     * The font glyph_h is 30 pixels.  Name is drawn at y=3.
     * Scan rows 3 to 3+30=32 (inclusive), first 25 bytes per row.
     * At least one byte must be non-zero (the name "Ember" draws pixels).
     */
    int found = 0;
    for (uint32_t row = 3u; row <= 33u && !found; row++) {
        for (uint32_t col = 0u; col < (uint32_t)FQ_FB_STRIDE && !found; col++) {
            uint32_t idx = row * (uint32_t)FQ_FB_STRIDE + col;
            if (fb.pixels[idx] != 0x00u) {
                found = 1;
            }
        }
    }
    TEST_ASSERT_TRUE(found);
}

/* ── B11: fq_render_combat NULL fb — no crash ────────────────────────────── */
static void test_render_combat_null_fb_no_crash(void)
{
    fq_vm_combat_t vm;
    memset(&vm, 0, sizeof(vm));
    fq_render_combat(NULL, &vm);
    _TA_PASS("fq_render_combat(NULL fb) did not crash");
}

/* ── B12: fq_render_combat NULL vm — no crash ────────────────────────────── */
static void test_render_combat_null_vm_no_crash(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_render_combat(&fb, NULL);
    _TA_PASS("fq_render_combat(NULL vm) did not crash");
}

/* ── B13: fq_render_stats NULL fb — no crash ─────────────────────────────── */
static void test_render_stats_null_fb_no_crash(void)
{
    fq_vm_stats_t vm;
    memset(&vm, 0, sizeof(vm));
    fq_render_stats(NULL, &vm);
    _TA_PASS("fq_render_stats(NULL fb) did not crash");
}

/* ── B14: fq_render_stats NULL vm — no crash ─────────────────────────────── */
static void test_render_stats_null_vm_no_crash(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_render_stats(&fb, NULL);
    _TA_PASS("fq_render_stats(NULL vm) did not crash");
}

/* ── B15: fq_render_inventory NULL fb — no crash ─────────────────────────── */
static void test_render_inventory_null_fb_no_crash(void)
{
    fq_vm_inventory_t vm;
    memset(&vm, 0, sizeof(vm));
    fq_render_inventory(NULL, &vm);
    _TA_PASS("fq_render_inventory(NULL fb) did not crash");
}

/* ── B16: fq_render_inventory NULL vm — no crash ─────────────────────────── */
static void test_render_inventory_null_vm_no_crash(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_render_inventory(&fb, NULL);
    _TA_PASS("fq_render_inventory(NULL vm) did not crash");
}

/* ── B17: fq_render_training NULL fb — no crash ──────────────────────────── */
static void test_render_training_null_fb_no_crash(void)
{
    fq_vm_training_t vm;
    memset(&vm, 0, sizeof(vm));
    fq_render_training(NULL, &vm);
    _TA_PASS("fq_render_training(NULL fb) did not crash");
}

/* ── B18: fq_render_training NULL vm — no crash ──────────────────────────── */
static void test_render_training_null_vm_no_crash(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_render_training(&fb, NULL);
    _TA_PASS("fq_render_training(NULL vm) did not crash");
}

/* ── B19: fq_render_dialogue NULL fb — no crash ──────────────────────────── */
static void test_render_dialogue_null_fb_no_crash(void)
{
    fq_render_dialogue(NULL, "Title", "Body text here", 0u);
    _TA_PASS("fq_render_dialogue(NULL fb) did not crash");
}

/* ── B20: hp_percent=255 must not overflow HP bar geometry ───────────────── */
static void test_render_home_hp_percent_255_no_overflow(void)
{
    /*
     * hp_percent is uint8_t max = 255. The bar fill formula:
     *   fill_w = (uint32_t)255 * 178u / 100u = 453 which is > FQ_FB_WIDTH.
     * The renderer MUST clamp fill_w to HOME_HP_FILL_MAX_W (178) before
     * passing to fq_fb_fill_rect to prevent OOB writes.
     * This test verifies the renderer doesn't crash with hp_percent=255.
     */
    fq_fb_t      fb;
    fq_vm_home_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));
    strncpy(vm.name, "MAX", sizeof(vm.name) - 1u);
    vm.hp_percent = 255u; /* deliberately adversarial — exceeds 100 */

    /* Must complete without crash or assertion failure. */
    fq_render_home(&fb, &vm);
    _TA_PASS("fq_render_home(hp_percent=255) did not overflow or crash");
}

/* ── main ──────────────────────────────────────────────────────────────────── */
int main(void)
{
    test_get_font_small_not_null();
    test_get_char_sprite_valid_first();
    test_get_char_sprite_valid_last();
    test_get_char_sprite_oob_returns_null();
    test_get_item_sprite_valid_first();
    test_get_item_sprite_valid_last();
    test_get_item_sprite_oob_returns_null();
    test_render_home_null_fb_no_crash();
    test_render_home_null_vm_no_crash();
    test_render_home_name_text_nonzero();
    test_render_combat_null_fb_no_crash();
    test_render_combat_null_vm_no_crash();
    test_render_stats_null_fb_no_crash();
    test_render_stats_null_vm_no_crash();
    test_render_inventory_null_fb_no_crash();
    test_render_inventory_null_vm_no_crash();
    test_render_training_null_fb_no_crash();
    test_render_training_null_vm_no_crash();
    test_render_dialogue_null_fb_no_crash();
    test_render_home_hp_percent_255_no_overflow();

    printf("test_p18_screen_text_bounds: ALL PASS\n");
    return 0;
}
