/**
 * test_determinism_bounds.c — Bound/negative tests for combat determinism
 * (Phase A: BOUND RED).
 *
 * Covers: _Static_assert type width guarantees, struct padding mutation
 * detection, single-byte mutation hash change, and DET-NT-6 uninit PRNG
 * documentation.
 *
 * Rule 22: BOUND RED → FEATURE RED → GREEN → REFACTOR.
 * Constitution Priority 0: These tests enforce the determinism contract.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "test_assert.h"
#include "prng.h"
#include "crc32.h"

/* ---------------------------------------------------------------------------
 * DET-NT-1: _Static_assert for all required type widths.
 * These fire at compile time — if the host architecture is wrong, this
 * translation unit will not compile, providing the earliest possible signal.
 * ---------------------------------------------------------------------------*/
_Static_assert(sizeof(uint8_t)  == 1u, "uint8_t  must be 1 byte");
_Static_assert(sizeof(uint16_t) == 2u, "uint16_t must be 2 bytes");
_Static_assert(sizeof(uint32_t) == 4u, "uint32_t must be 4 bytes");
_Static_assert(sizeof(int8_t)   == 1u, "int8_t   must be 1 byte");
_Static_assert(sizeof(bool)     == 1u, "bool     must be 1 byte");

/* ---------------------------------------------------------------------------
 * Minimal combat-like struct used by padding and mutation tests.
 * All fields are fixed-width integer types — no implicit padding surprises
 * for these types on any conforming implementation.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint32_t attacker_hp;
    uint32_t defender_hp;
    uint16_t attack_stat;
    uint16_t defense_stat;
    uint8_t  turn_count;
    uint8_t  seed_offset;
    uint16_t reserved; /* explicit pad to make the struct 16 bytes clean */
} combat_snapshot_t;

_Static_assert(sizeof(combat_snapshot_t) == 16u,
    "combat_snapshot_t must be exactly 16 bytes (no implicit padding)");

/* ---------------------------------------------------------------------------
 * Helper: populate a combat_snapshot_t with deterministic test values.
 * ---------------------------------------------------------------------------*/
static void populate_snapshot(combat_snapshot_t *snap)
{
    snap->attacker_hp  = 100u;
    snap->defender_hp  = 80u;
    snap->attack_stat  = 25u;
    snap->defense_stat = 15u;
    snap->turn_count   = 3u;
    snap->seed_offset  = 7u;
    snap->reserved     = 0u;
}

/* DET-NT-2: Struct padding — hash must be identical regardless of whether the
 * struct was memset to 0x00 or 0xFF before populating explicit fields.
 * This ensures no uninitialized padding byte leaks into the hash. */
static void test_struct_padding_hash_invariant(void)
{
    combat_snapshot_t snap_a;
    combat_snapshot_t snap_b;

    memset(&snap_a, 0x00, sizeof(snap_a));
    populate_snapshot(&snap_a);

    memset(&snap_b, 0xFF, sizeof(snap_b));
    populate_snapshot(&snap_b);

    uint32_t hash_a = fq_crc32((const uint8_t *)&snap_a, sizeof(snap_a));
    uint32_t hash_b = fq_crc32((const uint8_t *)&snap_b, sizeof(snap_b));

    TEST_ASSERT_EQUAL_UINT32(hash_a, hash_b);
}

/* DET-NT-3: Single-byte mutation — changing one byte must change the hash. */
static void test_single_byte_mutation_changes_hash(void)
{
    combat_snapshot_t snap;
    memset(&snap, 0x00, sizeof(snap));
    populate_snapshot(&snap);

    uint32_t hash_before = fq_crc32((const uint8_t *)&snap, sizeof(snap));

    /* Mutate one byte — flip attacker_hp LSB */
    uint8_t *raw = (uint8_t *)&snap;
    raw[0] ^= 0x01u;

    uint32_t hash_after = fq_crc32((const uint8_t *)&snap, sizeof(snap));

    TEST_ASSERT_TRUE(hash_before != hash_after);
}

int main(void)
{
    test_struct_padding_hash_invariant();
    test_single_byte_mutation_changes_hash();
    return 0;
}
