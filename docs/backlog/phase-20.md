# Phase 20: BLE Combat — Two-Device Multiplayer

## Item 1: NimBLE GATT Server + Client Integration

### User Story
As a player pressing "Battle" on my device, I want my device to advertise over BLE, find a nearby opponent, exchange character data, and begin combat — all automatically.

### Acceptance Criteria
- [ ] `hal_ble.c` fully wired to NimBLE stack (not stubs)
- [ ] Device advertises as "FQ-XXXX" (last 4 hex of MAC) with custom service UUID
- [ ] Scanning device discovers and connects within 10 seconds
- [ ] GATT characteristic allows writing `fq_packet_team_sync_t` (36 bytes) in both directions
- [ ] After team sync exchange, both devices derive shared PRNG seed via `fq_protocol_derive_seed(nonce_a, nonce_b)`
- [ ] `FQ_EVT_BLE_CONNECTED` posted to event bus, triggering `FQ_STATE_BATTLE_SETUP` → `FQ_STATE_BATTLE`
- [ ] Combat runs round-by-round with `fq_combat_step()`, exchanging `fq_packet_round_hash_t` after each round
- [ ] `fq_sync_verify_round()` validates hash match; mismatch → disconnect + "DESYNC" error screen
- [ ] Winner determined → `FQ_STATE_BATTLE_RESULT` with win/loss display
- [ ] XP awarded, save updated, return to HOME

### Negative Test Requirements
- **Disconnect mid-combat:** If BLE link drops during round 5, both devices must cleanly return to HOME without crashing or corrupting save data.
- **MTU too small:** If the negotiated MTU is < 36 bytes (team_sync packet size), abort pairing with a readable error message.

### Files to Create/Modify
- `components/fq_hal/src/hal_ble.c` (replace stub with NimBLE)
- `components/fq_hal/CMakeLists.txt` (add REQUIRES bt)
- `main/app_fsm.c` (BATTLE_SETUP and BATTLE state handlers for BLE events)
- `main/app_main.c` (combat context init, round stepping, hash exchange)
- `components/presentation/src/screens/screen_combat.c` (round-by-round updates)

---

## Item 2: Battle Result Screen

### User Story
As a player after combat, I want to see who won, how much XP I earned, and whether any items dropped.

### Acceptance Criteria
- [ ] `screen_battle_result.c` renders: winner name + sprite, "YOU WIN" / "YOU LOSE", XP earned, rounds survived
- [ ] Button A returns to HOME and saves updated stats
- [ ] On loss (creature death), transition to `FQ_STATE_REBIRTH` instead of HOME

### Files to Create
- `components/presentation/include/screens/screen_battle_result.h`
- `components/presentation/src/screens/screen_battle_result.c`

---

## Item 3: Rebirth Screen

### User Story
As a player whose creature just died, I want to see the rebirth screen showing my stat penalty, legacy tokens earned, and the option to spend tokens on the legacy tree.

### Acceptance Criteria
- [ ] `screen_rebirth.c` renders: old stats → new stats (with penalty applied), tokens earned, legacy tree visualization
- [ ] Button A spends a token on the next available legacy node
- [ ] Button B confirms rebirth and returns to HOME with the reborn character
- [ ] Character is saved after rebirth

### Files to Create
- `components/presentation/include/screens/screen_rebirth.h`
- `components/presentation/src/screens/screen_rebirth.c`
