# Phase 4: Combat Engine Core API

## Item 1: Define Combat Contexts and Setup

### User Story
As a combat designer, I need a way to initialize a combat state machine `fq_combat_ctx_t` with two fighters and a PRNG seed so I can repeatedly step through their combat logically.

### Acceptance Criteria
- [ ] `components/game/include/combat.h` defines `fq_combat_ctx_t` (opaque or explicit stack-allocatable).
- [ ] `fq_combat_init()` sets `current_round = 1`, zeroes out HP variables based on `f1->hp_max` and `f2->hp_max`.
- [ ] Determines First Attacker via initiative rolls (Call 1 and Call 2 from PRNG spec).

### Negative Test Requirements (from spec-challenger)
- **Initiative Tie-breaker Chaos:** Force exactly identical speed raw stats and force `fq_prng_range(0, 100)` to return exactly the same scalar. Ensure the logic explicitly handles the tie deterministically (e.g. Challenger always defaults 2nd) rather than endlessly looping PRNG draws and exploding stack limits.

### Implementation Steps
1. Write structural declarations for `fq_round_result_t`, `fq_combat_fighter_t`.
2. Implement `fq_combat_init` which copies fighters into the context and rolls initial PRNG values to set up Round 1.

### Test Expectations
- `test_combat_init.c` provides two mock fighters, verifies HP is full, and verifies `f1` or `f2` gained the initiative advantage deterministically based on seed `1234`.

### Files to Create/Modify
- `components/game/include/combat.h`
- `components/game/src/combat.c`
- `test/host/test_combat_stepper.c`

### Commit Messages
- `feat: implement combat state context and initialization`

---

## Item 2: Implement Round Stepper (Core Loop)

### User Story
As the game engine orchestrator, I need a stepped `fq_combat_step()` function that simulates exactly one round of combat (Attacker 1 attacks, Attacker 2 attacks, calculate damage, apply items) and returns a summary so I can render it to the UI in chunks.

### Acceptance Criteria
- [ ] `fq_combat_step(ctx)` fully executes PRNG Calls 3 through 8 (from Appendix A).
- [ ] Correctly resolves hit confirmation (attack vs dodge chance).
- [ ] Deducts appropriate HP and increments `current_round`.
- [ ] If HP drops to < 1 for either fighter, context is marked `finished = true`.

### Negative Test Requirements (from spec-challenger)
- **Ghost Round Execution:** Inject a context that `finished == true` and invoke `fq_combat_step()`. Ensure it immediately returns a NO_OP instead of continuing to roll the PRNG and inflict negative damage.
- **Negative HP Wrap Around:** Force a lethal strike of `-500` damage on `10` HP. Assert that the subtraction logic clamps HP to `0` and does not underflow an unsigned integer to `4,294,966,796`.

### Implementation Steps
1. In `combat.c`, draft the attack roll (d6 + precision modifiers).
2. Draft the dodge roll (1-100 vs speed computation).
3. If hit, sum effective strength and commit damage to context HP.
4. Duplicate for the responding fighter's strike.

### Test Expectations
- `test_combat_stepper.c` steps exactly once and verifies raw damage subtraction for seed `42`.

### Files to Create/Modify
- `components/game/src/combat_math.c` (Internal helper)
- `components/game/src/combat.c`

### Commit Messages
- `feat: implement fq_combat_step single-round execution`

---

## Item 3: Tactical Rerolls & Overtime

### User Story
As a competitive player, my intelligence stat should automatically shield me from bad rolls (rerolling attacks <= 2), and matches exceeding 8 rounds must force pure damage to prevent endless stalemates.

### Acceptance Criteria
- [ ] `fq_combat_step` respects Intelligence-based reroll heuristic limits (max charges = `eff_int / 4`).
- [ ] If `round > 8`, both fighters suffer `(round - 8)` pure overtime damage at the end of the round.
- [ ] Implements the `Lucky Star` 5% bonus attack check (PRNG loops 9-12).

### Negative Test Requirements (from spec-challenger)
- **Endless Reroll Recursion:** Provide a `test_mock_prng.c` setup that continually returns `1`. Ensure the Intelligent Reroll strictly aborts when `charges == 0` instead of recursing indefinitely.
- **Round Overflow:** Force the loop to exactly round `65535` (max `uint16_t`). Assert the increment logic clamps to `65535` instead of wrapping to round `0` and resetting the overtime penalty.

### Implementation Steps
1. Integrate reroll checks (Call 5 and 8) using conditional execution logic.
2. In the cleanup phase of `fq_combat_step`, apply overtime HP deduction.
3. Call the Lucky Star PRNG evaluation regardless of item existence (to preserve sync).

### Test Expectations
- `test_combat_overtime.c` forces combat to round 10 and checks that fighters take 2 pure damage automatically.

### Files to Create/Modify
- `components/game/src/combat.c`
- `test/host/test_combat_overtime.c`

### Commit Messages
- `feat: implement auto-rerolls and overtime penalty`
