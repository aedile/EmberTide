/**
 * protocol.h — FiestaQuest Phase-10 BLE Transport-Agnostic Packet Protocol
 *
 * Defines the wire-format DTOs (Data Transfer Objects) for BLE peer
 * synchronization. All serialization uses explicit byte-level little-endian
 * writes — NO struct casting, NO memcpy of struct internals.
 *
 * Architecture constraint: connectivity/ MUST NOT include game/ headers.
 * This module depends only on <stdint.h>, <stddef.h>, and the shared CRC32
 * lookup table (linked in via the host test build and via PRIV_REQUIRES game
 * in the ESP-IDF component build — see CMakeLists.txt note below).
 *
 * Wire format per packet:
 *   [0..3]   magic    "FQ01" (4 ASCII bytes, NOT null-terminated)
 *   [4]      type     fq_packet_type_t (uint8_t)
 *   [5..N-5] payload  field-by-field, little-endian
 *   [N-4..N-1] crc32  CRC32 of bytes [0..N-5], little-endian uint32_t
 *
 * N3: All multi-byte fields use explicit LE byte shifts. No struct casting.
 * N8: The CRC32 field is NOT included in the CRC32 computation.
 * N6: This header MUST NOT include any game/ headers.
 */

#ifndef FIESTAQUEST_CONNECTIVITY_PROTOCOL_H
#define FIESTAQUEST_CONNECTIVITY_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------------*/

/** Magic bytes that begin every FiestaQuest BLE packet. Not null-terminated. */
#define FQ_PACKET_MAGIC  "FQ01"

/** Length of the magic prefix (bytes 0-3 of every packet). */
#define FQ_PACKET_MAGIC_LEN  4u

/** Length of the CRC32 trailer (bytes N-4..N-1). */
#define FQ_PACKET_CRC_LEN  4u

/** Minimum overhead per packet: magic(4) + type(1) + crc(4). */
#define FQ_PACKET_OVERHEAD  (FQ_PACKET_MAGIC_LEN + 1u + FQ_PACKET_CRC_LEN)

/** Protocol version embedded in invite packets. */
#define FQ_PROTOCOL_VERSION  1u

/** Valid round number range: [1, 12]. Round 0 and >12 are invalid. */
#define FQ_ROUND_MIN  1u
#define FQ_ROUND_MAX  12u

/* ---------------------------------------------------------------------------
 * Packet type discriminator
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_PKT_INVITE      = 1,  /**< Exchange nonces for shared seed generation. */
    FQ_PKT_TEAM_SYNC   = 2,  /**< Share character summary with peer. */
    FQ_PKT_ROUND_HASH  = 3,  /**< Anti-cheat: hash of combat state at round N. */
    FQ_PKT_DISCONNECT  = 4   /**< Clean disconnection signal. */
} fq_packet_type_t;

/* ---------------------------------------------------------------------------
 * Error codes returned by packet operations
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_PKT_OK                   = 0,  /**< Operation succeeded. */
    FQ_PKT_ERR_NULL             = 1,  /**< NULL pointer argument. */
    FQ_PKT_ERR_MAGIC_MISMATCH   = 2,  /**< First 4 bytes != "FQ01". */
    FQ_PKT_ERR_CRC_MISMATCH     = 3,  /**< Trailer CRC does not match computed. */
    FQ_PKT_ERR_BUFFER_TOO_SMALL = 4,  /**< buf_size insufficient for this type. */
    FQ_PKT_ERR_UNKNOWN_TYPE     = 5,  /**< type field not in fq_packet_type_t. */
    FQ_PKT_ERR_INVALID_ROUND    = 6   /**< Round field out of [1,12] range. */
} fq_packet_err_t;

/* ---------------------------------------------------------------------------
 * fq_packet_invite_t — exchange nonces for shared PRNG seed generation.
 *
 * Payload (5 bytes):
 *   nonce    uint32_t LE  4 bytes
 *   version  uint8_t      1 byte
 *
 * Total wire size: FQ_PACKET_OVERHEAD(9) + 5 = 14 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint32_t nonce;    /**< Random value from this device. */
    uint8_t  version;  /**< Protocol version (FQ_PROTOCOL_VERSION). */
} fq_packet_invite_t;

/** Exact serialized byte count for an invite packet. */
#define FQ_PACKET_INVITE_SIZE  (FQ_PACKET_OVERHEAD + 4u + 1u)  /* 14 bytes */

/* ---------------------------------------------------------------------------
 * fq_packet_team_sync_t — character summary for opponent's display.
 *
 * Payload (27 bytes):
 *   name[12]         char[12]     12 bytes (fixed; may be NUL-padded after string)
 *   class_id         uint8_t       1 byte
 *   level            uint8_t       1 byte
 *   hp_max           uint16_t LE   2 bytes
 *   equipped[5]      uint16_t[5]  10 bytes LE each (5 × 2)
 *   equipped_count   uint8_t       1 byte
 *   (struct padding bytes are NOT serialized — explicit field-by-field write)
 *
 * Total wire size: FQ_PACKET_OVERHEAD(9) + 27 = 36 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    char     name[12];       /**< Character name (up to 12 bytes, may be NUL-padded). */
    uint8_t  class_id;       /**< fq_class_t value. */
    uint8_t  level;          /**< Character level 1-99. */
    uint16_t hp_max;         /**< Max HP for display. */
    uint16_t equipped[5];    /**< Item IDs in equipped slots. */
    uint8_t  equipped_count; /**< Number of active equipped items (0-5). */
} fq_packet_team_sync_t;

/** Exact serialized byte count for a team sync packet.
 *  Payload: name(12)+class_id(1)+level(1)+hp_max(2)+equipped[5](10)+equipped_count(1) = 27 */
#define FQ_PACKET_TEAM_SYNC_SIZE  (FQ_PACKET_OVERHEAD + 27u)  /* 36 bytes */

/* ---------------------------------------------------------------------------
 * fq_packet_round_hash_t — anti-cheat hash exchange after each round.
 *
 * Payload (5 bytes):
 *   round        uint8_t      1 byte   — must be in [FQ_ROUND_MIN, FQ_ROUND_MAX]
 *   combat_hash  uint32_t LE  4 bytes
 *
 * Total wire size: FQ_PACKET_OVERHEAD(9) + 5 = 14 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint8_t  round;        /**< Round number [FQ_ROUND_MIN, FQ_ROUND_MAX]. */
    uint32_t combat_hash;  /**< fq_generate_combat_hash() output. */
} fq_packet_round_hash_t;

/** Exact serialized byte count for a round hash packet. */
#define FQ_PACKET_ROUND_HASH_SIZE  (FQ_PACKET_OVERHEAD + 1u + 4u)  /* 14 bytes */

/* ---------------------------------------------------------------------------
 * Serialization / Deserialization API
 * ---------------------------------------------------------------------------*/

/**
 * fq_packet_serialize() — Serialize a packet struct into a byte buffer.
 *
 * Writes the magic prefix, type byte, payload (field-by-field LE), and CRC32
 * trailer into buf[0..buf_size-1].
 *
 * @param packet    Pointer to the packet struct (fq_packet_invite_t,
 *                  fq_packet_team_sync_t, or fq_packet_round_hash_t). NULL → 0.
 * @param type      Packet type discriminator.
 * @param buf       Output buffer. NULL → 0.
 * @param buf_size  Size of buf in bytes. If too small → 0.
 * @return          Number of bytes written, or 0 on any error.
 */
size_t fq_packet_serialize(const void *packet, fq_packet_type_t type,
                            uint8_t *buf, size_t buf_size);

/**
 * fq_packet_parse() — Deserialize a byte buffer into a packet struct.
 *
 * Validates magic, CRC32, type, and per-type field constraints in that order.
 * Writes the packet type to *out_type and the decoded struct into *out_packet.
 *
 * N2: A zero-length buffer (buf_size == 0) returns FQ_PKT_ERR_BUFFER_TOO_SMALL.
 * N4: Buffers smaller than FQ_PACKET_OVERHEAD are rejected before any field
 *     access — no OOB reads.
 * N11: Stateless — calling parse twice on the same buffer returns the same
 *      result (no internal state mutation).
 *
 * @param buf        Input buffer. NULL → FQ_PKT_ERR_NULL.
 * @param buf_size   Size of buf in bytes. 0 → FQ_PKT_ERR_BUFFER_TOO_SMALL.
 * @param out_type   Output: parsed type. NULL → FQ_PKT_ERR_NULL.
 * @param out_packet Output: parsed struct. NULL → FQ_PKT_ERR_NULL.
 * @return           FQ_PKT_OK on success, or a specific error code.
 */
fq_packet_err_t fq_packet_parse(const uint8_t *buf, size_t buf_size,
                                 fq_packet_type_t *out_type, void *out_packet);

/**
 * fq_protocol_derive_seed() — Derive a shared PRNG seed from two device nonces.
 *
 * Algorithm: XOR the two nonces. If result is 0 (N1: zero-guard), force to 1.
 * This matches the PRNG zero-guard in fq_prng_init().
 *
 * Commutative: fq_protocol_derive_seed(a, b) == fq_protocol_derive_seed(b, a).
 *
 * @param nonce_a  Nonce from device A.
 * @param nonce_b  Nonce from device B.
 * @return         Non-zero shared seed.
 */
uint32_t fq_protocol_derive_seed(uint32_t nonce_a, uint32_t nonce_b);



/* ---------------------------------------------------------------------------
 * fq_team_sync_clamp_equipped_count() — Clamp equipped_count to [0, 5].
 *
 * Phase 20: Validates the equipped_count field received from a peer before
 * using it to index into the equipped[] array (max length 5). Values > 5
 * are silently clamped to 5. This prevents any OOB array access.
 *
 * Pure function — no side effects.
 *
 * @param count  Received equipped_count value from fq_packet_team_sync_t.
 * @return       Clamped value in [0, 5].
 * ---------------------------------------------------------------------------*/
uint8_t fq_team_sync_clamp_equipped_count(uint8_t count);

/* ---------------------------------------------------------------------------
 * fq_team_sync_validate() — Basic validity check for a team sync packet.
 *
 * Phase 20: Called before using a received fq_packet_team_sync_t to initialise
 * a combat context. Rejects packets that would cause undefined behaviour.
 *
 * Validation rules:
 *   - pkt must not be NULL.
 *   - pkt->hp_max must be > 0 (a zero HP character cannot fight).
 *
 * @param pkt  Packet to validate. NULL returns 0 (invalid).
 * @return     1 if valid, 0 if invalid.
 * ---------------------------------------------------------------------------*/
int fq_team_sync_validate(const fq_packet_team_sync_t *pkt);

#endif /* FIESTAQUEST_CONNECTIVITY_PROTOCOL_H */
