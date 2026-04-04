# Phase 19: Interactive Gameplay — Training, Items, Onboarding

## Item 1: Character Creation / Onboarding Screen

### User Story
As a new player on first boot, I want to pick my class and see my creature before the game starts, instead of being handed a hardcoded Bruiser named "Ember."

### Architecture Decisions

**FSM state addition:** `FQ_STATE_ONBOARDING` is appended at the end of `fq_app_state_t` (value 11), before `FQ_STATE_COUNT`. Update `FQ_STATE_COUNT` to 12. Verify `sizeof(fq_app_ctx_t)` and update `_Static_assert` if needed. The state value is NOT serialized in save format — no compatibility concern.

**Title-to-Onboarding routing:** In `app_main.c`, after `fq_app_init()`, check if a valid save file exists via `hal_flash_read_save()`. If no valid save: set `app.state = FQ_STATE_ONBOARDING`. If valid save: load character/inventory, set state to TITLE as before. Remove the hardcoded "Ember" character creation at `app_main.c` line 378.

**Button mapping:** Button A cycles class forward only (wraps `FQ_CLASS_COUNT - 1` → 0 via modulo). Button B confirms selection. Forward-only — no backward cycling with 2 buttons.

**Name generation:** Use a test-injectable PRNG seed (on device: seeded from `esp_random()`, in host tests: deterministic seed). Name generation is a pure function `fq_generate_name(fq_prng_t *rng, char *out, size_t max_len)` in `components/game/`. All generated names MUST be <= 11 chars and null-terminated. Names are two-part ("prefix" + "suffix" from word tables).

**Idle suppression:** Add `FQ_STATE_ONBOARDING` to `is_idle_forbidden()` — idle must not activate during class selection.

### Acceptance Criteria
- [x] New FSM state `FQ_STATE_ONBOARDING` added (value 11), `FQ_STATE_COUNT` = 12, `_Static_assert` updated
- [x] `screen_onboarding.c` renders a class selection carousel: 5 classes with name, sprite preview, and stat summary
- [x] Button A cycles class forward (wraps via modulo), Button B confirms selection
- [x] After class selection, a two-part name is generated via `fq_generate_name()` (pure function, test-injectable PRNG)
- [x] All generated names <= 11 chars and null-terminated
- [x] Character is created via `fq_character_create()` with the chosen class and generated name
- [x] Character is saved to flash immediately after creation — if save fails, state remains ONBOARDING (next boot re-enters)
- [x] FSM transitions to HOME after successful save
- [x] Subsequent boots skip onboarding (valid save file detected in `app_main.c`)
- [x] `FQ_STATE_ONBOARDING` added to `is_idle_forbidden()`
- [x] `render_current_state()` switch handles ONBOARDING case
- [x] `fq_vm_onboarding_t` view model in `view_models.h` with class_index, sprite_base, class_name, stat values

### Negative Test Requirements (all covered in test_p19_interactive_bounds.c)
- **Class index wrap:** Button A at class 4 must wrap to 0, never reach 5 or 255.
- **Power loss during onboarding:** If save fails, next boot re-enters onboarding (no half-created character).
- **Generated name bounds:** 1000 names with various seeds, all <= 11 chars, all non-empty, all null-terminated.
- **NULL/empty name guard:** Name generator must never produce empty string (would trigger infinite onboarding loop).
- **FSM state value pinned:** `FQ_STATE_ONBOARDING == 11` and `FQ_STATE_COUNT == 12`.
- **sizeof(fq_app_ctx_t) verified:** Static assert updated after enum change.

### Files Created/Modified
- `components/game/include/name_gen.h` (new — `fq_generate_name()`)
- `components/game/src/name_gen.c` (new — 16-prefix x 16-suffix word tables, ≤10 chars + null)
- `components/presentation/include/screens/screen_onboarding.h` (new)
- `components/presentation/src/screens/screen_onboarding.c` (new)
- `components/presentation/include/view_models.h` (added `fq_vm_onboarding_t`, 24 bytes)
- `main/app_fsm.h` (added `FQ_STATE_ONBOARDING = 11`, `FQ_STATE_COUNT = 12`, added ctx fields)
- `main/app_fsm.c` (ONBOARDING dispatch: A=cycle, B=confirm+save)
- `main/app_main.c` (first-boot routing, remove hardcoded Ember, idle suppression, render case)
- `main/vm_builder.h/c` (added `fq_vm_build_onboarding`)

---

## Item 2: Training Session Input Handling

### User Story
As a player in the training screen, I want to play a timing-based mini-game where I press Button A at the right moment to score hits, earning XP for my creature.

### Architecture Decisions

**Timing model (pure game logic in `components/game/`):**
New `fq_training_session_t` struct and `fq_training_step()` / `fq_training_hit()` pure functions:
- 5 targets per session, each moves across a 0-100 position range at a constant speed
- Target advances by `target_speed` per tick (tick = 50ms poll interval from main loop)
- Hit zone: positions 40-60 (center 20% of the bar)
- Hit in zone: +20 points. Miss (A outside zone): -5 points (clamped to 0). Target passes without press: 0 points.
- Score clamped to 0-100 range at all times
- After 5 targets: state = DONE, final score displayed

**XP award flow (separate from level-up):**
1. Training score → XP: `xp_award = score * 2` (max 200 XP per session)
2. Add `xp_award` to `ch->xp` directly (new helper `fq_xp_award(ch, amount)` or inline)
3. Check `ch->xp >= fq_calc_xp_to_next(ch->level)` → call `fq_level_up()` if eligible
4. `fq_level_up()` at level 99 returns `GAME_ERR_INVALID` — handle gracefully (keep XP, no crash)

**Mini-game type selection:** `fq_vm_training_t.state == 0` (WAITING) serves as the type selection screen. Button A cycles game type (Speed/Power/Intel — affects `target_speed`: Speed=10, Power=6, Intel=3). Button B starts the selected game. The `fq_vm_training_t` gains fields: `target_pos` (uint8_t 0-100), `targets_done` (uint8_t 0-5), `game_type` (uint8_t 0-2).

**PRNG isolation:** Training target timing uses a deterministic tick counter, NOT the combat PRNG. No PRNG needed for basic timing game. Verified by bound test B9 and struct inspection.

**E-paper constraint:** With 300ms partial refresh, the target "movement" will be 6 distinct positions per session rather than smooth animation. Hit zone is forgiving (20% of bar width) to compensate.

### Acceptance Criteria
- [x] `fq_training_session_t` and `fq_training_step()` / `fq_training_hit()` as pure functions in `components/game/`
- [x] 5 targets per session, score clamped 0-100, hit zone at positions 40-60
- [x] `FQ_STATE_TRAINING` state=WAITING: Button A cycles game type, Button B starts game
- [x] `FQ_STATE_TRAINING` state=ACTIVE: Button A registers hit attempt, target auto-advances per tick
- [x] After 5 targets: state=DONE, score displayed, XP awarded (`score * 2`), auto-save
- [x] Level-up triggered if XP threshold met; level 99 handled gracefully
- [x] Button B exits training at any time (partial score, partial XP award)
- [x] `fq_vm_training_t` updated with `target_pos`, `targets_done`, `game_type` fields, `_Static_assert` added
- [x] `render_current_state()` switch handles TRAINING rendering
- [x] Training logic does NOT touch `combat.rng` (PRNG isolation)

### Negative Test Requirements (all covered in test_p19_interactive_bounds.c)
- **Score clamp:** Score values 101, 200, 255 all clamp to 100 for display and XP calc.
- **XP at level 99:** Award XP, attempt level-up, assert `GAME_ERR_INVALID`, XP preserved.
- **PRNG isolation:** Combat PRNG state unchanged after training session.
- **Zero targets zero XP:** Exit immediately via B, assert 0 XP awarded.
- **Hit outside active state:** Button A during WAITING or DONE state must not modify score.

### Files Created/Modified
- `components/game/include/training_session.h` (new — `fq_training_session_t`, step/hit/award_xp API)
- `components/game/src/training_session.c` (new — tick-based timing, zero PRNG calls)
- `main/app_fsm.c` (training state button handling: type select + hit + exit)
- `main/app_main.c` (training tick integration, XP award, render case)
- `components/presentation/include/view_models.h` (updated `fq_vm_training_t`, 24 bytes)
- `components/presentation/src/screens/screen_training.c` (target bar with hit-zone tick marks)
- `main/vm_builder.h/c` (added `fq_vm_build_training_session`)

---

## Item 3: Inventory Equip/Unequip

### User Story
As a player in the inventory screen, I want to press Button A to equip or unequip the currently highlighted item, so I can customize my combat loadout.

### Architecture Decisions

**`equipped_count` semantics clarified:** `fq_character_t.equipped_count` is the MAX SLOT LIMIT (4 default, 5 with Scavenger perk), NOT the current count. Current equipped count is derived by counting non-zero entries in `equipped[5]`. The spec AC referencing "equipped_count is updated" is corrected — the max slot limit does not change during equip/unequip.

**Equip/unequip pure function in `components/game/`:**
New `game_err_t fq_equip_toggle(fq_character_t *ch, const fq_inventory_t *inv, uint8_t inv_slot)`:
- If `inv.items[inv_slot]` is already in `ch->equipped[]`: remove it (set that equipped slot to 0) — UNEQUIP
- If not equipped and current count < `ch->equipped_count`: find first zero slot, assign item ID — EQUIP
- If not equipped and all slots full: return `GAME_ERR_OVERFLOW` (caller shows "FULL")
- Item ID 0 is the empty sentinel — reject equip of item ID 0
- Duplicate-aware per-slot-relative count: `copies_in_inv_up_to_slot` vs `copies_in_equipped` prevents false "already equipped" on duplicate item IDs

**Equipped flag in view model:**
Add `uint8_t item_equipped[32]` to `fq_vm_inventory_t` — boolean flags, 1 = equipped. Builder derives these by cross-referencing `equipped[]` against `inventory.items[]`. `_Static_assert(sizeof(fq_vm_inventory_t) == 580u)`.

**Cursor navigation with 2 buttons:**
Button B cycles cursor forward (wraps at `item_count`). Double-tap B (two B presses within 6 ticks = 300ms) exits to HOME with auto-save. `inv_b_last_tick` and `inv_b_press_count` tracked in `fq_app_ctx_t`.

**Save timing:** Save on exit to HOME (double-tap B), NOT on every toggle. This protects flash endurance.

### Acceptance Criteria
- [x] `fq_equip_toggle()` pure function in `components/game/` — equip, unequip, FULL rejection, item ID 0 rejection
- [x] `FQ_STATE_INVENTORY` handles Button B as cursor cycle, Button A as equip/unequip toggle
- [x] Exit mechanism: double-tap B (≤6 ticks) returns to HOME with auto-save
- [x] `fq_vm_inventory_t` gains `item_equipped[32]` flags, `_Static_assert` updated to 580
- [x] Equipped items visually distinguished (filled 7x7 "E" marker at cell top-left)
- [x] "FULL" indicator shown in tooltip when equip attempt fails (all slots occupied, cursor item not equipped)
- [x] `equipped_count` is the MAX SLOT LIMIT (not modified during equip/unequip)
- [x] Current equipped count derived by counting non-zero `equipped[]` entries
- [x] Cursor position clamped to `item_count - 1` at all times
- [x] `render_current_state()` switch handles INVENTORY rendering (wired via `fq_vm_build_inventory_ex`)

### Negative Test Requirements (all covered in test_p19_interactive_bounds.c)
- **Full slots rejected:** Fill all equipped slots, attempt one more, assert `GAME_ERR_OVERFLOW`.
- **Item ID 0 rejected:** Attempt to equip item ID 0, assert rejection.
- **Duplicate items:** Equip item 42 from two slots, unequip once, assert one copy remains.
- **equipped_count clamp:** Set `equipped_count` to 255, run equip logic, assert no out-of-bounds access on `equipped[5]`.
- **Cursor bounds:** Set cursor to 31 with `item_count=5`, assert clamped to 4.
- **Empty inventory:** `item_count=0`, Button A does nothing (no crash, no equip).

### Files Created/Modified
- `components/game/include/equip.h` (new — `fq_equip_toggle()`)
- `components/game/src/equip.c` (new — duplicate-aware per-slot-relative equip/unequip logic)
- `main/app_fsm.c` (inventory button handling: B=cursor+double-tap-exit, A=equip)
- `main/app_main.c` (save on inventory exit, inventory render with cursor/scroll)
- `components/presentation/include/view_models.h` (updated `fq_vm_inventory_t`, 580 bytes)
- `components/presentation/src/screens/screen_inventory.c` (equipped marker, FULL detection)
- `main/vm_builder.h/c` (added `fq_vm_build_inventory_ex` with equipped flags)
