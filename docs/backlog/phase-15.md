# Phase 15: Target Validation & Golden Integrations

## Item 1: Hardware E2E Combat Test

### User Story
As the final validation step, two physical FiestaQuest ESP32 devices must be able to boot, detect each other over BLE, exchange PRNG seeds, and execute 3 rounds of combat rendering visual damage numbers to the internal framebuffer without dropping the connection.

### Acceptance Criteria
- [ ] A dedicated `test/target/test_ble_combat.c` application exists.
- [ ] Device A starts in Host mode, Device B in Client mode.
- [ ] Completes the Phase 10 Handshake flawlessly.
- [ ] Stepper executes matching logic completely inline with Host test vectors.

### Negative Test Requirements (from spec-challenger)
- **Physical RF Interference:** Use the ESP-IDF `nimble_port_stop()` call mid-combat round to simulate physically walking out of range of Device B. Assert Device A cleans up the UI layer gracefully and returns to the Home screen without locking up in the E-paper redraw task.
- **Cross-Architecture Divergence Catch:** Run 1 device compiled with `-Os` (size optimization) and 1 compiled with `-O0` (no optimization). If any structs were padded differently, the BLE CRC handshake will fail instantly on Round 1.

### Implementation Steps
1. Create Unity target tests using the ESP-IDF `pytest-embedded` framework if CI runners with hardware exist, otherwise document manual QA checklist.

### Test Expectations
- Manual validation checklist pass.

### Files to Create/Modify
- `test/target/test_ble_combat.c`

### Commit Messages
- `test: ble target integration scenario validation`

---

## Item 2: Complete Project Burn & Final Release

### User Story
As the project owner, I need the `CMakeLists.txt` fully tuned so I can run `idf.py build` returning a production-ready `fiestaquest.bin` with no serial logging overhead that uses minimal battery power.

### Acceptance Criteria
- [ ] `sdkconfig` reduces logging from INFO to ERROR.
- [ ] Assertions are maintained but mapped to soft reboots rather than hardware panics.
- [ ] Compiler optimization `-Os` explicitly tested and validated against visual regressions.

### Negative Test Requirements (from spec-challenger)
- **OOM Panic (Out of Memory):** Under `-Os`, trigger a massive E-paper full-flush while simultaneously receiving a BLE MTU peak payload. Assert the heap checker guarantees at least `20kb` of minimum free RAM throughout the apex lifecycle spike, ensuring the final `.bin` will never suffer an out-of-memory stack collision.
- **Silent Assertion Ignore:** Validate that removing `CONFIG_COMPILER_CXX_EXCEPTIONS` does not silently bypass `TEST_ASSERT` boundary constraints.

### Implementation Steps
1. Run target analysis `idf.py size-components`.
2. Generate the factory bin.
3. Update `docs/fiestaquest-architecture.md` with final memory mappings.

### Test Expectations
- Clean compile, 0 warnings. Bin size < 1MB.

### Files to Create/Modify
- `sdkconfig`
- `CMakeLists.txt`

### Commit Messages
- `chore: optimize sdkconfig for production factory flash release`
