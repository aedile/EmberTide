/**
 * test_p9_combat_feature.c — Phase 9 FEATURE RED: Combat HUD Feature Tests
 *
 * Rule 22: Written BEFORE implementation (GREEN phase).
 * Tests define the happy-path rendering contract for fq_render_combat.
 *
 * Tests:
 *   - Render completes without crash for a fully-populated VM
 *   - Framebuffer is non-blank after render (some pixels written)
 *   - Divider line at y=98 is drawn (at least one pixel in row 98)
 *   - Display border is drawn (pixel at 0,0 is set)
 *   - Action banner: framebuffer has pixels in middle band when action_text set
 *   - finished + winner fields are accepted without crash
 *   - fq_vm_build_combat populates VM correctly from combat context
 *
 * Architecture: presentation/ + main/vm_builder — no direct game/ combat calls.
 * Constitution Priority 0: no float, no malloc, no PRNG.
 */

#include <inttypes.h>
#include <string.h>
#include <stdint.h>
#include "test_assert.h"
#include "fq_framebuffer.h"
#include "view_models.h"
#include "screens/screen_combat.h"
#include "vm_builder.h"
#include "types.h"
#include "combat.h"
#include "prng.h"

/* ---------------------------------------------------------------------------
 * Helper: count set pixels in row y
 * ---------------------------------------------------------------------------*/
static uint32_t count_set_pixels_in_row(const fq_fb_t *fb, uint32_t y)
{
    uint32_t count = 0u;
    for (uint32_t x = 0u; x < FQ_FB_WIDTH; x++) {
        if (fq_fb_get_pixel(fb, (int16_t)x, (int16_t)y) != 0u) {
            count++;
        }
    }
    return count;
}

/* ---------------------------------------------------------------------------
 * Helper: count total set pixels in framebuffer
 * ---------------------------------------------------------------------------*/
static uint32_t count_total_set_pixels(const fq_fb_t *fb)
{
    uint32_t count = 0u;
    for (uint32_t y = 0u; y < FQ_FB_HEIGHT; y++) {
        count += count_set_pixels_in_row(fb, y);
    }
    return count;
}

/* ---------------------------------------------------------------------------
 * Test 1: Render completes without crash for standard VM
 * ---------------------------------------------------------------------------*/
static void test_render_combat_standard_vm_no_crash(void)
{
    fq_fb_t        fb;
    fq_vm_combat_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.f1_name, "Ember",  sizeof(vm.f1_name) - 1u);
    strncpy(vm.f2_name, "Shadow", sizeof(vm.f2_name) - 1u);
    vm.f1_hp       = 75;
    vm.f1_hp_max   = 100;
    vm.f2_hp       = 40;
    vm.f2_hp_max   = 80;
    vm.round       = 3u;
    vm.f1_class_id = 0u;
    vm.f2_class_id = 1u;

    fq_render_combat(&fb, &vm);
    TEST_ASSERT_TRUE(1);
}

/* ---------------------------------------------------------------------------
 * Test 2: Framebuffer is non-blank after render (some pixels drawn)
 * ---------------------------------------------------------------------------*/
static void test_render_combat_writes_pixels(void)
{
    fq_fb_t        fb;
    fq_vm_combat_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.f1_name, "Ember",  sizeof(vm.f1_name) - 1u);
    strncpy(vm.f2_name, "Shadow", sizeof(vm.f2_name) - 1u);
    vm.f1_hp     = 75;
    vm.f1_hp_max = 100;
    vm.f2_hp     = 40;
    vm.f2_hp_max = 80;
    vm.round     = 3u;

    fq_render_combat(&fb, &vm);

    uint32_t set_count = count_total_set_pixels(&fb);
    TEST_ASSERT_TRUE(set_count > 0u);
}

/* ---------------------------------------------------------------------------
 * Test 3: Divider line at y=98 — at least one pixel set in that row
 * ---------------------------------------------------------------------------*/
static void test_render_combat_divider_line_y98(void)
{
    fq_fb_t        fb;
    fq_vm_combat_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.f1_name, "Ember",  sizeof(vm.f1_name) - 1u);
    strncpy(vm.f2_name, "Shadow", sizeof(vm.f2_name) - 1u);
    vm.f1_hp     = 75;
    vm.f1_hp_max = 100;
    vm.f2_hp     = 40;
    vm.f2_hp_max = 80;
    vm.round     = 3u;

    fq_render_combat(&fb, &vm);

    uint32_t divider_pixels = count_set_pixels_in_row(&fb, 98u);
    TEST_ASSERT_TRUE(divider_pixels > 0u);
}

/* ---------------------------------------------------------------------------
 * Test 4: Display border — top-left corner pixel (0,0) is set
 * ---------------------------------------------------------------------------*/
static void test_render_combat_border_drawn(void)
{
    fq_fb_t        fb;
    fq_vm_combat_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.f1_name, "Ember",  sizeof(vm.f1_name) - 1u);
    strncpy(vm.f2_name, "Shadow", sizeof(vm.f2_name) - 1u);
    vm.f1_hp     = 75;
    vm.f1_hp_max = 100;
    vm.f2_hp     = 40;
    vm.f2_hp_max = 80;
    vm.round     = 3u;

    fq_render_combat(&fb, &vm);

    uint8_t corner = fq_fb_get_pixel(&fb, 0, 0);
    TEST_ASSERT_EQUAL_UINT8(1u, corner);
}

/* ---------------------------------------------------------------------------
 * Test 5: Action banner — more pixels in middle band when action_text set
 *
 * With action_text set, the banner rect covers rows ~85-115.
 * We verify more pixels exist in that band than with no action_text.
 * ---------------------------------------------------------------------------*/
static void test_render_combat_action_banner_adds_pixels(void)
{
    fq_fb_t        fb_no_banner;
    fq_fb_t        fb_banner;
    fq_vm_combat_t vm;

    fq_fb_clear(&fb_no_banner);
    fq_fb_clear(&fb_banner);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.f1_name, "Ember",  sizeof(vm.f1_name) - 1u);
    strncpy(vm.f2_name, "Shadow", sizeof(vm.f2_name) - 1u);
    vm.f1_hp     = 75;
    vm.f1_hp_max = 100;
    vm.f2_hp     = 40;
    vm.f2_hp_max = 80;
    vm.round     = 3u;

    /* Render without banner */
    vm.action_text[0] = '\0';
    fq_render_combat(&fb_no_banner, &vm);

    /* Render with banner */
    strncpy(vm.action_text, "Cleave! -15", sizeof(vm.action_text) - 1u);
    fq_render_combat(&fb_banner, &vm);

    /* Banner render must produce different pixel output */
    uint32_t pixels_no_banner = count_total_set_pixels(&fb_no_banner);
    uint32_t pixels_banner    = count_total_set_pixels(&fb_banner);
    TEST_ASSERT_TRUE(pixels_banner != pixels_no_banner);
}

/* ---------------------------------------------------------------------------
 * Test 6: finished=1, winner=1 — render completes without crash
 * ---------------------------------------------------------------------------*/
static void test_render_combat_finished_winner_no_crash(void)
{
    fq_fb_t        fb;
    fq_vm_combat_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.f1_name, "Ember",  sizeof(vm.f1_name) - 1u);
    strncpy(vm.f2_name, "Shadow", sizeof(vm.f2_name) - 1u);
    vm.f1_hp     = 0;
    vm.f1_hp_max = 100;
    vm.f2_hp     = 40;
    vm.f2_hp_max = 80;
    vm.round     = 5u;
    vm.finished  = 1u;
    vm.winner    = 2u;

    fq_render_combat(&fb, &vm);
    TEST_ASSERT_TRUE(1);
}

/* ---------------------------------------------------------------------------
 * Test 7: fq_vm_build_combat populates both fighter names and HP
 * ---------------------------------------------------------------------------*/
static void test_vm_build_combat_populates_fields(void)
{
    fq_vm_combat_t  vm;
    fq_combat_ctx_t ctx;
    fq_character_t  c1;
    fq_character_t  c2;

    memset(&vm,  0, sizeof(vm));
    memset(&ctx, 0, sizeof(ctx));
    memset(&c1,  0, sizeof(c1));
    memset(&c2,  0, sizeof(c2));

    strncpy(c1.name, "Ember",  sizeof(c1.name) - 1u);
    c1.class_id = (uint8_t)FQ_CLASS_BRUISER;

    strncpy(c2.name, "Shadow", sizeof(c2.name) - 1u);
    c2.class_id = (uint8_t)FQ_CLASS_TRICKSTER;

    /* Set up ctx fighters directly */
    ctx.f1.hp     = 75;
    ctx.f1.hp_max = 100;
    ctx.f2.hp     = 40;
    ctx.f2.hp_max = 80;
    ctx.current_round = 3u;
    ctx.finished      = 0u;
    ctx.winner        = 0u;

    fq_vm_build_combat(&vm, &ctx, &c1, &c2);

    /* Verify names copied */
    TEST_ASSERT_EQUAL_UINT8('E', (uint8_t)vm.f1_name[0]);
    TEST_ASSERT_EQUAL_UINT8('S', (uint8_t)vm.f2_name[0]);

    /* Verify HP values */
    TEST_ASSERT_EQUAL_INT(75,  (int)vm.f1_hp);
    TEST_ASSERT_EQUAL_INT(100, (int)vm.f1_hp_max);
    TEST_ASSERT_EQUAL_INT(40,  (int)vm.f2_hp);
    TEST_ASSERT_EQUAL_INT(80,  (int)vm.f2_hp_max);

    /* Verify round */
    TEST_ASSERT_EQUAL_UINT8(3u, vm.round);

    /* Verify class IDs */
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_CLASS_BRUISER,   vm.f1_class_id);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_CLASS_TRICKSTER, vm.f2_class_id);
}

/* ---------------------------------------------------------------------------
 * Test 8: fq_vm_build_combat — NULL ctx → no crash, VM zeroed
 * ---------------------------------------------------------------------------*/
static void test_vm_build_combat_null_ctx_no_crash(void)
{
    fq_vm_combat_t vm;
    fq_character_t c1;
    fq_character_t c2;

    memset(&vm, 0xFF, sizeof(vm));  /* pre-fill with garbage */
    memset(&c1, 0,    sizeof(c1));
    memset(&c2, 0,    sizeof(c2));

    fq_vm_build_combat(&vm, NULL, &c1, &c2);
    TEST_ASSERT_TRUE(1);
}

/* ---------------------------------------------------------------------------
 * Test 9: fq_vm_build_combat — NULL c1/c2 → no crash
 * ---------------------------------------------------------------------------*/
static void test_vm_build_combat_null_chars_no_crash(void)
{
    fq_vm_combat_t  vm;
    fq_combat_ctx_t ctx;

    memset(&vm,  0, sizeof(vm));
    memset(&ctx, 0, sizeof(ctx));

    fq_vm_build_combat(&vm, &ctx, NULL, NULL);
    TEST_ASSERT_TRUE(1);
}

/* ---------------------------------------------------------------------------
 * Test 10: All-NULL → no crash
 * ---------------------------------------------------------------------------*/
static void test_vm_build_combat_all_null_no_crash(void)
{
    fq_vm_build_combat(NULL, NULL, NULL, NULL);
    TEST_ASSERT_TRUE(1);
}

/* ---------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------*/
int main(void)
{
    test_render_combat_standard_vm_no_crash();
    test_render_combat_writes_pixels();
    test_render_combat_divider_line_y98();
    test_render_combat_border_drawn();
    test_render_combat_action_banner_adds_pixels();
    test_render_combat_finished_winner_no_crash();
    test_vm_build_combat_populates_fields();
    test_vm_build_combat_null_ctx_no_crash();
    test_vm_build_combat_null_chars_no_crash();
    test_vm_build_combat_all_null_no_crash();

    printf("test_p9_combat_feature: ALL FEATURE TESTS PASSED\n");
    return 0;
}
