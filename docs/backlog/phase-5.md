# Phase 5: Item Engine & Synergies

## Item 1: Define Item Evaluation Loop

### User Story
As a combat logic engineer, I need a pluggable architecture to evaluate equipped "Jokers" exactly at predefined lifecycle triggers (ON_ATTACK, ON_DEFEND, ON_ROUND_START) synchronously during the combat round stepper.

### Acceptance Criteria
- [ ] `components/game/include/item_engine.h` defines conditional triggers and execution rules.
- [ ] `item_eval_trigger(ctx, TRIGGER_ENUM)` is invoked by `combat.c` at appropriate lifecycle boundaries.
- [ ] Items execute strictly in slot order (0 -> 4), resolving defender items entirely before attacker items.

### Negative Test Requirements (from spec-challenger)
- **Dead Slot Evaluation:** Pass an inventory containing slots `[0, EMPTY, 0, EMPTY, 0]`. Ensure the logic securely skips empty slots without dereferencing a Null-ID into the item metadata table.
- **Runaway Chain Interaction:** Provide 2 identical items that trigger conditionally "ON_HEAL -> HEAL". Assert that the engine limits recursion to 1 cycle per trigger boundary so it doesn't infinite loop.

### Implementation Steps
1. Add `item_eval_trigger` integration to `combat.c`.
2. Write a minimal rules engine in `item_engine.c` looping over `equipped_items[5]` and checking `.trigger == current_trigger`.

### Test Expectations
- `test_item_engine.c` supplies a passive "+1 damage" mock item, attacks, and asserts damage is base + 1.

### Files to Create/Modify
- `components/game/include/item_engine.h`
- `components/game/src/item_engine.c`
- `components/game/src/combat.c`
- `test/host/test_item_engine.c`

### Commit Messages
- `feat: build Joker item engine evaluation loop`

---

## Item 2: Implement Common/Uncommon Modifiers

### User Story
As a player, basic items like "Iron Fist" (+1 damage) and "Vampire Fang" (heal 5 HP on kill) need to functionally map to their design spec.

### Acceptance Criteria
- [ ] Complete functional mapping of Items ID 001-015 and 101-112 into massive `switch` statements or function pointers evaluated by the engine.
- [ ] Vampire Fang properly restores HP if the opponent state transitions to `< 1` this step.
- [ ] Haymaker correctly sets multiplier to x2 on a confirmed crit.

### Negative Test Requirements (from spec-challenger)
- **Heal Overflow:** Player has `MAX_HP=50` and is missing 1 HP. Vampire Fang heals `+5`. Ensure the HP caps strictly at `50` and does not exceed MAX bounds.
- **Double Modifier Crash:** Equip 2 "Haymakers" increasing crit multiplier. Limit test that multiplier applies additively (x3) or specifically overrides safely without floating point corruption.

### Implementation Steps
1. Expand condition parser inside `item_engine.c`.
2. Expand effect applicator inside `item_engine.c` mutating the `fq_combat_ctx_t`.

### Test Expectations
- Unit tests for "Bandage" (heals 1 HP if below 50% at ON_ROUND_END).

### Files to Create/Modify
- `components/game/src/item_engine.c`

### Commit Messages
- `feat: hardcode evaluation handlers for Common/Uncommon jokers`

---

## Item 3: Implement Complex Items (Chaos Orb, Time Loop)

### User Story
As a high-tier player, unpredictable legendary items like "Chaos Engine" and "Time Loop" must execute safely without permanently mutating my base stats or desyncing the PRNG.

### Acceptance Criteria
- [ ] "Chaos Orb" (204) properly swaps an effective stat only for the duration of the round context, calling the PRNG for random selection.
- [ ] "Time Loop" (207) records an HP snapshot at round 3 inside the `fq_combat_ctx_t` and forces both fighters' HP to match it at round 6 end.

### Negative Test Requirements (from spec-challenger)
- **Time Loop Snapshot Erase:** Assume the battle ends in Round 2. Does the `round_3_hp_snapshot` variable leak uninitialized memory if serialized? Test that it initializes completely to `0` during `fq_combat_init`.
- **Chaos Orb Sync Break:** Ensure the random selection is derived EXCLUSIVELY from the shared synchronous PRNG sequence, and not localized hardware ticks (`esp_timer_get_time()`), preventing devastating match desyncs globally. Check this via the `test_combat_sync` module.

### Implementation Steps
1. Add `round_3_hp_snapshot` variables to `fq_combat_ctx_t`.
2. Write distinct item handlers for 204 and 207 asserting correct constraint.

### Test Expectations
- `test_item_time_loop.c` forces item 207 into slot 1, damages fighters in rounds 4, 5, and confirms restoration to round 3 states at round 6.

### Files to Create/Modify
- `components/game/src/item_engine.c`
- `test/host/test_item_time_loop.c`

### Commit Messages
- `feat: implement complex Time Loop and Chaos engine items`
