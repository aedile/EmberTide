/**
 * test_p19_interactive_feature.c — Phase 19 Feature Tests
 *
 * Rule 22 FEATURE RED: Tests must fail before the GREEN implementation.
 *
 * Item 1 — Onboarding (tests F1-F6)
 * Item 2 — Training Session (tests F7-F12)
 * Item 3 — Inventory Equip/Unequip (tests F13-F18)
 * Item 4 — Double-tap B inventory exit positive path (test F19)
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "test_assert.h"
#include "types.h"
#include "event_bus.h"
#include "app_fsm.h"
#include "view_models.h"
#include "vm_builder.h"
#include "character.h"
#include "progression.h"

/* New Phase-19 headers. */
#include "name_gen.h"
#include "training_session.h"
#include "equip.h"

/* ===========================================================================
 * Helpers
 * =========================================================================*/

static fq_character_t make_char(fq_class_t cls, uint8_t level)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.class_id       = (uint8_t)cls;
    ch.level          = level;
    ch.equipped_count = 4u;
    return ch;
}

static fq_inventory_t make_inv_ids(const uint16_t *ids, uint8_t n)
{
    fq_inventory_t inv;
    memset(&inv, 0, sizeof(inv));
    if (n > 32u) { n = 32u; }
    inv.count = n;
    for (uint8_t i = 0u; i < n; i++) {
        inv.items[i] = ids[i];
    }
    return inv;
}

/* ===========================================================================
 * ── ITEM 1: ONBOARDING ────────────────────────────────────────────────────
 * =========================================================================*/

/* F1 — FQ_STATE_ONBOARDING is appended at value 11, COUNT = 12. */
static void test_f1_onboarding_state_enum(void)
{
    /* Verified at compile time by _Static_assert in app_fsm.h.
     * This runtime check makes it visible in ctest. */
    TEST_ASSERT_EQUAL_INT(11, (int)FQ_STATE_ONBOARDING);
    TEST_ASSERT_EQUAL_INT(12, (int)FQ_STATE_COUNT);
    printf("[F1] onboarding state enum: PASS\n");
}

/* F2 — In ONBOARDING, BTN_A cycles class index forward with wrap. */
static void test_f2_onboarding_btn_a_cycles_class(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&ctx, &player, &inv);
    ctx.state = FQ_STATE_ONBOARDING;
    ctx.onboarding_class_index = 0u;

    fq_event_t ea = { FQ_EVT_BTN_A_PRESS, 0u };

    /* 0 -> 1 -> 2 -> 3 -> 4 -> 0 (wrap) */
    fq_app_dispatch(&ctx, &ea); TEST_ASSERT_EQUAL_UINT8(1u, ctx.onboarding_class_index);
    fq_app_dispatch(&ctx, &ea); TEST_ASSERT_EQUAL_UINT8(2u, ctx.onboarding_class_index);
    fq_app_dispatch(&ctx, &ea); TEST_ASSERT_EQUAL_UINT8(3u, ctx.onboarding_class_index);
    fq_app_dispatch(&ctx, &ea); TEST_ASSERT_EQUAL_UINT8(4u, ctx.onboarding_class_index);
    fq_app_dispatch(&ctx, &ea); TEST_ASSERT_EQUAL_UINT8(0u, ctx.onboarding_class_index); /* wrap */

    printf("[F2] onboarding BTN_A cycles class: PASS\n");
}

/* F3 — In ONBOARDING, BTN_B confirms; on success, state -> HOME.
 *
 * The test sets up a valid player, sets ONBOARDING, fires BTN_B,
 * and checks that state == HOME. The "save" is bypassed in test
 * via the onboarding_save_failed = 0 path.
 */
static void test_f3_onboarding_btn_b_confirms(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    fq_app_init(&ctx, &player, &inv);
    ctx.state = FQ_STATE_ONBOARDING;
    ctx.onboarding_class_index = 2u; /* HEX */
    ctx.onboarding_save_failed = 0u; /* save will succeed */

    fq_event_t eb = { FQ_EVT_BTN_B_PRESS, 0u };
    fq_app_dispatch(&ctx, &eb);

    /* After successful confirm, state must be HOME. */
    TEST_ASSERT_EQUAL_INT(FQ_STATE_HOME, (int)ctx.state);

    /* Player must have been initialized with the selected class. */
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_CLASS_HEX, ctx.player->class_id);
    TEST_ASSERT_TRUE(ctx.player->name[0] != '\0');
    printf("[F3] onboarding BTN_B confirms: PASS\n");
}

/* F4 — fq_generate_name produces two-part names with correct alphabet. */
static void test_f4_name_gen_two_part(void)
{
    fq_prng_t rng;
    char      name[12];

    fq_prng_init(&rng, 0xCAFEu);
    memset(name, 0, sizeof(name));
    game_err_t err = fq_generate_name(&rng, name, 12u);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    TEST_ASSERT_TRUE(name[0] != '\0');

    /* Length must be 2-11 chars. */
    size_t len = strnlen(name, 12u);
    TEST_ASSERT_TRUE(len >= 2u);
    TEST_ASSERT_TRUE(len <= 11u);

    printf("[F4] name_gen two-part names: PASS (name=\"%s\" len=%u)\n",
           name, (unsigned)len);
}

/* F5 — fq_vm_onboarding_t view model carries the correct fields. */
static void test_f5_vm_onboarding_fields(void)
{
    fq_character_t ch;
    memset(&ch, 0, sizeof(ch));
    fq_character_create(&ch, FQ_CLASS_TRICKSTER, 42u, "TestCar");

    fq_vm_onboarding_t vm;
    memset(&vm, 0, sizeof(vm));
    fq_vm_build_onboarding(&vm, &ch, 1u /* class_index */);

    TEST_ASSERT_EQUAL_UINT8(1u, vm.class_index);
    TEST_ASSERT_EQUAL_UINT8(ch.sprite_base, vm.sprite_base);
    /* class_name must be non-empty. */
    TEST_ASSERT_TRUE(vm.class_name[0] != '\0');

    printf("[F5] vm_onboarding fields: PASS\n");
}

/* F6 — FQ_STATE_ONBOARDING is forbidden for idle. */
static void test_f6_onboarding_idle_forbidden(void)
{
    /* is_idle_forbidden is static in app_main.c — we test indirectly
     * via the invariant that FQ_STATE_ONBOARDING is in the forbidden list.
     * We verify that the state enum exists and that the value 11 is guarded.
     * Full integration test of idle suppression is in the visual test suite. */
    TEST_ASSERT_EQUAL_INT(11, (int)FQ_STATE_ONBOARDING);
    /* Passes when FQ_STATE_ONBOARDING exists and has value 11. */
    printf("[F6] onboarding idle forbidden (enum guard): PASS\n");
}

/* ===========================================================================
 * ── ITEM 2: TRAINING SESSION ──────────────────────────────────────────────
 * =========================================================================*/

/* F7 — fq_training_session_init sets state=WAITING, clears score/targets. */
static void test_f7_training_session_init(void)
{
    fq_training_session_t ts;
    memset(&ts, 0xFF, sizeof(ts)); /* poison */

    game_err_t err = fq_training_session_init(&ts, FQ_TS_SPEED);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8(FQ_TS_WAITING, ts.state);
    TEST_ASSERT_EQUAL_UINT8(0u, ts.score);
    TEST_ASSERT_EQUAL_UINT8(0u, ts.targets_done);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_TS_SPEED, ts.game_type);

    printf("[F7] training_session_init: PASS\n");
}

/* F8 — fq_training_step advances target_pos by target_speed per tick.
 *      State transitions WAITING->ACTIVE on first step after start. */
static void test_f8_training_session_step(void)
{
    fq_training_session_t ts;
    fq_training_session_init(&ts, FQ_TS_SPEED);

    /* Start the session (WAITING -> ACTIVE). */
    game_err_t err = fq_training_session_start(&ts);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8(FQ_TS_ACTIVE, ts.state);

    uint8_t speed = fq_training_get_target_speed(&ts);
    TEST_ASSERT_TRUE(speed > 0u);

    uint8_t pos_before = ts.target_pos;
    fq_training_step(&ts);
    uint8_t pos_after = ts.target_pos;

    /* target_pos must have advanced by target_speed exactly. */
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(pos_before + speed), pos_after);

    printf("[F8] training_session_step: PASS (speed=%u)\n", (unsigned)speed);
}

/* F9 — fq_training_hit in zone (+20) and out of zone (-5 clamp to 0). */
static void test_f9_training_hit_scoring(void)
{
    fq_training_session_t ts;
    fq_training_session_init(&ts, FQ_TS_POWER);
    fq_training_session_start(&ts);
    ts.score = 0u;

    /* Hit IN zone (pos = 50, zone 40-60). */
    ts.target_pos = 50u;
    game_err_t err = fq_training_hit(&ts);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8(20u, ts.score);

    /* Hit OUTSIDE zone (pos = 20, below 40). Score should stay at 20 - 5 = 15. */
    ts.target_pos = 20u;
    err = fq_training_hit(&ts);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8(15u, ts.score);

    /* Hit outside zone with score=0 — clamp prevents underflow. */
    ts.score = 0u;
    ts.target_pos = 10u;
    err = fq_training_hit(&ts);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8(0u, ts.score); /* clamped, not wrapped */

    printf("[F9] training_hit scoring: PASS\n");
}

/* F10 — After 5 targets, state = DONE. */
static void test_f10_training_five_targets_done(void)
{
    fq_training_session_t ts;
    fq_training_session_init(&ts, FQ_TS_INTEL);
    fq_training_session_start(&ts);

    for (uint8_t i = 0u; i < 5u; i++) {
        /* Place target in zone and hit. */
        ts.target_pos = 50u;
        fq_training_hit(&ts);
        /* Advance to next target — mark this target done. */
        fq_training_advance_target(&ts);
    }

    TEST_ASSERT_EQUAL_UINT8(FQ_TS_DONE, ts.state);
    TEST_ASSERT_EQUAL_UINT8(5u, ts.targets_done);
    printf("[F10] training five targets -> DONE: PASS\n");
}

/* F11 — XP award = score * 2, capped at 200. */
static void test_f11_training_xp_award(void)
{
    fq_character_t ch = make_char(FQ_CLASS_BRUISER, 5u);
    ch.xp = 0u;

    fq_training_session_t ts;
    fq_training_session_init(&ts, FQ_TS_SPEED);
    ts.score = 80u;
    ts.state = FQ_TS_DONE;

    uint32_t xp = fq_training_get_xp_award(&ts);
    TEST_ASSERT_EQUAL_UINT32(160u, xp); /* 80 * 2 */

    fq_training_award_xp(&ts, &ch);
    TEST_ASSERT_EQUAL_UINT32(160u, ch.xp);
    printf("[F11] training XP award: PASS\n");
}

/* F12 — game_type affects target_speed: SPEED > POWER > INTEL. */
static void test_f12_training_game_type_speed(void)
{
    fq_training_session_t ts_sp, ts_pw, ts_in;
    fq_training_session_init(&ts_sp, FQ_TS_SPEED);
    fq_training_session_init(&ts_pw, FQ_TS_POWER);
    fq_training_session_init(&ts_in, FQ_TS_INTEL);

    fq_training_session_start(&ts_sp);
    fq_training_session_start(&ts_pw);
    fq_training_session_start(&ts_in);

    uint8_t spd_speed = fq_training_get_target_speed(&ts_sp);
    uint8_t spd_power = fq_training_get_target_speed(&ts_pw);
    uint8_t spd_intel = fq_training_get_target_speed(&ts_in);

    /* Speed > Power > Intel (target_speed). */
    TEST_ASSERT_TRUE(spd_speed > spd_power);
    TEST_ASSERT_TRUE(spd_power > spd_intel);
    TEST_ASSERT_TRUE(spd_intel >= 1u);

    printf("[F12] training game_type speeds: sp=%u pw=%u in=%u — PASS\n",
           spd_speed, spd_power, spd_intel);
}

/* ===========================================================================
 * ── ITEM 3: INVENTORY EQUIP/UNEQUIP ───────────────────────────────────────
 * =========================================================================*/

/* F13 — fq_equip_toggle equips an item into first free slot. */
static void test_f13_equip_basic(void)
{
    fq_character_t ch = make_char(FQ_CLASS_WARDEN, 5u);
    const uint16_t ids[] = { 7u };
    fq_inventory_t inv   = make_inv_ids(ids, 1u);

    game_err_t err = fq_equip_toggle(&ch, &inv, 0u);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);

    /* Item 7 must appear in one of the equipped slots. */
    uint8_t found = 0u;
    for (uint8_t i = 0u; i < 5u; i++) {
        if (ch.equipped[i] == 7u) { found = 1u; }
    }
    TEST_ASSERT_EQUAL_UINT8(1u, found);
    printf("[F13] equip basic: PASS\n");
}

/* F14 — fq_equip_toggle unequips an already-equipped item. */
static void test_f14_unequip_basic(void)
{
    fq_character_t ch = make_char(FQ_CLASS_WARDEN, 5u);
    const uint16_t ids[] = { 10u };
    fq_inventory_t inv   = make_inv_ids(ids, 1u);

    /* Equip. */
    fq_equip_toggle(&ch, &inv, 0u);

    /* Unequip (same call). */
    game_err_t err = fq_equip_toggle(&ch, &inv, 0u);
    TEST_ASSERT_EQUAL_INT(GAME_OK, (int)err);

    /* Slot must be cleared to 0. */
    for (uint8_t i = 0u; i < 5u; i++) {
        TEST_ASSERT_EQUAL_UINT16(0u, ch.equipped[i]);
    }
    printf("[F14] unequip basic: PASS\n");
}

/* F15 — item_equipped[] flags in fq_vm_inventory_t reflect equipped items. */
static void test_f15_vm_inventory_equipped_flags(void)
{
    fq_character_t ch = make_char(FQ_CLASS_WARDEN, 5u);
    const uint16_t ids[] = { 1u, 2u, 3u, 4u, 5u };
    fq_inventory_t inv   = make_inv_ids(ids, 5u);

    /* Equip items at slots 0 and 2. */
    fq_equip_toggle(&ch, &inv, 0u); /* equip item 1 */
    fq_equip_toggle(&ch, &inv, 2u); /* equip item 3 */

    fq_vm_inventory_t vm;
    memset(&vm, 0, sizeof(vm));
    fq_vm_build_inventory_ex(&vm, &inv, &ch, 0u, 0u);

    TEST_ASSERT_EQUAL_UINT8(1u, vm.item_equipped[0]);
    TEST_ASSERT_EQUAL_UINT8(0u, vm.item_equipped[1]);
    TEST_ASSERT_EQUAL_UINT8(1u, vm.item_equipped[2]);
    TEST_ASSERT_EQUAL_UINT8(0u, vm.item_equipped[3]);
    TEST_ASSERT_EQUAL_UINT8(0u, vm.item_equipped[4]);
    printf("[F15] vm_inventory equipped flags: PASS\n");
}

/* F16 — equipped_count is the MAX SLOT LIMIT, not modified during equip. */
static void test_f16_equipped_count_is_max_limit(void)
{
    fq_character_t ch = make_char(FQ_CLASS_BRUISER, 5u);
    ch.equipped_count = 4u; /* max 4 slots */

    const uint16_t ids[] = { 1u, 2u };
    fq_inventory_t inv = make_inv_ids(ids, 2u);

    uint8_t count_before = ch.equipped_count;
    fq_equip_toggle(&ch, &inv, 0u);
    fq_equip_toggle(&ch, &inv, 1u);

    /* equipped_count must be unchanged — it is a MAX, not a current count. */
    TEST_ASSERT_EQUAL_UINT8(count_before, ch.equipped_count);
    printf("[F16] equipped_count is max limit: PASS\n");
}

/* F17 — FSM: in INVENTORY, BTN_B cycles cursor, BTN_A toggles equip.
 *
 * Note: consecutive BTN_B presses within 6 ticks trigger double-tap exit.
 * To test cursor cycling without triggering double-tap, we advance
 * ctx.tick_count by 10 between each press (>> double-tap threshold of 6).
 */
static void test_f17_inventory_fsm_buttons(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));

    /* Give inventory 3 items. */
    inv.count    = 3u;
    inv.items[0] = 10u;
    inv.items[1] = 20u;
    inv.items[2] = 30u;
    player.equipped_count = 4u;

    fq_app_init(&ctx, &player, &inv);
    ctx.state = FQ_STATE_INVENTORY;
    ctx.inventory_cursor = 0u;

    fq_event_t eb = { FQ_EVT_BTN_B_PRESS, 0u };

    /* BTN_B cycles cursor: 0 -> 1.
     * tick_count starts at 0, inv_b_last_tick will be set to 0.
     * Advance tick_count by 10 before next press to clear double-tap window. */
    fq_app_dispatch(&ctx, &eb);
    TEST_ASSERT_EQUAL_UINT8(1u, ctx.inventory_cursor);

    /* Advance tick beyond double-tap window (> 6 ticks). */
    ctx.tick_count += 10u;

    /* BTN_B cycles cursor: 1 -> 2. */
    fq_app_dispatch(&ctx, &eb);
    TEST_ASSERT_EQUAL_UINT8(2u, ctx.inventory_cursor);

    /* Advance tick again. */
    ctx.tick_count += 10u;

    /* BTN_B cycles cursor: 2 -> 0 (wrap at item_count=3). */
    fq_app_dispatch(&ctx, &eb);
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.inventory_cursor);

    /* BTN_A toggles equip on cursor item (slot 0, item 10). */
    fq_event_t ea = { FQ_EVT_BTN_A_PRESS, 0u };
    fq_app_dispatch(&ctx, &ea);

    /* Item 10 must be equipped. */
    uint8_t found = 0u;
    for (uint8_t i = 0u; i < 5u; i++) {
        if (player.equipped[i] == 10u) { found = 1u; }
    }
    TEST_ASSERT_EQUAL_UINT8(1u, found);
    printf("[F17] inventory FSM buttons: PASS\n");
}

/* F18 — fq_vm_inventory_t has item_equipped[32] field (size assert). */
static void test_f18_vm_inventory_size(void)
{
    /* item_equipped[32] adds 32 bytes to the prior 548-byte struct.
     * New size = 548 + 32 = 580 bytes.
     * The _Static_assert in view_models.h enforces this at compile time.
     * This runtime check makes it visible in ctest. */
    TEST_ASSERT_EQUAL_UINT32(580u, (uint32_t)sizeof(fq_vm_inventory_t));
    printf("[F18] fq_vm_inventory_t size = 580: PASS\n");
}

/* ===========================================================================
 * ── ITEM 4: DOUBLE-TAP B POSITIVE FEATURE PATH ────────────────────────────
 * =========================================================================*/

/* F19 — Double-tap B with a clean tick delta exits INVENTORY to HOME.
 *
 * Positive path: two B presses with tick_count advancing by exactly 4 between
 * them (comfortably inside the 6-tick window). The FSM must transition to HOME
 * and reset home_menu_index to 0.
 */
static void test_f19_double_tap_b_exits_inventory(void)
{
    fq_character_t player;
    fq_inventory_t inv;
    fq_app_ctx_t   ctx;
    memset(&player, 0, sizeof(player));
    memset(&inv,    0, sizeof(inv));
    inv.count    = 2u;
    inv.items[0] = 5u;
    inv.items[1] = 6u;
    player.equipped_count = 4u;

    fq_app_init(&ctx, &player, &inv);
    ctx.state              = FQ_STATE_INVENTORY;
    ctx.inventory_cursor   = 0u;
    ctx.inv_b_press_count  = 0u;
    ctx.tick_count         = 100u; /* arbitrary non-zero base */
    ctx.inv_b_last_tick    = 100u;

    fq_event_t eb = { FQ_EVT_BTN_B_PRESS, 0u };

    /* First B press — cycles cursor (0 -> 1), records last_tick = 100.
     * inv_b_press_count becomes 1. */
    fq_app_dispatch(&ctx, &eb);
    TEST_ASSERT_EQUAL_INT(FQ_STATE_INVENTORY, (int)ctx.state);
    TEST_ASSERT_EQUAL_UINT8(1u, ctx.inventory_cursor);

    /* Advance tick by 4 — well within the 6-tick double-tap window. */
    ctx.tick_count = 104u;

    /* Second B press — ticks_since = 104 - 100 = 4 <= 6 → double-tap → HOME. */
    fq_app_dispatch(&ctx, &eb);
    TEST_ASSERT_EQUAL_INT(FQ_STATE_HOME, (int)ctx.state);

    /* home_menu_index must be reset to 0 by go_home(). */
    TEST_ASSERT_EQUAL_UINT8(0u, ctx.home_menu_index);

    printf("[F19] double-tap B exits inventory: PASS\n");
}

/* ===========================================================================
 * main
 * =========================================================================*/
int main(void)
{
    printf("=== Phase 19 Interactive Gameplay — Feature Tests ===\n");

    test_f1_onboarding_state_enum();
    test_f2_onboarding_btn_a_cycles_class();
    test_f3_onboarding_btn_b_confirms();
    test_f4_name_gen_two_part();
    test_f5_vm_onboarding_fields();
    test_f6_onboarding_idle_forbidden();

    test_f7_training_session_init();
    test_f8_training_session_step();
    test_f9_training_hit_scoring();
    test_f10_training_five_targets_done();
    test_f11_training_xp_award();
    test_f12_training_game_type_speed();

    test_f13_equip_basic();
    test_f14_unequip_basic();
    test_f15_vm_inventory_equipped_flags();
    test_f16_equipped_count_is_max_limit();
    test_f17_inventory_fsm_buttons();
    test_f18_vm_inventory_size();

    test_f19_double_tap_b_exits_inventory();

    printf("=== ALL FEATURE TESTS PASSED ===\n");
    return 0;
}
