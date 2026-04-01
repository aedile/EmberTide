# Phase 2: Foundational Math & Determinism

## Item 1: Implement XOR-shift PRNG

### User Story
As a combat designer, I need a strictly deterministic PRNG so BLE combat sequences stay perfectly synchronized across two physical devices without sending huge arrays of pre-rolled dice.

### Acceptance Criteria
- [ ] `components/game/include/prng.h` defines `fq_prng_t`.
- [ ] `fq_prng_init` prevents a seed of `0` (replaces with `1`).
- [ ] `fq_prng_next` implements the frozen 32-bit xorshift (<<13, >>17, <<5).
- [ ] `fq_prng_range(min, max)` truncates using modulo bias intentionally.

### Negative Test Requirements (from spec-challenger)
- **Zero-Seed Deadlock:** Initialize the PRNG with exactly `0`. Assert that the function rejects it or overrides to `1`, and does NOT return `0` continuously (which freezes game logic loops).
- **Modulo Bias:** Validate `fq_prng_range(0, 10)` over 100,000 iterations to prove no value appears >1% more often than expected.

### Implementation Steps
1. Define the struct and function signatures in `prng.h`.
2. Implement in `components/game/src/prng.c`.
3. Ensure no floating point or `<time.h>` calls exist.

### Test Expectations
- `test_prng.c` asserts specific 10-iteration sequences for seeds 1, 42, and 0xDEADBEEF match expected mathematical hardcoded values.

### Files to Create/Modify
- `components/game/include/prng.h`
- `components/game/src/prng.c`
- `test/host/test_prng.c`

### Commit Messages
- `feat: implement frozen deterministic xorshift32 PRNG`

---

## Item 2: Implement CRC32 Hash Generator

### User Story
As a protocol engineer, I need a deterministic CRC-32 calculator to verify combat log parity and Save/Load integrity without external libraries.

### Acceptance Criteria
- [ ] `components/game/include/crc32.h` defines `fq_crc32(const uint8_t *data, size_t len)`.
- [ ] The implementation uses the standard IEEE 802.3 polynomial `0xEDB88320`.
- [ ] Uses a statically generated 256-entry lookup table for speed.

### Negative Test Requirements (from spec-challenger)
- **Null Pointer Derefence:** Pass `data = NULL` and `len = 100`. Assert that the function returns `0` rather than causing an ESP32 `LoadProhibited` hardware panic.
- **Empty Buffer:** Pass `len = 0`. Assert it returns `0x00000000`.

### Implementation Steps
1. Create `crc32.c` and embed the precomputed 256-word array lookup table.
2. Implement the byte-by-byte XOR loop.

### Test Expectations
- `test_crc32.c` validates `"123456789"` produces `0xCBF43926`.

### Files to Create/Modify
- `components/game/include/crc32.h`
- `components/game/src/crc32.c`
- `test/host/test_crc32.c`

### Commit Messages
- `feat: implement table-driven CRC32`

---

## Item 3: Implement Effective Stat Curve Lookup Table

### User Story
As a game balancer, I need an exact integer approximation of the logarithmic scaling curve so that high stats have diminishing returns, without ever invoking standard math library float functions.

### Acceptance Criteria
- [ ] `components/game/include/progression.h` defines `fq_effective_stat(uint8_t raw)`.
- [ ] Contains a frozen 256-entry `uint8_t` array scaling `raw` smoothly to `effective`.
- [ ] `raw == 0` is `0`, `raw == 50` is `16`.

### Negative Test Requirements (from spec-challenger)
- **Domain Mismatch / Out of Bounds:** Force the raw integer casting to evaluate an underflow (e.g. attempting to pass `-1` which casts to `uint8_t 255`). Verify the lookup array safely hits the 255th element without causing an out-of-bounds pointer crash into arbitrary adjacent memory.

### Implementation Steps
1. Write a temporary Python script during development to calculate `round(10 * ln(raw + 1) / ln(11))` for 0-255.
2. Embed the 256 integers as a `static const uint8_t` array in `progression.c`.

### Test Expectations
- `test_progression.c` checks critical boundary inputs (0, 10, 50, 255) against the strict array values.

### Files to Create/Modify
- `components/game/include/progression.h`
- `components/game/src/progression.c`
- `test/host/test_progression.c`

### Commit Messages
- `feat: embed frozen effective stat integer lookup table`

---

## Item 4: Assert Master Combat Determinism Constraints

### User Story
As an architect, I need a CI check that guarantees no test cases inadvertently rely on external state or host architectures (endianness, 32 vs 64 bit pointers), so cross-device sync never fails.

### Acceptance Criteria
- [ ] A new test `test_combat_determinism.c` is added.
- [ ] It repeatedly hashes identical sets of inputs and guarantees the exact same CRC32 outputs on 10,000 iterations.
- [ ] Validates that `sizeof` core primitives strictly matches 8, 16, or 32 bits natively.

### Negative Test Requirements (from spec-challenger)
- **Intentional Offset:** Manually mutate a single integer inside the padded combat struct during the deterministic loop. Assert the resulting hash COMPLETELY mismatches.
- **Floating Point Pollution:** Attempt `#include <math.h>` and `sin(1.0)` inside the test suite. If the compiler doesn't throw a forbidden library warning (via CMake flags), the test fails.

### Implementation Steps
1. Create `test_combat_determinism.c`.
2. Add static assertions for type depths: `_Static_assert(sizeof(uint32_t) == 4, "Arch fail")`.

### Test Expectations
- CTest suite runs it. It passes instantly.

### Files to Create/Modify
- `test/host/test_combat_determinism.c`

### Commit Messages
- `test: enforce arch bit-widths and entropy isolation`
