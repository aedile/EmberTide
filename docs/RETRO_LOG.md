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
