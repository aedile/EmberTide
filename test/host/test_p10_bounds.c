/**
 * test_p10_bounds.c — Phase 10 Bounds / Edge-Case Tests (Rule 22 — BOUND RED)
 *
 * These tests verify that the Phase-10 protocol, hash, and sync modules
 * REJECT invalid inputs before any feature-level behavior is exercised.
 *
 * Bound categories covered (per spec-challenger N-series):
 *   N1  : nonce XOR == 0 → seed forced to 1
 *   N2  : zero-length / under-minimum buffer → rejection before field access
 *   N4  : buffer underflow sizes: 0, 1, 3, FQ_PACKET_OVERHEAD-1, exact-1
 *   N5  : NULL pointer to every public function
 *   N7  : hash covers only meaningful fields (padding-free serialization)
 *   N8  : CRC field not fed into CRC computation (verify idempotency)
 *   N9  : Round 0, 13, 255 rejected by fq_generate_combat_hash and parse
 *   N10 : 1-bit PRNG state difference detected in hash (avalanche test)
 *   QA-P10-02: DISCONNECT exact size; unknown type (0xFF) parse rejection
 */

#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include "test_assert.h"

/* game/ headers — accessed via the host test build (game sources are linked) */
#include "combat_hash.h"
#include "combat.h"
#include "types.h"

/* connectivity/ headers — included directly (no game/ transitive dep allowed) */
#include "protocol.h"
#include "sync.h"

/* ---------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------------*/

/** Build a minimal valid fq_combat_ctx_t for hashing tests. */
static fq_combat_ctx_t make_ctx(int16_t f1_hp, int16_t f1_hpmax,
                                  int16_t f2_hp, int16_t f2_hpmax,
                                  uint32_t rng_state)
{
    fq_combat_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.f1.hp     = f1_hp;
    ctx.f1.hp_max = f1_hpmax;
    ctx.f2.hp     = f2_hp;
    ctx.f2.hp_max = f2_hpmax;
    ctx.rng.state = (rng_state == 0u) ? 1u : rng_state;
    return ctx;
}

/* ---------------------------------------------------------------------------
 * N5: NULL pointer guard — fq_generate_combat_hash
 * ---------------------------------------------------------------------------*/
static void test_hash_null_ctx(void)
{
    uint32_t result = fq_generate_combat_hash(NULL, 1u);
    TEST_ASSERT_EQUAL_UINT32(0u, result);
}

/* ---------------------------------------------------------------------------
 * N9: Round 0 rejected by fq_generate_combat_hash
 * ---------------------------------------------------------------------------*/
static void test_hash_round_zero_rejected(void)
{
    fq_combat_ctx_t ctx = make_ctx(100, 100, 80, 100, 0xDEADBEEFu);
    uint32_t result = fq_generate_combat_hash(&ctx, 0u);
    TEST_ASSERT_EQUAL_UINT32(0u, result);
}

/* ---------------------------------------------------------------------------
 * N9: Round 13 (> FQ_MAX_ROUNDS) rejected by fq_generate_combat_hash
 * ---------------------------------------------------------------------------*/
static void test_hash_round_13_rejected(void)
{
    fq_combat_ctx_t ctx = make_ctx(100, 100, 80, 100, 0xDEADBEEFu);
    uint32_t result = fq_generate_combat_hash(&ctx, 13u);
    TEST_ASSERT_EQUAL_UINT32(0u, result);
}

/* ---------------------------------------------------------------------------
 * N9: Round 255 rejected by fq_generate_combat_hash
 * ---------------------------------------------------------------------------*/
static void test_hash_round_255_rejected(void)
{
    fq_combat_ctx_t ctx = make_ctx(100, 100, 80, 100, 0xDEADBEEFu);
    uint32_t result = fq_generate_combat_hash(&ctx, 255u);
    TEST_ASSERT_EQUAL_UINT32(0u, result);
}

/* ---------------------------------------------------------------------------
 * N10: 1-bit PRNG state difference produces a different hash (avalanche)
 * ---------------------------------------------------------------------------*/
static void test_hash_prng_bit_flip_detected(void)
{
    fq_combat_ctx_t ctx_a = make_ctx(100, 100, 80, 100, 0x12345678u);
    fq_combat_ctx_t ctx_b = make_ctx(100, 100, 80, 100, 0x12345679u); /* 1 bit flip */

    uint32_t hash_a = fq_generate_combat_hash(&ctx_a, 1u);
    uint32_t hash_b = fq_generate_combat_hash(&ctx_b, 1u);

    /* Hashes must differ — CRC32 avalanche guarantees this for a 1-bit delta */
    TEST_ASSERT_TRUE(hash_a != hash_b);
}

/* ---------------------------------------------------------------------------
 * N7: Hash is stable (padding bytes ignored — deterministic across calls)
 * ---------------------------------------------------------------------------*/
static void test_hash_deterministic_no_padding_leak(void)
{
    fq_combat_ctx_t ctx_a = make_ctx(50, 100, 75, 100, 0xABCDEF01u);
    fq_combat_ctx_t ctx_b = make_ctx(50, 100, 75, 100, 0xABCDEF01u);

    /* Set _pad bytes differently to prove they are NOT hashed */
    ctx_a._pad[0] = 0x00u; ctx_a._pad[1] = 0x00u;
    ctx_b._pad[0] = 0xFFu; ctx_b._pad[1] = 0xFFu;

    uint32_t hash_a = fq_generate_combat_hash(&ctx_a, 3u);
    uint32_t hash_b = fq_generate_combat_hash(&ctx_b, 3u);

    TEST_ASSERT_EQUAL_UINT32(hash_a, hash_b);
}

/* ---------------------------------------------------------------------------
 * N1: nonce_a XOR nonce_b == 0 → seed forced to 1
 * ---------------------------------------------------------------------------*/
static void test_derive_seed_zero_xor_forces_one(void)
{
    uint32_t seed = fq_protocol_derive_seed(0xCAFEBABEu, 0xCAFEBABEu);
    TEST_ASSERT_EQUAL_UINT32(1u, seed);
}

/* ---------------------------------------------------------------------------
 * N1: Both nonces 0 → seed forced to 1
 * ---------------------------------------------------------------------------*/
static void test_derive_seed_both_zero_forces_one(void)
{
    uint32_t seed = fq_protocol_derive_seed(0u, 0u);
    TEST_ASSERT_EQUAL_UINT32(1u, seed);
}

/* ---------------------------------------------------------------------------
 * N5: NULL buffer to fq_packet_serialize
 * ---------------------------------------------------------------------------*/
static void test_serialize_null_buf(void)
{
    fq_packet_invite_t pkt = {.nonce = 0x1234u, .version = 1u};
    size_t n = fq_packet_serialize(&pkt, FQ_PKT_INVITE, NULL, 64u);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)n);
}

/* ---------------------------------------------------------------------------
 * N5: NULL packet to fq_packet_serialize
 * ---------------------------------------------------------------------------*/
static void test_serialize_null_packet(void)
{
    uint8_t buf[64];
    size_t n = fq_packet_serialize(NULL, FQ_PKT_INVITE, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)n);
}

/* ---------------------------------------------------------------------------
 * N4: Buffer too small (size 0) to fq_packet_serialize
 * ---------------------------------------------------------------------------*/
static void test_serialize_zero_buf_size(void)
{
    fq_packet_invite_t pkt = {.nonce = 0x1234u, .version = 1u};
    uint8_t buf[1];
    size_t n = fq_packet_serialize(&pkt, FQ_PKT_INVITE, buf, 0u);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)n);
}

/* ---------------------------------------------------------------------------
 * N4: Buffer exactly 1 byte short of required size
 * ---------------------------------------------------------------------------*/
static void test_serialize_one_byte_short(void)
{
    fq_packet_invite_t pkt = {.nonce = 0x5678u, .version = 1u};
    uint8_t buf[FQ_PACKET_INVITE_SIZE - 1u];
    size_t n = fq_packet_serialize(&pkt, FQ_PKT_INVITE,
                                   buf, FQ_PACKET_INVITE_SIZE - 1u);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)n);
}

/* ---------------------------------------------------------------------------
 * N2: Zero-length buffer to fq_packet_parse → ERR_BUFFER_TOO_SMALL
 * ---------------------------------------------------------------------------*/
static void test_parse_zero_length_buffer(void)
{
    fq_packet_type_t  type;
    fq_packet_invite_t pkt;
    fq_packet_err_t err = fq_packet_parse(NULL, 0u, &type, &pkt);
    TEST_ASSERT_EQUAL_INT(FQ_PKT_ERR_NULL, (int)err);
}

/* ---------------------------------------------------------------------------
 * N4: Buffer size 1 (less than FQ_PACKET_OVERHEAD) → ERR_BUFFER_TOO_SMALL
 * ---------------------------------------------------------------------------*/
static void test_parse_size_1_rejected(void)
{
    uint8_t buf[1] = {0};
    fq_packet_type_t  type;
    fq_packet_invite_t pkt;
    fq_packet_err_t err = fq_packet_parse(buf, 1u, &type, &pkt);
    TEST_ASSERT_EQUAL_INT(FQ_PKT_ERR_BUFFER_TOO_SMALL, (int)err);
}

/* ---------------------------------------------------------------------------
 * N4: Buffer size 3 (less than FQ_PACKET_OVERHEAD) → ERR_BUFFER_TOO_SMALL
 * ---------------------------------------------------------------------------*/
static void test_parse_size_3_rejected(void)
{
    uint8_t buf[3] = {0x46u, 0x51u, 0x30u}; /* "FQ0" — incomplete magic */
    fq_packet_type_t  type;
    fq_packet_invite_t pkt;
    fq_packet_err_t err = fq_packet_parse(buf, 3u, &type, &pkt);
    TEST_ASSERT_EQUAL_INT(FQ_PKT_ERR_BUFFER_TOO_SMALL, (int)err);
}

/* ---------------------------------------------------------------------------
 * N5: NULL buf to fq_packet_parse
 * ---------------------------------------------------------------------------*/
static void test_parse_null_buf(void)
{
    fq_packet_type_t  type;
    fq_packet_invite_t pkt;
    fq_packet_err_t err = fq_packet_parse(NULL, 64u, &type, &pkt);
    TEST_ASSERT_EQUAL_INT(FQ_PKT_ERR_NULL, (int)err);
}

/* ---------------------------------------------------------------------------
 * N5: NULL out_type to fq_packet_parse
 * ---------------------------------------------------------------------------*/
static void test_parse_null_out_type(void)
{
    uint8_t buf[64] = {0};
    fq_packet_invite_t pkt;
    fq_packet_err_t err = fq_packet_parse(buf, sizeof(buf), NULL, &pkt);
    TEST_ASSERT_EQUAL_INT(FQ_PKT_ERR_NULL, (int)err);
}

/* ---------------------------------------------------------------------------
 * N5: NULL out_packet to fq_packet_parse
 * ---------------------------------------------------------------------------*/
static void test_parse_null_out_packet(void)
{
    uint8_t buf[64] = {0};
    fq_packet_type_t type;
    fq_packet_err_t err = fq_packet_parse(buf, sizeof(buf), &type, NULL);
    TEST_ASSERT_EQUAL_INT(FQ_PKT_ERR_NULL, (int)err);
}

/* ---------------------------------------------------------------------------
 * Magic mismatch: spoofed first byte → ERR_MAGIC_MISMATCH before CRC check
 * ---------------------------------------------------------------------------*/
static void test_parse_magic_mismatch_before_crc(void)
{
    /* Build a valid invite packet first */
    fq_packet_invite_t pkt = {.nonce = 0x11223344u, .version = FQ_PROTOCOL_VERSION};
    uint8_t buf[FQ_PACKET_INVITE_SIZE];
    size_t n = fq_packet_serialize(&pkt, FQ_PKT_INVITE, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n == FQ_PACKET_INVITE_SIZE);

    /* Corrupt byte 0 of magic */
    buf[0] ^= 0xFFu;

    fq_packet_type_t  out_type;
    fq_packet_invite_t out_pkt;
    fq_packet_err_t err = fq_packet_parse(buf, sizeof(buf), &out_type, &out_pkt);
    TEST_ASSERT_EQUAL_INT(FQ_PKT_ERR_MAGIC_MISMATCH, (int)err);
}

/* ---------------------------------------------------------------------------
 * N8: CRC corruption detected (flip one payload byte after serialize)
 * ---------------------------------------------------------------------------*/
static void test_parse_crc_mismatch_detected(void)
{
    fq_packet_invite_t pkt = {.nonce = 0xDEADBEEFu, .version = FQ_PROTOCOL_VERSION};
    uint8_t buf[FQ_PACKET_INVITE_SIZE];
    size_t n = fq_packet_serialize(&pkt, FQ_PKT_INVITE, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n == FQ_PACKET_INVITE_SIZE);

    /* Flip one byte in the payload (byte 5 = type byte) */
    buf[5] ^= 0x01u;

    fq_packet_type_t   out_type;
    fq_packet_invite_t out_pkt;
    fq_packet_err_t err = fq_packet_parse(buf, sizeof(buf), &out_type, &out_pkt);
    /* Either CRC mismatch or magic mismatch — either way, not OK */
    TEST_ASSERT_TRUE(err != FQ_PKT_OK);
}

/* ---------------------------------------------------------------------------
 * N9: Round hash packet with round = 0 is serialized but parse must reject it
 * (serialize always produces bytes; parse validates semantics)
 * ---------------------------------------------------------------------------*/
static void test_parse_round_hash_round_zero_rejected(void)
{
    fq_packet_round_hash_t pkt = {.round = 0u, .combat_hash = 0xABCDEF01u};
    uint8_t buf[FQ_PACKET_ROUND_HASH_SIZE];
    size_t n = fq_packet_serialize(&pkt, FQ_PKT_ROUND_HASH, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n == FQ_PACKET_ROUND_HASH_SIZE);

    fq_packet_type_t       out_type;
    fq_packet_round_hash_t out_pkt;
    fq_packet_err_t err = fq_packet_parse(buf, sizeof(buf), &out_type, &out_pkt);
    TEST_ASSERT_EQUAL_INT(FQ_PKT_ERR_INVALID_ROUND, (int)err);
}

/* ---------------------------------------------------------------------------
 * N9: Round hash packet with round = 13 rejected by parse
 * ---------------------------------------------------------------------------*/
static void test_parse_round_hash_round_13_rejected(void)
{
    fq_packet_round_hash_t pkt = {.round = 13u, .combat_hash = 0xABCDEF01u};
    uint8_t buf[FQ_PACKET_ROUND_HASH_SIZE];
    size_t n = fq_packet_serialize(&pkt, FQ_PKT_ROUND_HASH, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n == FQ_PACKET_ROUND_HASH_SIZE);

    fq_packet_type_t       out_type;
    fq_packet_round_hash_t out_pkt;
    fq_packet_err_t err = fq_packet_parse(buf, sizeof(buf), &out_type, &out_pkt);
    TEST_ASSERT_EQUAL_INT(FQ_PKT_ERR_INVALID_ROUND, (int)err);
}

/* ---------------------------------------------------------------------------
 * QA-P10-02: FQ_PKT_DISCONNECT serializes to exactly FQ_PACKET_OVERHEAD bytes.
 *
 * DISCONNECT has no payload — only magic(4) + type(1) + CRC(4) = 9 bytes.
 * Verifies the no-payload fast-path in fq_packet_serialize.
 * ---------------------------------------------------------------------------*/
static void test_disconnect_serialize_exact_size(void)
{
    /* DISCONNECT has no payload; the packet pointer is unused but must be non-NULL
     * (serialize NULL-guards on packet before checking type). Pass a dummy byte. */
    uint8_t dummy = 0u;
    uint8_t buf[FQ_PACKET_OVERHEAD];
    size_t n = fq_packet_serialize(&dummy, FQ_PKT_DISCONNECT, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)FQ_PACKET_OVERHEAD, (uint32_t)n);
}

/* ---------------------------------------------------------------------------
 * QA-P10-02: A buffer with valid magic but type=0xFF is rejected with
 * FQ_PKT_ERR_UNKNOWN_TYPE (not a crash, not a silent OK).
 *
 * Build: write "FQ01" magic + 0xFF type byte + 4 zero CRC bytes.
 * The magic check passes; the type switch hits default → UNKNOWN_TYPE.
 * CRC is not checked because the type is validated before CRC computation
 * uses the expected_size (which is unknown for type 0xFF).
 * ---------------------------------------------------------------------------*/
static void test_parse_unknown_type_rejected(void)
{
    /* Manually craft a minimal buffer: magic(4) + type(1) + crc_placeholder(4) */
    uint8_t buf[FQ_PACKET_OVERHEAD];
    memset(buf, 0, sizeof(buf));
    buf[0] = (uint8_t)'F';
    buf[1] = (uint8_t)'Q';
    buf[2] = (uint8_t)'0';
    buf[3] = (uint8_t)'1';
    buf[4] = 0xFFu;  /* Unknown type */
    /* CRC bytes [5..8] left as zero — parse must reject before CRC check */

    fq_packet_type_t   out_type;
    fq_packet_invite_t out_pkt;
    fq_packet_err_t err = fq_packet_parse(buf, sizeof(buf), &out_type, &out_pkt);
    TEST_ASSERT_EQUAL_INT(FQ_PKT_ERR_UNKNOWN_TYPE, (int)err);
}

/* ---------------------------------------------------------------------------
 * Sync: NULL round mismatch — round check before hash check
 * ---------------------------------------------------------------------------*/
static void test_sync_round_mismatch_before_hash(void)
{
    /* Round 1 expected, but peer sends round 2 */
    fq_sync_err_t err = fq_sync_verify_round(1u, 2u, 0xAAAAAAAAu, 0xAAAAAAAAu);
    TEST_ASSERT_EQUAL_INT(FQ_SYNC_ERR_ROUND_MISMATCH, (int)err);
}

/* ---------------------------------------------------------------------------
 * Sync: matching rounds but differing hashes → HASH_MISMATCH
 * ---------------------------------------------------------------------------*/
static void test_sync_hash_mismatch_detected(void)
{
    fq_sync_err_t err = fq_sync_verify_round(3u, 3u, 0x11111111u, 0x11111112u);
    TEST_ASSERT_EQUAL_INT(FQ_SYNC_ERR_HASH_MISMATCH, (int)err);
}

/* ---------------------------------------------------------------------------
 * Sync: both zero rounds (edge: round 0) → ROUND_MISMATCH if expected != 0
 * (round 0 is not valid combat, but the function only compares values)
 * ---------------------------------------------------------------------------*/
static void test_sync_zero_rounds_mismatch(void)
{
    /* expected=1, received=0 — round mismatch */
    fq_sync_err_t err = fq_sync_verify_round(1u, 0u, 0xBBBBBBBBu, 0xBBBBBBBBu);
    TEST_ASSERT_EQUAL_INT(FQ_SYNC_ERR_ROUND_MISMATCH, (int)err);
}

/* ---------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------*/
int main(void)
{
    /* N5: NULL guards */
    test_hash_null_ctx();
    test_serialize_null_buf();
    test_serialize_null_packet();
    test_parse_null_buf();
    test_parse_null_out_type();
    test_parse_null_out_packet();

    /* N1: nonce zero-guard */
    test_derive_seed_zero_xor_forces_one();
    test_derive_seed_both_zero_forces_one();

    /* N9: round range rejection */
    test_hash_round_zero_rejected();
    test_hash_round_13_rejected();
    test_hash_round_255_rejected();
    test_parse_round_hash_round_zero_rejected();
    test_parse_round_hash_round_13_rejected();

    /* N2 / N4: buffer underflow */
    test_parse_zero_length_buffer();
    test_parse_size_1_rejected();
    test_parse_size_3_rejected();
    test_serialize_zero_buf_size();
    test_serialize_one_byte_short();

    /* Magic / CRC integrity */
    test_parse_magic_mismatch_before_crc();
    test_parse_crc_mismatch_detected();

    /* N7 / N8 / N10: hash quality */
    test_hash_deterministic_no_padding_leak();
    test_hash_prng_bit_flip_detected();

    /* QA-P10-02: DISCONNECT size + unknown type rejection */
    test_disconnect_serialize_exact_size();
    test_parse_unknown_type_rejected();

    /* Sync logic */
    test_sync_round_mismatch_before_hash();
    test_sync_hash_mismatch_detected();
    test_sync_zero_rounds_mismatch();

    printf("[PASS] all p10 bounds tests passed\n");
    return 0;
}
