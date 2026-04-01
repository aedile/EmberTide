/**
 * combat_hash.h — FiestaQuest Phase-10 Combat State Hash
 *
 * Produces a deterministic CRC32 fingerprint of a combat context at a given
 * round boundary. Both peers exchange this hash before advancing to the next
 * round; divergence indicates desync or tampering.
 *
 * Architecture: belongs in components/game/ — pure function, no HAL, no
 * connectivity/ dependencies.
 *
 * Constitution Priority 0: No floating point. No global mutable state.
 * Result is purely a function of (ctx, round). NULL ctx returns 0.
 *
 * Hashing strategy (N7 — hash only meaningful fields, not padding):
 *   Serializes the following fields field-by-field into a stack buffer using
 *   explicit byte shifts (little-endian). NO struct casting/memcpy of the
 *   whole struct — that would hash padding bytes and produce non-portable
 *   results.
 *
 *   Fields hashed (in order):
 *     round           (uint8_t,  1 byte)
 *     f1.hp           (int16_t,  2 bytes, LE)
 *     f2.hp           (int16_t,  2 bytes, LE)
 *     f1.hp_max       (int16_t,  2 bytes, LE)
 *     f2.hp_max       (int16_t,  2 bytes, LE)
 *     rng.state       (uint32_t, 4 bytes, LE)
 *   Total serialized: 13 bytes.
 *
 * N8: The CRC32 output itself is NOT fed back into the hash computation.
 */

#ifndef FIESTAQUEST_COMBAT_HASH_H
#define FIESTAQUEST_COMBAT_HASH_H

#include <stdint.h>
#include "combat.h"

/**
 * fq_generate_combat_hash() — Hash a combat context at a given round.
 *
 * Serializes round number + fighter HP pair + HP max pair + PRNG state into
 * a 13-byte stack buffer using explicit little-endian byte writes (no struct
 * casting), then returns fq_crc32() of that buffer.
 *
 * @param ctx    Pointer to the combat context. If NULL, returns 0.
 * @param round  The round number to embed in the hash (1-12 valid range).
 *               Round 0 or > FQ_MAX_ROUNDS returns 0 without hashing.
 * @return       CRC32 of the serialized fields, or 0 on invalid input.
 */
uint32_t fq_generate_combat_hash(const fq_combat_ctx_t *ctx, uint8_t round);

#endif /* FIESTAQUEST_COMBAT_HASH_H */
