# Phase 1: Project Skeleton & Toolchain Initialization

## Item 1: Establish CMake Root and Entrypoint

### User Story
As a developer, I need a standard ESP-IDF v5 CMake project structure with separate `game`, `presentation`, `hal`, `connectivity`, and `platform` components so I can enforce architectural boundaries from day one.

### Acceptance Criteria
- [x] Project root contains a valid `CMakeLists.txt` for ESP-IDF.
- [x] `main/app_main.c` exists with an empty `app_main()` function.
- [x] Component directories `game`, `presentation`, `hal`, `connectivity`, `platform` exist with minimal `CMakeLists.txt` files mapping their includes.

### Negative Test Requirements (from spec-challenger)
- **E1 Boundary Restriction:** try_compile at configure time proves game/ cannot include hal_*.h — FATAL_ERROR on violation.
- **N1a:** presentation/ cannot include hal_*.h — FATAL_ERROR on violation.
- **N1b:** game/ cannot include presentation headers — FATAL_ERROR on violation.
- **N1c:** connectivity/ cannot include game/ headers — FATAL_ERROR on violation.
- **N2:** `components/platform/` created as distinct layer between game/presentation and hal.

### Implementation Notes
- `CMakeLists.txt` REQUIRES/PRIV_REQUIRES enforce architectural boundaries:
  - `game/`: no REQUIRES (pure functions, no dependencies)
  - `presentation/`: REQUIRES `game` (not `hal`)
  - `platform/`: REQUIRES `hal` (added in later phases)
  - `connectivity/`: no game dependency
  - `main/`: REQUIRES `game`, `presentation`, `platform`, `connectivity`
- N8: In-source build guard (`FATAL_ERROR` if `CMAKE_SOURCE_DIR == CMAKE_BINARY_DIR`)

### Status: COMPLETE (branch: chore/phase-1-project-skeleton)

---

## Item 2: Setup `test/host` CTest suite

### User Story
As a developer practicing TDD, I need a host-compiled C testing environment (CTest) so I can validate my game logic instantly without flashing to an ESP32.

### Acceptance Criteria
- [x] `test/host/` directory exists with its own `CMakeLists.txt` configured as a standard host executable.
- [x] The host test executable links the `game` component successfully.
- [x] A dummy test `test_sanity.c` executes and passes via `ctest`.

### Negative Test Requirements (from spec-challenger)
- **E2 Failure Escalation:** `bound_e2_always_fails` with `WILL_FAIL TRUE` proves ctest catches non-zero exits.
- **N5:** Explicit comment in CMakeLists asserting zero ESP-IDF include paths.
- **N6:** `-Wall -Werror` set on all host test targets.
- **N8:** In-source build guard.

### Status: COMPLETE (branch: chore/phase-1-project-skeleton)

---

## Item 3: Setup `test/visual` compilation target

### User Story
As a UI developer, I need a native host compilation target that generates PNG files so I can visually inspect e-paper layouts without hardware.

### Acceptance Criteria
- [x] `test/visual/` exists with its own `CMakeLists.txt`.
- [x] Links both `game` and `presentation` components (screen_mgr.c excluded — device-only).
- [x] C implementation uses `stb_image_write.h` v1.16 to output a 200x200 pixel PNG.
- [x] `render_all_screens.c` builds and writes a completely black `output/blank.png`.

### Negative Test Requirements (from spec-challenger)
- **N3:** `stbi_write_png` return value checked; failure path tested (invalid path returns 0, not segfault).
- **N6:** `-Wall -Werror` on all visual test targets.
- **N7:** `_Static_assert(FB_SIZE_BYTES == 5000)` validates framebuffer size at compile time.
- **N8:** In-source build guard.

### Status: COMPLETE (branch: chore/phase-1-project-skeleton)

---

## Item 4: Configure ESP-IDF SDK defaults

### User Story
As a systems engineer, I need `sdkconfig.defaults` and `partitions.csv` explicitly defined so the ESP32-S3 boots with the correct memory mappings.

### Acceptance Criteria
- [x] `partitions.csv` allocates exactly the layout described in Design Doc 5.1 (two 1.5MB app partitions, ~4.805 MiB LittleFS data — NOT "5MB").
- [x] `sdkconfig.defaults` enforces Custom Partition Table.
- [x] `sdkconfig.defaults` enables SPIRAM (Octal PSRAM, 80 MHz) for future buffering needs.

### Negative Test Requirements (from spec-challenger)
- **N4:** `test_partitions.c` validates CSV arithmetic at test time: no overlaps, all partitions fit within 8 MiB flash, storage end = 0x800000.

### Partition Layout
| Name     | Offset   | Size     | End      | Notes                    |
|----------|----------|----------|----------|--------------------------|
| nvs      | 0x009000 | 0x006000 | 0x00F000 | NVS key-value store      |
| phy_init | 0x00F000 | 0x001000 | 0x010000 | PHY calibration data     |
| app0     | 0x010000 | 0x180000 | 0x190000 | Factory app (1.5 MiB)    |
| app1     | 0x190000 | 0x180000 | 0x310000 | OTA slot 0 (1.5 MiB)    |
| otadata  | 0x310000 | 0x002000 | 0x312000 | OTA selection data       |
| storage  | 0x312000 | 0x4EE000 | 0x800000 | LittleFS (~4.805 MiB)    |

### Status: COMPLETE (branch: chore/phase-1-project-skeleton)
