# Phase 20: BLE Combat — Two-Device Multiplayer

## Item 1: NimBLE GATT Server + Client Integration

### User Story
As a player pressing "Battle" on my device, I want my device to advertise over BLE, find a nearby opponent, exchange character data, and begin combat — all automatically.

### Architecture Decisions (from spec-challenger)

**BLE HAL remains stubbed for host tests.** The real NimBLE wiring is target-only code behind `#ifdef CONFIG_BT_ENABLED`. Host tests use `mock_hal_ble.c` exclusively. The combat orchestration logic in `app_main.c` / `app_fsm.c` must be testable without real BLE.

**Combat hash function:** New `uint32_t fq_generate_combat_hash(const fq_combat_ctx_t *ctx)` in `components/game/combat.h`. CRC32 over the deterministic state (fighter HPs, round, PRNG position). Must NOT advance PRNG. Must be pure.

**`fq_app_ctx_t` expansion:** Add fields for BLE combat orchestration:
- `fq_character_t opponent` — received from team_sync
- `uint32_t shared_seed` — derived from nonce exchange
- `uint8_t my_nonce[4]` — local nonce for seed derivation
- `uint8_t battle_won` — 1=won, 0=lost for result screen
- `uint16_t xp_earned` — for result screen display
- `uint8_t rounds_survived` — for result screen
- Update `_Static_assert` for new size

**BLE disconnect event:** Add `FQ_EVT_BLE_DISCONNECTED` to event bus. Handle in BATTLE_SETUP, BATTLE, BATTLE_RESULT — all transition to HOME, clear `combat_active`, do NOT save (preserve pre-battle state).

**Role resolution:** Lower MAC address advertises, higher scans. `hal_ble_get_mac()` accessor needed (stub returns fixed test MAC, real reads from NimBLE).

**BATTLE_SETUP timeout/cancel:** BTN_B cancels (returns to HOME). 200-tick timeout (10s at 50ms) auto-returns to HOME if no connection.

**DESYNC handling:** On hash mismatch, `hal_ble_disconnect()` + transition to HOME with a transient "DESYNC" banner on the home screen (not a new FSM state).

**Idle suppression:** Add BATTLE_RESULT and REBIRTH to `is_idle_forbidden()`.

### Acceptance Criteria
- [ ] `hal_ble.c` fully wired to NimBLE stack (target build only, behind `CONFIG_BT_ENABLED`)
- [ ] Device advertises as "FQ-XXXX" (last 4 hex of MAC) with custom service UUID
- [ ] Role resolution: lower MAC advertises, higher scans
- [ ] Scanning device discovers and connects within 10 seconds
- [ ] GATT characteristic allows writing `fq_packet_team_sync_t` (36 bytes) in both directions
- [ ] After team sync exchange, both devices derive shared PRNG seed via `fq_protocol_derive_seed(nonce_a, nonce_b)`
- [ ] `FQ_EVT_BLE_CONNECTED` posted to event bus, triggering `FQ_STATE_BATTLE_SETUP` → `FQ_STATE_BATTLE`
- [ ] Combat runs round-by-round with `fq_combat_step()`, exchanging `fq_packet_round_hash_t` after each round
- [ ] `fq_generate_combat_hash(ctx)` — pure CRC32 over deterministic state, no PRNG advancement
- [ ] `fq_sync_verify_round()` validates hash match; mismatch → disconnect + HOME with "DESYNC" banner
- [ ] Winner determined → `FQ_STATE_BATTLE_RESULT` with win/loss display
- [ ] `FQ_EVT_BLE_DISCONNECTED` handled in all battle states — clean return to HOME, no save corruption
- [ ] BTN_B in BATTLE_SETUP cancels and returns to HOME
- [ ] BATTLE_SETUP auto-timeout at 200 ticks (10s) returns to HOME
- [ ] `render_current_state()` handles BATTLE, BATTLE_SETUP cases
- [ ] `fq_app_ctx_t` expanded with opponent, shared_seed, battle results; `_Static_assert` updated

### Negative Test Requirements
- **Disconnect mid-combat:** BLE disconnect at round 5 → state==HOME, combat_active==0, save unchanged.
- **MTU too small:** If `(negotiated_MTU - 3) < FQ_PACKET_TEAM_SYNC_SIZE`, abort with error message.
- **Team sync with equipped_count > 5:** Must clamp to 5 before combat init (no OOB on `equipped[5]`).
- **Team sync with hp_max == 0:** Reject or handle gracefully (no combat, return to HOME).
- **Identical nonces:** `fq_protocol_derive_seed(N, N)` returns 1 (zero guard active).
- **Combat hash determinism:** Same context → same hash. Different context → different hash. No PRNG advancement.
- **BATTLE_SETUP cancel:** BTN_B returns to HOME immediately.
- **BATTLE_SETUP timeout:** No connection within 200 ticks → HOME.

### Files to Create/Modify
- `components/fq_hal/src/hal_ble.c` (wire NimBLE behind `CONFIG_BT_ENABLED`)
- `components/fq_hal/include/hal_ble.h` (add `hal_ble_get_mac()`)
- `components/game/include/combat.h` (add `fq_generate_combat_hash()`)
- `components/game/src/combat.c` (implement hash)
- `main/app_fsm.h` (expand `fq_app_ctx_t`, add `FQ_EVT_BLE_DISCONNECTED`)
- `main/app_fsm.c` (BATTLE orchestration, disconnect handling, SETUP timeout/cancel)
- `main/app_main.c` (round-by-round combat tick, hash exchange, render cases, idle suppression)
- `test/host/mock_hal_ble.c` (add `mock_ble_get_mac()`)

---

## Item 2: Battle Result Screen

### User Story
As a player after combat, I want to see who won, how much XP I earned, and whether any items dropped.

### Architecture Decisions

**XP award formula:** `xp_award = 50 + (opponent_level * 10)` on win. 0 XP on loss. Saturate at UINT32_MAX. New `fq_combat_award_xp(fq_character_t *ch, uint8_t opponent_level, uint8_t won)` in `components/game/progression.h`. Triggers `fq_level_up()` if threshold met.

**Win/loss counter:** Increment `ch->wins` or `ch->losses` (uint16_t). Saturate at UINT16_MAX.

**Auto-save:** Transition BATTLE_RESULT → HOME triggers `do_auto_save()` in `app_main.c`.

**Death routing:** If `ch->hp_max > 0` but combat ends with `ch->is_dead == 1` (HP reached 0), BTN_A on BATTLE_RESULT routes to REBIRTH instead of HOME.

### Acceptance Criteria
- [ ] `fq_vm_battle_result_t` defined: winner_name, you_won, xp_earned, rounds_survived, player_sprite_base. `_Static_assert` size pinned.
- [ ] `screen_battle_result.c` renders: winner name + sprite, "YOU WIN" / "YOU LOSE", XP earned, rounds survived
- [ ] `fq_combat_award_xp()` in `components/game/progression.h/c`: XP = `50 + opponent_level * 10` on win, 0 on loss
- [ ] XP addition saturates at UINT32_MAX
- [ ] `wins`/`losses` incremented with UINT16_MAX saturation
- [ ] Button A on win → HOME + auto-save. Button A on loss with `is_dead` → REBIRTH.
- [ ] `render_current_state()` handles BATTLE_RESULT
- [ ] Visual golden: `scene_battle_result.png`

### Negative Test Requirements
- **XP uint32 saturation:** Award that would overflow → saturates.
- **wins/losses uint16 saturation:** At UINT16_MAX → no wrap.
- **Dead player routes to REBIRTH:** `is_dead==1` → REBIRTH, not HOME.
- **Alive player routes to HOME:** `is_dead==0` → HOME.

### Files to Create/Modify
- `components/game/include/progression.h` (add `fq_combat_award_xp()`)
- `components/game/src/progression.c` (implement)
- `components/presentation/include/screens/screen_battle_result.h` (new)
- `components/presentation/src/screens/screen_battle_result.c` (new)
- `components/presentation/include/view_models.h` (add `fq_vm_battle_result_t`)
- `main/vm_builder.h/c` (add `fq_vm_build_battle_result`)

---

## Item 3: Rebirth Screen

### User Story
As a player whose creature just died, I want to see the rebirth screen showing my stat penalty, legacy tokens earned, and the option to spend tokens on the legacy tree.

### Architecture Decisions

**Rebirth formula:** New `game_err_t fq_rebirth(fq_character_t *ch)` in `components/game/progression.h`:
- Level resets to 1
- All stats (STR/SPD/PRC/INT) halved (integer division, min 0 — use class base stats as floor)
- `hp_max` recalculated from new stats
- `xp` reset to 0
- `is_dead` cleared
- `rebirth_count` incremented (saturate at 255)
- Legacy tokens earned: `tokens = old_level / 10` (min 1 per rebirth). `legacy_points` incremented (saturate at 255).

**Legacy tree:** 32-bit bitmask, each bit = one perk node. Nodes are sequential (bit 0 first, then bit 1, etc.). Spending a token sets the next unset bit. Each node provides a small permanent bonus applied by `fq_legacy_apply_bonuses()` (already exists). For Phase 20, the tree is linear (no branching). Rendering: show a row of 8 circles, filled = unlocked, empty = locked, with a count "Tokens: N".

**Button mapping:** Button A spends a token (if available). Button B confirms rebirth and returns to HOME with auto-save.

### Acceptance Criteria
- [ ] `fq_rebirth(ch)` in `components/game/progression.h/c`: level reset, stats halved (floored at class base), tokens earned
- [ ] `rebirth_count` saturates at 255, `legacy_points` saturates at 255
- [ ] `fq_vm_rebirth_t` defined: old_level, old_stats[4], new_level, new_stats[4], tokens_earned, tokens_available, legacy_tree, next_node. `_Static_assert` size pinned.
- [ ] `screen_rebirth.c` renders: old stats → new stats, tokens earned, legacy tree (8 circles)
- [ ] Button A spends a token on next legacy node (no-op if 0 tokens or all 32 nodes filled)
- [ ] Button B confirms rebirth and returns to HOME + auto-save
- [ ] `render_current_state()` handles REBIRTH
- [ ] Visual golden: `scene_rebirth.png`

### Negative Test Requirements
- **Rebirth at level 1:** Stats don't underflow. Class base stats are the floor.
- **rebirth_count at 255:** Saturates, no wrap.
- **legacy_points at 255:** Saturates, no wrap.
- **Spend with 0 tokens:** Button A is no-op, no underflow.
- **All 32 nodes filled:** Button A is no-op.
- **Rebirth at rebirth_count 0 (first death):** tokens earned = max(1, level/10), rebirth_count becomes 1.
