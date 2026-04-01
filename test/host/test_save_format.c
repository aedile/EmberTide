/**
 * test_save_format.c — Phase 3, Phase B (FEATURE RED)
 *
 * Feature tests for the save serializer/deserializer happy paths.
 *
 * Covers:
 *   - Full serialize/deserialize round-trip
 *   - Endianness verification (0x11223344 → 44 33 22 11)
 *   - Rival log field-by-field (happy path subset here; deep bounds in test_types_bounds.c)
 */

#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "types.h"
#include "save_format.h"
#include "test_assert.h"

/* -------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------*/

static void make_reference_character(fq_character_t *ch)
{
    memset(ch, 0, sizeof(*ch));
    ch->id             = 0x12345678u;
    ch->xp             = 9999u;
    ch->legacy_tree    = 0xA5A5A5A5u;
    ch->hp_max         = 350u;
    ch->wins           = 42u;
    ch->losses         = 7u;
    ch->equipped[0]    = 5u;
    ch->equipped[1]    = 12u;
    ch->equipped[2]    = 0u;
    ch->equipped[3]    = 0u;
    ch->equipped[4]    = 0u;
    memcpy(ch->name, "Solstice", 8u);
    ch->name[8]        = '\0';
    ch->save_version   = FQ_SAVE_VERSION_CURRENT;
    ch->class_id       = (uint8_t)FQ_CLASS_TRICKSTER;
    ch->level          = 12u;
    ch->strength       = 14u;
    ch->speed          = 18u;
    ch->precision      = 11u;
    ch->intelligence   = 9u;
    ch->rebirth_count  = 1u;
    ch->legacy_points  = 2u;
    ch->is_dead        = 0u;
    ch->sprite_base    = 2u;
    ch->cosmetic_slots[0] = 3u;
    ch->cosmetic_slots[1] = 0u;
    ch->cosmetic_slots[2] = 1u;
    ch->cosmetic_slots[3] = 0u;
    ch->title          = 5u;
    ch->equipped_count = 2u;
    ch->wildcard_passive = 0u;

    ch->rival_log[0].opponent_id   = 0xDEADC0DEu;
    ch->rival_log[0].encounters    = 4u;
    ch->rival_log[0].wins          = 3u;
    ch->rival_log[0].last_fight_ts = 0x600000FFu;
    ch->rival_log[0].is_nemesis    = 1u;
}

static void make_reference_inventory(fq_inventory_t *inv)
{
    memset(inv, 0, sizeof(*inv));
    inv->items[0] = 101u;
    inv->items[1] = 202u;
    inv->items[2] = 303u;
    inv->count    = 3u;
}

/* -------------------------------------------------------------------------
 * FEAT-1: Full round-trip — serialize then deserialize produces identical struct
 * -------------------------------------------------------------------------*/
static void test_full_roundtrip(void)
{
    uint8_t buf[FQ_SAVE_MAX_SIZE];
    fq_character_t ch_orig;
    fq_inventory_t inv_orig;
    make_reference_character(&ch_orig);
    make_reference_inventory(&inv_orig);

    size_t written = fq_save_serialize(&ch_orig, &inv_orig, buf, sizeof(buf));
    TEST_ASSERT_TRUE(written > 0u);
    TEST_ASSERT_EQUAL_UINT32(FQ_SAVE_SERIALIZED_SIZE_V1, (uint32_t)written);
    printf("[INFO] serialize wrote %zu bytes\n", written);

    fq_character_t ch_read;
    fq_inventory_t inv_read;
    fq_save_err_t err = fq_save_deserialize(buf, written, &ch_read, &inv_read);
    TEST_ASSERT_EQUAL_INT(FQ_SAVE_OK, (int)err);

    /* Character fields */
    TEST_ASSERT_EQUAL_UINT32(ch_orig.id,          ch_read.id);
    TEST_ASSERT_EQUAL_UINT32(ch_orig.xp,          ch_read.xp);
    TEST_ASSERT_EQUAL_UINT32(ch_orig.legacy_tree, ch_read.legacy_tree);
    TEST_ASSERT_EQUAL_UINT16(ch_orig.hp_max,      ch_read.hp_max);
    TEST_ASSERT_EQUAL_UINT16(ch_orig.wins,        ch_read.wins);
    TEST_ASSERT_EQUAL_UINT16(ch_orig.losses,      ch_read.losses);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.class_id,     ch_read.class_id);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.level,        ch_read.level);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.strength,     ch_read.strength);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.speed,        ch_read.speed);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.precision,    ch_read.precision);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.intelligence, ch_read.intelligence);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.rebirth_count, ch_read.rebirth_count);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.legacy_points, ch_read.legacy_points);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.is_dead,      ch_read.is_dead);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.sprite_base,  ch_read.sprite_base);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.title,        ch_read.title);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.equipped_count, ch_read.equipped_count);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.wildcard_passive, ch_read.wildcard_passive);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.save_version, ch_read.save_version);

    /* Name */
    TEST_ASSERT_TRUE(memcmp(ch_orig.name, ch_read.name, sizeof(ch_orig.name)) == 0);

    /* cosmetic_slots */
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_UINT8(ch_orig.cosmetic_slots[i], ch_read.cosmetic_slots[i]);
    }

    /* equipped */
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_UINT16(ch_orig.equipped[i], ch_read.equipped[i]);
    }

    /* rival_log[0] */
    TEST_ASSERT_EQUAL_UINT32(ch_orig.rival_log[0].opponent_id,   ch_read.rival_log[0].opponent_id);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.rival_log[0].encounters,     ch_read.rival_log[0].encounters);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.rival_log[0].wins,           ch_read.rival_log[0].wins);
    TEST_ASSERT_EQUAL_UINT32(ch_orig.rival_log[0].last_fight_ts, ch_read.rival_log[0].last_fight_ts);
    TEST_ASSERT_EQUAL_UINT8(ch_orig.rival_log[0].is_nemesis,     ch_read.rival_log[0].is_nemesis);

    /* Inventory */
    TEST_ASSERT_EQUAL_UINT8(inv_orig.count, inv_read.count);
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_UINT16(inv_orig.items[i], inv_read.items[i]);
    }
}

/* -------------------------------------------------------------------------
 * FEAT-2: Endianness verification
 * Serialize a character with id=0x11223344. The first four bytes of the id
 * field in the byte stream must be 0x44, 0x33, 0x22, 0x11 (little-endian).
 *
 * The serialization format is: [version:1][...character fields...][...inv...][crc:4]
 * We verify by finding the 0x44 0x33 0x22 0x11 pattern in the buffer.
 * -------------------------------------------------------------------------*/
static void test_little_endian_serialization(void)
{
    uint8_t buf[FQ_SAVE_MAX_SIZE];
    fq_character_t ch;
    fq_inventory_t inv;
    make_reference_character(&ch);
    make_reference_inventory(&inv);

    ch.id = 0x11223344u;

    size_t written = fq_save_serialize(&ch, &inv, buf, sizeof(buf));
    TEST_ASSERT_TRUE(written > 0u);

    /* Scan buffer for the little-endian sequence 44 33 22 11 */
    int found = 0;
    for (size_t i = 0u; i + 3u < written; i++) {
        if (buf[i] == 0x44u && buf[i+1u] == 0x33u &&
            buf[i+2u] == 0x22u && buf[i+3u] == 0x11u) {
            found = 1;
            printf("[INFO] LE pattern found at offset %zu\n", i);
            break;
        }
    }
    TEST_ASSERT_TRUE(found);

    /* Also verify big-endian pattern 11 22 33 44 is NOT present */
    int found_be = 0;
    for (size_t i = 0u; i + 3u < written; i++) {
        if (buf[i] == 0x11u && buf[i+1u] == 0x22u &&
            buf[i+2u] == 0x33u && buf[i+3u] == 0x44u) {
            found_be = 1;
            break;
        }
    }
    TEST_ASSERT_TRUE(!found_be);
}

/* -------------------------------------------------------------------------
 * FEAT-3: First byte of serialized buffer is FQ_SAVE_VERSION_CURRENT
 * -------------------------------------------------------------------------*/
static void test_version_byte_is_first(void)
{
    uint8_t buf[FQ_SAVE_MAX_SIZE];
    fq_character_t ch;
    fq_inventory_t inv;
    make_reference_character(&ch);
    make_reference_inventory(&inv);

    size_t written = fq_save_serialize(&ch, &inv, buf, sizeof(buf));
    TEST_ASSERT_TRUE(written > 0u);

    TEST_ASSERT_EQUAL_UINT8(FQ_SAVE_VERSION_CURRENT, buf[0]);
}

/* -------------------------------------------------------------------------
 * FEAT-4: Name round-trip — null termination enforced at name[11] on deserialize
 * -------------------------------------------------------------------------*/
static void test_name_null_termination_on_deserialize(void)
{
    uint8_t buf[FQ_SAVE_MAX_SIZE];
    fq_character_t ch;
    fq_inventory_t inv;
    make_reference_character(&ch);
    make_reference_inventory(&inv);

    /* Set name to exactly 11 chars, no null at end (we'll force it) */
    memcpy(ch.name, "ABCDEFGHIJK", 11);
    ch.name[11] = '\0'; /* serializer sees null-terminated */

    size_t written = fq_save_serialize(&ch, &inv, buf, sizeof(buf));
    TEST_ASSERT_TRUE(written > 0u);

    fq_character_t ch2;
    fq_inventory_t inv2;
    fq_save_err_t err = fq_save_deserialize(buf, written, &ch2, &inv2);
    TEST_ASSERT_EQUAL_INT(FQ_SAVE_OK, (int)err);

    /* Deserializer MUST null-terminate at name[11] */
    TEST_ASSERT_EQUAL_UINT8(0u, (uint8_t)ch2.name[11]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'A', (uint8_t)ch2.name[0]);
}

/* -------------------------------------------------------------------------
 * FEAT-5: save_version field is round-tripped in the character struct
 * -------------------------------------------------------------------------*/
static void test_save_version_roundtrip(void)
{
    uint8_t buf[FQ_SAVE_MAX_SIZE];
    fq_character_t ch;
    fq_inventory_t inv;
    make_reference_character(&ch);
    make_reference_inventory(&inv);

    ch.save_version = FQ_SAVE_VERSION_CURRENT;

    size_t written = fq_save_serialize(&ch, &inv, buf, sizeof(buf));
    TEST_ASSERT_TRUE(written > 0u);

    fq_character_t ch2;
    fq_inventory_t inv2;
    fq_save_err_t err = fq_save_deserialize(buf, written, &ch2, &inv2);
    TEST_ASSERT_EQUAL_INT(FQ_SAVE_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8(FQ_SAVE_VERSION_CURRENT, ch2.save_version);
}

/* -------------------------------------------------------------------------
 * FEAT-6: Zero inventory round-trip
 * -------------------------------------------------------------------------*/
static void test_zero_inventory_roundtrip(void)
{
    uint8_t buf[FQ_SAVE_MAX_SIZE];
    fq_character_t ch;
    fq_inventory_t inv;
    make_reference_character(&ch);
    memset(&inv, 0, sizeof(inv));
    inv.count = 0u;

    size_t written = fq_save_serialize(&ch, &inv, buf, sizeof(buf));
    TEST_ASSERT_TRUE(written > 0u);

    fq_character_t ch2;
    fq_inventory_t inv2;
    fq_save_err_t err = fq_save_deserialize(buf, written, &ch2, &inv2);
    TEST_ASSERT_EQUAL_INT(FQ_SAVE_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8(0u, inv2.count);
}

/* -------------------------------------------------------------------------
 * FEAT-7: Two sequential serializations of the same data produce identical buffers
 * (determinism)
 * -------------------------------------------------------------------------*/
static void test_serialize_is_deterministic(void)
{
    uint8_t buf1[FQ_SAVE_MAX_SIZE];
    uint8_t buf2[FQ_SAVE_MAX_SIZE];
    fq_character_t ch;
    fq_inventory_t inv;
    make_reference_character(&ch);
    make_reference_inventory(&inv);

    size_t w1 = fq_save_serialize(&ch, &inv, buf1, sizeof(buf1));
    size_t w2 = fq_save_serialize(&ch, &inv, buf2, sizeof(buf2));

    TEST_ASSERT_EQUAL_UINT32(FQ_SAVE_SERIALIZED_SIZE_V1, (uint32_t)w1);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)w1, (uint32_t)w2);
    TEST_ASSERT_EQUAL_INT(0, memcmp(buf1, buf2, w1));
}

/* -------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------*/
int main(void)
{
    printf("=== Phase 3 Feature Tests (Save Format) ===\n");

    test_full_roundtrip();
    test_little_endian_serialization();
    test_version_byte_is_first();
    test_name_null_termination_on_deserialize();
    test_save_version_roundtrip();
    test_zero_inventory_roundtrip();
    test_serialize_is_deterministic();

    printf("=== All Phase 3 Feature Tests PASSED ===\n");
    return 0;
}
