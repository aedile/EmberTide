# Phase 10: Connectivity Data Protocol

## Item 1: BLE Nonce & Handshake DTOs (Data Transfer Objects)

### User Story
As a network engineer, before I ever touch the ESP-IDF Bluetooth stack, I must encode the exact byte-structures 2 devices will send to each other to guarantee they are mathematically synchronized.

### Acceptance Criteria
- [ ] `components/connectivity/include/protocol.h` defines `fq_packet_invite_t`, `fq_packet_team_sync_t`, `fq_packet_round_hash_t`.
- [ ] All packets begin with `magic = 'F','Q','0','1'`.
- [ ] All packets end with a `crc32`.
- [ ] Defines the master PRNG seed exchange algorithm (XORing Device A and Device B timestamps).

### Negative Test Requirements (from spec-challenger)
- **Garbage Ingestion Padding:** Provide a packet buffer that is 3 bytes longer than `sizeof(fq_packet_team_sync_t)`. Ensure the deserializer drops the extra bytes securely without clobbering the heap.
- **Magic Number Spoofing:** Alter the first 4 bytes of a valid encrypted payload. Ensure `fq_packet_parse` instantly rejects it with `ERR_MAGIC_MISMATCH` before attempting to validate the CRC.

### Implementation Steps
1. Write struct serialization identically to the Save/Load LittleFS logic (explicit byte offsets, NO struct casting).
2. Create state enums for the handshake (E.g. `STATE_WAITING_TEAM`, `STATE_READY`).

### Test Expectations
- `test_protocol_serialization.c` asserts a mocked `team_sync_t` serializes accurately to exactly N bytes and handles garbage ingestion without crashing.

### Files to Create/Modify
- `components/connectivity/include/protocol.h`
- `components/connectivity/src/protocol.c`
- `test/host/test_protocol.c`

### Commit Messages
- `feat: ble transport-agnostic serialization packets`

---

## Item 2: Round Sync Hash Verification

### User Story
As a combat fairness validator, at the end of every round both devices must exchange a hash of their `fq_combat_ctx_t` before executing the next round, so any cheating or floating-point desync immediately halts the match.

### Acceptance Criteria
- [ ] `components/connectivity/include/sync.h` provides `fq_generate_combat_hash(ctx)`.
- [ ] Provides `fq_verify_peer_hash(peer_crc)`.
- [ ] Emits a DISCONNECT or DESYNC error state if they diverge.

### Negative Test Requirements (from spec-challenger)
- **Spoofed Round Roll-forward:** Both devices generate identical hashes for Round 1. Device B purposefully executes Round 2 locally and sends the R2 hash when Device A expects the R1 hash sequence validation. Assert the validation protocol catches out-of-order hashes by embedding the `current_round` integer directly into the serialized packet footprint.
- **Endless Hash Standoff:** Device A never receives Device B's hash. Assert the combat context stepper implements a soft-timeout limit returning `TIMED_OUT` so the ESP32 doesn't wait indefinitely, blocking the main UI loop.

### Implementation Steps
1. `fq_generate_combat_hash` just hashes the current fighters' HPs, Stamina, and internal PRNG counter variable.
2. The logic strictly requires that Round (N+1) cannot execute until the CRC for Round N is confirmed matching.

### Test Expectations
- `test_combat_sync.c` runs two combat contexts independently. Mutates the PRNG of Context B by 1 call, confirms that `fq_verify_peer_hash` correctly throws `DESYNC_FATAL`.

### Files to Create/Modify
- `components/connectivity/include/sync.h`
- `components/connectivity/src/sync.c`
- `test/host/test_combat_sync.c`

### Commit Messages
- `feat: combat PRNG contextual state hasher for anti-cheat sync`
