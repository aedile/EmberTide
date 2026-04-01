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
