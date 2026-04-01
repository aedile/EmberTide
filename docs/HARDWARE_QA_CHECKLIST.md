# Hardware QA Checklist — FiestaQuest v1.0

Manual validation checklist for physical ESP32-S3 device testing.
All items must be checked before a production firmware release.

---

## Prerequisites

- [ ] Two ESP32-S3-ePaper-1.54 boards (Waveshare variant of ESP32-S3-PICO-1-N8R8)
- [ ] ESP-IDF v5.x installed with Xtensa LX7 toolchain
- [ ] Both devices flashed with firmware built from the SAME git SHA
- [ ] Serial monitor open on both devices (`idf.py monitor` at 115200 baud)
- [ ] nRF Connect app available on iOS or Android for BLE inspection
- [ ] `idf.py build` produced zero warnings under `-Wall -Werror`
- [ ] Host test gate passed: `ctest --output-on-failure` — 63/63

---

## Boot Sequence

- [ ] Device powers on; e-paper displays Title screen within 3 seconds
- [ ] Title screen pixel layout matches `test/visual/golden/scene_home.png`
- [ ] Button A press transitions Title screen to Home screen
- [ ] Character name, level, and HP bar render correctly on Home screen
- [ ] Home screen matches `test/visual/golden/scene_home.png` (200x200 pixels, no ghosting)
- [ ] No assert panic or watchdog reset during boot

---

## Save / Load

- [ ] Power cycle (full power off, not reset) preserves character data on next boot
- [ ] First boot on empty flash creates default save without crashing
- [ ] `hal_flash_read_save()` returns `HAL_FLASH_ERR_NOT_FOUND` on empty NVS (not a hard fault)
- [ ] After default save created, second boot loads saved data correctly
- [ ] Serial log shows: `"SAVE: loaded character [name]"` (not `"SAVE: not found"`)

---

## Combat (BLE — 2 devices required)

- [ ] Device A visible in nRF Connect with service UUID `FQ01`
- [ ] Device A serial shows: `"BLE: advertising with UUID FQ01"`
- [ ] Device B detects Device A and initiates connection
- [ ] Device B serial shows: `"BLE: connected to [Device A MAC]"`
- [ ] Device A serial shows: `"BLE: client connected"`
- [ ] Phase 10 handshake completes: `fq_packet_invite_t` exchanged, seed derived
- [ ] Serial on BOTH devices shows identical derived PRNG seed value
- [ ] Combat rounds execute: both devices produce matching round hashes (rounds 1, 2, 3)
- [ ] HP bars update on both displays after each round
- [ ] Action text renders correctly in combat HUD
- [ ] Winner determined correctly after 12 rounds or KO condition
- [ ] Post-combat: both devices return to Home screen

---

## Display

- [ ] All 8 screen types render without pixel artifacts (see `test/visual/golden/`)
  - [ ] Blank / boot splash
  - [ ] Title screen
  - [ ] Home screen
  - [ ] Inventory screen
  - [ ] Stats screen
  - [ ] Combat HUD
  - [ ] Dialogue screen
  - [ ] Training screen
- [ ] Partial refresh works: no full-screen ghosting on combat HP bar updates
- [ ] Deep sleep mode activates after inactivity timeout (default: configurable in `sdkconfig`)
- [ ] E-paper displays correct content on wake from deep sleep (no blank/garbage screen)

---

## Audio

- [ ] Button A press produces audible beep (piezo)
- [ ] Button B press produces distinct tone from Button A
- [ ] Combat hit event produces a distinct short tone
- [ ] Combat KO / win event produces a longer distinct tone
- [ ] No audio lockup after rapid button pressing (50 presses in 5 seconds)

---

## Negative / Stress Tests

### Mid-Combat BLE Disconnect
- [ ] Start combat session (at least 3 rounds completed)
- [ ] Physically walk Device B out of BLE range OR call `nimble_port_stop()` via debug
- [ ] Device A serial shows: `"BLE: disconnect event received"`
- [ ] Device A FSM transitions from `FQ_STATE_BATTLE` to `FQ_STATE_HOME`
- [ ] Device A e-paper redraws Home screen (no combat HUD ghost)
- [ ] No hard fault, no watchdog reset, no assert panic on Device A
- [ ] Heap free after recovery >= heap free before combat (no BLE buffer leak)

### Rapid Button Pressing
- [ ] Press Button A 50 times within 5 seconds
- [ ] Device does not crash, freeze, or trigger watchdog reset
- [ ] UI remains responsive after the burst (next button press processes correctly)

### Power Loss During Save
- [ ] Begin a save operation (navigate away from combat to trigger auto-save)
- [ ] Immediately cut USB power during the save window
- [ ] Restore power and boot device
- [ ] Save file is intact: character data loads correctly OR factory default is loaded cleanly
- [ ] No NVS corruption, no assert, no mount failure logged

### Cross-Architecture Divergence (Production sdkconfig)
- [ ] Flash Device A with production build: `idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.production" build`
- [ ] Flash Device B with development build: `idf.py build` (default sdkconfig.defaults)
- [ ] Conduct full BLE combat session
- [ ] Round 1 hash on Device A == Round 1 hash on Device B
- [ ] If hashes diverge: BLOCKER — struct padding differs between optimization levels.
      File a defect referencing `test/target/test_ble_combat.c` NEGATIVE TEST N2.

### OOM Peak Load
- [ ] Trigger a full e-paper screen transition (not partial refresh) on Device A
- [ ] Simultaneously, Device B sends 256-byte MTU payload to Device A
- [ ] Device A serial shows: `"HEAP min free: [N] bytes"`
- [ ] PASS: N >= 20480 (20 KB minimum free heap)
- [ ] FAIL (BLOCKER): N < 20480 — heap collision risk. Audit BLE RX buffer allocation;
      ensure `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL` is used (not PSRAM heap).
      See `sdkconfig.defaults` SPIRAM DMA warning (line A5).

---

## Production Binary Validation

- [ ] `idf.py size-components` run on production build
- [ ] Total binary (`.bin`) size < 1 MB
- [ ] Flash usage breakdown logged: `app`, `ota_0`, `nvs`, `storage`, `factory`
- [ ] `idf.py build` with `sdkconfig.production` produces zero warnings under `-Wall -Werror`
- [ ] Factory bin (`build/fiestaquest.bin`) generated and archived with firmware SHA tag

---

## Sign-Off

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Firmware Engineer | | | |
| QA Lead | | | |
| Project Owner | | | |
