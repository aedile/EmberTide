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
