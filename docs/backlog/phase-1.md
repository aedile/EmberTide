# Phase 1: Project Skeleton & Toolchain Initialization

## Item 1: Establish CMake Root and Entrypoint

### User Story
As a developer, I need a standard ESP-IDF v5 CMake project structure with separate `game`, `presentation`, `hal`, and `connectivity` components so I can enforce architectural boundaries from day one.

### Acceptance Criteria
- [ ] Project root contains a valid `CMakeLists.txt` for ESP-IDF.
- [ ] `main/app_main.c` exists with an empty `app_main()` function.
- [ ] Component directories `game`, `presentation`, `hal`, `connectivity` exist with minimal `CMakeLists.txt` files mapping their includes.

### Negative Test Requirements (from spec-challenger)
- **Boundary Restriction:** A test compilation MUST be written that attempts to include a `<hal_*.h>` file from inside the `game` component. Ensure `idf.py build` explicitly FAILS with a missing header error, proving the CMake boundaries are water-tight.

### Implementation Steps
1. Create `CMakeLists.txt` specifying `project(fiestaquest)`.
2. Create directories: `components/game/include`, `components/game/src`, and matching `CMakeLists.txt`.
3. Repeat for `presentation`, `hal`, and `connectivity`.
4. Create `main/app_main.c` and `main/CMakeLists.txt`.

### Test Expectations
- `idf.py build` succeeds without errors for the default target.

### Files to Create/Modify
- `CMakeLists.txt` (Root)
- `main/app_main.c`
- `main/CMakeLists.txt`
- `components/game/CMakeLists.txt`
- `components/presentation/CMakeLists.txt`
- `components/hal/CMakeLists.txt`
- `components/connectivity/CMakeLists.txt`

### Commit Messages
- `chore: init ESP-IDF project skeleton and component boundaries`

---

## Item 2: Setup `test/host` CTest suite

### User Story
As a developer practicing TDD, I need a host-compiled C testing environment (CTest) so I can validate my game logic instantly without flashing to an ESP32.

### Acceptance Criteria
- [ ] `test/host/` directory exists with its own `CMakeLists.txt` configured as a standard host executable.
- [ ] The host test executable links the `game` component successfully.
- [ ] A dummy test `test_sanity.c` executes and passes via `ctest`.

### Negative Test Requirements (from spec-challenger)
- **Failure Escalation:** A deliberate failing CTest MUST be added to prove that `ctest` returns a non-zero exit code when an assertion fails. If it returns 0, the CI pipe is broken and fake greens will occur.

### Implementation Steps
1. Create `test/host/CMakeLists.txt` defining a native executable (e.g. `add_executable(host_tests ...)`).
2. Wire the `game` component sources into this native build.
3. Write `test_sanity.c` with a simple `assert(1 == 1)`.

### Test Expectations
- `cd test/host && cmake -B build && cmake --build build && ctest` succeeds.

### Files to Create/Modify
- `test/host/CMakeLists.txt`
- `test/host/test_sanity.c`

### Commit Messages
- `test: scaffold CTest host executable for game logic profiling`

---

## Item 3: Setup `test/visual` compilation target

### User Story
As a UI developer, I need a native host compilation target that generates PNG files so I can visually inspect e-paper layouts without hardware.

### Acceptance Criteria
- [ ] `test/visual/` exists with its own `CMakeLists.txt`.
- [ ] Links both `game` and `presentation` components.
- [ ] C implementation uses `stb_image_write.h` to output a 200x200 pixel PNG.
- [ ] `render_all_screens.c` builds and writes a completely black `output/blank.png`.

### Negative Test Requirements (from spec-challenger)
- **Missing Asset Handling:** The visual tester MUST purposefully attempt to write to a read-only directory or read a missing file and assert that `stb_image` gracefully handles the failure rather than segfaulting the host CI pipeline.

### Implementation Steps
1. Download `stb_image_write.h` (single-header lib) into `test/visual/vendors/`.
2. Create `test/visual/CMakeLists.txt`.
3. Create `render_all_screens.c` which allocates a 5000-byte 1-bit buffer, fills it with 1s, and writes via `stbi_write_png`.

### Test Expectations
- `cd test/visual && cmake -B build && cmake --build build && ./build/render_all_screens` generates `output/blank.png`.

### Files to Create/Modify
- `test/visual/CMakeLists.txt`
- `test/visual/vendors/stb_image_write.h`
- `test/visual/render_all_screens.c`

### Commit Messages
- `test: scaffold visual regression generator writing PNGs`

---

## Item 4: Configure ESP-IDF SDK defaults

### User Story
As a systems engineer, I need `sdkconfig.defaults` and `partitions.csv` explicitly defined so the ESP32-S3 boots with the correct memory mappings.

### Acceptance Criteria
- [ ] `partitions.csv` allocates exactly the layout described in Design Doc 5.1 (Two 1.5MB app partitions, 5MB LittleFS data).
- [ ] `sdkconfig.defaults` enforces Custom Partition Table.
- [ ] `sdkconfig.defaults` enables SPIRAM (PSRAM) for future buffering needs.

### Negative Test Requirements (from spec-challenger)
- **Flash Boundary Overflow:** Attempt to compile a deliberately bloated binary (`#pragma` or giant array) that exceeds 1.5MB. Ensure `idf.py build` halts with an explicit partition overflow error before flashing to prevent silent corruption of the OTA partition.

### Implementation Steps
1. Create `partitions.csv` replicating: factory, ota_0, ota_1, otadata, littlefs.
2. Create `sdkconfig.defaults` with `CONFIG_PARTITION_TABLE_CUSTOM=y`, `CONFIG_SPIRAM=y`.

### Test Expectations
- `idf.py build` succeeds and prints the custom partition map.

### Files to Create/Modify
- `partitions.csv`
- `sdkconfig.defaults`

### Commit Messages
- `chore: configure 8MB flash partitions and S3 PSRAM defaults`
