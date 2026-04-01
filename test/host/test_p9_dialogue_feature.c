/**
 * test_p9_dialogue_feature.c — Phase 9 FEATURE RED: Dialogue & Training Tests
 *
 * Rule 22: Written BEFORE implementation (GREEN phase).
 * Tests define the happy-path rendering contract for fq_render_dialogue
 * and fq_render_training.
 *
 * Tests:
 *   - Dialogue render produces non-blank output (border + text area drawn)
 *   - Title + body render completes without crash
 *   - show_yes_no=1 produces different output from show_yes_no=0
 *   - Training state=0/1/2 render completes without crash
 *   - Training score=0 / score=100 both render without crash
 *   - Training vm game_name populated correctly
 *
 * Architecture: presentation/ only — no game/ headers.
 * Constitution Priority 0: no float, no malloc, no PRNG.
 */

#include <inttypes.h>
#include <string.h>
#include <stdint.h>
#include "test_assert.h"
#include "fq_framebuffer.h"
#include "view_models.h"
#include "ui_widgets.h"
#include "screens/screen_training.h"

/* ---------------------------------------------------------------------------
 * Helper: count total set pixels
 * ---------------------------------------------------------------------------*/
static uint32_t count_total_set_pixels(const fq_fb_t *fb)
{
    uint32_t count = 0u;
    for (uint32_t i = 0u; i < FQ_FB_SIZE; i++) {
        uint8_t b = fb->pixels[i];
        /* Count set bits via Brian Kernighan */
        while (b) {
            count++;
            b = b & (uint8_t)(b - 1u);
        }
    }
    return count;
}

/* ---------------------------------------------------------------------------
 * Test 1: Dialogue with title+body produces non-blank output (border drawn)
 * ---------------------------------------------------------------------------*/
static void test_dialogue_writes_pixels(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_render_dialogue(&fb, "REBIRTH", "Your character has fallen.", 0u);

    uint32_t pixels = count_total_set_pixels(&fb);
    TEST_ASSERT_TRUE(pixels > 0u);
}

/* ---------------------------------------------------------------------------
 * Test 2: show_yes_no=1 produces different (more) pixels than show_yes_no=0
 * ---------------------------------------------------------------------------*/
static void test_dialogue_yes_no_differs_from_no_buttons(void)
{
    fq_fb_t fb0;
    fq_fb_t fb1;
    fq_fb_clear(&fb0);
    fq_fb_clear(&fb1);

    fq_render_dialogue(&fb0, "REBIRTH", "Would you like to be reborn?", 0u);
    fq_render_dialogue(&fb1, "REBIRTH", "Would you like to be reborn?", 1u);

    uint32_t p0 = count_total_set_pixels(&fb0);
    uint32_t p1 = count_total_set_pixels(&fb1);
    TEST_ASSERT_TRUE(p0 != p1);
}

/* ---------------------------------------------------------------------------
 * Test 3: Dialogue box overlays bottom portion of screen
 * The dialogue box targets the bottom ~80px (y=120 to y=199).
 * Pixels below y=120 must include the border (at least one set pixel).
 * ---------------------------------------------------------------------------*/
static void test_dialogue_bottom_region_has_pixels(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_render_dialogue(&fb, "TEST", "Some body text here.", 0u);

    uint32_t bottom_pixels = 0u;
    for (uint32_t y = 120u; y < FQ_FB_HEIGHT; y++) {
        for (uint32_t x = 0u; x < FQ_FB_WIDTH; x++) {
            if (fq_fb_get_pixel(&fb, (int16_t)x, (int16_t)y) != 0u) {
                bottom_pixels++;
            }
        }
    }
    TEST_ASSERT_TRUE(bottom_pixels > 0u);
}

/* ---------------------------------------------------------------------------
 * Test 4: Empty title and empty body — render completes, border still drawn
 * ---------------------------------------------------------------------------*/
static void test_dialogue_empty_strings_render(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_render_dialogue(&fb, "", "", 0u);

    uint32_t pixels = count_total_set_pixels(&fb);
    /* Border still draws, so some pixels must be set */
    TEST_ASSERT_TRUE(pixels > 0u);
}

/* ---------------------------------------------------------------------------
 * Test 5: Training screen state=0 (waiting) — renders without crash
 * ---------------------------------------------------------------------------*/
static void test_training_state_waiting_renders(void)
{
    fq_fb_t          fb;
    fq_vm_training_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.game_name, "Speed", sizeof(vm.game_name) - 1u);
    vm.score      = 0u;
    vm.difficulty = 1u;
    vm.state      = 0u;  /* waiting */

    fq_render_training(&fb, &vm);
    TEST_ASSERT_TRUE(1);
}

/* ---------------------------------------------------------------------------
 * Test 6: Training screen state=1 (active) — renders without crash
 * ---------------------------------------------------------------------------*/
static void test_training_state_active_renders(void)
{
    fq_fb_t          fb;
    fq_vm_training_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.game_name, "Power", sizeof(vm.game_name) - 1u);
    vm.score      = 50u;
    vm.difficulty = 2u;
    vm.state      = 1u;  /* active */

    fq_render_training(&fb, &vm);
    TEST_ASSERT_TRUE(1);
}

/* ---------------------------------------------------------------------------
 * Test 7: Training screen state=2 (done), score=70 — renders without crash
 * ---------------------------------------------------------------------------*/
static void test_training_state_done_renders(void)
{
    fq_fb_t          fb;
    fq_vm_training_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.game_name, "Speed", sizeof(vm.game_name) - 1u);
    vm.score      = 70u;
    vm.difficulty = 3u;
    vm.state      = 2u;  /* done */

    fq_render_training(&fb, &vm);
    TEST_ASSERT_TRUE(1);
}

/* ---------------------------------------------------------------------------
 * Test 8: Training screen produces non-blank output (border drawn)
 * ---------------------------------------------------------------------------*/
static void test_training_writes_pixels(void)
{
    fq_fb_t          fb;
    fq_vm_training_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.game_name, "Intel", sizeof(vm.game_name) - 1u);
    vm.score = 85u;
    vm.state = 2u;

    fq_render_training(&fb, &vm);

    uint32_t pixels = count_total_set_pixels(&fb);
    TEST_ASSERT_TRUE(pixels > 0u);
}

/* ---------------------------------------------------------------------------
 * Test 9: Training score=0 — renders without crash
 * ---------------------------------------------------------------------------*/
static void test_training_score_zero_renders(void)
{
    fq_fb_t          fb;
    fq_vm_training_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.game_name, "Speed", sizeof(vm.game_name) - 1u);
    vm.score = 0u;
    vm.state = 2u;

    fq_render_training(&fb, &vm);
    TEST_ASSERT_TRUE(1);
}

/* ---------------------------------------------------------------------------
 * Test 10: Training score=100 — renders without crash
 * ---------------------------------------------------------------------------*/
static void test_training_score_100_renders(void)
{
    fq_fb_t          fb;
    fq_vm_training_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.game_name, "Power", sizeof(vm.game_name) - 1u);
    vm.score = 100u;
    vm.state = 2u;

    fq_render_training(&fb, &vm);
    TEST_ASSERT_TRUE(1);
}

/* ---------------------------------------------------------------------------
 * Test 11: fq_vm_training_t field access — game_name null-terminated
 * ---------------------------------------------------------------------------*/
static void test_training_vm_game_name_null_terminated(void)
{
    fq_vm_training_t vm;
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.game_name, "Speed", sizeof(vm.game_name) - 1u);
    vm.game_name[sizeof(vm.game_name) - 1u] = '\0';

    TEST_ASSERT_EQUAL_UINT8('S', (uint8_t)vm.game_name[0]);
    TEST_ASSERT_EQUAL_UINT8('\0', (uint8_t)vm.game_name[sizeof(vm.game_name) - 1u]);
}

/* ---------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------*/
int main(void)
{
    test_dialogue_writes_pixels();
    test_dialogue_yes_no_differs_from_no_buttons();
    test_dialogue_bottom_region_has_pixels();
    test_dialogue_empty_strings_render();
    test_training_state_waiting_renders();
    test_training_state_active_renders();
    test_training_state_done_renders();
    test_training_writes_pixels();
    test_training_score_zero_renders();
    test_training_score_100_renders();
    test_training_vm_game_name_null_terminated();

    printf("test_p9_dialogue_feature: ALL FEATURE TESTS PASSED\n");
    return 0;
}
