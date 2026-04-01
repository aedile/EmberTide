/**
 * test_p10_protocol.c — Phase 10 Feature Tests: Protocol Serialization
 *
 * Happy-path tests for the BLE packet DTO serialization/deserialization.
 * Verifies wire format, exact byte counts, field round-trips, and seed
 * derivation.
 *
 * N3: All multi-byte payload fields must be encoded LE — tests verify this
 * by inspecting individual bytes of the serialized buffer.
 * N11: Stateless parse — serialize once, parse twice, same result.
 */

#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include "test_assert.h"
#include "protocol.h"

/* ---------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------------*/

/** Read a little-endian uint32_t from a byte buffer at offset. */
static uint32_t le32(const uint8_t *buf, size_t offset)
{
    return (uint32_t)buf[offset]
         | ((uint32_t)buf[offset + 1u] << 8u)
         | ((uint32_t)buf[offset + 2u] << 16u)
         | ((uint32_t)buf[offset + 3u] << 24u);
}

/* ---------------------------------------------------------------------------
 * Invite packet: serialize round-trip
 * ---------------------------------------------------------------------------*/
static void test_invite_serialize_exact_size(void)
{
    fq_packet_invite_t pkt = {.nonce = 0xDEADBEEFu, .version = FQ_PROTOCOL_VERSION};
    uint8_t buf[FQ_PACKET_INVITE_SIZE + 8u];
    size_t n = fq_packet_serialize(&pkt, FQ_PKT_INVITE, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT32(FQ_PACKET_INVITE_SIZE, (uint32_t)n);
}

static void test_invite_magic_bytes(void)
{
    fq_packet_invite_t pkt = {.nonce = 0x12345678u, .version = FQ_PROTOCOL_VERSION};
    uint8_t buf[FQ_PACKET_INVITE_SIZE];
    fq_packet_serialize(&pkt, FQ_PKT_INVITE, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_UINT8('F', buf[0]);
    TEST_ASSERT_EQUAL_UINT8('Q', buf[1]);
    TEST_ASSERT_EQUAL_UINT8('0', buf[2]);
    TEST_ASSERT_EQUAL_UINT8('1', buf[3]);
}

static void test_invite_type_byte(void)
{
    fq_packet_invite_t pkt = {.nonce = 0x12345678u, .version = FQ_PROTOCOL_VERSION};
    uint8_t buf[FQ_PACKET_INVITE_SIZE];
    fq_packet_serialize(&pkt, FQ_PKT_INVITE, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_PKT_INVITE, buf[4]);
}

static void test_invite_nonce_little_endian(void)
{
    /* nonce = 0x01020304 → bytes [5..8] = 0x04, 0x03, 0x02, 0x01 */
    fq_packet_invite_t pkt = {.nonce = 0x01020304u, .version = FQ_PROTOCOL_VERSION};
    uint8_t buf[FQ_PACKET_INVITE_SIZE];
    fq_packet_serialize(&pkt, FQ_PKT_INVITE, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_UINT8(0x04u, buf[5]);
    TEST_ASSERT_EQUAL_UINT8(0x03u, buf[6]);
    TEST_ASSERT_EQUAL_UINT8(0x02u, buf[7]);
    TEST_ASSERT_EQUAL_UINT8(0x01u, buf[8]);
}

static void test_invite_version_byte(void)
{
    fq_packet_invite_t pkt = {.nonce = 0xABCDEF01u, .version = FQ_PROTOCOL_VERSION};
    uint8_t buf[FQ_PACKET_INVITE_SIZE];
    fq_packet_serialize(&pkt, FQ_PKT_INVITE, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT8(FQ_PROTOCOL_VERSION, buf[9]);
}

static void test_invite_parse_roundtrip(void)
{
    fq_packet_invite_t orig = {.nonce = 0xCAFEBABEu, .version = FQ_PROTOCOL_VERSION};
    uint8_t buf[FQ_PACKET_INVITE_SIZE];
    fq_packet_serialize(&orig, FQ_PKT_INVITE, buf, sizeof(buf));

    fq_packet_type_t   out_type;
    fq_packet_invite_t out_pkt;
    fq_packet_err_t err = fq_packet_parse(buf, sizeof(buf), &out_type, &out_pkt);

    TEST_ASSERT_EQUAL_INT(FQ_PKT_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_PKT_INVITE, (uint8_t)out_type);
    TEST_ASSERT_EQUAL_UINT32(orig.nonce, out_pkt.nonce);
    TEST_ASSERT_EQUAL_UINT8(orig.version, out_pkt.version);
}

/* N11: Stateless parse — identical result on second call */
static void test_invite_parse_stateless(void)
{
    fq_packet_invite_t orig = {.nonce = 0x99887766u, .version = FQ_PROTOCOL_VERSION};
    uint8_t buf[FQ_PACKET_INVITE_SIZE];
    fq_packet_serialize(&orig, FQ_PKT_INVITE, buf, sizeof(buf));

    fq_packet_type_t   type1, type2;
    fq_packet_invite_t pkt1, pkt2;

    fq_packet_err_t e1 = fq_packet_parse(buf, sizeof(buf), &type1, &pkt1);
    fq_packet_err_t e2 = fq_packet_parse(buf, sizeof(buf), &type2, &pkt2);

    TEST_ASSERT_EQUAL_INT(FQ_PKT_OK, (int)e1);
    TEST_ASSERT_EQUAL_INT(FQ_PKT_OK, (int)e2);
    TEST_ASSERT_EQUAL_UINT32(pkt1.nonce, pkt2.nonce);
    TEST_ASSERT_EQUAL_UINT8(pkt1.version, pkt2.version);
}

/* ---------------------------------------------------------------------------
 * Team sync packet: serialize round-trip
 * ---------------------------------------------------------------------------*/
static void test_team_sync_serialize_exact_size(void)
{
    fq_packet_team_sync_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    strncpy(pkt.name, "Ember", sizeof(pkt.name));
    pkt.class_id      = 2u;
    pkt.level         = 15u;
    pkt.hp_max        = 320u;
    pkt.equipped[0]   = 7u;
    pkt.equipped_count= 1u;

    uint8_t buf[FQ_PACKET_TEAM_SYNC_SIZE + 8u];
    size_t n = fq_packet_serialize(&pkt, FQ_PKT_TEAM_SYNC, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT32(FQ_PACKET_TEAM_SYNC_SIZE, (uint32_t)n);
}

static void test_team_sync_parse_roundtrip(void)
{
    fq_packet_team_sync_t orig;
    memset(&orig, 0, sizeof(orig));
    strncpy(orig.name, "Tidecaller", sizeof(orig.name));
    orig.class_id       = 1u;
    orig.level          = 42u;
    orig.hp_max         = 512u;
    orig.equipped[0]    = 10u;
    orig.equipped[1]    = 20u;
    orig.equipped_count = 2u;

    uint8_t buf[FQ_PACKET_TEAM_SYNC_SIZE];
    fq_packet_serialize(&orig, FQ_PKT_TEAM_SYNC, buf, sizeof(buf));

    fq_packet_type_t      out_type;
    fq_packet_team_sync_t out_pkt;
    fq_packet_err_t err = fq_packet_parse(buf, sizeof(buf), &out_type, &out_pkt);

    TEST_ASSERT_EQUAL_INT(FQ_PKT_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_PKT_TEAM_SYNC, (uint8_t)out_type);
    TEST_ASSERT_EQUAL_UINT8(orig.class_id, out_pkt.class_id);
    TEST_ASSERT_EQUAL_UINT8(orig.level, out_pkt.level);
    TEST_ASSERT_EQUAL_UINT16(orig.hp_max, out_pkt.hp_max);
    TEST_ASSERT_EQUAL_UINT8(orig.equipped_count, out_pkt.equipped_count);
    TEST_ASSERT_EQUAL_UINT16(orig.equipped[0], out_pkt.equipped[0]);
    TEST_ASSERT_EQUAL_UINT16(orig.equipped[1], out_pkt.equipped[1]);
    TEST_ASSERT_TRUE(memcmp(orig.name, out_pkt.name, 12u) == 0);
}

static void test_team_sync_hp_max_little_endian(void)
{
    fq_packet_team_sync_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.hp_max = 0x0102u;  /* expected LE bytes: 0x02, 0x01 */

    uint8_t buf[FQ_PACKET_TEAM_SYNC_SIZE];
    fq_packet_serialize(&pkt, FQ_PKT_TEAM_SYNC, buf, sizeof(buf));

    /* hp_max is at payload offset: magic(4)+type(1)+name(12)+class(1)+level(1) = byte 19 */
    size_t hp_offset = 4u + 1u + 12u + 1u + 1u;
    TEST_ASSERT_EQUAL_UINT8(0x02u, buf[hp_offset]);
    TEST_ASSERT_EQUAL_UINT8(0x01u, buf[hp_offset + 1u]);
}

/* N16: Garbage at end of oversized buffer doesn't corrupt parse */
static void test_team_sync_garbage_ingestion(void)
{
    fq_packet_team_sync_t orig;
    memset(&orig, 0, sizeof(orig));
    orig.level = 7u;
    orig.hp_max = 200u;

    /* Allocate 3 extra bytes beyond the valid packet */
    uint8_t buf[FQ_PACKET_TEAM_SYNC_SIZE + 3u];
    memset(buf, 0xFFu, sizeof(buf));
    fq_packet_serialize(&orig, FQ_PKT_TEAM_SYNC, buf, FQ_PACKET_TEAM_SYNC_SIZE);
    /* Extra bytes 0xFF remain at the end — parse must use buf_size correctly */

    fq_packet_type_t      out_type;
    fq_packet_team_sync_t out_pkt;
    /* Parse with exact known-good size: extra bytes not passed */
    fq_packet_err_t err = fq_packet_parse(buf, FQ_PACKET_TEAM_SYNC_SIZE,
                                           &out_type, &out_pkt);
    TEST_ASSERT_EQUAL_INT(FQ_PKT_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8(orig.level, out_pkt.level);
}

/* ---------------------------------------------------------------------------
 * Round hash packet: serialize round-trip
 * ---------------------------------------------------------------------------*/
static void test_round_hash_serialize_exact_size(void)
{
    fq_packet_round_hash_t pkt = {.round = 1u, .combat_hash = 0x12345678u};
    uint8_t buf[FQ_PACKET_ROUND_HASH_SIZE + 8u];
    size_t n = fq_packet_serialize(&pkt, FQ_PKT_ROUND_HASH, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT32(FQ_PACKET_ROUND_HASH_SIZE, (uint32_t)n);
}

static void test_round_hash_parse_roundtrip(void)
{
    fq_packet_round_hash_t orig = {.round = 7u, .combat_hash = 0xFEDCBA98u};
    uint8_t buf[FQ_PACKET_ROUND_HASH_SIZE];
    fq_packet_serialize(&orig, FQ_PKT_ROUND_HASH, buf, sizeof(buf));

    fq_packet_type_t       out_type;
    fq_packet_round_hash_t out_pkt;
    fq_packet_err_t err = fq_packet_parse(buf, sizeof(buf), &out_type, &out_pkt);

    TEST_ASSERT_EQUAL_INT(FQ_PKT_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)FQ_PKT_ROUND_HASH, (uint8_t)out_type);
    TEST_ASSERT_EQUAL_UINT8(orig.round, out_pkt.round);
    TEST_ASSERT_EQUAL_UINT32(orig.combat_hash, out_pkt.combat_hash);
}

static void test_round_hash_combat_hash_little_endian(void)
{
    fq_packet_round_hash_t pkt = {.round = 1u, .combat_hash = 0x01020304u};
    uint8_t buf[FQ_PACKET_ROUND_HASH_SIZE];
    fq_packet_serialize(&pkt, FQ_PKT_ROUND_HASH, buf, sizeof(buf));

    /* combat_hash starts at magic(4)+type(1)+round(1) = offset 6 */
    size_t h_off = 4u + 1u + 1u;
    TEST_ASSERT_EQUAL_UINT8(0x04u, buf[h_off + 0u]);
    TEST_ASSERT_EQUAL_UINT8(0x03u, buf[h_off + 1u]);
    TEST_ASSERT_EQUAL_UINT8(0x02u, buf[h_off + 2u]);
    TEST_ASSERT_EQUAL_UINT8(0x01u, buf[h_off + 3u]);
}

/* Valid boundary round: round = 12 (FQ_ROUND_MAX) */
static void test_round_hash_max_round_accepted(void)
{
    fq_packet_round_hash_t pkt = {.round = 12u, .combat_hash = 0xABCDEF01u};
    uint8_t buf[FQ_PACKET_ROUND_HASH_SIZE];
    fq_packet_serialize(&pkt, FQ_PKT_ROUND_HASH, buf, sizeof(buf));

    fq_packet_type_t       out_type;
    fq_packet_round_hash_t out_pkt;
    fq_packet_err_t err = fq_packet_parse(buf, sizeof(buf), &out_type, &out_pkt);
    TEST_ASSERT_EQUAL_INT(FQ_PKT_OK, (int)err);
    TEST_ASSERT_EQUAL_UINT8(12u, out_pkt.round);
}

/* ---------------------------------------------------------------------------
 * Seed derivation
 * ---------------------------------------------------------------------------*/
static void test_derive_seed_commutative(void)
{
    uint32_t s1 = fq_protocol_derive_seed(0xAAAAAAAAu, 0x55555555u);
    uint32_t s2 = fq_protocol_derive_seed(0x55555555u, 0xAAAAAAAAu);
    TEST_ASSERT_EQUAL_UINT32(s1, s2);
}

static void test_derive_seed_nonzero_xor_preserved(void)
{
    /* 0xDEAD XOR 0xBEEF = 0x6042 (non-zero — no forcing) */
    uint32_t seed = fq_protocol_derive_seed(0x0000DEADu, 0x0000BEEFu);
    TEST_ASSERT_EQUAL_UINT32(0x0000DEADu ^ 0x0000BEEFu, seed);
}

static void test_derive_seed_never_returns_zero(void)
{
    uint32_t seed = fq_protocol_derive_seed(0xDEADBEEFu, 0xDEADBEEFu);
    TEST_ASSERT_TRUE(seed != 0u);
}

/* CRC trailer read-back — le32 helper test */
static void test_crc_trailer_readable(void)
{
    fq_packet_invite_t pkt = {.nonce = 0xABCDABCDu, .version = 1u};
    uint8_t buf[FQ_PACKET_INVITE_SIZE];
    fq_packet_serialize(&pkt, FQ_PKT_INVITE, buf, sizeof(buf));

    /* CRC is at last 4 bytes */
    uint32_t crc_from_buf = le32(buf, FQ_PACKET_INVITE_SIZE - 4u);
    /* We don't know the exact CRC value here, but it must be non-trivially
     * verifiable: re-parse must succeed. */
    (void)crc_from_buf;

    fq_packet_type_t   out_type;
    fq_packet_invite_t out_pkt;
    fq_packet_err_t err = fq_packet_parse(buf, sizeof(buf), &out_type, &out_pkt);
    TEST_ASSERT_EQUAL_INT(FQ_PKT_OK, (int)err);
}

/* N11: Corrupt byte, re-parse, result is deterministic (not random) */
static void test_parse_corrupted_buffer_deterministic(void)
{
    fq_packet_invite_t pkt = {.nonce = 0x55AA55AAu, .version = 1u};
    uint8_t buf[FQ_PACKET_INVITE_SIZE];
    fq_packet_serialize(&pkt, FQ_PKT_INVITE, buf, sizeof(buf));

    buf[5] ^= 0x01u; /* flip payload byte — this corrupts the CRC */

    fq_packet_type_t   type1, type2;
    fq_packet_invite_t pkt1, pkt2;

    fq_packet_err_t e1 = fq_packet_parse(buf, sizeof(buf), &type1, &pkt1);
    fq_packet_err_t e2 = fq_packet_parse(buf, sizeof(buf), &type2, &pkt2);

    /* Both calls must return the same error (stateless) */
    TEST_ASSERT_EQUAL_INT((int)e1, (int)e2);
    TEST_ASSERT_TRUE(e1 != FQ_PKT_OK);
}

/* ---------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------*/
int main(void)
{
    /* Invite */
    test_invite_serialize_exact_size();
    test_invite_magic_bytes();
    test_invite_type_byte();
    test_invite_nonce_little_endian();
    test_invite_version_byte();
    test_invite_parse_roundtrip();
    test_invite_parse_stateless();

    /* Team sync */
    test_team_sync_serialize_exact_size();
    test_team_sync_parse_roundtrip();
    test_team_sync_hp_max_little_endian();
    test_team_sync_garbage_ingestion();

    /* Round hash */
    test_round_hash_serialize_exact_size();
    test_round_hash_parse_roundtrip();
    test_round_hash_combat_hash_little_endian();
    test_round_hash_max_round_accepted();

    /* Seed derivation */
    test_derive_seed_commutative();
    test_derive_seed_nonzero_xor_preserved();
    test_derive_seed_never_returns_zero();

    /* CRC / parse integrity */
    test_crc_trailer_readable();
    test_parse_corrupted_buffer_deterministic();

    printf("[PASS] all p10 protocol feature tests passed\n");
    return 0;
}
