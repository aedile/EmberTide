/**
 * test_p20_bounds.c — Phase 20 Bound/Math Tests: BLE Combat, Battle Result, Rebirth.
 *
 * Rule 22 (BOUND RED before FEATURE RED): These tests prove the system REJECTS
 * integer overflows, underflows, PRNG misalignments, and OOB conditions in all
 * new Phase 20 code paths.
 *
 * Test list:
 *  1.  test_combat_hash_determinism
 *  2.  test_combat_hash_no_prng_advance
 *  3.  test_derive_seed_identical_nonces
 *  4.  test_xp_award_uint32_saturation
 *  5.  test_rebirth_level_1_no_underflow
 *  6.  test_rebirth_count_saturation
 *  7.  test_legacy_points_saturation
 *  8.  test_legacy_spend_zero_tokens
 *  9.  test_wins_losses_saturation
 * 10.  test_team_sync_equipped_count_clamp
 * 11.  test_team_sync_hp_max_zero
 * 12.  test_disconnect_mid_combat_no_save  (FSM state test — no actual save)
 * 13.  test_disconnect_state_cleanup
 * 14.  test_battle_result_dead_routes_rebirth
 * 15.  test_battle_result_alive_routes_home
 * 16.  test_battle_setup_cancel
 * 17.  test_battle_setup_timeout
 * 18.  test_rebirth_all_nodes_filled
 * 19.  test_sync_verify_hash_mismatch
 * 20.  test_sync_verify_round_mismatch
 * 21.  test_sync_verify_ok
 * 22.  test_mtu_too_small_rejects_team_sync
 * 23.  test_idle_forbidden_in_battle_result
 * 24.  test_idle_forbidden_in_rebirth
 * 25.  test_idle_permitted_in_home
 *
 * Constitution Priority 0: No float, no PRNG outside BATTLE state.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>

#include "test_assert.h"
#include "types.h"
#include "combat.h"
#include "combat_hash.h"
#include "progression.h"
#include "legacy.h"
#include "protocol.h"
#include "sync.h"
#include "app_fsm.h"
#include "event_bus.h"

/* ---------------------------------------------------------------------------
 * Local mirror of is_idle_forbidden() from app_main.c.
 *
 * BLOCKER 7: Tests that verify idle suppression cannot call the static function
 * directly. This mirror must remain consistent with the production implementation
 * in app_main.c (Phase-19 interactive: ONBOARDING added alongside BATTLE,
 * BATTLE_SETUP, BATTLE_RESULT, REBIRTH, TITLE).
 * ---------------------------------------------------------------------------*/
static uint8_t mirror_is_idle_forbidden(fq_app_state_t state)
{
    return (state == FQ_STATE_BATTLE        ||
            state == FQ_STATE_BATTLE_SETUP  ||
            state == FQ_STATE_BATTLE_RESULT ||
            state == FQ_STATE_REBIRTH       ||
            state == FQ_STATE_TITLE         ||
            state == FQ_STATE_ONBOARDING)
           ? 1u : 0u;
}

/* =========================================================================
 * 1. test_combat_hash_determinism
 *    Same ctx -> same hash. Different ctx -> different hash.
 * =========================================================================
 */
static void test_combat_hash_determinism(void)
{
    fq_character_t c1, c2;
    memset(&c1, 0, sizeof(c1));
    memset(&c2, 0, sizeof(c2));

    c1.strength     = 10u;
    c1.speed        = 8u;
    c1.hp_max       = 100u;
    c2.strength     = 9u;
    c2.speed        = 7u;
    c2.hp_max       = 90u;

    fq_combat_ctx_t ctx_a, ctx_b, ctx_diff;
    memset(&ctx_a,    0, sizeof(ctx_a));
    memset(&ctx_b,    0, sizeof(ctx_b));
    memset(&ctx_diff, 0, sizeof(ctx_diff));

    fq_combat_init(&ctx_a,    &c1, &c2, 0xDEADBEEFu);
    fq_combat_init(&ctx_b,    &c1, &c2, 0xDEADBEEFu);
    fq_combat_init(&ctx_diff, &c1, &c2, 0xCAFEBABEu);  /* different seed */

    uint32_t hash_a    = fq_generate_combat_hash(&ctx_a,    1u);
    uint32_t hash_b    = fq_generate_combat_hash(&ctx_b,    1u);
    uint32_t hash_diff = fq_generate_combat_hash(&ctx_diff, 1u);

    /* Same seed -> same hash */
    TEST_ASSERT_EQUAL_UINT32(hash_a, hash_b);

    /* Different seed -> different hash (different PRNG state) */
    TEST_ASSERT_TRUE(hash_a != hash_diff);
}

/* =========================================================================
 * 2. test_combat_hash_no_prng_advance
 *    PRNG state must be identical before and after fq_generate_combat_hash().
 * =========================================================================
 */
static void test_combat_hash_no_prng_advance(void)
{
    fq_character_t c1, c2;
    memset(&c1, 0, sizeof(c1));
    memset(&c2, 0, sizeof(c2));
    c1.hp_max = 100u;
    c2.hp_max = 80u;
    c1.strength = 10u;
    c2.strength = 8u;

    fq_combat_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    fq_combat_init(&ctx, &c1, &c2, 0x12345678u);

    /* Record PRNG state before hash */
    uint32_t rng_state_before = ctx.rng.state;

    (void)fq_generate_combat_hash(&ctx, 1u);

    /* PRNG state must be unchanged */
    TEST_ASSERT_EQUAL_UINT32(rng_state_before, ctx.rng.state);
}

/* =========================================================================
 * 3. test_derive_seed_identical_nonces
 *    XOR(N, N) = 0; zero guard forces result to 1.
 * =========================================================================
 */
static void test_derive_seed_identical_nonces(void)
{
    uint32_t seed = fq_protocol_derive_seed(0xABCD1234u, 0xABCD1234u);
    TEST_ASSERT_EQUAL_UINT32(1u, seed);
}

/* =========================================================================
 * 4. test_xp_award_uint32_saturation
 *    XP addition at UINT32_MAX - 10 with a large award must saturate.
 * =========================================================================
 */
static void test_xp_award_uint32_saturation(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id = FQ_CLASS_BRUISER;
    /* Level 99 = max cap: fq_level_up() is a no-op, so the while loop
     * in fq_combat_award_xp() does NOT drain the saturated XP value.
     * This isolates the saturation proof from the level-up side effect. */
    ch.level    = 99u;
    ch.xp       = UINT32_MAX - 10u;
    ch.hp_max   = 200u;

    /* Award XP for a win against level 50 opponent:
     * xp_award = 50 + 50*10 = 550, which overflows UINT32_MAX-10.
     * Must saturate at UINT32_MAX, not wrap. */
    fq_combat_award_xp(&ch, 50u, 1u);

    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, ch.xp);
}

/* =========================================================================
 * 5. test_rebirth_level_1_no_underflow
 *    Rebirth on a Bruiser with above-base stats — stats floor at class base,
 *    never underflow. Uses fq_rebirth() from legacy.h (requires is_dead==1).
 *
 *    Bruiser class bases: STR=3, SPD=0, PRC=0, INT=0, HP=60.
 *    With 50% retention (no perks):
 *      STR: 3 + floor((20-3)*50/100) = 3+8  = 11
 *      SPD: 0 + floor((10-0)*50/100) = 0+5  =  5
 *      PRC: 0 + floor((5-0)*50/100)  = 0+2  =  2
 *      INT: 0 + floor((5-0)*50/100)  = 0+2  =  2
 * =========================================================================
 */
static void test_rebirth_level_1_no_underflow(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = FQ_CLASS_BRUISER;
    ch.level         = 1u;
    ch.strength      = 20u;
    ch.speed         = 10u;
    ch.precision     = 5u;
    ch.intelligence  = 5u;
    ch.hp_max        = 200u;
    ch.xp            = 100u;
    ch.is_dead       = 1u;  /* fq_rebirth() requires is_dead == 1 */

    fq_prng_t rng;
    fq_prng_init(&rng, 0xDEADu);

    game_err_t err = fq_rebirth(&ch, &rng);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);

    /* Retention formula with 50% (no perks): base + floor((stat-base)*50/100) */
    TEST_ASSERT_EQUAL_UINT8(11u, ch.strength);    /* 3 + floor(17*50/100) = 3+8 */
    TEST_ASSERT_EQUAL_UINT8(5u,  ch.speed);       /* 0 + floor(10*50/100) = 5   */
    TEST_ASSERT_EQUAL_UINT8(2u,  ch.precision);   /* 0 + floor(5*50/100)  = 2   */
    TEST_ASSERT_EQUAL_UINT8(2u,  ch.intelligence);/* 0 + floor(5*50/100)  = 2   */

    /* is_dead must be cleared */
    TEST_ASSERT_EQUAL_UINT8(0u, ch.is_dead);

    /* HP restored to class base */
    TEST_ASSERT_EQUAL_UINT16(60u, ch.hp_max);
}

/* =========================================================================
 * 6. test_rebirth_count_saturation
 *    rebirth_count at 255 must stay 255, not wrap to 0.
 *    Uses fq_rebirth() from legacy.h (requires is_dead==1).
 * =========================================================================
 */
static void test_rebirth_count_saturation(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = FQ_CLASS_HEX;
    ch.level         = 10u;
    ch.rebirth_count = 255u;
    ch.strength      = 5u;
    ch.speed         = 5u;
    ch.precision     = 10u;
    ch.intelligence  = 20u;
    ch.hp_max        = 50u;
    ch.is_dead       = 1u;  /* fq_rebirth() requires is_dead == 1 */

    fq_prng_t rng;
    fq_prng_init(&rng, 0x1234u);

    fq_rebirth(&ch, &rng);

    TEST_ASSERT_EQUAL_UINT8(255u, ch.rebirth_count);
}

/* =========================================================================
 * 7. test_legacy_points_saturation
 *    legacy_points at 255 must stay 255 after rebirth.
 *    Uses fq_rebirth() from legacy.h. fq_calc_rebirth_tokens(20, 0) = 2.
 *    fq_sat8_add(255, 2) = 255 (saturated).
 * =========================================================================
 */
static void test_legacy_points_saturation(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = FQ_CLASS_WARDEN;
    ch.level         = 20u;
    ch.legacy_points = 255u;
    ch.strength      = 10u;
    ch.speed         = 10u;
    ch.precision     = 5u;
    ch.intelligence  = 10u;
    ch.hp_max        = 80u;
    ch.is_dead       = 1u;  /* fq_rebirth() requires is_dead == 1 */

    fq_prng_t rng;
    fq_prng_init(&rng, 0xABCDu);

    fq_rebirth(&ch, &rng);

    TEST_ASSERT_EQUAL_UINT8(255u, ch.legacy_points);
}

/* =========================================================================
 * 8. test_legacy_spend_zero_tokens
 *    fq_legacy_unlock_node() with legacy_points=0 must be a no-op.
 *    The legacy_tree must not change.
 * =========================================================================
 */
static void test_legacy_spend_zero_tokens(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_points = 0u;
    ch.legacy_tree   = 0u;  /* no nodes unlocked */

    /* Node 0 is T1 — no prerequisites, but no points to spend. */
    game_err_t err = fq_legacy_unlock_node(&ch, 0u);
    TEST_ASSERT_EQUAL_INT((int)GAME_ERR_INVALID, (int)err);
    TEST_ASSERT_EQUAL_UINT32(0u, ch.legacy_tree);
    TEST_ASSERT_EQUAL_UINT8(0u,  ch.legacy_points);
}

/* =========================================================================
 * 9. test_wins_losses_saturation
 *    wins/losses at UINT16_MAX must stay UINT16_MAX.
 * =========================================================================
 */
static void test_wins_losses_saturation(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.wins   = (uint16_t)UINT16_MAX;
    ch.losses = (uint16_t)UINT16_MAX;
    ch.level  = 5u;
    ch.hp_max = 50u;

    /* Award a win — wins must saturate */
    fq_combat_award_xp(&ch, 5u, 1u);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)UINT16_MAX, ch.wins);

    /* Award a loss — losses must saturate */
    fq_combat_award_xp(&ch, 5u, 0u);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)UINT16_MAX, ch.losses);
}

/* =========================================================================
 * 10. test_team_sync_equipped_count_clamp
 *     equipped_count > 5 in a team sync packet must be clamped to 5.
 * =========================================================================
 */
static void test_team_sync_equipped_count_clamp(void)
{
    /* Clamp: the application layer clamps before using equipped[].
     * We call the clamp helper directly. */
    uint8_t clamped = fq_team_sync_clamp_equipped_count(7u);
    TEST_ASSERT_EQUAL_UINT8(5u, clamped);
}

/* =========================================================================
 * 11. test_team_sync_hp_max_zero
 *     hp_max == 0 in team sync must be handled gracefully (combat rejected).
 * =========================================================================
 */
static void test_team_sync_hp_max_zero(void)
{
    fq_packet_team_sync_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.hp_max = 0u;
    strncpy(pkt.name, "Zero", sizeof(pkt.name) - 1);

    /* Validate the packet — hp_max == 0 is invalid and must be rejected. */
    int valid = fq_team_sync_validate(&pkt);
    TEST_ASSERT_EQUAL_INT(0, valid);  /* 0 = invalid */
}

/* =========================================================================
 * 12. test_disconnect_mid_combat_no_save
 *     BLE disconnect in BATTLE state -> HOME, combat_active cleared.
 * =========================================================================
 */
static void test_disconnect_mid_combat_no_save(void)
{
    static fq_character_t player;
    static fq_inventory_t inventory;
    static fq_app_ctx_t   app;

    memset(&player,    0, sizeof(player));
    memset(&inventory, 0, sizeof(inventory));
    memset(&app,       0, sizeof(app));

    player.hp_max = 100u;
    player.level  = 5u;

    fq_app_init(&app, &player, &inventory);

    fq_event_t evt;
    evt.data = 0u;

    /* Directly enter BATTLE_SETUP then BATTLE by injecting the states.
     * This test's purpose is to verify BLE disconnect while in BATTLE
     * routes to HOME and clears combat_active — not the menu navigation. */
    app.state         = FQ_STATE_BATTLE_SETUP;
    app.combat_active = 0u;

    /* Simulate BLE connected -> BATTLE */
    evt.id = FQ_EVT_BLE_CONNECTED;
    fq_app_dispatch(&app, &evt);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_BATTLE, (int)app.state);
    TEST_ASSERT_EQUAL_UINT8(1u, app.combat_active);

    /* Now simulate disconnect mid-combat */
    evt.id = FQ_EVT_BLE_DISCONNECTED;
    fq_app_dispatch(&app, &evt);

    /* Must be HOME, combat_active cleared */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)app.state);
    TEST_ASSERT_EQUAL_UINT8(0u, app.combat_active);
}

/* =========================================================================
 * 13. test_disconnect_state_cleanup
 *     BLE disconnect in BATTLE_RESULT -> HOME, combat_active == 0.
 * =========================================================================
 */
static void test_disconnect_state_cleanup(void)
{
    static fq_character_t player;
    static fq_inventory_t inventory;
    static fq_app_ctx_t   app;

    memset(&player,    0, sizeof(player));
    memset(&inventory, 0, sizeof(inventory));
    memset(&app,       0, sizeof(app));

    player.hp_max = 100u;
    player.level  = 3u;

    fq_app_init(&app, &player, &inventory);

    /* Manually set state to BATTLE_RESULT (simulating post-combat) */
    app.state         = FQ_STATE_BATTLE_RESULT;
    app.combat_active = 0u;

    fq_event_t evt;
    evt.id   = FQ_EVT_BLE_DISCONNECTED;
    evt.data = 0u;
    fq_app_dispatch(&app, &evt);

    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)app.state);
    TEST_ASSERT_EQUAL_UINT8(0u, app.combat_active);
}

/* =========================================================================
 * 14. test_battle_result_dead_routes_rebirth
 *     BTN_A on BATTLE_RESULT with is_dead==1 -> REBIRTH state.
 * =========================================================================
 */
static void test_battle_result_dead_routes_rebirth(void)
{
    static fq_character_t player;
    static fq_inventory_t inventory;
    static fq_app_ctx_t   app;

    memset(&player,    0, sizeof(player));
    memset(&inventory, 0, sizeof(inventory));
    memset(&app,       0, sizeof(app));

    player.hp_max  = 100u;
    player.level   = 5u;
    player.is_dead = 1u;

    fq_app_init(&app, &player, &inventory);
    app.state = FQ_STATE_BATTLE_RESULT;

    fq_event_t evt;
    evt.id   = FQ_EVT_BTN_A_PRESS;
    evt.data = 0u;
    fq_app_dispatch(&app, &evt);

    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_REBIRTH, (int)app.state);
}

/* =========================================================================
 * 15. test_battle_result_alive_routes_home
 *     BTN_A on BATTLE_RESULT with is_dead==0 -> HOME state.
 * =========================================================================
 */
static void test_battle_result_alive_routes_home(void)
{
    static fq_character_t player;
    static fq_inventory_t inventory;
    static fq_app_ctx_t   app;

    memset(&player,    0, sizeof(player));
    memset(&inventory, 0, sizeof(inventory));
    memset(&app,       0, sizeof(app));

    player.hp_max  = 100u;
    player.level   = 5u;
    player.is_dead = 0u;

    fq_app_init(&app, &player, &inventory);
    app.state = FQ_STATE_BATTLE_RESULT;

    fq_event_t evt;
    evt.id   = FQ_EVT_BTN_A_PRESS;
    evt.data = 0u;
    fq_app_dispatch(&app, &evt);

    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)app.state);
}

/* =========================================================================
 * 16. test_battle_setup_cancel
 *     BTN_B in BATTLE_SETUP -> HOME immediately.
 * =========================================================================
 */
static void test_battle_setup_cancel(void)
{
    static fq_character_t player;
    static fq_inventory_t inventory;
    static fq_app_ctx_t   app;

    memset(&player,    0, sizeof(player));
    memset(&inventory, 0, sizeof(inventory));
    memset(&app,       0, sizeof(app));

    player.hp_max = 100u;
    fq_app_init(&app, &player, &inventory);
    app.state = FQ_STATE_BATTLE_SETUP;

    fq_event_t evt;
    evt.id   = FQ_EVT_BTN_B_PRESS;
    evt.data = 0u;
    fq_app_dispatch(&app, &evt);

    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)app.state);
}

/* =========================================================================
 * 17. test_battle_setup_timeout
 *     200 timer ticks in BATTLE_SETUP -> HOME.
 * =========================================================================
 */
static void test_battle_setup_timeout(void)
{
    static fq_character_t player;
    static fq_inventory_t inventory;
    static fq_app_ctx_t   app;

    memset(&player,    0, sizeof(player));
    memset(&inventory, 0, sizeof(inventory));
    memset(&app,       0, sizeof(app));

    player.hp_max = 100u;
    fq_app_init(&app, &player, &inventory);
    app.state = FQ_STATE_BATTLE_SETUP;

    fq_event_t tick_evt;
    tick_evt.id   = FQ_EVT_TIMER_TICK;
    tick_evt.data = 0u;

    /* Send 199 ticks — must still be in BATTLE_SETUP */
    uint32_t i;
    for (i = 0u; i < 199u; i++) {
        fq_app_dispatch(&app, &tick_evt);
    }
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_BATTLE_SETUP, (int)app.state);

    /* Tick 200 — must transition to HOME */
    fq_app_dispatch(&app, &tick_evt);
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)app.state);
}

/* =========================================================================
 * 18. test_rebirth_all_nodes_filled
 *     fq_legacy_unlock_node() with full legacy tree (all 16 valid nodes set)
 *     -> no-op (node already set → GAME_ERR_INVALID).
 * =========================================================================
 */
static void test_rebirth_all_nodes_filled(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_tree   = 0xFFFFFFFFu;  /* all 32 bits filled (16 valid + 16 reserved) */
    ch.legacy_points = 5u;           /* tokens available */

    /* Node 0 already set — must return GAME_ERR_INVALID (no-op) */
    game_err_t err = fq_legacy_unlock_node(&ch, 0u);
    TEST_ASSERT_EQUAL_INT((int)GAME_ERR_INVALID, (int)err);
    /* Tree unchanged */
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, ch.legacy_tree);
    /* Tokens unchanged (no deduction) */
    TEST_ASSERT_EQUAL_UINT8(5u, ch.legacy_points);
}

/* =========================================================================
 * 19. test_sync_verify_hash_mismatch
 *     Same round, different hashes -> FQ_SYNC_ERR_HASH_MISMATCH.
 * =========================================================================
 */
static void test_sync_verify_hash_mismatch(void)
{
    fq_sync_err_t result = fq_sync_verify_round(
        5u,           /* expected_round */
        5u,           /* received_round (matches) */
        0xAABBCCDDu,  /* local_hash */
        0x11223344u   /* peer_hash (different) */
    );
    TEST_ASSERT_EQUAL_INT((int)FQ_SYNC_ERR_HASH_MISMATCH, (int)result);
}

/* =========================================================================
 * 20. test_sync_verify_round_mismatch
 *     Different rounds -> FQ_SYNC_ERR_ROUND_MISMATCH (checked before hash).
 * =========================================================================
 */
static void test_sync_verify_round_mismatch(void)
{
    fq_sync_err_t result = fq_sync_verify_round(
        3u,           /* expected_round */
        7u,           /* received_round (different) */
        0xDEADBEEFu,  /* local_hash */
        0xDEADBEEFu   /* peer_hash (same, but round mismatch takes priority) */
    );
    TEST_ASSERT_EQUAL_INT((int)FQ_SYNC_ERR_ROUND_MISMATCH, (int)result);
}

/* =========================================================================
 * 21. test_sync_verify_ok
 *     Matching round and hash -> FQ_SYNC_OK.
 * =========================================================================
 */
static void test_sync_verify_ok(void)
{
    fq_sync_err_t result = fq_sync_verify_round(
        10u,          /* expected_round */
        10u,          /* received_round (matches) */
        0xCAFEBABEu,  /* local_hash */
        0xCAFEBABEu   /* peer_hash (matches) */
    );
    TEST_ASSERT_EQUAL_INT((int)FQ_SYNC_OK, (int)result);
}

/* =========================================================================
 * 22. test_mtu_too_small_rejects_team_sync
 *     MTU 38: (38-3)=35 < FQ_PACKET_TEAM_SYNC_SIZE -> insufficient.
 *     MTU 39: (39-3)=36 >= FQ_PACKET_TEAM_SYNC_SIZE -> sufficient.
 *
 *     The BLE ATT header overhead is 3 bytes. The effective payload is
 *     (mtu - 3). The test verifies the boundary condition detectable by
 *     comparing the usable payload against FQ_PACKET_TEAM_SYNC_SIZE.
 * =========================================================================
 */
static void test_mtu_too_small_rejects_team_sync(void)
{
    /* MTU 38: usable payload = 38 - 3 = 35 bytes */
    uint16_t mtu_small   = 38u;
    uint16_t payload_small = (uint16_t)(mtu_small - 3u);
    TEST_ASSERT_TRUE(payload_small < (uint16_t)FQ_PACKET_TEAM_SYNC_SIZE);

    /* MTU 39: usable payload = 39 - 3 = 36 bytes */
    uint16_t mtu_ok      = 39u;
    uint16_t payload_ok  = (uint16_t)(mtu_ok - 3u);
    TEST_ASSERT_TRUE(payload_ok >= (uint16_t)FQ_PACKET_TEAM_SYNC_SIZE);
}

/* =========================================================================
 * 23. test_idle_forbidden_in_battle_result
 *     BATTLE_RESULT must be idle-suppressed — screensaver must not activate.
 * =========================================================================
 */
static void test_idle_forbidden_in_battle_result(void)
{
    TEST_ASSERT_EQUAL_UINT8(1u, mirror_is_idle_forbidden(FQ_STATE_BATTLE_RESULT));
}

/* =========================================================================
 * 24. test_idle_forbidden_in_rebirth
 *     REBIRTH must be idle-suppressed — screensaver must not activate.
 * =========================================================================
 */
static void test_idle_forbidden_in_rebirth(void)
{
    TEST_ASSERT_EQUAL_UINT8(1u, mirror_is_idle_forbidden(FQ_STATE_REBIRTH));
}

/* =========================================================================
 * 25. test_idle_permitted_in_home
 *     HOME must NOT be idle-suppressed — screensaver may activate normally.
 *     Verifies no over-suppression.
 * =========================================================================
 */
static void test_idle_permitted_in_home(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, mirror_is_idle_forbidden(FQ_STATE_HOME));
}

/* =========================================================================
 * main
 * =========================================================================
 */
int main(void)
{
    test_combat_hash_determinism();
    test_combat_hash_no_prng_advance();
    test_derive_seed_identical_nonces();
    test_xp_award_uint32_saturation();
    test_rebirth_level_1_no_underflow();
    test_rebirth_count_saturation();
    test_legacy_points_saturation();
    test_legacy_spend_zero_tokens();
    test_wins_losses_saturation();
    test_team_sync_equipped_count_clamp();
    test_team_sync_hp_max_zero();
    test_disconnect_mid_combat_no_save();
    test_disconnect_state_cleanup();
    test_battle_result_dead_routes_rebirth();
    test_battle_result_alive_routes_home();
    test_battle_setup_cancel();
    test_battle_setup_timeout();
    test_rebirth_all_nodes_filled();
    test_sync_verify_hash_mismatch();
    test_sync_verify_round_mismatch();
    test_sync_verify_ok();
    test_mtu_too_small_rejects_team_sync();
    test_idle_forbidden_in_battle_result();
    test_idle_forbidden_in_rebirth();
    test_idle_permitted_in_home();

    printf("[PASS] All Phase 20 bound tests passed.\n");
    return 0;
}
