/**
 * protocol.c — FiestaQuest Phase-10 BLE Transport-Agnostic Packet Protocol
 *
 * Implements fq_packet_serialize(), fq_packet_parse(), and
 * fq_protocol_derive_seed().
 *
 * Architecture note — CRC32 dependency:
 *   This file uses fq_crc32() from components/game/src/crc32.c.
 *   The connectivity/ public headers do NOT include any game/ headers (N6),
 *   but this implementation file privately includes crc32.h.
 *
 *   ESP-IDF build: components/connectivity/CMakeLists.txt declares
 *   PRIV_REQUIRES game so that crc32.h is reachable at compile time but is
 *   NOT re-exported via connectivity/'s public include path.
 *
 *   Host test build: the add_conn_test() CMake function adds both
 *   GAME_INCLUDE and CONN_INCLUDE to the target, so crc32.h resolves via
 *   the game include path.
 *
 * N3: All multi-byte fields use explicit LE byte shifts. No struct casting.
 * N8: CRC covers bytes [0..N-5] only — the CRC field is NOT included.
 * N2: Zero-length buffer rejected before any field access.
 * N4: Buffers < FQ_PACKET_OVERHEAD rejected before field reads.
 * N11: parse() is stateless — no internal mutable state.
 */

#include "protocol.h"

/* Private CRC32 dependency (not re-exported via public connectivity/ headers) */
#include "crc32.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------------*/

/** Write a uint32_t as 4 LE bytes into buf at offset. */
static void write_le32(uint8_t *buf, size_t offset, uint32_t val)
{
    buf[offset + 0u] = (uint8_t)(val & 0xFFu);
    buf[offset + 1u] = (uint8_t)((val >> 8u)  & 0xFFu);
    buf[offset + 2u] = (uint8_t)((val >> 16u) & 0xFFu);
    buf[offset + 3u] = (uint8_t)((val >> 24u) & 0xFFu);
}

/** Write a uint16_t as 2 LE bytes into buf at offset. */
static void write_le16(uint8_t *buf, size_t offset, uint16_t val)
{
    buf[offset + 0u] = (uint8_t)(val & 0xFFu);
    buf[offset + 1u] = (uint8_t)((val >> 8u) & 0xFFu);
}

/** Read a uint32_t from 4 LE bytes at buf[offset]. */
static uint32_t read_le32(const uint8_t *buf, size_t offset)
{
    return (uint32_t)buf[offset + 0u]
         | ((uint32_t)buf[offset + 1u] << 8u)
         | ((uint32_t)buf[offset + 2u] << 16u)
         | ((uint32_t)buf[offset + 3u] << 24u);
}

/** Read a uint16_t from 2 LE bytes at buf[offset]. */
static uint16_t read_le16(const uint8_t *buf, size_t offset)
{
    return (uint16_t)((uint16_t)buf[offset + 0u]
                    | ((uint16_t)buf[offset + 1u] << 8u));
}

/**
 * write_header() — Write the 5-byte packet header: magic(4) + type(1).
 * Returns the number of bytes written (always 5).
 */
static size_t write_header(uint8_t *buf, fq_packet_type_t type)
{
    buf[0] = (uint8_t)'F';
    buf[1] = (uint8_t)'Q';
    buf[2] = (uint8_t)'0';
    buf[3] = (uint8_t)'1';
    buf[4] = (uint8_t)type;
    return 5u;
}

/**
 * write_crc_trailer() — Compute CRC over buf[0..payload_end-1] and append it.
 * Returns the total packet length (payload_end + 4).
 */
static size_t write_crc_trailer(uint8_t *buf, size_t payload_end)
{
    uint32_t crc = fq_crc32(buf, payload_end);
    write_le32(buf, payload_end, crc);
    return payload_end + FQ_PACKET_CRC_LEN;
}

/* ---------------------------------------------------------------------------
 * Payload size for each type (excluding magic, type byte, and CRC trailer).
 * ---------------------------------------------------------------------------*/
static size_t payload_size_for_type(fq_packet_type_t type)
{
    switch (type) {
        case FQ_PKT_INVITE:
            /* nonce(4) + version(1) = 5 */
            return 5u;
        case FQ_PKT_TEAM_SYNC:
            /* name(12)+class_id(1)+level(1)+hp_max(2)+equipped[5](10)+equipped_count(1) = 27 */
            return 27u;
        case FQ_PKT_ROUND_HASH:
            /* round(1) + combat_hash(4) = 5 */
            return 5u;
        case FQ_PKT_DISCONNECT:
            /* No payload. */
            return 0u;
        default:
            return 0u;
    }
}

/* ---------------------------------------------------------------------------
 * fq_packet_serialize
 * ---------------------------------------------------------------------------*/
size_t fq_packet_serialize(const void *packet, fq_packet_type_t type,
                            uint8_t *buf, size_t buf_size)
{
    size_t required;
    size_t pos;
    const fq_packet_invite_t     *inv;
    const fq_packet_team_sync_t  *ts;
    const fq_packet_round_hash_t *rh;
    uint8_t i;

    if (packet == NULL || buf == NULL) {
        return 0u;
    }

    required = FQ_PACKET_OVERHEAD + payload_size_for_type(type);
    if (buf_size < required) {
        return 0u;
    }

    /* Write header (magic + type) */
    pos = write_header(buf, type);

    /* Write payload field-by-field */
    switch (type) {
        case FQ_PKT_INVITE:
            inv = (const fq_packet_invite_t *)packet;
            write_le32(buf, pos, inv->nonce);
            pos += 4u;
            buf[pos] = inv->version;
            pos += 1u;
            break;

        case FQ_PKT_TEAM_SYNC:
            ts = (const fq_packet_team_sync_t *)packet;
            /* name: exactly 12 bytes, copy raw (may contain embedded nulls) */
            memcpy(buf + pos, ts->name, 12u);
            pos += 12u;
            buf[pos] = ts->class_id;
            pos += 1u;
            buf[pos] = ts->level;
            pos += 1u;
            write_le16(buf, pos, ts->hp_max);
            pos += 2u;
            for (i = 0u; i < 5u; i++) {
                write_le16(buf, pos, ts->equipped[i]);
                pos += 2u;
            }
            buf[pos] = ts->equipped_count;
            pos += 1u;
            break;

        case FQ_PKT_ROUND_HASH:
            rh = (const fq_packet_round_hash_t *)packet;
            buf[pos] = rh->round;
            pos += 1u;
            write_le32(buf, pos, rh->combat_hash);
            pos += 4u;
            break;

        case FQ_PKT_DISCONNECT:
            /* No payload. */
            break;

        default:
            return 0u;
    }

    /* Append CRC32 trailer covering bytes [0..pos-1] */
    return write_crc_trailer(buf, pos);
}

/* ---------------------------------------------------------------------------
 * fq_packet_parse
 * ---------------------------------------------------------------------------*/
fq_packet_err_t fq_packet_parse(const uint8_t *buf, size_t buf_size,
                                 fq_packet_type_t *out_type, void *out_packet)
{
    fq_packet_type_t     pkt_type;
    size_t               expected_size;
    uint32_t             stored_crc;
    uint32_t             computed_crc;
    size_t               payload_pos;
    fq_packet_invite_t     *inv;
    fq_packet_team_sync_t  *ts;
    fq_packet_round_hash_t *rh;
    uint8_t              i;

    /* N5: NULL guard — check all output pointers first */
    if (buf == NULL || out_type == NULL || out_packet == NULL) {
        return FQ_PKT_ERR_NULL;
    }

    /* N2/N4: Minimum viable buffer check */
    if (buf_size < FQ_PACKET_OVERHEAD) {
        return FQ_PKT_ERR_BUFFER_TOO_SMALL;
    }

    /* Magic check — before CRC (fast-fail on spoofed magic) */
    if (buf[0] != (uint8_t)'F' || buf[1] != (uint8_t)'Q' ||
        buf[2] != (uint8_t)'0' || buf[3] != (uint8_t)'1') {
        return FQ_PKT_ERR_MAGIC_MISMATCH;
    }

    /* Extract type byte */
    pkt_type = (fq_packet_type_t)buf[4];

    /* Validate type and determine expected total size */
    switch (pkt_type) {
        case FQ_PKT_INVITE:
            expected_size = FQ_PACKET_INVITE_SIZE;
            break;
        case FQ_PKT_TEAM_SYNC:
            expected_size = FQ_PACKET_TEAM_SYNC_SIZE;
            break;
        case FQ_PKT_ROUND_HASH:
            expected_size = FQ_PACKET_ROUND_HASH_SIZE;
            break;
        case FQ_PKT_DISCONNECT:
            expected_size = FQ_PACKET_OVERHEAD;
            break;
        default:
            return FQ_PKT_ERR_UNKNOWN_TYPE;
    }

    if (buf_size < expected_size) {
        return FQ_PKT_ERR_BUFFER_TOO_SMALL;
    }

    /* CRC check — covers bytes [0..expected_size-5], i.e., excluding the 4-byte trailer */
    stored_crc   = read_le32(buf, expected_size - FQ_PACKET_CRC_LEN);
    computed_crc = fq_crc32(buf, expected_size - FQ_PACKET_CRC_LEN);

    if (stored_crc != computed_crc) {
        return FQ_PKT_ERR_CRC_MISMATCH;
    }

    /* Deserialize payload */
    payload_pos = 5u;  /* after magic(4) + type(1) */

    switch (pkt_type) {
        case FQ_PKT_INVITE:
            inv = (fq_packet_invite_t *)out_packet;
            inv->nonce   = read_le32(buf, payload_pos);
            payload_pos += 4u;
            inv->version = buf[payload_pos];
            break;

        case FQ_PKT_TEAM_SYNC:
            ts = (fq_packet_team_sync_t *)out_packet;
            memcpy(ts->name, buf + payload_pos, 12u);
            payload_pos += 12u;
            ts->class_id = buf[payload_pos++];
            ts->level    = buf[payload_pos++];
            ts->hp_max   = read_le16(buf, payload_pos);
            payload_pos += 2u;
            for (i = 0u; i < 5u; i++) {
                ts->equipped[i] = read_le16(buf, payload_pos);
                payload_pos += 2u;
            }
            ts->equipped_count = buf[payload_pos];
            break;

        case FQ_PKT_ROUND_HASH:
            rh = (fq_packet_round_hash_t *)out_packet;
            rh->round       = buf[payload_pos++];
            rh->combat_hash = read_le32(buf, payload_pos);

            /* N9: Validate round range AFTER CRC passes */
            if (rh->round < (uint8_t)FQ_ROUND_MIN ||
                rh->round > (uint8_t)FQ_ROUND_MAX) {
                return FQ_PKT_ERR_INVALID_ROUND;
            }
            break;

        case FQ_PKT_DISCONNECT:
            /* No payload to decode. */
            break;

        default:
            return FQ_PKT_ERR_UNKNOWN_TYPE;
    }

    *out_type = pkt_type;
    return FQ_PKT_OK;
}

/* ---------------------------------------------------------------------------
 * fq_protocol_derive_seed
 * ---------------------------------------------------------------------------*/
uint32_t fq_protocol_derive_seed(uint32_t nonce_a, uint32_t nonce_b)
{
    uint32_t seed = nonce_a ^ nonce_b;
    /* N1: XOR result of 0 → force to 1 (matches PRNG zero-guard) */
    if (seed == 0u) {
        seed = 1u;
    }
    return seed;
}

/* ---------------------------------------------------------------------------
 * Phase 20: fq_team_sync_clamp_equipped_count, fq_team_sync_validate
 * ---------------------------------------------------------------------------*/

uint8_t fq_team_sync_clamp_equipped_count(uint8_t count)
{
    return (count > 5u) ? 5u : count;
}

int fq_team_sync_validate(const fq_packet_team_sync_t *pkt)
{
    if (pkt == NULL) {
        return 0;
    }
    if (pkt->hp_max == 0u) {
        return 0;
    }
    return 1;
}
