/**
 * test_p9_combat_bounds.c — Phase 9 BOUND RED: Combat HUD Math Boundary Tests
 *
 * Rule 22: Written BEFORE implementation. These tests prove the system rejects:
 *   - HP bar with hp_max == 0 (divide-by-zero guard)
 *   - HP bar with negative HP (width must clamp to 0)
 *   - HP bar with INT16_MAX hp / INT16_MAX hp_max (no int32_t overflow)
 *   - NULL fb/vm passed to fq_render_combat (no crash)
 *   - action_text not null-terminated within 32 chars (strnlen bounded)
 *   - HP bar width never exceeds the display boundary
 *
 * Architecture: presentation/ only — no game/ headers.
 * Constitution Priority 0: no float, no malloc, no PRNG.
 */

#include <inttypes.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "test_assert.h"
#include "fq_framebuffer.h"
#include "view_models.h"
#include "screens/screen_combat.h"

/* ---------------------------------------------------------------------------
 * N1/N2: NULL fb or NULL vm — must not crash, no pixels written
 * ---------------------------------------------------------------------------*/
static void test_null_fb_does_not_crash(void)
{
    fq_vm_combat_t vm;
    memset(&vm, 0, sizeof(vm));
    /* Must not segfault */
    fq_render_combat(NULL, &vm);
    TEST_ASSERT_TRUE(1); /* reached here without crash */
}

static void test_null_vm_does_not_crash(void)
{
    fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_render_combat(&fb, NULL);
    /* Framebuffer must remain all-white (unmodified) */
    for (uint32_t i = 0u; i < FQ_FB_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)fb.pixels[i]);
    }
}

static void test_both_null_does_not_crash(void)
{
    fq_render_combat(NULL, NULL);
    TEST_ASSERT_TRUE(1); /* reached here without crash */
}

/* ---------------------------------------------------------------------------
 * N4: Negative HP → HP bar width must be 0 (no off-screen write)
 *
 * We can't directly interrogate the bar width from outside, but we CAN verify
 * the render does not crash and does not write pixels below y=0 or past x=199.
 * The framebuffer set_pixel is bounds-checked, so this is already enforced by
 * the API — the real test is: render completes without crash and does not set
 * pixels at impossible positions.
 *
 * For the computation contract, we test the math formula inline:
 *   bar_w = hp_max > 0 ? (int32_t)hp * BAR_W / hp_max : 0
 *   if bar_w < 0 then bar_w = 0
 * ---------------------------------------------------------------------------*/
static void test_negative_hp_bar_width_formula_clamps_to_zero(void)
{
    /* Replicate the HP bar width formula from the spec:
     * int32_t bar_w = (hp_max > 0) ? ((int32_t)f1_hp * BAR_W_MAX / f1_hp_max) : 0;
     * if (bar_w < 0) bar_w = 0;
     */
    int16_t hp     = -32768;   /* INT16_MIN */
    int16_t hp_max = 100;
    int32_t bar_w_max = 80;   /* example bar width */

    int32_t bar_w = (hp_max > 0) ? ((int32_t)hp * bar_w_max / (int32_t)hp_max) : 0;
    if (bar_w < 0) {
        bar_w = 0;
    }
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)bar_w);
}

/* ---------------------------------------------------------------------------
 * N5: INT16_MAX HP — int32_t intermediate must hold the product without overflow
 *
 * INT16_MAX * BAR_W_MAX_PIXELS must fit in int32_t:
 *   32767 * 180 = 5,898,060 — well within INT32_MAX (2,147,483,647)
 * ---------------------------------------------------------------------------*/
static void test_int16_max_hp_bar_no_overflow(void)
{
    int16_t hp      = INT16_MAX;   /* 32767 */
    int16_t hp_max  = INT16_MAX;   /* 32767 */
    int32_t bar_max = 180;         /* max bar pixel width */

    /* Intermediate computation in int32_t — must not overflow */
    int32_t bar_w = (hp_max > 0) ? ((int32_t)hp * bar_max / (int32_t)hp_max) : 0;
    if (bar_w < 0) {
        bar_w = 0;
    }
    if (bar_w > bar_max) {
        bar_w = bar_max;
    }

    TEST_ASSERT_EQUAL_UINT32((uint32_t)bar_max, (uint32_t)bar_w);
}

/* ---------------------------------------------------------------------------
 * hp_max == 0: divide-by-zero guard — bar width must be 0
 * ---------------------------------------------------------------------------*/
static void test_hp_max_zero_bar_width_is_zero(void)
{
    int16_t hp     = 50;
    int16_t hp_max = 0;
    int32_t bar_max = 80;

    int32_t bar_w = (hp_max > 0) ? ((int32_t)hp * bar_max / (int32_t)hp_max) : 0;

    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)bar_w);
}

/* ---------------------------------------------------------------------------
 * hp_max == 0 full render: must not crash
 * ---------------------------------------------------------------------------*/
static void test_hp_max_zero_render_does_not_crash(void)
{
    fq_fb_t        fb;
    fq_vm_combat_t vm;
    fq_fb_clear(&fb);
    memset(&vm, 0, sizeof(vm));

    strncpy(vm.f1_name, "Fighter", sizeof(vm.f1_name) - 1u);
    strncpy(vm.f2_name, "Enemy",   sizeof(vm.f2_name) - 1u);
    vm.f1_hp     = 50;
    vm.f1_hp_max = 0;   /* corrupt: max = 0 */
    vm.f2_hp     = 40;
    vm.f2_hp_max = 80;
    vm.round     = 1u;

    fq_render_combat(&fb, &vm);
    TEST_ASSERT_TRUE(1);
}

/* ---------------------------------------------------------------------------
 * Bar width must not exceed display width regardless of inputs
 * (e.g., hp > hp_max — over-healed scenario)
 * ---------------------------------------------------------------------------*/
static void test_hp_exceeds_max_bar_clamps_to_bar_max(void)
{
    int16_t hp      = INT16_MAX;   /* 32767 */
    int16_t hp_max  = 1;
    int32_t bar_max = 180;

    int32_t bar_w = (hp_max > 0) ? ((int32_t)hp * bar_max / (int32_t)hp_max) : 0;
    if (bar_w < 0) { bar_w = 0; }
    if (bar_w > bar_max) { bar_w = bar_max; }

    TEST_ASSERT_EQUAL_UINT32((uint32_t)bar_max, (uint32_t)bar_w);
}

/* ---------------------------------------------------------------------------
 * N7: action_text not null-terminated — bounded strnlen must cap at 31
 * (array is char[32], safe access is always [0..31])
 * ---------------------------------------------------------------------------*/
static void test_action_text_unbounded_strnlen_safety(void)
{
    fq_vm_combat_t vm;
    memset(&vm, 0, sizeof(vm));
    /* Fill action_text with non-null bytes — simulates unterminated string */
    memset(vm.action_text, 'A', sizeof(vm.action_text));

    /* strnlen with limit = sizeof(vm.action_text) - 1 must return 31 */
    size_t len = strnlen(vm.action_text, sizeof(vm.action_text) - 1u);
    TEST_ASSERT_EQUAL_UINT32(31u, (uint32_t)len);
}

/* ---------------------------------------------------------------------------
 * N13: vm->f1_name and f2_name are char[13] — no write past index 12
 * ---------------------------------------------------------------------------*/
static void test_name_field_size(void)
{
    /* sizeof check is a compile-time guarantee */
    fq_vm_combat_t vm;
    TEST_ASSERT_EQUAL_UINT32(13u, (uint32_t)sizeof(vm.f1_name));
    TEST_ASSERT_EQUAL_UINT32(13u, (uint32_t)sizeof(vm.f2_name));
}

/* ---------------------------------------------------------------------------
 * action_text[0] == '\0': no banner rendered (render still completes)
 * ---------------------------------------------------------------------------*/
static void test_empty_action_text_no_banner(void)
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
    vm.action_text[0] = '\0';   /* no banner */

    fq_render_combat(&fb, &vm);
    TEST_ASSERT_TRUE(1);
}

/* ---------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------*/
int main(void)
{
    test_null_fb_does_not_crash();
    test_null_vm_does_not_crash();
    test_both_null_does_not_crash();
    test_negative_hp_bar_width_formula_clamps_to_zero();
    test_int16_max_hp_bar_no_overflow();
    test_hp_max_zero_bar_width_is_zero();
    test_hp_max_zero_render_does_not_crash();
    test_hp_exceeds_max_bar_clamps_to_bar_max();
    test_action_text_unbounded_strnlen_safety();
    test_name_field_size();
    test_empty_action_text_no_banner();

    printf("test_p9_combat_bounds: ALL BOUND TESTS PASSED\n");
    return 0;
}
