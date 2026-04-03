/**
 * test_p17_title_fix_bounds.c — Bound Tests: Title Screen Fix
 *
 * Rule 22: BOUND RED before FEATURE RED.
 *
 * Proves the system REJECTS or handles boundary conditions for the
 * title screen fixes introduced in phase 17 patch:
 *
 *   B1: TITLE + BTN_B_PRESS → HOME (after fix; fails before fix).
 *       PRNG isolation: combat.rng.state unchanged by TITLE dispatch.
 *
 *   B2: Ghost events in TITLE leave state unchanged (12 distinct event
 *       IDs that are NOT BTN_A_PRESS or BTN_B_PRESS).
 *
 *   B3: Idempotency — BTN_B after TITLE→HOME does HOME→BATTLE_SETUP,
 *       not a double-advance; no state corruption.
 *
 * Constitution Priority 0: combat.rng.state must be unmodified after
 * all TITLE-state dispatches.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stddef.h>

#include "test_assert.h"
#include "types.h"
#include "event_bus.h"
#include "app_fsm.h"

/* Ghost event IDs that must NOT trigger TITLE → HOME */
static const fq_event_id_t k_ghost_events[] = {
    FQ_EVT_NONE,
    FQ_EVT_BTN_A_LONG,
    FQ_EVT_BTN_B_LONG,
    FQ_EVT_TIMER_TICK,
    FQ_EVT_BLE_CONNECTED,
    FQ_EVT_BLE_PACKET_RX,
    FQ_EVT_BLE_DISCONNECTED,
    FQ_EVT_COMBAT_ROUND_COMPLETE,
    FQ_EVT_SAVE_COMPLETE,
    FQ_EVT_OTA_PROGRESS,
    FQ_EVT_AUTO_SLEEP_TIMEOUT,
};
#define K_GHOST_COUNT  ((size_t)(sizeof(k_ghost_events) / sizeof(k_ghost_events[0])))

/* Helper: init a fresh context (starts in TITLE after auto BOOT→TITLE) */
static void init_fresh(fq_app_ctx_t   *ctx,
                       fq_character_t *player,
                       fq_inventory_t *inv)
{
    memset(player, 0, sizeof(*player));
    memset(inv,    0, sizeof(*inv));
    game_err_t err = fq_app_init(ctx, player, inv);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_TITLE, (int)ctx->state);
}

int main(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    game_err_t     err;
    size_t         i;

    /* -------------------------------------------------------------------
     * B1: TITLE + BTN_B_PRESS → HOME
     *     After the fix, BTN_B_PRESS in TITLE must transition to HOME.
     *     PRNG isolation: combat.rng.state must be unchanged.
     * ------------------------------------------------------------------- */
    init_fresh(&ctx, &player, &inv);

    {
        uint32_t rng_before = ctx.combat.rng.state;
        fq_event_t btn_b = { FQ_EVT_BTN_B_PRESS, 0u };
        err = fq_app_dispatch(&ctx, &btn_b);
        TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);
        /* Must advance to HOME after the fix */
        TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)ctx.state);
        /* Constitution P0: PRNG must not have been touched */
        TEST_ASSERT_EQUAL_UINT32(rng_before, ctx.combat.rng.state);
    }

    /* -------------------------------------------------------------------
     * B2: Ghost events in TITLE must leave state TITLE unchanged.
     * ------------------------------------------------------------------- */
    for (i = 0u; i < K_GHOST_COUNT; i++) {
        fq_app_ctx_t   ghost_ctx;
        fq_character_t ghost_player;
        fq_inventory_t ghost_inv;
        fq_event_t     ghost_evt;

        init_fresh(&ghost_ctx, &ghost_player, &ghost_inv);
        ghost_evt.id   = k_ghost_events[i];
        ghost_evt.data = 0u;

        err = fq_app_dispatch(&ghost_ctx, &ghost_evt);
        TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);
        /* State must remain TITLE for all ghost events */
        TEST_ASSERT_EQUAL_INT((int)FQ_STATE_TITLE, (int)ghost_ctx.state);
    }

    /* -------------------------------------------------------------------
     * B3: Idempotency — extra BTN_B after TITLE→HOME cycles menu, then
     *     BTN_A selects BATTLE_SETUP. No double-advance or state corruption.
     *     Phase-19: BTN_B in HOME now cycles menu (no state change); BTN_A
     *     selects the highlighted item.
     * ------------------------------------------------------------------- */
    init_fresh(&ctx, &player, &inv);

    {
        fq_event_t btn_b = { FQ_EVT_BTN_B_PRESS, 0u };
        fq_event_t btn_a = { FQ_EVT_BTN_A_PRESS, 0u };

        /* First press: TITLE → HOME */
        fq_app_dispatch(&ctx, &btn_b);
        TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)ctx.state);

        /* Second press: HOME BTN_B cycles menu to index=1 (BATTLE), stays HOME */
        fq_app_dispatch(&ctx, &btn_b);
        TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)ctx.state);
        TEST_ASSERT_EQUAL_UINT8(1u, ctx.home_menu_index);

        /* BTN_A selects BATTLE → BATTLE_SETUP */
        fq_app_dispatch(&ctx, &btn_a);
        TEST_ASSERT_EQUAL_INT((int)FQ_STATE_BATTLE_SETUP, (int)ctx.state);
    }

    printf("test_p17_title_fix_bounds: PASS\n");
    return 0;
}
