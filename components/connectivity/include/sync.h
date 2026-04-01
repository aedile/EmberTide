/**
 * sync.h — FiestaQuest Phase-10 Round Synchronization Verification
 *
 * Provides a stateless verifier for round-hash exchange between two peers.
 * After each combat round, both devices serialize their combat state into a
 * hash (via fq_generate_combat_hash in game/) and exchange those hashes.
 * This module compares the local and peer hashes.
 *
 * Architecture constraint: connectivity/ MUST NOT include game/ headers.
 * This module works with plain uint32_t hashes and uint8_t round numbers,
 * NOT with fq_combat_ctx_t. The caller (application layer) is responsible
 * for producing the hash from the combat context before passing it here.
 *
 * Constitution Priority 0: No floating point. No global mutable state.
 * Pure function — result is solely a function of its parameters.
 */

#ifndef FIESTAQUEST_CONNECTIVITY_SYNC_H
#define FIESTAQUEST_CONNECTIVITY_SYNC_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Sync result codes
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_SYNC_OK                = 0,  /**< Rounds and hashes match — proceed. */
    FQ_SYNC_ERR_NULL          = 1,  /**< Reserved for future pointer args. */
    FQ_SYNC_ERR_ROUND_MISMATCH= 2,  /**< expected_round != received_round. */
    FQ_SYNC_ERR_HASH_MISMATCH = 3,  /**< local_hash != peer_hash. */
    FQ_SYNC_ERR_TIMED_OUT     = 4   /**< Peer hash not received in time (signal). */
} fq_sync_err_t;

/**
 * fq_sync_verify_round() — Verify peer round number and hash match local state.
 *
 * Checks round match FIRST (fast-fail on desync), then hash match.
 * Both checks use equality comparison — no integer arithmetic.
 *
 * @param expected_round  The round number the local device expects.
 * @param received_round  The round number reported by the peer packet.
 * @param local_hash      Hash of local combat state at expected_round.
 * @param peer_hash       Hash received from peer for received_round.
 * @return                FQ_SYNC_OK if both match;
 *                        FQ_SYNC_ERR_ROUND_MISMATCH if rounds differ;
 *                        FQ_SYNC_ERR_HASH_MISMATCH if rounds match but hashes differ.
 */
fq_sync_err_t fq_sync_verify_round(uint8_t  expected_round,
                                    uint8_t  received_round,
                                    uint32_t local_hash,
                                    uint32_t peer_hash);

#endif /* FIESTAQUEST_CONNECTIVITY_SYNC_H */
