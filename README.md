# EmberTide

**EmberTide** is a highly-deterministic, C-based ESP32 e-paper auto-battler and virtual pet, prioritizing hardware-safeties, bit-perfect deterministic syncing over BLE, and deep RPG mechanics.

## Current Project Status
- **LLM Dev Harness Initialized:** The automated PM/Developer subagent workflow has been perfectly adapted to C/ESP-IDF development. Our constraints prioritize Math Safety, Pointer bounds checking, and pure Functional FSMs instead of Python conventions.
- **Backlog Decomposed:** We have successfully broken down the high-level Game Design and Architecture Docs into a rigid **15-Phase TDD Backlog** (`docs/backlog/BACKLOG.md`).
- **Deep Spec-Challenged Criterias:** Every backlog task has been rigorously reviewed by the `spec-challenger` subagent, embedding explicit Negative Test Requirements (such as verifying integer wraps, array overflow bounds, and PRNG deadlock checks) directly into the acceptance criteria.
- **Visual Regression Prepared:** We have generated the core 1-bit pixel art (`sprite-map.md`) and python reference visual generators, which will drive the C host-based pixel-diff testing in the coming phases.

## Final Vision (What it will look like)
When Phase 15 is complete, EmberTide will be an OTA-upgradable binary running on a custom ESP-IDF / FreeRTOS PCB featuring:
1. **Host-Tested Determinism:** 100% of the game math (combat rounds, critical hits, speed ties) is tested locally via CTest to guarantee a zero-float deterministic PRNG lock.
2. **True BLE Peer-to-Peer:** Devices physically sync up utilizing the `fq_packet_team_sync_t` handshakes. Combat resolves securely via PRNG hash evaluations across devices, preventing cheating.
3. **E-Paper Presentation:** Gorgeous, custom variable-width fonts, sprite blitters, and dynamic framing on a strict 200x200 pixel 1-bit canvas buffer mapped directly to SPI commands.
4. **Persistent Progress:** Saves pushed efficiently to `LittleFS` binary structures, with deep end-game mechanics (Legacy Rebirth Trees).

### Repository Structure
- `docs/` - System architecture, design docs, and the 15-phase iterative backlog.
- `.claude/` - The AI workflow hooks, templates, and constitution guiding the automated build process.
- `main/` - The eventual ESP-IDF entrance thread (`app_main.c`) and FreeRTOS event loops.
- `components/` - Sub-modules (e.g., `game`, `presentation`, `hal`, `connectivity`).
- `test/` - Native host testing, CI visual checking wrappers, and target evaluations.

## Development Workflow
Development strictly enforces **Red -> Green -> Refactor** via automated CI gates intercepted through bash wrappers. No code is merged until host regressions parse successfully.
