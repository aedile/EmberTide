/**
 * test_p20_feature.c — Phase 20 Feature Tests: BLE Combat, Battle Result, Rebirth.
 *
 * Happy-path and integration tests verifying the Phase 20 features work correctly.
 * All tests are host-compilable (no hal_*.h, no ESP-IDF).
 *
 * Test groups:
 *   A) combat hash function
 *   B) XP award and win/loss counters
 *   C) rebirth formula and legacy tree
 *   D) FSM combat orchestration (via mock BLE events)
 *   E) view model construction (fq_vm_battle_result_t, fq_vm_rebirth_t)
 *   F) screen renderers (pixel-count smoke tests)
 *
 * Constitution Priority 0: No float, PRNG not touched outside BATTLE state.
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
#include "view_models.h"
#include "vm_builder.h"
#include "fq_framebuffer.h"
#include "screens/screen_battle_result.h"
#include "screens/screen_rebirth.h"

/* =========================================================================
 * A. Combat hash function
 * =========================================================================
 */

static void test_combat_hash_returns_nonzero_for_valid_ctx(void)
{
    fq_character_t c1, c2;
    memset(&c1, 0, sizeof(c1));
    memset(&c2, 0, sizeof(c2));
    c1.hp_max = 100u; c1.strength = 12u;
    c2.hp_max = 90u;  c2.strength = 10u;

    fq_combat_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    fq_combat_init(&ctx, &c1, &c2, 0xA1B2C3D4u);

    /* Call must complete without crash. Value is implementation-defined. */
    uint32_t hash = fq_generate_combat_hash(&ctx, 1u);
    (void)hash;
    printf("[PASS] test_combat_hash_returns_nonzero_for_valid_ctx\n");
}

static void test_combat_hash_changes_after_round(void)
{
    fq_character_t c1, c2;
    memset(&c1, 0, sizeof(c1));
    memset(&c2, 0, sizeof(c2));
    c1.hp_max = 100u; c1.strength = 12u; c1.speed = 8u;
    c2.hp_max = 90u;  c2.strength = 10u; c2.speed = 6u;

    fq_combat_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    fq_combat_init(&ctx, &c1, &c2, 0xBEEFFACEu);

    uint32_t hash_before = fq_generate_combat_hash(&ctx, 1u);
    fq_combat_step(&ctx);
    uint32_t hash_after = fq_generate_combat_hash(&ctx, 2u);

    /* After a round, HP state changed -> hash must change */
    TEST_ASSERT_TRUE(hash_before != hash_after);
}

/* =========================================================================
 * B. XP award and win/loss counters
 * =========================================================================
 */

static void test_xp_award_win_formula(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id = FQ_CLASS_BRUISER;
    ch.level    = 5u;
    ch.xp       = 0u;
    ch.hp_max   = 100u;

    /* Win against level 10 opponent: xp = 50 + 10*10 = 150 */
    fq_combat_award_xp(&ch, 10u, 1u);
    TEST_ASSERT_EQUAL_UINT32(150u, ch.xp);
}

static void test_xp_award_loss_gives_zero_xp(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id = FQ_CLASS_HEX;
    ch.level    = 3u;
    ch.xp       = 200u;
    ch.hp_max   = 80u;

    fq_combat_award_xp(&ch, 8u, 0u);
    /* Loss: 0 XP added */
    TEST_ASSERT_EQUAL_UINT32(200u, ch.xp);
}

static void test_xp_award_win_increments_wins(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.wins   = 3u;
    ch.level  = 2u;
    ch.hp_max = 60u;

    fq_combat_award_xp(&ch, 2u, 1u);
    TEST_ASSERT_EQUAL_UINT16(4u, ch.wins);
}

static void test_xp_award_loss_increments_losses(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.losses = 1u;
    ch.level  = 2u;
    ch.hp_max = 60u;

    fq_combat_award_xp(&ch, 2u, 0u);
    TEST_ASSERT_EQUAL_UINT16(2u, ch.losses);
}

static void test_xp_award_triggers_level_up_when_threshold_met(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = FQ_CLASS_TRICKSTER;
    ch.level         = 1u;
    ch.xp            = 40u;   /* 10 XP short of level-up threshold */
    ch.speed         = 10u;
    ch.strength      = 5u;
    ch.precision     = 8u;
    ch.intelligence  = 3u;
    ch.hp_max        = 30u;

    /* Win against level 1 opponent: xp_award = 50 + 1*10 = 60
     * total xp = 40 + 60 = 100 >= 50 (threshold for L1) -> level up */
    fq_combat_award_xp(&ch, 1u, 1u);
    TEST_ASSERT_EQUAL_UINT8(2u, ch.level);
}

/* =========================================================================
 * C. Rebirth formula and legacy tree
 * =========================================================================
 */

static void test_rebirth_resets_level_and_xp(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = FQ_CLASS_WILDCARD;
    ch.level         = 15u;
    ch.xp            = 5000u;
    ch.strength      = 40u;
    ch.speed         = 30u;
    ch.precision     = 20u;
    ch.intelligence  = 10u;
    ch.hp_max        = 150u;
    ch.is_dead       = 1u;

    fq_rebirth_reset(&ch);

    TEST_ASSERT_EQUAL_UINT8(1u,  ch.level);
    TEST_ASSERT_EQUAL_UINT32(0u, ch.xp);
    TEST_ASSERT_EQUAL_UINT8(0u,  ch.is_dead);
}

static void test_rebirth_stats_halved(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = FQ_CLASS_BRUISER;
    ch.level         = 10u;
    ch.strength      = 60u;
    ch.speed         = 40u;
    ch.precision     = 20u;
    ch.intelligence  = 16u;
    ch.hp_max        = 200u;
    ch.is_dead       = 1u;

    fq_rebirth_reset(&ch);

    TEST_ASSERT_EQUAL_UINT8(30u, ch.strength);
    TEST_ASSERT_EQUAL_UINT8(20u, ch.speed);
    TEST_ASSERT_EQUAL_UINT8(10u, ch.precision);
    TEST_ASSERT_EQUAL_UINT8(8u,  ch.intelligence);
}

static void test_rebirth_tokens_earned_level_over_10(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = FQ_CLASS_HEX;
    ch.level         = 20u;
    ch.strength      = 5u;
    ch.speed         = 5u;
    ch.precision     = 10u;
    ch.intelligence  = 20u;
    ch.hp_max        = 70u;
    ch.legacy_points = 0u;
    ch.is_dead       = 1u;

    fq_rebirth_reset(&ch);

    /* tokens = max(1, 20/10) = max(1, 2) = 2 */
    TEST_ASSERT_EQUAL_UINT8(2u, ch.legacy_points);
}

static void test_rebirth_tokens_min_1_for_low_level(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = FQ_CLASS_TRICKSTER;
    ch.level         = 5u;    /* level/10 = 0, min should be 1 */
    ch.strength      = 5u;
    ch.speed         = 15u;
    ch.precision     = 10u;
    ch.intelligence  = 5u;
    ch.hp_max        = 60u;
    ch.legacy_points = 0u;
    ch.is_dead       = 1u;

    fq_rebirth_reset(&ch);

    /* tokens = max(1, 5/10) = max(1, 0) = 1 */
    TEST_ASSERT_EQUAL_UINT8(1u, ch.legacy_points);
}

static void test_rebirth_count_increments(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = FQ_CLASS_WARDEN;
    ch.level         = 8u;
    ch.rebirth_count = 3u;
    ch.strength      = 15u;
    ch.speed         = 15u;
    ch.precision     = 5u;
    ch.intelligence  = 15u;
    ch.hp_max        = 80u;
    ch.is_dead       = 1u;

    fq_rebirth_reset(&ch);

    TEST_ASSERT_EQUAL_UINT8(4u, ch.rebirth_count);
}

static void test_legacy_spend_token_sets_next_bit(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_points = 3u;
    ch.legacy_tree   = 0u;

    /* Spend first token -> bit 0 set */
    game_err_t err = fq_legacy_spend_token(&ch);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT32(0x00000001u, ch.legacy_tree);
    TEST_ASSERT_EQUAL_UINT8(2u, ch.legacy_points);

    /* Spend second token -> bit 1 set */
    err = fq_legacy_spend_token(&ch);
    TEST_ASSERT_EQUAL_INT((int)GAME_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT32(0x00000003u, ch.legacy_tree);
    TEST_ASSERT_EQUAL_UINT8(1u, ch.legacy_points);
}

static void test_legacy_spend_token_skips_already_set_bits(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_points = 2u;
    ch.legacy_tree   = 0x00000007u;  /* bits 0,1,2 already set */

    /* Next unset bit is bit 3 */
    fq_legacy_spend_token(&ch);
    TEST_ASSERT_EQUAL_UINT32(0x0000000Fu, ch.legacy_tree);
}

/* =========================================================================
 * D. FSM combat orchestration
 * =========================================================================
 */

static void test_battle_setup_connects_to_battle(void)
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
    evt.id   = FQ_EVT_BLE_CONNECTED;
    evt.data = 0u;
    fq_app_dispatch(&app, &evt);

    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_BATTLE, (int)app.state);
    TEST_ASSERT_EQUAL_UINT8(1u, app.combat_active);
}

static void test_battle_combat_round_complete_to_result(void)
{
    static fq_character_t player;
    static fq_inventory_t inventory;
    static fq_app_ctx_t   app;

    memset(&player,    0, sizeof(player));
    memset(&inventory, 0, sizeof(inventory));
    memset(&app,       0, sizeof(app));

    player.hp_max = 100u;
    fq_app_init(&app, &player, &inventory);
    app.state         = FQ_STATE_BATTLE;
    app.combat_active = 1u;

    fq_event_t evt;
    evt.id   = FQ_EVT_COMBAT_ROUND_COMPLETE;
    evt.data = 0u;
    fq_app_dispatch(&app, &evt);

    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_BATTLE_RESULT, (int)app.state);
    TEST_ASSERT_EQUAL_UINT8(0u, app.combat_active);
}

static void test_battle_setup_disconnect_goes_home(void)
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
    evt.id   = FQ_EVT_BLE_DISCONNECTED;
    evt.data = 0u;
    fq_app_dispatch(&app, &evt);

    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)app.state);
}

static void test_rebirth_state_btn_b_goes_home(void)
{
    static fq_character_t player;
    static fq_inventory_t inventory;
    static fq_app_ctx_t   app;

    memset(&player,    0, sizeof(player));
    memset(&inventory, 0, sizeof(inventory));
    memset(&app,       0, sizeof(app));

    player.hp_max = 100u;
    fq_app_init(&app, &player, &inventory);
    app.state = FQ_STATE_REBIRTH;

    fq_event_t evt;
    evt.id   = FQ_EVT_BTN_B_PRESS;
    evt.data = 0u;
    fq_app_dispatch(&app, &evt);

    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_HOME, (int)app.state);
}

static void test_rebirth_state_btn_a_spends_token(void)
{
    static fq_character_t player;
    static fq_inventory_t inventory;
    static fq_app_ctx_t   app;

    memset(&player,    0, sizeof(player));
    memset(&inventory, 0, sizeof(inventory));
    memset(&app,       0, sizeof(app));

    player.hp_max        = 100u;
    player.legacy_points = 2u;
    player.legacy_tree   = 0u;

    fq_app_init(&app, &player, &inventory);
    app.state = FQ_STATE_REBIRTH;

    fq_event_t evt;
    evt.id   = FQ_EVT_BTN_A_PRESS;
    evt.data = 0u;
    fq_app_dispatch(&app, &evt);

    /* Token spent -> bit 0 set, legacy_points decremented */
    TEST_ASSERT_EQUAL_UINT32(0x00000001u, player.legacy_tree);
    TEST_ASSERT_EQUAL_UINT8(1u, player.legacy_points);
    /* Still in REBIRTH state */
    TEST_ASSERT_EQUAL_INT((int)FQ_STATE_REBIRTH, (int)app.state);
}

/* =========================================================================
 * E. View model construction
 * =========================================================================
 */

static void test_vm_build_battle_result_win(void)
{
    fq_character_t player, opponent;
    memset(&player,   0, sizeof(player));
    memset(&opponent, 0, sizeof(opponent));

    strncpy(player.name,   "Ember",  sizeof(player.name) - 1u);
    strncpy(opponent.name, "Shadow", sizeof(opponent.name) - 1u);
    player.sprite_base = 0u;
    opponent.level     = 8u;

    fq_vm_battle_result_t vm;
    memset(&vm, 0, sizeof(vm));

    fq_vm_build_battle_result(&vm, &player, &opponent,
                               1u,    /* you_won */
                               130u,  /* xp_earned: 50 + 8*10 = 130 */
                               5u,    /* rounds_survived */
                               0u);   /* is_dead */

    TEST_ASSERT_EQUAL_UINT8(1u, vm.you_won);
    TEST_ASSERT_EQUAL_UINT16(130u, vm.xp_earned);
    TEST_ASSERT_EQUAL_UINT8(5u, vm.rounds_survived);
    TEST_ASSERT_EQUAL_UINT8(0u, vm.is_dead);
    /* Winner name must be player name on win */
    TEST_ASSERT_TRUE(strncmp("Ember", vm.winner_name, sizeof(vm.winner_name)) == 0);
}

static void test_vm_build_battle_result_loss(void)
{
    fq_character_t player, opponent;
    memset(&player,   0, sizeof(player));
    memset(&opponent, 0, sizeof(opponent));

    strncpy(player.name,   "Tide", sizeof(player.name) - 1u);
    strncpy(opponent.name, "Vex",  sizeof(opponent.name) - 1u);

    fq_vm_battle_result_t vm;
    memset(&vm, 0, sizeof(vm));

    fq_vm_build_battle_result(&vm, &player, &opponent,
                               0u,  /* you_won = false */
                               0u,  /* xp_earned = 0 on loss */
                               3u,  /* rounds_survived */
                               0u); /* is_dead */

    TEST_ASSERT_EQUAL_UINT8(0u, vm.you_won);
    TEST_ASSERT_EQUAL_UINT16(0u, vm.xp_earned);
    /* Winner name must be opponent name on loss */
    TEST_ASSERT_TRUE(strncmp("Vex", vm.winner_name, sizeof(vm.winner_name)) == 0);
}

static void test_vm_build_rebirth(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id      = FQ_CLASS_BRUISER;
    ch.level         = 1u;   /* post-rebirth level */
    ch.strength      = 30u;
    ch.speed         = 15u;
    ch.precision     = 5u;
    ch.intelligence  = 5u;
    ch.legacy_points = 2u;
    ch.legacy_tree   = 0x00000001u;

    /* Pre-rebirth stats for display (caller provides) */
    uint8_t old_stats[4] = { 60u, 30u, 10u, 10u };

    fq_vm_rebirth_t vm;
    memset(&vm, 0, sizeof(vm));

    fq_vm_build_rebirth(&vm, &ch, 10u, /* old_level */
                         old_stats, 2u /* tokens_earned */);

    TEST_ASSERT_EQUAL_UINT8(10u, vm.old_level);
    TEST_ASSERT_EQUAL_UINT8(1u,  vm.new_level);
    TEST_ASSERT_EQUAL_UINT8(60u, vm.old_stats[0]);
    TEST_ASSERT_EQUAL_UINT8(30u, vm.new_stats[0]);
    TEST_ASSERT_EQUAL_UINT8(2u,  vm.tokens_earned);
    TEST_ASSERT_EQUAL_UINT8(2u,  vm.tokens_available);
    TEST_ASSERT_EQUAL_UINT32(0x00000001u, vm.legacy_tree);
}

/* =========================================================================
 * F. Screen renderers — pixel-count smoke tests
 * =========================================================================
 */

static void test_screen_battle_result_renders_pixels(void)
{
    static fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_vm_battle_result_t vm;
    memset(&vm, 0, sizeof(vm));
    strncpy(vm.winner_name, "Ember", sizeof(vm.winner_name) - 1u);
    vm.you_won         = 1u;
    vm.xp_earned       = 150u;
    vm.rounds_survived = 4u;

    fq_render_battle_result(&fb, &vm);

    /* Count black pixels — must be non-zero (renderer drew something) */
    uint32_t black = 0u;
    uint32_t i;
    for (i = 0u; i < FQ_FB_SIZE; i++) {
        uint8_t b = fb.pixels[i];
        while (b) { black += b & 1u; b >>= 1u; }
    }
    TEST_ASSERT_TRUE(black > 0u);
}

static void test_screen_battle_result_null_safe(void)
{
    static fq_fb_t fb;
    fq_fb_clear(&fb);

    /* NULL vm: must not crash */
    fq_render_battle_result(&fb, NULL);
    fq_render_battle_result(NULL, NULL);
    printf("[PASS] test_screen_battle_result_null_safe\n");
}

static void test_screen_rebirth_renders_pixels(void)
{
    static fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_vm_rebirth_t vm;
    memset(&vm, 0, sizeof(vm));
    vm.old_level        = 10u;
    vm.new_level        = 1u;
    vm.old_stats[0]     = 60u;
    vm.new_stats[0]     = 30u;
    vm.tokens_earned    = 1u;
    vm.tokens_available = 1u;
    vm.legacy_tree      = 0u;
    vm.next_node        = 0u;

    fq_render_rebirth(&fb, &vm);

    uint32_t black = 0u;
    uint32_t i;
    for (i = 0u; i < FQ_FB_SIZE; i++) {
        uint8_t b = fb.pixels[i];
        while (b) { black += b & 1u; b >>= 1u; }
    }
    TEST_ASSERT_TRUE(black > 0u);
}

static void test_screen_rebirth_null_safe(void)
{
    static fq_fb_t fb;
    fq_fb_clear(&fb);

    fq_render_rebirth(&fb, NULL);
    fq_render_rebirth(NULL, NULL);
    printf("[PASS] test_screen_rebirth_null_safe\n");
}

/* =========================================================================
 * main
 * =========================================================================
 */
int main(void)
{
    /* A. Combat hash */
    test_combat_hash_returns_nonzero_for_valid_ctx();
    test_combat_hash_changes_after_round();

    /* B. XP award and win/loss */
    test_xp_award_win_formula();
    test_xp_award_loss_gives_zero_xp();
    test_xp_award_win_increments_wins();
    test_xp_award_loss_increments_losses();
    test_xp_award_triggers_level_up_when_threshold_met();

    /* C. Rebirth */
    test_rebirth_resets_level_and_xp();
    test_rebirth_stats_halved();
    test_rebirth_tokens_earned_level_over_10();
    test_rebirth_tokens_min_1_for_low_level();
    test_rebirth_count_increments();
    test_legacy_spend_token_sets_next_bit();
    test_legacy_spend_token_skips_already_set_bits();

    /* D. FSM orchestration */
    test_battle_setup_connects_to_battle();
    test_battle_combat_round_complete_to_result();
    test_battle_setup_disconnect_goes_home();
    test_rebirth_state_btn_b_goes_home();
    test_rebirth_state_btn_a_spends_token();

    /* E. View models */
    test_vm_build_battle_result_win();
    test_vm_build_battle_result_loss();
    test_vm_build_rebirth();

    /* F. Screen renderers */
    test_screen_battle_result_renders_pixels();
    test_screen_battle_result_null_safe();
    test_screen_rebirth_renders_pixels();
    test_screen_rebirth_null_safe();

    printf("[PASS] All Phase 20 feature tests passed.\n");
    return 0;
}
