/**
 * test_p10_combat_sync.c — Phase 10 Feature Tests: Combat Hash & Sync
 *
 * Tests fq_generate_combat_hash() and fq_sync_verify_round().
 *
 * Key scenarios:
 *   - Two independent combat contexts with same seed produce identical hashes
 *   - One extra PRNG advance on context B → hash differs → sync detects desync
 *   - Round number is part of the hash (different round → different hash)
 *   - HP change is part of the hash
 *   - fq_sync_verify_round happy-path (matching rounds and hashes)
 */

#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "test_assert.h"

/* game/ headers */
#include "combat_hash.h"
#include "combat.h"
#include "prng.h"
#include "types.h"

/* connectivity/ headers */
#include "sync.h"

/* ---------------------------------------------------------------------------
 * Pinned CRC32 regression constant (QA-P10-01).
 *
 * Serialized buffer for: round=1, f1.hp=100, f2.hp=80, f1.hp_max=100,
 *                        f2.hp_max=100, rng.state=1
 * Bytes: 01 64 00 50 00 64 00 64 00 01 00 00 00  (13 bytes LE)
 * CRC32 (IEEE 802.3 reflected, poly 0xEDB88320): 0x3FDACA50
 *
 * This constant must never be changed without a Constitution-level review.
 * If this test fails after a refactor, the serialization format has changed
 * and all on-device combat state hashes will desync from each other.
 * ---------------------------------------------------------------------------*/
#define KNOWN_HASH_PINNED  0x3FDACA50u

/* ---------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------------*/

/** Build a minimal fq_combat_ctx_t. rng_state=0 → forced to 1 by PRNG guard. */
static fq_combat_ctx_t make_ctx(int16_t f1_hp, int16_t f1_max,
                                  int16_t f2_hp, int16_t f2_max,
                                  uint32_t rng_state)
{
    fq_combat_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.f1.hp     = f1_hp;
    ctx.f1.hp_max = f1_max;
    ctx.f2.hp     = f2_hp;
    ctx.f2.hp_max = f2_max;
    ctx.rng.state = (rng_state == 0u) ? 1u : rng_state;
    return ctx;
}

/* ---------------------------------------------------------------------------
 * Identical contexts → identical hashes
 * ---------------------------------------------------------------------------*/
static void test_identical_contexts_produce_identical_hashes(void)
{
    fq_combat_ctx_t a = make_ctx(100, 100, 80, 100, 0xDEADBEEFu);
    fq_combat_ctx_t b = make_ctx(100, 100, 80, 100, 0xDEADBEEFu);

    uint32_t ha = fq_generate_combat_hash(&a, 1u);
    uint32_t hb = fq_generate_combat_hash(&b, 1u);

    TEST_ASSERT_EQUAL_UINT32(ha, hb);
    TEST_ASSERT_TRUE(ha != 0u);  /* Valid contexts produce non-zero hashes */
}

/* ---------------------------------------------------------------------------
 * One extra PRNG advance on context B → hash diverges
 * ---------------------------------------------------------------------------*/
static void test_prng_advance_causes_hash_divergence(void)
{
    fq_combat_ctx_t ctx_a = make_ctx(90, 100, 90, 100, 0x12345678u);
    fq_combat_ctx_t ctx_b = make_ctx(90, 100, 90, 100, 0x12345678u);

    /* Advance B's PRNG by exactly one call */
    fq_prng_next(&ctx_b.rng);

    uint32_t ha = fq_generate_combat_hash(&ctx_a, 2u);
    uint32_t hb = fq_generate_combat_hash(&ctx_b, 2u);

    TEST_ASSERT_TRUE(ha != hb);
}

/* ---------------------------------------------------------------------------
 * Round number is part of the hash (same ctx, different round → different hash)
 * ---------------------------------------------------------------------------*/
static void test_round_number_affects_hash(void)
{
    fq_combat_ctx_t ctx = make_ctx(75, 100, 60, 100, 0xABCDEF01u);

    uint32_t h1 = fq_generate_combat_hash(&ctx, 1u);
    uint32_t h2 = fq_generate_combat_hash(&ctx, 2u);

    TEST_ASSERT_TRUE(h1 != h2);
}

/* ---------------------------------------------------------------------------
 * HP change causes hash divergence (catching cheated HP modification)
 * ---------------------------------------------------------------------------*/
static void test_hp_change_causes_hash_divergence(void)
{
    fq_combat_ctx_t honest = make_ctx(75, 100, 60, 100, 0x55AA55AAu);
    fq_combat_ctx_t cheated = make_ctx(100, 100, 60, 100, 0x55AA55AAu); /* F1 HP cheated */

    uint32_t h_honest  = fq_generate_combat_hash(&honest,  3u);
    uint32_t h_cheated = fq_generate_combat_hash(&cheated, 3u);

    TEST_ASSERT_TRUE(h_honest != h_cheated);
}

/* ---------------------------------------------------------------------------
 * HP max change causes hash divergence
 * ---------------------------------------------------------------------------*/
static void test_hp_max_change_causes_hash_divergence(void)
{
    fq_combat_ctx_t a = make_ctx(80, 100, 70, 100, 0xBEEFCAFEu);
    fq_combat_ctx_t b = make_ctx(80, 200, 70, 100, 0xBEEFCAFEu); /* F1 hp_max cheated */

    uint32_t ha = fq_generate_combat_hash(&a, 1u);
    uint32_t hb = fq_generate_combat_hash(&b, 1u);

    TEST_ASSERT_TRUE(ha != hb);
}

/* ---------------------------------------------------------------------------
 * Hash is deterministic (calling twice returns same value, ctx not mutated)
 * ---------------------------------------------------------------------------*/
static void test_hash_is_deterministic(void)
{
    fq_combat_ctx_t ctx = make_ctx(50, 100, 50, 100, 0x87654321u);

    uint32_t h1 = fq_generate_combat_hash(&ctx, 5u);
    uint32_t h2 = fq_generate_combat_hash(&ctx, 5u);

    TEST_ASSERT_EQUAL_UINT32(h1, h2);
}

/* ---------------------------------------------------------------------------
 * fq_sync_verify_round: happy path — matching rounds and hashes
 * ---------------------------------------------------------------------------*/
static void test_sync_ok_when_both_match(void)
{
    fq_combat_ctx_t ctx = make_ctx(70, 100, 55, 100, 0xFAFAFAFAu);
    uint32_t hash = fq_generate_combat_hash(&ctx, 4u);

    fq_sync_err_t err = fq_sync_verify_round(4u, 4u, hash, hash);
    TEST_ASSERT_EQUAL_INT(FQ_SYNC_OK, (int)err);
}

/* ---------------------------------------------------------------------------
 * Simulate the spoofed round roll-forward attack:
 * Device B executes Round 2 locally and sends the R2 hash when A expects R1.
 * ---------------------------------------------------------------------------*/
static void test_sync_spoofed_round_rollforward_detected(void)
{
    fq_combat_ctx_t ctx_a = make_ctx(80, 100, 80, 100, 0x11223344u);
    fq_combat_ctx_t ctx_b = make_ctx(80, 100, 80, 100, 0x11223344u);

    /* Both complete round 1 — advance B's PRNG to simulate B executing round 2 */
    fq_prng_next(&ctx_b.rng);

    uint32_t hash_a_r1 = fq_generate_combat_hash(&ctx_a, 1u);
    uint32_t hash_b_r2 = fq_generate_combat_hash(&ctx_b, 2u);

    /* A expects round 1, B sends round 2 → round mismatch detected first */
    fq_sync_err_t err = fq_sync_verify_round(1u, 2u, hash_a_r1, hash_b_r2);
    TEST_ASSERT_EQUAL_INT(FQ_SYNC_ERR_ROUND_MISMATCH, (int)err);
}

/* ---------------------------------------------------------------------------
 * Same round, different hashes → hash mismatch
 * ---------------------------------------------------------------------------*/
static void test_sync_same_round_diverged_hashes(void)
{
    fq_combat_ctx_t ctx_a = make_ctx(65, 100, 50, 100, 0xAAAAAAAAu);
    fq_combat_ctx_t ctx_b = make_ctx(65, 100, 51, 100, 0xAAAAAAAAu); /* 1 HP diff */

    uint32_t ha = fq_generate_combat_hash(&ctx_a, 6u);
    uint32_t hb = fq_generate_combat_hash(&ctx_b, 6u);

    fq_sync_err_t err = fq_sync_verify_round(6u, 6u, ha, hb);
    TEST_ASSERT_EQUAL_INT(FQ_SYNC_ERR_HASH_MISMATCH, (int)err);
}

/* ---------------------------------------------------------------------------
 * Hash produces a known fixed output for a known fixed input (QA-P10-01).
 *
 * Serialized buffer:
 *   [0]     round=1          → 0x01
 *   [1..2]  f1.hp=100        → 0x64 0x00
 *   [3..4]  f2.hp=80         → 0x50 0x00
 *   [5..6]  f1.hp_max=100    → 0x64 0x00
 *   [7..8]  f2.hp_max=100    → 0x64 0x00
 *   [9..12] rng.state=1      → 0x01 0x00 0x00 0x00
 *
 * Expected CRC32 (IEEE 802.3, poly 0xEDB88320): KNOWN_HASH_PINNED = 0x3FDACA50
 *
 * If this test fails, the serialization format changed and on-device hashes
 * will desync. This is a Constitution Priority-0 determinism violation.
 * ---------------------------------------------------------------------------*/
static void test_hash_known_fixed_value(void)
{
    fq_combat_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.f1.hp     = 100;
    ctx.f1.hp_max = 100;
    ctx.f2.hp     = 80;
    ctx.f2.hp_max = 100;
    ctx.rng.state = 1u;

    uint32_t h = fq_generate_combat_hash(&ctx, 1u);

    /* Pinned regression value — must never change without a Constitution review. */
    TEST_ASSERT_EQUAL_UINT32(KNOWN_HASH_PINNED, h);
}

/* ---------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------*/
int main(void)
{
    test_identical_contexts_produce_identical_hashes();
    test_prng_advance_causes_hash_divergence();
    test_round_number_affects_hash();
    test_hp_change_causes_hash_divergence();
    test_hp_max_change_causes_hash_divergence();
    test_hash_is_deterministic();
    test_sync_ok_when_both_match();
    test_sync_spoofed_round_rollforward_detected();
    test_sync_same_round_diverged_hashes();
    test_hash_known_fixed_value();

    printf("[PASS] all p10 combat sync feature tests passed\n");
    return 0;
}
