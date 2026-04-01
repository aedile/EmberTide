# Phase 6: Training & Progression

## Item 1: Implement Mini-game Logic

### User Story
As a player, I want deterministic mini-games (e.g. reflex timings or matching patterns) that generate a normalized score so I can train my beast without the random variation of real combat.

### Acceptance Criteria
- [ ] `components/game/include/training.h` defines `fq_minigame_t` logic loops with scores bounded from `0` to `100`.
- [ ] Provides functional hooks to run "Speed", "Power", or "Intel" mini-games depending on player selection.
- [ ] Incorporates integer math calculating difficulty scaling as the beast levels up.

### Negative Test Requirements (from spec-challenger)
- **Score Overflow:** Force a minigame execution where the player perfectly hits 65,000 targets (simulating a macro or long session). Assert that the math `(hits * 10) / targets` does not integer-overflow during the intermediate multiplication step before division.
- **Divide by Zero:** Trigger a minigame score evaluation where `total_targets = 0` (e.g. the game ended instantly). Ensure the logic safely returns a `0` score rather than a hardware Div-by-Zero panic.

### Implementation Steps
1. Create `training.c` with a pure functional FSM for mini-game states (Wait, Active, Success, Fail).
2. Write scoring normalization `min(100, (hits * 10) / total_targets)`.

### Test Expectations
- `test_training.c` executes the state machine for "Speed" game simulating perfect reaction times (100 score) and misses (0 score).

### Files to Create/Modify
- `components/game/include/training.h`
- `components/game/src/training.c`
- `test/host/test_training.c`

### Commit Messages
- `feat: implement mini-game functional FSMs and bounded scoring`

---

## Item 2: Implement XP Curve and Stat Point Allocation

### User Story
As an RPG player, my stats need to grow slowly at first and require massive XP later so that the end game stays challenging.

### Acceptance Criteria
- [ ] `components/game/include/progression.h` adds `fq_calc_xp_to_next(uint8_t current_level)`.
- [ ] XP formula uses integer exponentiation approximations `base * (level^2)`.
- [ ] Adds `fq_level_up(fq_character_t *c)` which subtracts XP, increments level, and adjusts base stats.

### Negative Test Requirements (from spec-challenger)
- **XP Hoarding Overflow:** Give a character exactly `4,294,967,295` XP (`UINT32_MAX`) while at level 1. Assert that invoking `fq_level_up` continuously caps them precisely at Level 99 and `MAX_XP` without wrapping back to 0.
- **Stat Cap Breach:** Force 100 consecutive level-ups on a Bruiser (biased +2 STR per level). Assert that the raw STR stat caps safely at `255` (`uint8_t`) rather than wrapping back to `0`.

### Implementation Steps
1. In `progression.c`, add the `fq_calc_xp_to_next` with predefined scalar values to prevent overflow up to level 99.
2. Implement stat distribution logic based on Class modifiers (e.g. Bruisers gain +2 STR, +1 SPD instead of uniform +1).

### Test Expectations
- `test_level_up.c` feeds 50,000 XP to a level 1 Bruiser and asserts they reach level 99 with heavily biased Strength.

### Files to Create/Modify
- `components/game/src/progression.c`
- `test/host/test_level_up.c`

### Commit Messages
- `feat: implement class-biased level up curves`

---

## Item 3: Implement Rebirth & Legacy Unlocks

### User Story
As a long-term player, when my character dies I want to permanently unlock starting bonuses ("Legacy Tree") using Rebirth Tokens based on how far I progressed.

### Acceptance Criteria
- [ ] `components/game/include/legacy.h` defines a 32-bit `legacy_unlocked_nodes` bitmask.
- [ ] Modifies `fq_character_t` creation to apply static bonuses (e.g. Node 1 = +5 max HP globally).
- [ ] Implements Rebirth Token calculation `fq_calc_rebirth_tokens(level, wins)`.

### Negative Test Requirements (from spec-challenger)
- **Token Hoarding Overflow:** Force a situation where a player generates `65535` tokens and earns 1 more. Verify `uint16_t` saturation math clamps it at `0xFFFF`.
- **Invalid Node Bit:** Pass a corrupt `legacy_unlocked_nodes` bitmask (e.g. `0xFFFFFFFF`) into the builder. Ensure the stat applicator safely ignores undefined bits without referencing out-of-bounds pointer functions or tables.

### Implementation Steps
1. Create `legacy.c`.
2. Define bitmask identifiers `LEGACY_NODE_HP_1 = (1 << 0)`, `LEGACY_NODE_GOLD_1 = (1 << 1)`.
3. Wire the evaluation into the `fq_character_create` lifecycle event.

### Test Expectations
- `test_legacy.c` asserts a character spawned while `mask = 0` has 50 HP. A character spawned while `mask = (1 << 0)` has 55 HP.

### Files to Create/Modify
- `components/game/include/legacy.h`
- `components/game/src/legacy.c`
- `components/game/src/character.c`
- `test/host/test_legacy.c`

### Commit Messages
- `feat: rebirth token calculation and bitmask persistent modifiers`
