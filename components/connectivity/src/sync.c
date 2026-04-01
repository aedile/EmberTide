/**
 * sync.c — FiestaQuest Phase-10 Round Synchronization Verification
 *
 * Implements fq_sync_verify_round().
 *
 * Pure function — no global state, no game/ types, no HAL includes.
 * Works exclusively with uint8_t round numbers and uint32_t hashes.
 *
 * Constitution Priority 0: No floating point. Stateless. Deterministic.
 */

#include "sync.h"

fq_sync_err_t fq_sync_verify_round(uint8_t  expected_round,
                                    uint8_t  received_round,
                                    uint32_t local_hash,
                                    uint32_t peer_hash)
{
    /* Check round match FIRST — fast-fail on desync (roll-forward attack) */
    if (expected_round != received_round) {
        return FQ_SYNC_ERR_ROUND_MISMATCH;
    }

    /* Check hash match */
    if (local_hash != peer_hash) {
        return FQ_SYNC_ERR_HASH_MISMATCH;
    }

    return FQ_SYNC_OK;
}
