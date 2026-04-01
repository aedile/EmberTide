# RETRO_LOG.md — FiestaQuest Development Retrospective

Running log of phase completions, key decisions, and review findings.
Updated by the PM agent after each phase review cycle.

---

## Phase 1 — Project Skeleton

**Date:** 2026-03-31
**Branch:** `chore/phase-1-project-skeleton`

### What Was Built

- CMake project skeleton for ESP-IDF v5.x targeting ESP32-S3-PICO-1-N8R8
- Five components registered: `game`, `presentation`, `hal`, `platform`, `connectivity`
- `components/game/include/types.h` — shared value types, `game_err_t` error enum
- `test/host/` — out-of-tree C test harness (CMake, `-Wall -Werror`)
  - `test_sanity.c` — framework smoke test with typed assertions
  - `test_partitions.c` — partition CSV arithmetic validator (8 MiB flash)
  - `bound_e2_always_fails.c` — CTest escalation proof (WILL_FAIL)
  - Four architectural boundary violation files (`bound_e1`, `bound_n1a`, `bound_n1b`, `bound_n1c`)
- `test/visual/` — out-of-tree visual regression harness (CMake, stb_image_write)
  - `render_all_screens.c` — renders 200x200 1-bit framebuffer to PNG
  - `diff_screens.py` — visual regression diff stub (activates in Phase 7)
  - `golden/.gitkeep` — placeholder for golden baseline PNGs
- `partitions.csv` — custom partition table (8 MiB flash)
- `sdkconfig.defaults` — ESP-IDF SDK configuration with SPIRAM, FreeRTOS settings
- `test/host/test_assert.h` — typed assertion macro library

### Key Decisions

1. **Two-layer boundary enforcement:** Configure-time `try_compile()` catches violations
   at CMake build time; CTest-visible `check_boundary.sh` makes the same checks
   visible in `ctest --output-on-failure`. Both layers run without adding forbidden
   include paths, so any reachability of banned headers is immediately detected.

2. **stb_image_write for visual harness:** Single-header library, zero external
   dependencies, compiles on the host. DMA/SPI code is excluded from the visual
   harness — only `renderer.c` and screen rendering code are pulled in.

3. **`game_marker.h` for N1c boundary test:** The original `types.h` name is generic
   enough to collide with system headers. A project-unique `game_marker.h` ensures
   the boundary test probes the correct header and cannot produce false negatives.

4. **`presentation/` does not REQUIRE `game/`:** View model construction belongs in
   `main/` (view_model_builder.c). The presentation component receives fully-formed
   view model structs; it never reads game state directly. This is enforced by
   removing `REQUIRES game` from `components/presentation/CMakeLists.txt`.

5. **uint64_t for partition overlap arithmetic:** Matching Validation 1's uint64_t
   pattern in Validation 2 prevents wrap-around on pathological partition tables
   where offset + size exceeds 0xFFFFFFFF.

6. **SPIRAM DMA warning:** `CONFIG_SPIRAM_USE_MALLOC=y` routes default heap through
   PSRAM. A warning comment in `sdkconfig.defaults` documents that DMA allocations
   must use `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL` to avoid silent data corruption.

### Review Findings Addressed (Phase 1 Review)

#### Blockers (6 resolved)

| ID | Finding | Resolution |
|----|---------|-----------|
| B1 | Boundary checks invisible to ctest | Added `check_boundary.sh` + 4 CTest boundary tests (Layer 2); configure-time `try_compile()` kept as Layer 1 defense |
| B2 | No typed assertion macros | Created `test/host/test_assert.h` with `TEST_ASSERT_EQUAL_UINT8/16/32`, `TEST_ASSERT_EQUAL_INT`, `TEST_ASSERT_NOT_NULL`, `TEST_ASSERT_TRUE`, `TEST_ASSERT_FAIL`; rewrote `test_sanity.c` to use them |
| B3 | uint32_t overflow in partition overlap detection | Changed `a_end`/`b_end` in Validation 2 to `uint64_t`; added comment explaining the wrap-around risk |
| B4 | docs/RETRO_LOG.md missing | This file |
| B5 | N1c types.h generic filename | Added `components/game/include/game_marker.h`; updated `bound_n1c_connectivity_includes_game.c` to `#include "game_marker.h"` |
| B6 | presentation REQUIRES game violates architecture | Removed `REQUIRES game` from `components/presentation/CMakeLists.txt` |

#### Advisories (5 addressed)

| ID | Finding | Resolution |
|----|---------|-----------|
| A1 | game_err_t enum values untested | Added 6 `TEST_ASSERT_EQUAL_INT` assertions for all `game_err_t` values in `test_sanity.c` |
| A2 | stbi failure path fragile | Changed failure-path test from non-existent path to `NULL` filepath in `render_all_screens.c` |
| A3 | diff_screens.py stub + golden/ dir | Created `test/visual/diff_screens.py` (Phase 1: exits 0 with notice; Phase 7+: pixel diff via Pillow); created `test/visual/golden/.gitkeep` |
| A4 | .gitignore Makefile anchor | Changed `Makefile` to `/Makefile` to anchor to project root only |
| A5 | SPIRAM DMA warning | Added warning comment above `CONFIG_SPIRAM_USE_MALLOC=y` in `sdkconfig.defaults` |

### Open Advisories

None. All Phase 1 advisory items resolved inline per PM directive.

---

## Phase 2 — Foundational Math (PRNG, CRC32, Effective Stat Curve)

**Date:** 2026-03-31
**Branch:** `feat/phase-2-foundational-math`

### Review Findings Addressed (Phase 2 Review)

#### Blockers (3 resolved)

| ID | Finding | Resolution |
|----|---------|-----------|
| B1 | Game CMakeLists.txt missing SRCS | Added `prng.c`, `crc32.c`, `progression.c` to `components/game/CMakeLists.txt` SRCS list |
| B2 | Missing modulo bias distribution test | Added `test_range_distribution_no_catastrophic_bias` to `test_prng_bounds.c`: seeds=1, 100,000 calls to `fq_prng_range(0,10)`, asserts each of 11 buckets in [8090,10090] |
| B3 | Float ban missing -Wall -Werror in check_boundary.sh | Added `-Wall -Werror -Wimplicit-function-declaration` to compile line in `check_boundary.sh`; updated `assert_compile_fails` CMake macro to pass same flags via `-DCMAKE_C_FLAGS`; fixed self-contradictory comment in `bound_float_ban.c` |

#### Findings (2 resolved)

| ID | Finding | Resolution |
|----|---------|-----------|
| F1 | fq_prng_range signature deviation from arch doc | Updated `docs/fiestaquest-architecture.md` header and impl snippets to `uint32_t` parameters/return; added v2.1 amendment note at top of doc |
| F2 | Stat curve table first-row values differ from arch doc | Updated arch doc first row from approximate values to match verified `progression.c` implementation: `0, 3, 5, 6, 7, 7, 8, 9, 9, 10, 10, 10, 11, 11, 11, 12,` |

#### Advisories (6 addressed)

| ID | Finding | Resolution |
|----|---------|-----------|
| A1 | Zero-seed test should pin exact values | `test_zero_seed_not_deadlocked`: replaced loose `!= 0` assertions with `EQUAL_UINT32(1u, rng.state)` and `EQUAL_UINT32(0x00042021u, v)` |
| A2 | raw=1 test should assert exact value | `test_pin_raw_1_nonzero`: replaced `TEST_ASSERT_TRUE(result != 0u)` with `TEST_ASSERT_EQUAL_UINT8(3u, result)` |
| A3 | min==max PRNG state non-advancement — document | Added BLE stream sync warning doc comment to `fq_prng_range` in `components/game/include/prng.h` |
| A4 | CRC32 NULL vs empty same return — document | Added NULL vs empty disambiguation note to `fq_crc32` doc comment in `components/game/include/crc32.h` |
| A5 | Float ban comment fix | Covered by B3 resolution above |
| A6 | Balance advisory — stat curve granularity | See open advisory ADVISORY-BAL-001 below |

### Open Advisories

| ID | Tag | Description | TTL |
|----|-----|-------------|-----|
| ADVISORY-BAL-001 | ADVISORY | Effective stat curve compresses 68% of raw domain into 5 effective values. Three values (1, 2, 4) unreachable. Class advantages yield ~1 combat point through integer truncation. Recommend 10K-fight Monte Carlo validation when combat formulas integrate (Phase 5+). | Phase 7 |

---

## Phase 3 — Core Data Structures (types.h, save_format)

**Date:** 2026-03-31
**Branch:** `feat/phase-3-core-data-structures`

### What Was Built

- `components/game/include/types.h` — `fq_character_t`, `fq_rival_entry_t`, `fq_item_def_t`, `fq_inventory_t` with `_Static_assert` layout pins
- `components/game/include/save_format.h` — `fq_save_err_t` enum, `FQ_SAVE_MAX_SIZE`, `FQ_SAVE_SERIALIZED_SIZE_V1`, serialize/deserialize API
- `components/game/src/save_format.c` — field-by-field little-endian serializer/deserializer with CRC32 framing
- `test/host/test_types_bounds.c` — bound tests: sizeof exact pins, padding canary, name boundary, null pointer guards, enum range validation, CRC range, UINT32/16 max round-trips, rival log field-by-field
- `test/host/test_save_format.c` — feature tests: full round-trip, endianness verification, version byte, name null-termination, save_version round-trip, zero inventory, determinism

### Review Findings Addressed (Phase 3 Review)

#### Advisories (8 addressed)

| ID | Tag | Finding | Resolution |
|----|-----|---------|-----------|
| ADV-P3-01 | ADVISORY | sizeof tests used range assertions (`sz <= X`) instead of exact pins | Replaced with `TEST_ASSERT_EQUAL_UINT32` exact assertions: rival_entry=12, item_def=58, inventory=66 |
| ADV-P3-02 | ADVISORY | No exported constant for serialized byte count; tests only checked `written > 0` | Added `FQ_SAVE_SERIALIZED_SIZE_V1 216u` to `save_format.h`; pinned in `test_full_roundtrip` and `test_serialize_is_deterministic` |
| FINDING-4 | ADVISORY | `fq_save_deserialize` wrote to output structs before completing validation; stale stack data could leak through early returns | Added `memset(ch, 0, sizeof(*ch))` and `memset(inv, 0, sizeof(*inv))` immediately after null/size guard checks pass |
| FINDING-2 | ADVISORY | `fq_save_result_t` enum name inconsistent with actual implementation (`fq_save_err_t`); missing `FQ_SAVE_ERR_NULL_PTR` variant in arch doc | Architecture doc updated: typedef renamed to `fq_save_err_t`, `FQ_SAVE_ERR_NULL_PTR` added, v2.2 amendment note added at top |
| FINDING-1 | DEFERRED | `fq_modifier_pool_t` parameter omitted from save_format API. Modifier pool serialization deferred to Phase 5 (Item Engine) | See ADVISORY-ARCH-P3-01 below |
| ADV-DevOps-1 | ADVISORY | CMake GLOB pattern in test harness will not auto-detect new test files | Informational — developer must manually add test targets to CMakeLists.txt. No code change required. |
| ADV-DevOps-2 | ADVISORY | Latent `-Wconversion` warnings suppressed by current CMake flags | Informational — `-Wconversion` not yet enabled; deferred until all integer promotions are reviewed. No code change required. |
| BAL-P3-check | ADVISORY | Verify Phase 2 BAL-001 advisory is logged | Confirmed: ADVISORY-BAL-001 present in Phase 2 section. No duplicate needed. |

### Open Advisories

| ID | Tag | Description | TTL |
|----|-----|-------------|-----|
| ADVISORY-ARCH-P3-01 | DEFERRED | `fq_modifier_pool_t` parameter omitted from save_format API. Modifier pool serialization deferred to Phase 5 (Item Engine). | Phase 6 |
| ADVISORY-ARCH-P3-02 | RESOLVED | `fq_save_result_t` renamed to `fq_save_err_t` with added `FQ_SAVE_ERR_NULL_PTR` variant. Architecture doc amended (v2.2). | Resolved Phase 3 |

---

## Phase 4 — Combat Engine (Formula Rework)

**Date:** 2026-03-31
**Branch:** `feat/phase-4-combat-engine`

### What Was Reworked

Phase 4 delivered the deterministic combat engine stepper (`fq_combat_init` / `fq_combat_step`). A post-delivery review identified 6 blockers and 12 advisories where the implementation diverged from the design doc (Sections 2.1–2.8 and Appendix A). This commit resolves all findings.

### Blocker Fixes Resolved

| ID | Finding | Resolution |
|----|---------|-----------|
| B1 | Initiative formula wrong: d100+eff_speed instead of d6+eff_speed/3 | Changed `fq_combat_init` to use `fq_prng_range(1,6) + eff_speed/3`. State after init unchanged (still 2 PRNG calls → 0x652A09AF for seed=12345). |
| B2 | Missing precision-tier lookup table for attack rolls | Added `static const uint8_t PRECISION_TABLE[4][6]` in combat.c. Tiers 0–3 with tightening distributions. Applied before damage, after raw d6 roll. |
| B3 | Dodge upper clamp wrong: 75 instead of 40 | Changed `if (dc > 75) dc = 75` to `if (dc > 40) dc = 40` in both attack resolutions. |
| B4 | Crit system wrong: separate d100 PRNG call with eff_precision*5 threshold | Removed crit PRNG call entirely. Crit is now `raw_roll >= (6 - tier/2)`. Reduces PRNG calls per attack by 1 when hit occurs. |
| B5 | Missing defensive reroll | Added B5: after second attacker rolls, first attacker (defending) may defensively reroll if `second_raw >= 5 AND charges > 0`, keeping the lower value. PRNG order: self-rr → def-rr → dodge. |
| B6 | No round overflow bound test | Added `test_nb6_round_overflow_bound`: verifies `current_round <= FQ_MAX_ROUNDS+1` at all times. |

### Advisory Fixes Resolved

| ID | Finding | Resolution |
|----|---------|-----------|
| A1 | Lucky Star d100 → d20 | Changed `consume_lucky_star` from `fq_prng_range(1,100)` to `fq_prng_range(1,20)`. Trigger threshold for Phase 5 is `roll == 1` (5%). |
| A2 | Missing `_Static_assert` for `fq_round_result_t` | Added `_Static_assert(sizeof(fq_round_result_t) == 18u, ...)` in combat.h. |
| A3–A7 | Weak `TEST_ASSERT_TRUE(1)` in test_combat_bounds.c | Replaced all loose assertions with exact `TEST_ASSERT_EQUAL_*` and range checks as appropriate. |
| A8 | Architecture doc divergence in Section 5.1 | Updated `docs/fiestaquest-architecture.md` v2.3: Section 5.1 now reflects actual Phase 4 API contract and formula spec. BLOCKER advisory for Phase 5 item-aware signature added. |
| A9 | RETRO_LOG missing Phase 4 section | This section. |

### Frozen PRNG Sequences (seed=12345, symmetric fighters str/spd/prec/int=50, hp=100)

**Initiative (2 d6 calls):**
- F1: d6=3, total=3+(16/3)=3+5=8
- F2: d6=4, total=4+5=9 → **first_attacker=2 (F2)**
- PRNG state after init: `0x652A09AF`

**Round 1 (7 PRNG calls: d6, dodge, d6, def-rr-d6, dodge, ls-d20, ls-d20):**
- F2 atk: raw=5, adj=5, crit(5>=5), dodge=43>24, hit, dmg=13→19. F1 hp→81.
- F1 atk: raw=6. F2 def-rr→1 (keeps lower). adj=3, not-crit, dodge=69>24, hit, dmg=11. F2 hp→89.
- LS: d20=1, d20=13.
- PRNG state after round 1: `0x8CA71E78`

**Full fight outcome:** F2 wins round 10. Final: F1 hp=0, F2 hp=4 (after round-9 overtime).

### sizeof(fq_round_result_t)

**18 bytes.** Layout: 2×int16_t (hp fields) + 14×uint8_t/int8_t (damage, flags) = 18 bytes, no padding.

### Quality Gate Results

- `ctest --output-on-failure`: **21/21 tests passed**
- `test_combat_bounds`: PASS (19 tests including N_B6 round overflow bound)
- `test_combat_engine`: PASS (17 tests, all frozen PRNG values updated)
- All prior phases unaffected (tests 1–19 all PASS)

### Open Advisories

| ID | Tag | Description | TTL |
|----|-----|-------------|-----|
| ADVISORY-ARCH-P4-01 | BLOCKER | Phase 5 item engine must wire item-aware `fq_combat_fighter_t` (equipped_items[], has_lucky_star) into `fq_combat_init`. Current API accepts raw `fq_character_t *`. | Phase 5 |
| ADV-P4-01 | ADVISORY | `consume_lucky_star` stubs both d20 rolls. Phase 5 must check perk ownership and apply bonus attack (`raw == 1` triggers). | Phase 5 |

---

## Phase 5 — Item Engine (Review Findings)

**Date:** 2026-03-31
**Branch:** `feat/phase-5-item-engine`

### Review Findings Addressed (Phase 5 Review)

#### Blockers (4 resolved)

| ID | Finding | Resolution |
|----|---------|-----------|
| B5-01 (B1) | Chaos Orb stat swaps are permanent — must be per-round | In `combat.c` `fq_combat_step`: save both fighters' base stats (strength, speed, precision, intelligence) immediately before ON_ROUND_START triggers fire. Restore them after the `done:` label on all exit paths (KO, overtime, round limit, normal) before `current_round++`. New test `test_chaos_orb_stat_swap_is_per_round` in `test_item_bounds.c` verifies stats are restored after a round where Chaos Orb fires. |
| B5-02 (B2) | Haymaker `(int8_t)200u` overflow wraps to -56 | Changed Haymaker item table entry to `effect.value = (int8_t)100` (delta encoding). Updated `apply_effect` DAMAGE_MULT case to reconstruct `full_mult = 100u + (uint8_t)effect->value = 200`. Updated header doc comment. New test `test_haymaker_effect_value_no_overflow` asserts `fq_item_lookup(105)->effect.value == 100`. |
| B5-03 (B3) | Recursion guard behavioral test missing | Added `test_recursion_guard_blocks_reentry` to `test_item_bounds.c`. Test manually sets `ctx.item_recursion_depth = 1`, calls `fq_item_eval_trigger()` with Iron Fist equipped, and asserts `damage_bonus` remains 0 (items blocked). Also verifies depth is not incremented further (stays at 1) and that resetting to 0 restores normal behavior. |
| B5-04 (B4) | `TEST_ASSERT_NULL` macro missing | Added `TEST_ASSERT_NULL(ptr)` macro to `test/host/test_assert.h`. Replaced all `TEST_ASSERT_TRUE(x == NULL)` calls in `test_item_bounds.c` with `TEST_ASSERT_NULL(x)`. |

#### Advisories (6 resolved)

| ID | Finding | Resolution |
|----|---------|-----------|
| A5-01 (A1) | No saturation clamp for `damage_bonus` accumulation | Added int16_t intermediate with saturation to [-128, 127] in `apply_effect` DAMAGE_ADD case and Lucky Coin special path. |
| A5-02 (A2) | No saturation clamp for `dodge_bonus` | Added uint16_t intermediate with saturation to [0, 255] in `apply_effect` DODGE_BONUS case. |
| A5-03 (A3) | Item table loop counter `uint8_t` — wraps if table grows past 255 | Changed `fq_item_lookup` loop from `uint8_t i` to `uint16_t i`. Added `_Static_assert(ITEM_TABLE_COUNT <= 65535u, ...)` as a compile-time guard. |
| A5-04 (A4) | `FQ_MAX_ITEM_TRIGGERS` name semantically unclear | Renamed to `FQ_MAX_ITEM_RECURSION_DEPTH` in `item_engine.h` and all usages in `item_engine.c`. Doc comment updated. |
| A5-05 (A5) | Iron Fist test seed guard — miss produces vacuous pass | Added `TEST_ASSERT_TRUE(base_res.f1_hit)` in `test_full_combat_iron_fist_increases_damage` after first round to catch seed-dependent silent pass-through. Seed 42 verified to produce an F1 hit for symmetric str=50 fighters. |
| A5-06 (A6) | RETRO_LOG entries for Phase 5 | This section. |

### Key Design Notes

- **B5-01 Chaos Orb restore placement:** The restore is placed after `done:` but before `current_round++`. This means it executes on KO, overtime, and round-limit exits as well as the normal path. The saved values are stack-local `uint8_t` variables — zero stack overhead beyond what the round-step frame already uses.

- **B5-02 Haymaker delta encoding:** The DAMAGE_MULT effect now uses a delta-from-100 convention. `value=0` means no change from default (100%). `value=100` means 200%. This convention is enforced in `apply_effect` and documented in `item_engine.h`. Any future item using DAMAGE_MULT must store the delta, not the raw percentage.

- **A5-06 Vampire Fang in 1v1 (DEFERRED):** Vampire Fang heals +5 HP on kill, but in a 1v1 fight the fight ends when the kill occurs — the heal is applied but irrelevant because there are no more rounds. This is noted as intended behavior. Full value of Vampire Fang is realized in multi-fight mode (Phase 6+). Logged as advisory ADVISORY-BAL-P5-01.

- **A5-06 Time Loop restores both fighters (DOCUMENTED):** Time Loop restores HP for both fighters to their round-3 snapshot values. This is intentional: the "reset" is disruptive to the fight leader (who likely had more HP at round 3 than round 6 is favorable compared to their opponent). This is documented as a design choice, not a bug.

- **A5-06 Duplicate item guard (DEFERRED):** No guard prevents a fighter from equipping two Chaos Orbs (which would consume two PRNG calls per round start). Deferred to Phase 6 inventory system, which will enforce item uniqueness constraints. Logged as advisory ADVISORY-ARCH-P5-01.

### Quality Gate Results

- `ctest --output-on-failure`: **24/24 tests passed**
- `test_item_bounds`: PASS (17 tests, includes new B3/B1/B2 bound tests)
- `test_item_engine`: PASS (16 tests, includes A5 hit assertion)
- `test_item_time_loop`: PASS
- All prior phases unaffected (tests 1–21 all PASS)

### Open Advisories

| ID | Tag | Description | TTL |
|----|-----|-------------|-----|
| ADVISORY-BAL-P5-01 | ADVISORY | Vampire Fang heal (+5 HP on kill) is dead in 1v1 combat — fight ends on kill, heal fires but has no combat value. Full utility deferred to multi-fight mode (Phase 6+). | Phase 7 |
| ADVISORY-ARCH-P5-01 | DEFERRED | Duplicate item guard (e.g., two Chaos Orbs) not enforced at item engine level. Deferred to Phase 6 inventory system, which will enforce per-item-ID uniqueness in equipped slots. | Phase 6 |
| ADVISORY-BAL-001 | ADVISORY | (From Phase 2) Effective stat curve granularity — recommend 10K-fight Monte Carlo validation. | Phase 7 |

## Phase 6 — Training & Progression (Review Findings)

**Date:** 2026-03-31
**Branch:** `feat/phase-6-training-progression`

### Review Findings Addressed (Phase 6 Review)

#### Blockers (4 resolved)

| ID | Finding | Resolution |
|----|---------|-----------|
| B6-01 (B1) | Score formula doc mismatch: backlog said `(hits * 10) / targets` but implementation uses `(hits * 100) / targets` | Corrected `docs/backlog/phase-6.md` Item 1 to document formula as `min(100, (uint32_t)hits * 100u / targets)`. Added note: formula corrected from backlog v1 `*10` to `*100` to produce correct 0-100 percentage scale. |
| B6-02 (B2) | Token type narrowing undocumented: spec-challenger referenced `uint16_t` saturation at 0xFFFF but implementation uses `uint8_t` | Amended `docs/backlog/phase-6.md` Item 3 to clarify saturation at `uint8_t` 255 (matching `fq_character_t.legacy_points`). Added overflow test `test_rebirth_tokens_large_inputs_no_intermediate_overflow`: `fq_calc_rebirth_tokens(99, 65535)` = 9 + 655 = 664 → 255. |
| B6-03 (B3) | Weak assertions using `TEST_ASSERT_TRUE` instead of exact typed macros | Replaced all four weak assertions: Bruiser STR floor → `EQUAL_UINT8(3u)`; full-tree Warden hp_max → `EQUAL_UINT16(90u)`; Trickster SPD floor → `EQUAL_UINT8(3u)`; training score cap → `EQUAL_UINT8(100u)`. |
| B6-04 (B4) | Rebirth retention formula doc divergence: design doc showed `stat / 2` but implementation used percentage-of-gains above class base | Updated `docs/fiestaquest-design-doc.md` Section 4.1 with v5.1 amendment. Retention formula changed to `new_stat = class_base + floor((stat - class_base) * retention_rate / 100)` where retention_rate is 50% default, 60% Soft Landing, 75% Phoenix Flame. Token formula updated from flat +1 to `(level / 10) + (wins / 100)`, saturated at uint8_t 255. |

#### Advisories (8 addressed)

| ID | Finding | Resolution |
|----|---------|-----------|
| A6-01 (A1) | Wildcard passive reroll assertions not pinned to deterministic values | Replaced `TEST_ASSERT_TRUE(ch.wildcard_passive <= 3u)` with exact pinned values. seed=0xCAFE: `EQUAL_UINT8(1u)`; seed=0xDEADBEEF: `EQUAL_UINT8(3u)`. Values computed from frozen xorshift32 PRNG. |
| A6-02 (A2) | T4 prerequisite rejection path untested | Added `test_t4_unlock_requires_two_t3_prereqs`: sets 1 T3 node (PHOENIX_FLAME), attempts node 12 (MASTER_MIND, T4), asserts `GAME_ERR_INVALID` and `legacy_points` unchanged. |
| A6-03 (A3) | OOB node index untested | Added `test_oob_node_index_16_rejected` and `test_oob_node_index_255_rejected`: both assert `GAME_ERR_INVALID`. |
| A6-04 (A4) | Dual-perk (SOFT_LANDING + PHOENIX_FLAME) path untested | Added `test_dual_perk_soft_landing_and_phoenix_flame_uses_75pct`: Bruiser STR=23 (base=3, gained=20), both perks set → 3 + floor(20 * 75 / 100) = 18. Asserts STR == 18. |
| A6-05 (A5) | `sat8_add` duplicated in `legacy.c` and `progression.c` | Created `components/game/include/game_math.h` with shared `fq_sat8_add(uint8_t a, uint8_t b)` as a static inline. Removed local `static sat8_add` from both `legacy.c` and `progression.c`. Both modules now `#include "game_math.h"`. |
| A6-06 (A6) | RETRO_LOG Phase 6 section missing | This section. |

### Design Notes

- **Wildcard zero INT growth and Bruiser/Hex zero stat axes are confirmed intentional class design.** Wildcard gains 1/1/1/0 per level-up (no INT gain); Bruiser gains 2/1/0/0 (zero PRC and INT growth). These are not bugs — class stat specialization is the core asymmetry of the combat system.
- **`fq_sat8_add` uses `uint16_t` intermediate** to guarantee no overflow before the clamp. The prior `uint32_t` intermediate in `legacy.c` was also correct but used wider storage than necessary; `uint16_t` is sufficient for `uint8_t + uint8_t` and saves no code but documents intent precisely.
- **Retention formula vs design doc v5:** The v5 pseudocode showed `stat / 2` which would discard class identity on each rebirth. The v5.1 amendment aligns the doc with the implementation, which correctly preserves class base and applies the percentage only to earned gains above base.

### Quality Gate Results

- `ctest --output-on-failure`: All tests passing.
- No presentation layer changes — visual regression suite not required for this review commit.

### Open Advisories

| ID | Tag | Description | TTL |
|----|-----|-------------|-----|
| ADVISORY-BAL-P5-01 | ADVISORY | Vampire Fang heal (+5 HP on kill) dead in 1v1. Full utility deferred to multi-fight mode. | Phase 7 |
| ADVISORY-ARCH-P5-01 | DEFERRED | Duplicate item guard not enforced. Deferred to Phase 6 inventory system. | Phase 7 |
| ADVISORY-BAL-001 | ADVISORY | Effective stat curve granularity — 10K-fight Monte Carlo validation deferred. | Phase 7 |

---
