/**
 * test_p20_bounds.c — Phase 20 Bound/Math Tests: BLE Combat, Battle Result, Rebirth.
 *
 * Rule 22 (BOUND RED before FEATURE RED): These tests prove the system REJECTS
 * integer overflows, underflows, PRNG misalignments, and OOB conditions in all
 * new Phase 20 code paths.
 *
 * All 18 spec-challenger required bound tests are implemented here.
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
#include "protocol.h"
#include "app_fsm.h"
#include "event_bus.h"

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
 *    Rebirth on level-1 character — stats at class base floor, no underflow.
 * =========================================================================
 */
static void test_rebirth_level_1_no_underflow(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = FQ_CLASS_BRUISER;
    ch.level         = 1u;
    ch.strength      = 20u;  /* Bruiser base = 20 */
    ch.speed         = 10u;  /* Bruiser base = 10 */
    ch.precision     = 5u;   /* Bruiser base = 5  */
    ch.intelligence  = 5u;   /* Bruiser base = 5  */
    ch.hp_max        = 60u;
    ch.xp            = 100u;
    ch.is_dead       = 1u;

    game_err_t err = fq_rebirth_reset(&ch);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);

    /* Level must reset to 1 */
    TEST_ASSERT_EQUAL_UINT8(1u, ch.level);

    /* Stats halved (floor 0) — for level-1 bruiser at base:
     * strength: 20/2=10, speed: 10/2=5, precision: 5/2=2, intelligence: 5/2=2 */
    TEST_ASSERT_EQUAL_UINT8(10u, ch.strength);
    TEST_ASSERT_EQUAL_UINT8(5u,  ch.speed);
    TEST_ASSERT_EQUAL_UINT8(2u,  ch.precision);
    TEST_ASSERT_EQUAL_UINT8(2u,  ch.intelligence);

    /* XP reset to 0, is_dead cleared */
    TEST_ASSERT_EQUAL_UINT32(0u, ch.xp);
    TEST_ASSERT_EQUAL_UINT8(0u, ch.is_dead);

    /* No stat may underflow (uint8_t) — all remain <= 255 */
    TEST_ASSERT_TRUE(ch.strength      <= 255u);
    TEST_ASSERT_TRUE(ch.speed         <= 255u);
    TEST_ASSERT_TRUE(ch.precision     <= 255u);
    TEST_ASSERT_TRUE(ch.intelligence  <= 255u);
}

/* =========================================================================
 * 6. test_rebirth_count_saturation
 *    rebirth_count at 255 must stay 255, not wrap to 0.
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
    ch.is_dead       = 1u;

    fq_rebirth_reset(&ch);

    TEST_ASSERT_EQUAL_UINT8(255u, ch.rebirth_count);
}

/* =========================================================================
 * 7. test_legacy_points_saturation
 *    legacy_points at 255 must stay 255 after rebirth.
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
    ch.is_dead       = 1u;

    fq_rebirth_reset(&ch);

    TEST_ASSERT_EQUAL_UINT8(255u, ch.legacy_points);
}

/* =========================================================================
 * 8. test_legacy_spend_zero_tokens
 *    Spending a legacy token with legacy_points=0 must be a no-op.
 *    The legacy_tree must not change.
 * =========================================================================
 */
static void test_legacy_spend_zero_tokens(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_points = 0u;
    ch.legacy_tree   = 0u;  /* no nodes unlocked */

    game_err_t err = fq_legacy_spend_token(&ch);
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
 *     fq_legacy_spend_token() with full legacy tree (all 32 bits set) -> no-op.
 * =========================================================================
 */
static void test_rebirth_all_nodes_filled(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_tree   = 0xFFFFFFFFu;  /* all 32 nodes filled */
    ch.legacy_points = 5u;           /* tokens available */

    game_err_t err = fq_legacy_spend_token(&ch);
    /* All nodes full — must return GAME_ERR_INVALID (no-op) */
    TEST_ASSERT_EQUAL_INT((int)GAME_ERR_INVALID, (int)err);
    /* Tree unchanged */
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, ch.legacy_tree);
    /* Tokens unchanged (no deduction) */
    TEST_ASSERT_EQUAL_UINT8(5u, ch.legacy_points);
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

    printf("[PASS] All Phase 20 bound tests passed.\n");
    return 0;
}
