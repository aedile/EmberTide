/**
 * combat_hash.c — FiestaQuest Phase-10 Combat State Hash
 *
 * Implementation of fq_generate_combat_hash().
 *
 * Serializes only the meaningful fields of a combat context into a 13-byte
 * stack buffer using explicit little-endian byte writes, then computes
 * CRC32 of that buffer.
 *
 * N7: Only meaningful fields are hashed — padding bytes are NOT included.
 * N8: The CRC32 output is not fed back into the computation.
 * N3: All multi-byte fields use explicit LE byte shifts.
 *
 * Constitution Priority 0: No floating point. Pure function. No global state.
 */

#include "combat_hash.h"
#include "crc32.h"

/* ---------------------------------------------------------------------------
 * Serialized buffer layout (13 bytes total):
 *   [0]      round           uint8_t
 *   [1..2]   f1.hp           int16_t LE
 *   [3..4]   f2.hp           int16_t LE
 *   [5..6]   f1.hp_max       int16_t LE
 *   [7..8]   f2.hp_max       int16_t LE
 *   [9..12]  rng.state       uint32_t LE
 * ---------------------------------------------------------------------------*/
#define HASH_BUF_LEN  13u

/** Minimum valid round number (first round of combat). */
#define COMBAT_HASH_ROUND_MIN  1u

uint32_t fq_generate_combat_hash(const fq_combat_ctx_t *ctx, uint8_t round)
{
    uint8_t buf[HASH_BUF_LEN];

    if (ctx == NULL) {
        return 0u;
    }

    /* N9: round must be in [1, FQ_MAX_ROUNDS] (from combat.h) */
    if (round < COMBAT_HASH_ROUND_MIN || round > (uint8_t)FQ_MAX_ROUNDS) {
        return 0u;
    }

    /* Serialize field-by-field, little-endian. No struct casting. */

    /* [0] round */
    buf[0] = round;

    /* [1..2] f1.hp (int16_t → 2 LE bytes via uint16_t reinterpretation) */
    buf[1] = (uint8_t)((uint16_t)ctx->f1.hp & 0xFFu);
    buf[2] = (uint8_t)(((uint16_t)ctx->f1.hp >> 8u) & 0xFFu);

    /* [3..4] f2.hp */
    buf[3] = (uint8_t)((uint16_t)ctx->f2.hp & 0xFFu);
    buf[4] = (uint8_t)(((uint16_t)ctx->f2.hp >> 8u) & 0xFFu);

    /* [5..6] f1.hp_max */
    buf[5] = (uint8_t)((uint16_t)ctx->f1.hp_max & 0xFFu);
    buf[6] = (uint8_t)(((uint16_t)ctx->f1.hp_max >> 8u) & 0xFFu);

    /* [7..8] f2.hp_max */
    buf[7] = (uint8_t)((uint16_t)ctx->f2.hp_max & 0xFFu);
    buf[8] = (uint8_t)(((uint16_t)ctx->f2.hp_max >> 8u) & 0xFFu);

    /* [9..12] rng.state (uint32_t LE) */
    buf[9]  = (uint8_t)(ctx->rng.state & 0xFFu);
    buf[10] = (uint8_t)((ctx->rng.state >> 8u)  & 0xFFu);
    buf[11] = (uint8_t)((ctx->rng.state >> 16u) & 0xFFu);
    buf[12] = (uint8_t)((ctx->rng.state >> 24u) & 0xFFu);

    return fq_crc32(buf, HASH_BUF_LEN);
}
