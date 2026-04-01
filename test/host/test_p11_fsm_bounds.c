/**
 * test_p11_fsm_bounds.c — Phase 11 Bound Tests: Root State Machine
 *
 * Rule 22: Written BEFORE implementation to prove the system REJECTS:
 *   N3:  FSM with garbage context (memset 0xFF) dispatched with NULL evt
 *        returns GAME_ERR_NULL_PTR and does not crash.
 *   N4:  Invalid state transitions — BOOT + BTN_A_PRESS stays in BOOT
 *        (BOOT only exits via init auto-transition, not button presses).
 *   N6:  PRNG isolation — combat.rng.state unchanged by non-battle events.
 *   N8:  State persistence across 10 unhandled events — state stays unchanged.
 *   E3:  Ghost event — FQ_EVT_COMBAT_ROUND_COMPLETE in FQ_STATE_TITLE is
 *        silently ignored, state stays TITLE.
 *   FSM NULL ctx → GAME_ERR_NULL_PTR.
 *   FSM NULL evt → GAME_ERR_NULL_PTR.
 *
 * Constitution Priority 0: verifies PRNG isolation contract.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "test_assert.h"
#include "types.h"
#include "event_bus.h"
#include "app_fsm.h"

int main(void)
{
    /* -----------------------------------------------------------------------
     * FSM NULL ctx guard.
     * ----------------------------------------------------------------------- */
    fq_event_t evt = { FQ_EVT_BTN_A_PRESS, 0u };
    game_err_t err = fq_app_dispatch(NULL, &evt);
    TEST_ASSERT_EQUAL_INT((int)GAME_ERR_NULL_PTR, (int)err);

    /* -----------------------------------------------------------------------
     * FSM NULL evt guard.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t ctx;
    fq_character_t player;
    fq_inventory_t inv;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&ctx, &player, &inv);

    err = fq_app_dispatch(&ctx, NULL);
    TEST_ASSERT_EQUAL_INT((int)GAME_ERR_NULL_PTR, (int)err);

    /* -----------------------------------------------------------------------
     * N3: Garbage context (0xFF fill) dispatched with NULL evt.
     * Must return GAME_ERR_NULL_PTR and not crash.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t garbage_ctx;
    memset(&garbage_ctx, 0xFF, sizeof(garbage_ctx));
    err = fq_app_dispatch(&garbage_ctx, NULL);
    TEST_ASSERT_EQUAL_INT((int)GAME_ERR_NULL_PTR, (int)err);

    /* -----------------------------------------------------------------------
     * N4: BOOT + BTN_A_PRESS stays in BOOT.
     *
     * After init, ctx is in TITLE (auto-transition from BOOT on init).
     * We manually force state back to BOOT to test the BOOT handler.
     * BOOT must only exit via the automatic init transition.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t boot_ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&boot_ctx, &player, &inv);
    /* Force state to BOOT to simulate invalid pre-init state */
    boot_ctx.state = FQ_STATE_BOOT;

    fq_event_t btn_a = { FQ_EVT_BTN_A_PRESS, 0u };
    err = fq_app_dispatch(&boot_ctx, &btn_a);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);
    /* Must remain in BOOT — button presses do not transition out of BOOT */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_BOOT, (int)boot_ctx.state);

    /* -----------------------------------------------------------------------
     * E3: Ghost event — COMBAT_ROUND_COMPLETE in TITLE is silently ignored.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t title_ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&title_ctx, &player, &inv);
    /* After init we are in TITLE */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_TITLE, (int)title_ctx.state);

    fq_event_t ghost_evt = { FQ_EVT_COMBAT_ROUND_COMPLETE, 0u };
    err = fq_app_dispatch(&title_ctx, &ghost_evt);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);
    /* State must remain TITLE — ghost event silently ignored */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_TITLE, (int)title_ctx.state);

    /* -----------------------------------------------------------------------
     * N6: PRNG isolation — combat.rng.state unchanged by non-battle events.
     *
     * After init, dispatch various non-battle events (BTN_A_PRESS, TIMER_TICK,
     * BLE_CONNECTED, SAVE_COMPLETE) and verify combat.rng.state has not changed.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t prng_ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&prng_ctx, &player, &inv);

    /* Record the initial combat.rng.state */
    uint32_t initial_rng_state = prng_ctx.combat.rng.state;

    /* Drive to HOME via BTN_A from TITLE */
    fq_event_t to_home = { FQ_EVT_BTN_A_PRESS, 0u };
    fq_app_dispatch(&prng_ctx, &to_home);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)prng_ctx.state);

    /* Dispatch several non-battle events */
    fq_event_t timer_tick    = { FQ_EVT_TIMER_TICK,        0u };
    fq_event_t ble_conn      = { FQ_EVT_BLE_CONNECTED,     0u };
    fq_event_t save_complete = { FQ_EVT_SAVE_COMPLETE,      0u };
    fq_event_t ota_progress  = { FQ_EVT_OTA_PROGRESS,       0u };

    fq_app_dispatch(&prng_ctx, &timer_tick);
    fq_app_dispatch(&prng_ctx, &ble_conn);
    fq_app_dispatch(&prng_ctx, &save_complete);
    fq_app_dispatch(&prng_ctx, &ota_progress);

    /* combat.rng.state MUST be unchanged */
    TEST_ASSERT_EQUAL_UINT32(initial_rng_state, prng_ctx.combat.rng.state);

    /* -----------------------------------------------------------------------
     * N8: State persistence across 10 unhandled events.
     *
     * In INVENTORY state, dispatch 10 events that have no defined transition
     * (e.g., TIMER_TICK, BLE_PACKET_RX, OTA_PROGRESS). State must remain
     * INVENTORY throughout.
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t persistent_ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&persistent_ctx, &player, &inv);

    /* Navigate: TITLE → HOME (BTN_A) → INVENTORY (BTN_A) */
    fq_app_dispatch(&persistent_ctx, &(fq_event_t){ FQ_EVT_BTN_A_PRESS, 0u });
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)persistent_ctx.state);
    fq_app_dispatch(&persistent_ctx, &(fq_event_t){ FQ_EVT_BTN_A_PRESS, 0u });
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_INVENTORY, (int)persistent_ctx.state);

    /* Dispatch 10 unhandled events */
    for (int i = 0; i < 10; i++) {
        fq_event_t unhandled = { FQ_EVT_TIMER_TICK, (uint32_t)i };
        err = fq_app_dispatch(&persistent_ctx, &unhandled);
        TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);
        TEST_ASSERT_EQUAL_INT((int)FQ_STATE_INVENTORY, (int)persistent_ctx.state);
    }

    /* -----------------------------------------------------------------------
     * A3 (QA P11-03): combat_active=0 guard — COMBAT_ROUND_COMPLETE in
     * BATTLE state with combat_active=0 must NOT transition state.
     *
     * Expectation:
     *   - state stays FQ_STATE_BATTLE
     *   - combat_active stays 0
     *   - PRNG state (combat.rng.state) is unchanged
     * ----------------------------------------------------------------------- */
    fq_app_ctx_t guard_ctx;
    fq_character_t guard_player;
    fq_inventory_t guard_inv;
    memset(&guard_player, 0, sizeof(guard_player));
    memset(&guard_inv,    0, sizeof(guard_inv));
    fq_app_init(&guard_ctx, &guard_player, &guard_inv);

    /* Navigate: TITLE → HOME → BATTLE_SETUP → BATTLE */
    fq_app_dispatch(&guard_ctx, &(fq_event_t){ FQ_EVT_BTN_A_PRESS,   0u });
    fq_app_dispatch(&guard_ctx, &(fq_event_t){ FQ_EVT_BTN_B_PRESS,   0u });
    fq_app_dispatch(&guard_ctx, &(fq_event_t){ FQ_EVT_BLE_CONNECTED, 0u });
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_BATTLE, (int)guard_ctx.state);
    TEST_ASSERT_EQUAL_UINT8(1u, guard_ctx.combat_active);

    /* Force combat_active = 0 to simulate the guard condition */
    guard_ctx.combat_active = 0u;

    /* Record PRNG state before the guarded dispatch */
    uint32_t rng_before = guard_ctx.combat.rng.state;

    /* Dispatch COMBAT_ROUND_COMPLETE — must be a no-op when combat_active==0 */
    fq_event_t round_done = { FQ_EVT_COMBAT_ROUND_COMPLETE, 0u };
    err = fq_app_dispatch(&guard_ctx, &round_done);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);

    /* State must remain BATTLE */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_BATTLE, (int)guard_ctx.state);
    /* combat_active must remain 0 */
    TEST_ASSERT_EQUAL_UINT8(0u, guard_ctx.combat_active);
    /* PRNG state must be unchanged — no combat logic ran */
    TEST_ASSERT_EQUAL_UINT32(rng_before, guard_ctx.combat.rng.state);

    printf("test_p11_fsm_bounds: PASS\n");
    return 0;
}
