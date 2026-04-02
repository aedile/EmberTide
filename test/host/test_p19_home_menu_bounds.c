/**
 * test_p19_home_menu_bounds.c — Phase 19 Bound Tests: Home Screen Navigation Menu
 *
 * Bound constraints tested:
 *   B1: menu_index overflow — wraps at 4, never exceeds 3.
 *   B2: home_menu_index overflow in FSM ctx — saturates / wraps on increment.
 *   B3: fq_render_home with vm->menu_index > 3 must not write OOB pixels
 *       (renderer must clamp menu_index to [0,3]).
 *   B4: NULL fb/vm guards on fq_render_home still hold with new menu rendering.
 *   B5: menu_index = 255 in vm is safely clamped — no negative array index.
 *
 * These tests MUST FAIL before implementation (RED phase).
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "test_assert.h"
#include "types.h"
#include "event_bus.h"
#include "app_fsm.h"
#include "view_models.h"
#include "fq_framebuffer.h"
#include "screens/screen_home.h"
#include "vm_builder.h"

/* Helper: build a zeroed vm_home with the given menu_index */
static fq_vm_home_t make_vm(uint8_t menu_index)
{
    fq_vm_home_t vm;
    memset(&vm, 0, sizeof(vm));
    strncpy(vm.name, "Ember", sizeof(vm.name) - 1u);
    vm.level      = 1u;
    vm.hp_percent = 100u;
    vm.menu_index = menu_index;
    return vm;
}

/* Helper: cycle menu BTN_B presses and return final home_menu_index */
static uint8_t cycle_menu(uint8_t n)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&ctx, &player, &inv);

    /* Advance to HOME state */
    fq_event_t e_press = { FQ_EVT_BTN_A_PRESS, 0u };
    fq_app_dispatch(&ctx, &e_press);

    /* Cycle menu */
    fq_event_t e_b = { FQ_EVT_BTN_B_PRESS, 0u };
    for (uint8_t i = 0u; i < n; i++) {
        fq_app_dispatch(&ctx, &e_b);
    }
    return ctx.home_menu_index;
}

int main(void)
{
    /* -----------------------------------------------------------------------
     * B1: menu_index must wrap at 4.
     *     After 4 BTN_B presses from HOME, index wraps back to 0.
     * ----------------------------------------------------------------------- */
    uint8_t idx = cycle_menu(4u);
    TEST_ASSERT_EQUAL_UINT8(0u, idx);

    /* B1b: After 5 presses = index 1 */
    idx = cycle_menu(5u);
    TEST_ASSERT_EQUAL_UINT8(1u, idx);

    /* B1c: After 7 presses = index 3 (last valid) */
    idx = cycle_menu(7u);
    TEST_ASSERT_EQUAL_UINT8(3u, idx);

    /* -----------------------------------------------------------------------
     * B2: home_menu_index never exceeds 3 even on many presses.
     *     After 100 BTN_B presses the index must be in [0,3].
     * ----------------------------------------------------------------------- */
    idx = cycle_menu(100u);
    TEST_ASSERT_TRUE(idx <= 3u);

    /* -----------------------------------------------------------------------
     * B3: fq_render_home with menu_index = 255 must not crash.
     *     The renderer must clamp the value to [0,3].
     *     We just verify it does not abort (if it ran past bounds it would
     *     likely segfault or corrupt memory caught by address sanitizer).
     * ----------------------------------------------------------------------- */
    static fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_vm_home_t vm_oob = make_vm(255u);
    fq_render_home(&fb, &vm_oob);
    /* If we reach here without aborting, the clamping is safe. */

    /* -----------------------------------------------------------------------
     * B4: NULL guards — no crash with new menu rendering path.
     * ----------------------------------------------------------------------- */
    fq_render_home(NULL, NULL);
    fq_render_home(&fb, NULL);
    fq_vm_home_t vm_valid = make_vm(0u);
    fq_render_home(NULL, &vm_valid);
    /* All three must be silent no-ops — reaching here means pass. */

    /* -----------------------------------------------------------------------
     * B5: menu_index = 4 is a boundary exactly — must wrap to 0 in renderer.
     *     (4 is one past the last valid index.)
     * ----------------------------------------------------------------------- */
    fq_fb_clear(&fb);
    fq_vm_home_t vm_4 = make_vm(4u);
    fq_render_home(&fb, &vm_4);
    /* Must not crash. */

    printf("test_p19_home_menu_bounds: PASS\n");
    return 0;
}
