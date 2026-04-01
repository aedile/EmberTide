/**
 * test_vm_builder.c — Phase 8: VM Builder Feature Tests (FEATURE RED)
 *
 * Covers happy-path value propagation through the VM builder functions.
 * All assertions use specific value checks (Constitution Priority 4).
 *
 * HOST-ONLY: no hal_*.h included.
 */

#include "test_assert.h"
#include "view_models.h"
#include "vm_builder.h"
#include "types.h"
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

/* ── fq_vm_build_home: field mapping ────────────────────────────────────── */

static void test_build_home_name_copied(void)
{
    fq_vm_home_t   vm;
    fq_character_t ch;
    memset(&vm, 0, sizeof(vm));
    memset(&ch, 0, sizeof(ch));

    /* Use a 5-char name plus null terminator within the 12-byte field. */
    strncpy(ch.name, "Ember", sizeof(ch.name) - 1u);
    ch.name[sizeof(ch.name) - 1u] = '\0';

    fq_vm_build_home(&vm, &ch);

    TEST_ASSERT_EQUAL_UINT8((uint8_t)'E', (uint8_t)vm.name[0]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'m', (uint8_t)vm.name[1]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'b', (uint8_t)vm.name[2]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'e', (uint8_t)vm.name[3]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'r', (uint8_t)vm.name[4]);
    TEST_ASSERT_EQUAL_UINT8(0u,           (uint8_t)vm.name[5]);
}

static void test_build_home_class_id_copied(void)
{
    fq_vm_home_t   vm;
    fq_character_t ch;
    memset(&vm, 0, sizeof(vm));
    memset(&ch, 0, sizeof(ch));

    ch.class_id = (uint8_t)FQ_CLASS_HEX;

    fq_vm_build_home(&vm, &ch);

    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_CLASS_HEX, vm.class_id);
}

static void test_build_home_level_copied(void)
{
    fq_vm_home_t   vm;
    fq_character_t ch;
    memset(&vm, 0, sizeof(vm));
    memset(&ch, 0, sizeof(ch));

    ch.level = 7u;

    fq_vm_build_home(&vm, &ch);

    TEST_ASSERT_EQUAL_UINT8(7u, vm.level);
}

static void test_build_home_wins_losses_copied(void)
{
    fq_vm_home_t   vm;
    fq_character_t ch;
    memset(&vm, 0, sizeof(vm));
    memset(&ch, 0, sizeof(ch));

    ch.wins   = 42u;
    ch.losses = 17u;

    fq_vm_build_home(&vm, &ch);

    TEST_ASSERT_EQUAL_UINT16(42u, vm.wins);
    TEST_ASSERT_EQUAL_UINT16(17u, vm.losses);
}

static void test_build_home_sprite_base_copied(void)
{
    fq_vm_home_t   vm;
    fq_character_t ch;
    memset(&vm, 0, sizeof(vm));
    memset(&ch, 0, sizeof(ch));

    ch.sprite_base = 3u;

    fq_vm_build_home(&vm, &ch);

    TEST_ASSERT_EQUAL_UINT8(3u, vm.sprite_base);
}

/**
 * Phase-8 spec (backlog item 1):
 * "takes a character with 10/20 HP, generates a View Model, and asserts
 *  the View Model hp_percent is exactly 50%."
 *
 * fq_character_t has hp_max but no hp_current (hp_current is a combat
 * transient). The home screen builder represents "current HP" using
 * hp_max (full HP) and the maximum possible HP is the character's
 * hp_max. For the 50% test, we call the builder directly with the
 * explicit hp formula via the with_hp variant that accepts separate
 * hp_current and hp_max values. If the builder only has hp_max, we
 * test what the spec actually requires: hp formula correctness.
 *
 * The builder API `fq_vm_build_home` maps ch->hp_max as current HP
 * (since the home screen always shows you at your max HP — combat HP
 * is not persisted to the save record). A separate function
 * `fq_vm_build_home_ex` is available for testing with explicit
 * hp_current / hp_max to prove the formula.
 */
static void test_build_home_hp_percent_formula_50(void)
{
    fq_vm_home_t vm;
    memset(&vm, 0, sizeof(vm));

    /* hp_current=10, hp_max=20 → 10*100/20 = 50. */
    fq_vm_build_home_ex(&vm, 10u, 20u);

    TEST_ASSERT_EQUAL_UINT8(50u, vm.hp_percent);
}

static void test_build_home_hp_percent_formula_75(void)
{
    fq_vm_home_t vm;
    memset(&vm, 0, sizeof(vm));

    /* hp_current=15, hp_max=20 → 15*100/20 = 75. */
    fq_vm_build_home_ex(&vm, 15u, 20u);

    TEST_ASSERT_EQUAL_UINT8(75u, vm.hp_percent);
}

static void test_build_home_hp_percent_over_max_clamped_100(void)
{
    fq_vm_home_t vm;
    memset(&vm, 0, sizeof(vm));

    /* hp_current=300, hp_max=100 → would be 300, clamped to 100. */
    fq_vm_build_home_ex(&vm, 300u, 100u);

    TEST_ASSERT_EQUAL_UINT8(100u, vm.hp_percent);
}

/* ── fq_vm_build_inventory: field mapping ───────────────────────────────── */

static void test_build_inventory_count_mapped(void)
{
    fq_vm_inventory_t vm;
    fq_inventory_t    inv;
    memset(&vm,  0, sizeof(vm));
    memset(&inv, 0, sizeof(inv));

    inv.count = 5u;

    fq_vm_build_inventory(&vm, &inv);

    TEST_ASSERT_EQUAL_UINT8(5u, vm.item_count);
}

/**
 * Tests that fq_item_lookup is called for a known item and the name is
 * placed in the vm. Item ID 1 ("Iron Fist") is the first entry in the
 * item table from Phase 5.
 */
static void test_build_inventory_known_item_name_populated(void)
{
    fq_vm_inventory_t vm;
    fq_inventory_t    inv;
    memset(&vm,  0, sizeof(vm));
    memset(&inv, 0, sizeof(inv));

    inv.items[0] = 1u; /* Item ID 1 — Iron Fist */
    inv.count    = 1u;

    fq_vm_build_inventory(&vm, &inv);

    /* Name must not be empty. */
    TEST_ASSERT_TRUE(vm.item_names[0][0] != '\0');
    /* Name must be null-terminated within its 16-byte buffer. */
    TEST_ASSERT_EQUAL_UINT8(0u, (uint8_t)vm.item_names[0][15]);
}

static void test_build_inventory_unknown_item_name_empty(void)
{
    fq_vm_inventory_t vm;
    fq_inventory_t    inv;
    memset(&vm,  0xFF, sizeof(vm)); /* Poison. */
    memset(&inv, 0,    sizeof(inv));

    inv.items[0] = 0xFFFFu; /* Unknown item ID — fq_item_lookup returns NULL. */
    inv.count    = 1u;

    fq_vm_build_inventory(&vm, &inv);

    /* Unknown item → name must be empty string (not garbage). */
    TEST_ASSERT_EQUAL_UINT8(0u, (uint8_t)vm.item_names[0][0]);
}

static void test_build_inventory_rarity_zero_for_unknown(void)
{
    fq_vm_inventory_t vm;
    fq_inventory_t    inv;
    memset(&vm,  0xFF, sizeof(vm));
    memset(&inv, 0,    sizeof(inv));

    inv.items[0] = 0xFFFFu;
    inv.count    = 1u;

    fq_vm_build_inventory(&vm, &inv);

    TEST_ASSERT_EQUAL_UINT8(0u, vm.item_rarities[0]);
}

/* ── fq_vm_build_stats: field mapping ───────────────────────────────────── */

static void test_build_stats_name_copied(void)
{
    fq_vm_stats_t  vm;
    fq_character_t ch;
    memset(&vm, 0, sizeof(vm));
    memset(&ch, 0, sizeof(ch));

    strncpy(ch.name, "Tide", sizeof(ch.name) - 1u);
    ch.name[sizeof(ch.name) - 1u] = '\0';

    fq_vm_build_stats(&vm, &ch);

    TEST_ASSERT_EQUAL_UINT8((uint8_t)'T', (uint8_t)vm.name[0]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'i', (uint8_t)vm.name[1]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'d', (uint8_t)vm.name[2]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'e', (uint8_t)vm.name[3]);
    TEST_ASSERT_EQUAL_UINT8(0u,           (uint8_t)vm.name[4]);
}

static void test_build_stats_level_copied(void)
{
    fq_vm_stats_t  vm;
    fq_character_t ch;
    memset(&vm, 0, sizeof(vm));
    memset(&ch, 0, sizeof(ch));

    ch.level = 12u;

    fq_vm_build_stats(&vm, &ch);

    TEST_ASSERT_EQUAL_UINT8(12u, vm.level);
}

static void test_build_stats_stats_copied(void)
{
    fq_vm_stats_t  vm;
    fq_character_t ch;
    memset(&vm, 0, sizeof(vm));
    memset(&ch, 0, sizeof(ch));

    ch.strength     = 50u;
    ch.speed        = 30u;
    ch.precision    = 20u;
    ch.intelligence = 10u;

    fq_vm_build_stats(&vm, &ch);

    TEST_ASSERT_EQUAL_UINT8(50u, vm.strength);
    TEST_ASSERT_EQUAL_UINT8(30u, vm.speed);
    TEST_ASSERT_EQUAL_UINT8(20u, vm.precision);
    TEST_ASSERT_EQUAL_UINT8(10u, vm.intelligence);
}

static void test_build_stats_hp_max_copied(void)
{
    fq_vm_stats_t  vm;
    fq_character_t ch;
    memset(&vm, 0, sizeof(vm));
    memset(&ch, 0, sizeof(ch));

    ch.hp_max = 350u;

    fq_vm_build_stats(&vm, &ch);

    TEST_ASSERT_EQUAL_UINT16(350u, vm.hp_max);
}

static void test_build_stats_xp_copied(void)
{
    fq_vm_stats_t  vm;
    fq_character_t ch;
    memset(&vm, 0, sizeof(vm));
    memset(&ch, 0, sizeof(ch));

    ch.xp    = 1234u;
    ch.level = 5u;

    fq_vm_build_stats(&vm, &ch);

    TEST_ASSERT_EQUAL_UINT32(1234u, vm.xp);
    /* fq_calc_xp_to_next(5) = 50*5*5 = 1250. */
    TEST_ASSERT_EQUAL_UINT32(1250u, vm.xp_to_next);
}

static void test_build_stats_rebirth_count_copied(void)
{
    fq_vm_stats_t  vm;
    fq_character_t ch;
    memset(&vm, 0, sizeof(vm));
    memset(&ch, 0, sizeof(ch));

    ch.rebirth_count = 3u;

    fq_vm_build_stats(&vm, &ch);

    TEST_ASSERT_EQUAL_UINT8(3u, vm.rebirth_count);
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(void)
{
    /* Home screen builder */
    test_build_home_name_copied();
    test_build_home_class_id_copied();
    test_build_home_level_copied();
    test_build_home_wins_losses_copied();
    test_build_home_sprite_base_copied();
    test_build_home_hp_percent_formula_50();
    test_build_home_hp_percent_formula_75();
    test_build_home_hp_percent_over_max_clamped_100();

    /* Inventory builder */
    test_build_inventory_count_mapped();
    test_build_inventory_known_item_name_populated();
    test_build_inventory_unknown_item_name_empty();
    test_build_inventory_rarity_zero_for_unknown();

    /* Stats builder */
    test_build_stats_name_copied();
    test_build_stats_level_copied();
    test_build_stats_stats_copied();
    test_build_stats_hp_max_copied();
    test_build_stats_xp_copied();
    test_build_stats_rebirth_count_copied();

    printf("test_vm_builder: ALL PASS\n");
    return 0;
}
