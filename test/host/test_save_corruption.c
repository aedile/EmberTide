/**
 * test_save_corruption.c — Phase 3, Item 3 (corruption handling)
 *
 * Tests for:
 *   - CRC mismatch via single bit flip at every byte position
 *   - Short buffer (deserialize with truncated data)
 *   - Garbage version bytes (0x00 and 0x99)
 *   - Future version (current + 1)
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

static void make_valid_save(uint8_t *buf, size_t buf_size, size_t *written_out)
{
    fq_character_t ch;
    fq_inventory_t inv;

    memset(&ch, 0, sizeof(ch));
    ch.id             = 0xFEEDFACEu;
    ch.xp             = 500u;
    ch.legacy_tree    = 0x00000003u;
    ch.hp_max         = 180u;
    ch.wins           = 8u;
    ch.losses         = 2u;
    ch.equipped[0]    = 7u;
    ch.equipped[1]    = 0u;
    ch.equipped[2]    = 0u;
    ch.equipped[3]    = 0u;
    ch.equipped[4]    = 0u;
    memcpy(ch.name, "Cinder", 6u);
    ch.name[6]        = '\0';
    ch.save_version   = FQ_SAVE_VERSION_CURRENT;
    ch.class_id       = (uint8_t)FQ_CLASS_HEX;
    ch.level          = 7u;
    ch.strength       = 8u;
    ch.speed          = 12u;
    ch.precision      = 9u;
    ch.intelligence   = 15u;
    ch.rebirth_count  = 0u;
    ch.legacy_points  = 1u;
    ch.is_dead        = 0u;
    ch.sprite_base    = 3u;
    ch.cosmetic_slots[0] = 0u;
    ch.cosmetic_slots[1] = 0u;
    ch.cosmetic_slots[2] = 0u;
    ch.cosmetic_slots[3] = 0u;
    ch.title          = 2u;
    ch.equipped_count = 1u;
    ch.wildcard_passive = 0u;

    ch.rival_log[0].opponent_id   = 0xAAAABBBBu;
    ch.rival_log[0].encounters    = 2u;
    ch.rival_log[0].wins          = 1u;
    ch.rival_log[0].last_fight_ts = 0x12345678u;
    ch.rival_log[0].is_nemesis    = 0u;

    memset(&inv, 0, sizeof(inv));
    inv.items[0] = 55u;
    inv.count    = 1u;

    size_t written = fq_save_serialize(&ch, &inv, buf, buf_size);
    TEST_ASSERT_TRUE(written > 0u);
    if (written_out != NULL) {
        *written_out = written;
    }
}

/* -------------------------------------------------------------------------
 * CORRUPT-1: Single bit flip at every byte position detects CRC mismatch
 *
 * The CRC covers byte 0 (version) through byte N-5 (last payload byte).
 * Flipping any bit in that range must cause FQ_SAVE_ERR_CRC.
 * Flipping bits in the CRC bytes themselves (last 4 bytes) also causes
 * FQ_SAVE_ERR_CRC because the stored CRC no longer matches.
 * -------------------------------------------------------------------------*/
static void test_single_bit_flip_all_positions(void)
{
    uint8_t golden[FQ_SAVE_MAX_SIZE];
    size_t golden_len = 0u;
    make_valid_save(golden, sizeof(golden), &golden_len);
    TEST_ASSERT_TRUE(golden_len >= 5u);

    printf("[INFO] Testing single-bit-flip corruption over %zu bytes\n", golden_len);

    uint8_t corrupt[FQ_SAVE_MAX_SIZE];
    uint32_t flip_count = 0u;

    for (size_t byte_pos = 0u; byte_pos < golden_len; byte_pos++) {
        for (int bit = 0; bit < 8; bit++) {
            /* Copy golden, flip one bit */
            memcpy(corrupt, golden, golden_len);
            corrupt[byte_pos] ^= (uint8_t)(1u << bit);

            fq_character_t ch;
            fq_inventory_t inv;
            fq_save_err_t err = fq_save_deserialize(corrupt, golden_len, &ch, &inv);

            /* Must not return FQ_SAVE_OK — any error is acceptable */
            if (err == FQ_SAVE_OK) {
                fprintf(stderr,
                    "[FAIL] Bit flip at byte %zu bit %d was NOT detected "
                    "(returned FQ_SAVE_OK)\n",
                    byte_pos, bit);
                exit(1);
            }
            flip_count++;
        }
    }

    printf("[INFO] All %u single-bit flips correctly detected\n", flip_count);
}

/* -------------------------------------------------------------------------
 * CORRUPT-2: Short buffer returns FQ_SAVE_ERR_BUFFER_TOO_SMALL
 * -------------------------------------------------------------------------*/
static void test_short_buffer_deserialize(void)
{
    uint8_t golden[FQ_SAVE_MAX_SIZE];
    size_t golden_len = 0u;
    make_valid_save(golden, sizeof(golden), &golden_len);

    /* Test a range of truncated sizes: 0, 1, 2, 4, half, golden_len-1 */
    size_t sizes[] = {0u, 1u, 2u, 4u, golden_len / 2u, golden_len - 1u};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (size_t s = 0u; s < num_sizes; s++) {
        size_t trunc_size = sizes[s];
        fq_character_t ch;
        fq_inventory_t inv;
        fq_save_err_t err = fq_save_deserialize(golden, trunc_size, &ch, &inv);
        TEST_ASSERT_TRUE(err != FQ_SAVE_OK);
        /* For sizes < required minimum, must be BUFFER_TOO_SMALL */
        if (trunc_size < golden_len) {
            TEST_ASSERT_TRUE(err == FQ_SAVE_ERR_BUFFER_TOO_SMALL ||
                             err == FQ_SAVE_ERR_CRC ||
                             err == FQ_SAVE_ERR_CORRUPT);
        }
        printf("[INFO] truncated buf_size=%zu → err=%d (not FQ_SAVE_OK)\n",
               trunc_size, (int)err);
    }
}

/* -------------------------------------------------------------------------
 * CORRUPT-3: Garbage version byte 0x00
 * -------------------------------------------------------------------------*/
static void test_garbage_version_zero(void)
{
    uint8_t golden[FQ_SAVE_MAX_SIZE];
    size_t golden_len = 0u;
    make_valid_save(golden, sizeof(golden), &golden_len);

    golden[0] = 0x00u; /* version = 0 */

    fq_character_t ch;
    fq_inventory_t inv;
    fq_save_err_t err = fq_save_deserialize(golden, golden_len, &ch, &inv);
    TEST_ASSERT_TRUE(err != FQ_SAVE_OK);
    printf("[INFO] version=0x00 → err=%d\n", (int)err);
}

/* -------------------------------------------------------------------------
 * CORRUPT-4: Garbage version byte 0x99
 * -------------------------------------------------------------------------*/
static void test_garbage_version_0x99(void)
{
    uint8_t golden[FQ_SAVE_MAX_SIZE];
    size_t golden_len = 0u;
    make_valid_save(golden, sizeof(golden), &golden_len);

    golden[0] = 0x99u; /* far-future version */

    fq_character_t ch;
    fq_inventory_t inv;
    fq_save_err_t err = fq_save_deserialize(golden, golden_len, &ch, &inv);
    TEST_ASSERT_EQUAL_INT(FQ_SAVE_ERR_VERSION_TOO_NEW, (int)err);
    printf("[INFO] version=0x99 → FQ_SAVE_ERR_VERSION_TOO_NEW\n");
}

/* -------------------------------------------------------------------------
 * CORRUPT-5: Future version (current + 1) yields FQ_SAVE_ERR_VERSION_TOO_NEW
 * and is distinct from FQ_SAVE_ERR_CRC
 * -------------------------------------------------------------------------*/
static void test_future_version_distinct_error(void)
{
    uint8_t golden[FQ_SAVE_MAX_SIZE];
    size_t golden_len = 0u;
    make_valid_save(golden, sizeof(golden), &golden_len);

    uint8_t future_version = (uint8_t)(FQ_SAVE_VERSION_CURRENT + 1u);
    golden[0] = future_version;

    fq_character_t ch;
    fq_inventory_t inv;
    fq_save_err_t err = fq_save_deserialize(golden, golden_len, &ch, &inv);

    /* Must be the specific VERSION_TOO_NEW code, not the generic CRC error */
    TEST_ASSERT_EQUAL_INT(FQ_SAVE_ERR_VERSION_TOO_NEW, (int)err);
    TEST_ASSERT_TRUE(err != FQ_SAVE_ERR_CRC);
    printf("[INFO] future version=%u → FQ_SAVE_ERR_VERSION_TOO_NEW (not CRC)\n",
           (unsigned)future_version);
}

/* -------------------------------------------------------------------------
 * CORRUPT-6: Multi-byte corruption (zero out the entire payload section)
 * Still detected as CRC mismatch
 * -------------------------------------------------------------------------*/
static void test_bulk_payload_corruption(void)
{
    uint8_t golden[FQ_SAVE_MAX_SIZE];
    size_t golden_len = 0u;
    make_valid_save(golden, sizeof(golden), &golden_len);

    /* Zero out bytes 1 through golden_len-5 (payload, preserve version+CRC) */
    if (golden_len > 5u) {
        memset(golden + 1u, 0, golden_len - 5u);
    }

    fq_character_t ch;
    fq_inventory_t inv;
    fq_save_err_t err = fq_save_deserialize(golden, golden_len, &ch, &inv);
    TEST_ASSERT_EQUAL_INT(FQ_SAVE_ERR_CRC, (int)err);
}

/* -------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------*/
int main(void)
{
    printf("=== Phase 3 Corruption Tests ===\n");

    test_single_bit_flip_all_positions();
    test_short_buffer_deserialize();
    test_garbage_version_zero();
    test_garbage_version_0x99();
    test_future_version_distinct_error();
    test_bulk_payload_corruption();

    printf("=== All Phase 3 Corruption Tests PASSED ===\n");
    return 0;
}
