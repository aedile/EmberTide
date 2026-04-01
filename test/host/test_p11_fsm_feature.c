/**
 * test_p11_fsm_feature.c — Phase 11 Feature Tests: Root State Machine
 *
 * Happy-path contract for fq_app_ctx_t and fq_app_dispatch():
 *
 * Transitions under test:
 *   BOOT  → TITLE    (automatic on fq_app_init)
 *   TITLE → HOME     (BTN_A_PRESS)
 *   HOME  → INVENTORY  (BTN_A_PRESS)
 *   HOME  → BATTLE_SETUP (BTN_B_PRESS)
 *   HOME  → TRAINING   (BTN_A_LONG)
 *   HOME  → STATS      (BTN_B_LONG)
 *   INVENTORY → HOME   (BTN_B_PRESS)
 *   STATS     → HOME   (BTN_B_PRESS)
 *   TRAINING  → HOME   (BTN_B_PRESS)
 *   BATTLE_SETUP → BATTLE (BLE_CONNECTED)
 *   BATTLE → BATTLE_RESULT (COMBAT_ROUND_COMPLETE with finished flag in data)
 *   BATTLE_RESULT → HOME (BTN_A_PRESS)
 *
 * Additional:
 *   - fq_app_init sets state = FQ_STATE_TITLE, player/inv pointers wired.
 *   - fq_app_init wires player pointer to ctx->player.
 *   - tick_count starts at 0.
 *   - combat_active starts at 0.
 *   - Unknown event in a state is silently ignored (GAME_OK, state unchanged).
 *   - N5: Re-entrant posting — post from within a dispatch loop is safe
 *     because the bus is a plain data structure with no callbacks.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "test_assert.h"
#include "types.h"
#include "event_bus.h"
#include "app_fsm.h"

/* Helper: dispatch a single event id with data=0, assert GAME_OK */
static void dispatch_ok(fq_app_ctx_t *ctx, fq_event_id_t id)
{
    fq_event_t e = { id, 0u };
    game_err_t err = fq_app_dispatch(ctx, &e);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);
}

int main(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;

    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));

    /* -----------------------------------------------------------------------
     * fq_app_init: state auto-transitions BOOT → TITLE.
     * ----------------------------------------------------------------------- */
    game_err_t init_err = fq_app_init(&ctx, &player, &inv);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)init_err);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_TITLE, (int)ctx.state);
    TEST_ASSERT_EQUAL_UINT32(0u, ctx.tick_count);
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.combat_active);

    /* Player pointer must be wired */
    TEST_ASSERT_TRUE(ctx.player == &player);
    TEST_ASSERT_TRUE(ctx.inventory == &inv);

    /* -----------------------------------------------------------------------
     * fq_app_init NULL guards.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t null_ctx;
    game_err_t null_init = fq_app_init(NULL, &player, &inv);
    TEST_ASSERT_EQUAL_INT((int)GAME_ERR_NULL_PTR, (int)null_init);

    null_init = fq_app_init(&null_ctx, NULL, &inv);
    TEST_ASSERT_EQUAL_INT((int)GAME_ERR_NULL_PTR, (int)null_init);

    null_init = fq_app_init(&null_ctx, &player, NULL);
    TEST_ASSERT_EQUAL_INT((int)GAME_ERR_NULL_PTR, (int)null_init);

    /* -----------------------------------------------------------------------
     * TITLE → HOME on BTN_A_PRESS.
     * ----------------------------------------------------------------------- */
    dispatch_ok(&ctx, FQ_EVT_BTN_A_PRESS);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)ctx.state);

    /* -----------------------------------------------------------------------
     * HOME → INVENTORY on BTN_A_PRESS.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t home_ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&home_ctx, &player, &inv);
    dispatch_ok(&home_ctx, FQ_EVT_BTN_A_PRESS); /* → HOME */
    dispatch_ok(&home_ctx, FQ_EVT_BTN_A_PRESS); /* HOME → INVENTORY */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_INVENTORY, (int)home_ctx.state);

    /* -----------------------------------------------------------------------
     * HOME → BATTLE_SETUP on BTN_B_PRESS.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t bs_ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&bs_ctx, &player, &inv);
    dispatch_ok(&bs_ctx, FQ_EVT_BTN_A_PRESS); /* → HOME */
    dispatch_ok(&bs_ctx, FQ_EVT_BTN_B_PRESS); /* HOME → BATTLE_SETUP */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_BATTLE_SETUP, (int)bs_ctx.state);

    /* -----------------------------------------------------------------------
     * HOME → TRAINING on BTN_A_LONG.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t tr_ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&tr_ctx, &player, &inv);
    dispatch_ok(&tr_ctx, FQ_EVT_BTN_A_PRESS); /* → HOME */
    dispatch_ok(&tr_ctx, FQ_EVT_BTN_A_LONG);  /* HOME → TRAINING */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_TRAINING, (int)tr_ctx.state);

    /* -----------------------------------------------------------------------
     * HOME → STATS on BTN_B_LONG.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t st_ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&st_ctx, &player, &inv);
    dispatch_ok(&st_ctx, FQ_EVT_BTN_A_PRESS); /* → HOME */
    dispatch_ok(&st_ctx, FQ_EVT_BTN_B_LONG);  /* HOME → STATS */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_STATS, (int)st_ctx.state);

    /* -----------------------------------------------------------------------
     * INVENTORY → HOME on BTN_B_PRESS.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t inv_ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&inv_ctx, &player, &inv);
    dispatch_ok(&inv_ctx, FQ_EVT_BTN_A_PRESS); /* → HOME */
    dispatch_ok(&inv_ctx, FQ_EVT_BTN_A_PRESS); /* → INVENTORY */
    dispatch_ok(&inv_ctx, FQ_EVT_BTN_B_PRESS); /* INVENTORY → HOME */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)inv_ctx.state);

    /* -----------------------------------------------------------------------
     * STATS → HOME on BTN_B_PRESS.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t stats_ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&stats_ctx, &player, &inv);
    dispatch_ok(&stats_ctx, FQ_EVT_BTN_A_PRESS); /* → HOME */
    dispatch_ok(&stats_ctx, FQ_EVT_BTN_B_LONG);  /* → STATS */
    dispatch_ok(&stats_ctx, FQ_EVT_BTN_B_PRESS); /* STATS → HOME */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)stats_ctx.state);

    /* -----------------------------------------------------------------------
     * TRAINING → HOME on BTN_B_PRESS.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t train_ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&train_ctx, &player, &inv);
    dispatch_ok(&train_ctx, FQ_EVT_BTN_A_PRESS); /* → HOME */
    dispatch_ok(&train_ctx, FQ_EVT_BTN_A_LONG);  /* → TRAINING */
    dispatch_ok(&train_ctx, FQ_EVT_BTN_B_PRESS); /* TRAINING → HOME */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)train_ctx.state);

    /* -----------------------------------------------------------------------
     * BATTLE_SETUP → BATTLE on BLE_CONNECTED.
     * combat_active must be 1 after entering BATTLE.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t battle_ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&battle_ctx, &player, &inv);
    dispatch_ok(&battle_ctx, FQ_EVT_BTN_A_PRESS); /* → HOME */
    dispatch_ok(&battle_ctx, FQ_EVT_BTN_B_PRESS); /* → BATTLE_SETUP */
    dispatch_ok(&battle_ctx, FQ_EVT_BLE_CONNECTED); /* BATTLE_SETUP → BATTLE */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_BATTLE, (int)battle_ctx.state);
    TEST_ASSERT_EQUAL_UINT8(1u, battle_ctx.combat_active);

    /* -----------------------------------------------------------------------
     * BATTLE → BATTLE_RESULT on COMBAT_ROUND_COMPLETE.
     * combat_active must be 0 after leaving BATTLE.
     * ----------------------------------------------------------------------- */
    fq_event_t combat_done = { FQ_EVT_COMBAT_ROUND_COMPLETE, 0u };
    game_err_t br_err = fq_app_dispatch(&battle_ctx, &combat_done);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)br_err);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_BATTLE_RESULT, (int)battle_ctx.state);
    TEST_ASSERT_EQUAL_UINT8(0u, battle_ctx.combat_active);

    /* -----------------------------------------------------------------------
     * BATTLE_RESULT → HOME on BTN_A_PRESS.
     * ----------------------------------------------------------------------- */
    dispatch_ok(&battle_ctx, FQ_EVT_BTN_A_PRESS);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)battle_ctx.state);

    /* -----------------------------------------------------------------------
     * Unknown event in HOME is silently ignored.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t ignore_ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&ignore_ctx, &player, &inv);
    dispatch_ok(&ignore_ctx, FQ_EVT_BTN_A_PRESS); /* → HOME */
    /* Dispatch an event with no transition defined in HOME */
    fq_event_t unknown = { FQ_EVT_AUTO_SLEEP_TIMEOUT, 0u };
    game_err_t ign_err = fq_app_dispatch(&ignore_ctx, &unknown);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)ign_err);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)ignore_ctx.state);

    /* -----------------------------------------------------------------------
     * N5: Re-entrant posting — post an event to ctx->bus from within a
     * dispatch call. Because there are no callbacks, this is inherently safe:
     * the event is queued and can be popped on the next iteration.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t reentrant_ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&reentrant_ctx, &player, &inv);
    dispatch_ok(&reentrant_ctx, FQ_EVT_BTN_A_PRESS); /* → HOME */

    /* Post an event directly into the bus */
    uint8_t post_rc = fq_event_bus_post(&reentrant_ctx.bus, FQ_EVT_BTN_A_PRESS, 0u);
    TEST_ASSERT_EQUAL_UINT8(1u, post_rc);
    TEST_ASSERT_EQUAL_UINT8(1u, fq_event_bus_pending(&reentrant_ctx.bus));

    /* Pop and dispatch it — this is how the main loop would process it */
    fq_event_t queued_evt;
    fq_event_bus_pop(&reentrant_ctx.bus, &queued_evt);
    game_err_t rr_err = fq_app_dispatch(&reentrant_ctx, &queued_evt);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)rr_err);
    /* HOME → INVENTORY (BTN_A_PRESS in HOME) */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_INVENTORY, (int)reentrant_ctx.state);

    /* -----------------------------------------------------------------------
     * State enum values check.
     * ----------------------------------------------------------------------- */
    TEST_ASSERT_EQUAL_INT(0,  (int)FQ_STATE_BOOT);
    TEST_ASSERT_EQUAL_INT(1,  (int)FQ_STATE_TITLE);
    TEST_ASSERT_EQUAL_INT(2,  (int)FQ_STATE_HOME);
    TEST_ASSERT_EQUAL_INT(3,  (int)FQ_STATE_BATTLE_SETUP);
    TEST_ASSERT_EQUAL_INT(4,  (int)FQ_STATE_BATTLE);
    TEST_ASSERT_EQUAL_INT(5,  (int)FQ_STATE_BATTLE_RESULT);
    TEST_ASSERT_EQUAL_INT(6,  (int)FQ_STATE_INVENTORY);
    TEST_ASSERT_EQUAL_INT(7,  (int)FQ_STATE_STATS);
    TEST_ASSERT_EQUAL_INT(8,  (int)FQ_STATE_TRAINING);
    TEST_ASSERT_EQUAL_INT(9,  (int)FQ_STATE_REBIRTH);
    TEST_ASSERT_EQUAL_INT(10, (int)FQ_STATE_SETTINGS);
    TEST_ASSERT_EQUAL_INT(11, (int)FQ_STATE_COUNT);

    /* -----------------------------------------------------------------------
     * A2 (QA P11-02): tick_count increments on TIMER_TICK in ALL states.
     *
     * Init ctx, dispatch TIMER_TICK twice, assert tick_count == 2.
     * Fires in TITLE state to verify the ALL-STATES contract from the header.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t tick_ctx;
    fq_character_t tick_player;
    fq_inventory_t tick_inv;
    memset(&tick_player, 0, sizeof(tick_player));
    memset(&tick_inv,    0, sizeof(tick_inv));
    fq_app_init(&tick_ctx, &tick_player, &tick_inv);
    /* After init we are in TITLE, tick_count == 0 */
    TEST_ASSERT_EQUAL_UINT32(0u, tick_ctx.tick_count);

    fq_event_t tick_evt = { FQ_EVT_TIMER_TICK, 0u };
    game_err_t tick_err;

    tick_err = fq_app_dispatch(&tick_ctx, &tick_evt);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)tick_err);
    TEST_ASSERT_EQUAL_UINT32(1u, tick_ctx.tick_count);

    tick_err = fq_app_dispatch(&tick_ctx, &tick_evt);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)tick_err);
    TEST_ASSERT_EQUAL_UINT32(2u, tick_ctx.tick_count);

    printf("test_p11_fsm_feature: PASS\n");
    return 0;
}
