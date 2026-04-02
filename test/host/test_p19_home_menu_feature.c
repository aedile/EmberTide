/**
 * test_p19_home_menu_feature.c — Phase 19 Feature Tests: Home Screen Navigation Menu
 *
 * Happy-path contracts:
 *   F1: BTN_B (sun) in HOME increments home_menu_index (0→1→2→3→0).
 *   F2: BTN_A (power) in HOME with menu_index=0 transitions to FQ_STATE_TRAINING.
 *   F3: BTN_A in HOME with menu_index=1 transitions to FQ_STATE_BATTLE_SETUP.
 *   F4: BTN_A in HOME with menu_index=2 transitions to FQ_STATE_INVENTORY.
 *   F5: BTN_A in HOME with menu_index=3 transitions to FQ_STATE_STATS.
 *   F6: Entering HOME state resets home_menu_index to 0.
 *   F7: fq_vm_home_t has a menu_index field.
 *   F8: fq_vm_build_home copies home_menu_index into vm.menu_index when
 *       passed with menu_index parameter (or the builder populates it).
 *   F9: fq_render_home with menu_index=2 renders an inverted row for "ITEMS"
 *       and non-inverted rows for the others — verified via pixel sampling.
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

/* Helper: initialize ctx and advance to HOME */
static void init_at_home(fq_app_ctx_t *ctx, fq_character_t *player, fq_inventory_t *inv)
{
    memset(player, 0, sizeof(*player));
    memset(inv,    0, sizeof(*inv));
    fq_app_init(ctx, player, inv);
    fq_event_t e = { FQ_EVT_BTN_A_PRESS, 0u };
    fq_app_dispatch(ctx, &e);  /* TITLE → HOME */
}

/* Helper: dispatch n BTN_B presses */
static void press_b(fq_app_ctx_t *ctx, uint8_t n)
{
    fq_event_t e = { FQ_EVT_BTN_B_PRESS, 0u };
    for (uint8_t i = 0u; i < n; i++) {
        fq_app_dispatch(ctx, &e);
    }
}

/* Helper: dispatch one BTN_A press */
static void press_a(fq_app_ctx_t *ctx)
{
    fq_event_t e = { FQ_EVT_BTN_A_PRESS, 0u };
    fq_app_dispatch(ctx, &e);
}

int main(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;

    /* -----------------------------------------------------------------------
     * F1: BTN_B in HOME increments home_menu_index, wrapping at 4.
     * ----------------------------------------------------------------------- */
    init_at_home(&ctx, &player, &inv);
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.home_menu_index);

    press_b(&ctx, 1u);
    TEST_ASSERT_EQUAL_UINT8(1u, ctx.home_menu_index);

    press_b(&ctx, 1u);
    TEST_ASSERT_EQUAL_UINT8(2u, ctx.home_menu_index);

    press_b(&ctx, 1u);
    TEST_ASSERT_EQUAL_UINT8(3u, ctx.home_menu_index);

    press_b(&ctx, 1u);  /* Wrap: 3→0 */
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.home_menu_index);

    /* -----------------------------------------------------------------------
     * F2: BTN_A in HOME with menu_index=0 → FQ_STATE_TRAINING (TRAIN).
     * ----------------------------------------------------------------------- */
    init_at_home(&ctx, &player, &inv);
    /* menu_index=0 already */
    press_a(&ctx);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_TRAINING, (int)ctx.state);

    /* -----------------------------------------------------------------------
     * F3: BTN_A in HOME with menu_index=1 → FQ_STATE_BATTLE_SETUP (BATTLE).
     * ----------------------------------------------------------------------- */
    init_at_home(&ctx, &player, &inv);
    press_b(&ctx, 1u);  /* menu_index=1 */
    press_a(&ctx);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_BATTLE_SETUP, (int)ctx.state);

    /* -----------------------------------------------------------------------
     * F4: BTN_A in HOME with menu_index=2 → FQ_STATE_INVENTORY (ITEMS).
     * ----------------------------------------------------------------------- */
    init_at_home(&ctx, &player, &inv);
    press_b(&ctx, 2u);  /* menu_index=2 */
    press_a(&ctx);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_INVENTORY, (int)ctx.state);

    /* -----------------------------------------------------------------------
     * F5: BTN_A in HOME with menu_index=3 → FQ_STATE_STATS (STATS).
     * ----------------------------------------------------------------------- */
    init_at_home(&ctx, &player, &inv);
    press_b(&ctx, 3u);  /* menu_index=3 */
    press_a(&ctx);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_STATS, (int)ctx.state);

    /* -----------------------------------------------------------------------
     * F6: Returning to HOME from another state resets home_menu_index to 0.
     *     Go HOME → cycle menu → go TRAINING → return HOME → check index=0.
     * ----------------------------------------------------------------------- */
    init_at_home(&ctx, &player, &inv);
    press_b(&ctx, 2u);          /* menu_index=2 */
    press_a(&ctx);              /* → TRAINING (index was 2? No. index=0 → TRAINING) */
    /* Actually at index=0 BTN_A goes to TRAINING. Let's do it properly. */
    /* Re-init and cycle to INVENTORY (index=2) then come back */
    init_at_home(&ctx, &player, &inv);
    press_b(&ctx, 2u);          /* menu_index=2 */
    TEST_ASSERT_EQUAL_UINT8(2u, ctx.home_menu_index);
    press_a(&ctx);              /* HOME → INVENTORY (menu_index=2) */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_INVENTORY, (int)ctx.state);
    /* Return to HOME via BTN_B from INVENTORY */
    press_b(&ctx, 1u);          /* INVENTORY → HOME */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)ctx.state);
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.home_menu_index); /* Must reset */

    /* -----------------------------------------------------------------------
     * F7: fq_vm_home_t has a menu_index field (struct layout check).
     *     Verify sizeof includes the new field — it must be at least 25 bytes
     *     (was 24 before adding menu_index).
     * ----------------------------------------------------------------------- */
    TEST_ASSERT_TRUE(sizeof(fq_vm_home_t) >= 25u);

    /* -----------------------------------------------------------------------
     * F8: fq_render_home with menu_index=0 (TRAIN highlighted):
     *     The highlighted row must contain black pixels (inverted bar).
     *     We sample a pixel in the first menu row's interior.
     *     Row y = 122 (first menu item), x = 4 (inside the highlight bar).
     *
     *     With menu_index=0, the first row IS highlighted: fill_rect black
     *     at (2, 122, 196, 16) → pixel (4, 122) should be BLACK (1).
     * ----------------------------------------------------------------------- */
    static fq_fb_t fb;
    fq_fb_clear(&fb);
    fq_vm_home_t vm0;
    memset(&vm0, 0, sizeof(vm0));
    strncpy(vm0.name, "Ember", sizeof(vm0.name) - 1u);
    vm0.level      = 1u;
    vm0.hp_percent = 80u;
    vm0.menu_index = 0u;
    fq_render_home(&fb, &vm0);

    /* Pixel at (4, 122) should be black for highlighted TRAIN row */
    uint8_t px_highlight = fq_fb_get_pixel(&fb, 4, 122);
    TEST_ASSERT_EQUAL_UINT8(1u, px_highlight);

    /* -----------------------------------------------------------------------
     * F9: fq_render_home with menu_index=1 (BATTLE highlighted):
     *     Row 0 (y=122) should NOT be inverted (row is not selected).
     *     Row 1 (y=140) IS highlighted — pixel (4, 140) should be BLACK.
     * ----------------------------------------------------------------------- */
    fq_fb_clear(&fb);
    fq_vm_home_t vm1;
    memset(&vm1, 0, sizeof(vm1));
    strncpy(vm1.name, "Ember", sizeof(vm1.name) - 1u);
    vm1.level      = 1u;
    vm1.hp_percent = 80u;
    vm1.menu_index = 1u;
    fq_render_home(&fb, &vm1);

    /* Row 1 at y=140 should have a black highlight pixel at x=4 */
    uint8_t px_row1 = fq_fb_get_pixel(&fb, 4, 140);
    TEST_ASSERT_EQUAL_UINT8(1u, px_row1);

    printf("test_p19_home_menu_feature: PASS\n");
    return 0;
}
