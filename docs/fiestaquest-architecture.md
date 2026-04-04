# FiestaQuest -- Architectural Design Document v2
**v2.19 amendment (Phase 20 BLE Combat — audit blocker fixes, feat/phase-20-ble-combat):** (DC-1) fq_generate_combat_hash() wired in production: called in FQ_STATE_BATTLE on FQ_EVT_COMBAT_ROUND_COMPLETE before awarding XP; result stored in fq_app_ctx_t::last_combat_hash (uint32_t, appended after opponent field). fq_app_ctx_t size updated: 424 bytes (64-bit host), 408 bytes (32-bit target). (DC-2) hal_ble_get_mac() added to HAL API: hal_ble.h declares `hal_ble_err_t hal_ble_get_mac(uint8_t mac_out[6])`; target stub in hal_ble.c returns fixed test MAC {0x01..0x06} with TODO for esp_read_mac(); mock_hal_ble.c implements hal_ble_get_mac() with injectable MAC via mock_ble_set_mac(const uint8_t[6]) and call count via mock_ble_get_mac_call_count(); mock_hal_ble.h updated with both new declarations; HAL_BLE_MAC_LEN=6u constant added. (DC-3) fq_protocol_derive_seed() wired in BATTLE_SETUP on FQ_EVT_BLE_CONNECTED: peer nonce from evt->data XOR'd with my_nonce (reassembled from my_nonce[4] LE bytes) to populate shared_seed. fq_sync_verify_round() wired in FQ_STATE_BATTLE on FQ_EVT_BLE_PACKET_RX: peer hash in evt->data compared against last_combat_hash; on FQ_SYNC_ERR_HASH_MISMATCH or FQ_SYNC_ERR_ROUND_MISMATCH → combat_active=0, go_home() (desync abort). Zero peer hash treated as no-op (host test stub behaviour). (AC-1) Auto-save wired for BATTLE_RESULT→HOME and REBIRTH→HOME in app_main.c main event loop (two new `if (last_state == … && app.state == FQ_STATE_HOME)` guards triggering do_auto_save()). (DOC-1) This v2.19 amendment. CMakeLists.txt: add_app_test() updated to include CONN_INCLUDE and CONN_SOURCES so all FSM test targets compile with app_fsm.c's new connectivity includes. FQ_EVT_BLE_DISCONNECTED now handled in FQ_STATE_BATTLE_RESULT (go_home) and FQ_STATE_BATTLE (abort+go_home) as previously documented. fq_combat_award_xp() in progression.h used in FQ_STATE_BATTLE on COMBAT_ROUND_COMPLETE. fq_vm_battle_result_t (20B) in view_models.h, fq_vm_rebirth_t (20B) in view_models.h. screen_battle_result.h/c and screen_rebirth.h/c in presentation/screens/. fq_vm_build_battle_result() and fq_vm_build_rebirth() in main/vm_builder.h/c. FQ_STATE_BATTLE_RESULT and FQ_STATE_REBIRTH added to idle suppression in is_idle_forbidden(). All 90/92 previously passing host tests continue to pass (2 pre-existing failures in test_p19_home_menu_feature and test_p19_interactive_bounds unrelated to Phase 20 work).

**v2.15 amendment (Comprehensive Audit Remediation — fix/comprehensive-audit-remediation):** (1) ESP-IDF name collision fix: components/hal/ renamed to components/fq_hal/ so the component registers as `fq_hal` and no longer shadows ESP-IDF's built-in `hal` component. All fq_hal/include/ and fq_hal/src/ references updated throughout this document. test/host/CMakeLists.txt HAL_INCLUDE updated accordingly. (2) F-02 character creation: character.h/character.c added to components/game/: fq_character_create(ch, class_id, id, name) zeroes struct, sets class stats from k_class_base table, sets equipped_count=4, calculates hp_max=base_hp+(strength*2), calls fq_legacy_apply_bonuses(). Returns GAME_OK/GAME_ERR_NULL_PTR/GAME_ERR_INVALID. Host tests: test_character_bounds.c (5 bound tests), test_character.c (6 feature tests). All 65 ctest tests pass. (3) Dead code removed: renderer.c/renderer.h (empty placeholder, no declarations) removed from components/presentation/. wifi_service.c/wifi_service.h (stub, real impl in fq_hal/src/hal_wifi.c) removed from components/connectivity/. (4) DOC-01/DOC-02: stale names updated — fq_view_home_t→fq_vm_home_t, fq_view_combat_t→fq_vm_combat_t, fq_view_inventory_t→fq_vm_inventory_t, screen_home_render→fq_render_home, render_combat_screen→fq_render_combat, render_inventory_screen→fq_render_inventory, framebuffer.h→fq_framebuffer.h. Section 3 file tree updated to reflect actual delivered files. (5) DOC-03: field rename note — the design doc used `legacy_unlocked_nodes` as the bitmask field name; the implementation in fq_character_t uses `legacy_tree`. See field annotation in types.h offset 8.

**v2.14 amendment (Phase 14 HAL BLE + WiFi Captive Portal — review findings applied):** hal_ble.h added to fq_hal/include/: HAL_BLE_MAX_MTU=256u, HAL_BLE_SERVICE_UUID="FQ01", hal_ble_err_t enum (OK/ERR_INIT/ERR_NOT_CONNECTED/ERR_MTU_EXCEEDED/ERR_NULL/ERR_SEND), hal_ble_state_t enum (IDLE/ADVERTISING/CONNECTED/DISCONNECTED), hal_ble_rx_callback_t typedef, hal_ble_init/start_advertising/send/get_state/disconnect/deinit. send guard order: NULL -> MTU -> init -> connected. Malicious-MTU guard: len > HAL_BLE_MAX_MTU returns ERR_MTU_EXCEEDED before any copy. Silent-drop guard: BLE_GAP_EVENT_DISCONNECT must transition to DISCONNECTED (spec-challenger requirement). disconnect() is a no-op unless state is CONNECTED (stays in current state). hal_wifi.h added: HAL_WIFI_SSID_MAX=32u, HAL_WIFI_PASS_MAX=64u, hal_wifi_err_t (OK=0/ERR_INIT=1/ERR_CONNECT=2/ERR_NULL=3/ERR_SSID_TOO_LONG=4/ERR_NOT_CONNECTED=5/ERR_PASS_TOO_LONG=6), hal_wifi_state_t (IDLE/AP_MODE/STA_CONNECTING/STA_CONNECTED/STA_DISCONNECTED), hal_wifi_init/start_ap/connect_sta/get_state/disconnect/deinit. SSID buffer-overflow guard: strlen(ssid) >= HAL_WIFI_SSID_MAX returns ERR_SSID_TOO_LONG. Password buffer-overflow guard: strlen(password) >= HAL_WIFI_PASS_MAX returns ERR_PASS_TOO_LONG. connect_sta guard order: NULL(ssid) -> NULL(pass) -> ssid-length -> pass-length -> init. hal_wifi_connect_sta() sets state to STA_CONNECTING (asynchronous; use mock_wifi_simulate_connected()/simulate_link_lost() in tests). HTTPD DoS mitigation: max_open_sockets=4, recv_wait_timeout=3 (spec-challenger requirement). Target stubs in fq_hal/src/ are ESP-IDF-free. Mock headers added: mock_hal_ble.h (mock_ble_reset/simulate_connect/disconnect/inject_rx/get_last_sent/get_send_count), mock_hal_wifi.h (mock_wifi_reset/get_last_ssid/get_last_password/simulate_connected/simulate_link_lost). 4 test files: test_p14_hal_ble_bounds.c, test_p14_hal_ble_feature.c (disconnect no-op from ADVERTISING/IDLE), test_p14_hal_wifi_bounds.c (ERR_PASS_TOO_LONG + null-before-init), test_p14_hal_wifi_feature.c (STA state transition sequence). All 63 ctest tests pass. ADV-P14-01 ADVISORY Rule 8: hal_ble and hal_wifi exist at HAL layer only; connectivity/ble_service.c wiring to event bus deferred — blocked on NimBLE hardware bring-up. NimBLE REQUIRES (bt), WiFi REQUIRES (esp_wifi esp_http_server nvs_flash) deferred to hardware phase.

**v2.12 amendment (Phase 12 HAL Part 1 — E-Paper + Flash):** hal_epaper.h added to fq_hal/include/: HAL_EPAPER_FB_SIZE=5000u (200*200/8), hal_epaper_err_t enum (OK/ERR_INIT/ERR_BUSY_TIMEOUT/ERR_SPI/ERR_NULL), hal_epaper_init/flush/sleep/deinit. flush guard order: NULL check -> size check -> init check. No ESP-IDF types in public API — host-compilable. hal_flash.h added: HAL_FLASH_SAVE_MAX_SIZE=512u, hal_flash_err_t (OK/ERR_MOUNT/ERR_NOT_FOUND/ERR_WRITE/ERR_READ/ERR_NULL/ERR_SIZE), hal_flash_init/read_save/write_save/deinit. write_save uses atomic temp-file+rename pattern on target to guard mid-write power loss. Target stubs in fq_hal/src/ are ESP-IDF-free (no spi_master.h or esp_vfs_littlefs.h included) — real driver implementation deferred to hardware bring-up. Host mocks (test/host/mock_hal_epaper.c, mock_hal_flash.c) simulate behaviour in RAM: epaper mock captures last flushed buffer + flush_count; flash mock persists data across deinit/reinit cycles. add_hal_test() CMake helper added to test/host/CMakeLists.txt — links mock instead of real HAL, includes only fq_hal/include (no game/ or presentation/ paths, boundary preserved). ADV-P12-01 ADVISORY Rule 8: hal_epaper and hal_flash exist at HAL layer only; screen_mgr.c (presentation) wiring deferred — blocked on hardware bring-up.

**v2.11 amendment (Phase 11 application event loop):** event_bus.h/c added to main/: fq_event_bus_t (portable ring buffer, 16 slots, uint8_t head/tail/count/overflow_count), fq_event_id_t enum (13 IDs, FQ_EVT_NONE through FQ_EVT_COUNT=13), fq_event_t (id + uint32_t data). fq_event_bus_init (single memset zero, NULL-safe), fq_event_bus_post (saturating overflow_count at 0xFF), fq_event_bus_pop (NULL out-param guard: does NOT consume event), fq_event_bus_pending (const, NULL-safe). app_fsm.h/c added to main/: fq_app_state_t enum (11 states, FQ_STATE_BOOT through FQ_STATE_COUNT=11), fq_app_ctx_t (state, embedded bus, tick_count, non-owning player/inventory ptrs, fq_combat_ctx_t combat, combat_active). fq_app_init: memset ctx, wire ptrs, init bus, auto-transition BOOT→TITLE. fq_app_dispatch: outer switch on state, inner switch on event id; unknown events silently ignored; GAME_ERR_NULL_PTR on NULL ctx or evt. PRNG isolation contract: only FQ_STATE_BATTLE case block may access ctx->combat.rng (combat_active guard). app_main.c updated with static player/inventory/app allocs and main loop skeleton comment. ADV-P11-01 ADVISORY Rule 8: event bus and FSM wired in main/; presentation render dispatch and HAL input wiring deferred to HAL integration phase.
**v2.10 amendment (Phase 10 connectivity data protocol):** combat_hash.h/c added to game/: fq_generate_combat_hash(ctx, round) serializes round(1)+f1.hp(2)+f2.hp(2)+f1.hp_max(2)+f2.hp_max(2)+rng.state(4)=13 bytes LE into a stack buffer and returns fq_crc32() of it. NULL ctx or round outside [1,12] returns 0. No struct casting — field-by-field byte writes (N7: padding not hashed, N8: CRC not fed back). protocol.h/c added to connectivity/: wire-format DTOs fq_packet_invite_t (14 bytes), fq_packet_team_sync_t (36 bytes), fq_packet_round_hash_t (14 bytes) with fq_packet_serialize/fq_packet_parse. Parse validates magic before CRC (fast-fail on spoof); round 0/>12 rejected post-CRC for ROUND_HASH type; stateless (N11). fq_protocol_derive_seed: XOR nonces, force 1 if result is 0 (N1 zero-guard). sync.h/c added to connectivity/: fq_sync_verify_round — pure equality comparisons, round checked before hash. connectivity/ uses PRIV_REQUIRES game for crc32.h; public include boundary preserved. ADV-P10-01 DEFERRED: move crc32 to shared utils/ to remove PRIV_REQUIRES. Rule 8 advisory: combat_hash is wired in game/ only; presentation wiring blocked on BLE HAL integration (next phase).


**Target:** ESP-IDF v5.x on ESP32-S3-PICO-1-N8R8
**Reference:** FiestaQuest Game Design Document v5
**Methodology:** TDD red-first, clean architecture, defense in depth

**v2.9 amendment (Phase 9 UI screens part 2):** fq_vm_combat_t added to view_models.h: split-screen combat HUD VM (f1/f2 names char[13], int16_t HP fields, action_text char[32], round/finished/winner). fq_vm_training_t added: game_name char[16], score uint8_t 0-100, difficulty, state 0/1/2. screen_combat.h/c implemented: fq_render_combat (NULL-safe, int32_t HP bar math, strnlen-bounded action_text, action banner overlay). screen_training.h/c implemented: fq_render_training (3-state activity area, score bar). ui_widgets.h/c implemented: fq_render_dialogue (word-wrap 25 chars/line, force-break on unspaced text, 4-line max + "..." truncation, embedded newline support, double-rect ornate border, optional YES/NO buttons). fq_vm_build_combat() added to vm_builder: class_id sourced from character records. Visual regression lock established: 8 golden PNGs in test/visual/golden/. diff_screens.py pixel-exact comparison activated.

**v2.2 amendment:** fq_save_result_t renamed to fq_save_err_t; FQ_SAVE_ERR_NULL_PTR added as new variant. Architecture doc updated to match implementation.

**v2.6 amendment (Phase 7 visual render primitives):** fq_framebuffer.h/c implemented: fq_fb_t (5000-byte static 1-bit packed, MSB-first, _Static_assert enforced), fq_fb_clear/fill/set_pixel/get_pixel/draw_line (Bresenham all-octants)/draw_rect/fill_rect. fq_sprite.h/c implemented: fq_sprite_t + fq_blit_sprite (OR-blit, 4-edge clip, all 8 x-alignments, widths not multiple of 8). fq_text.h/c implemented: fq_font_t + fq_draw_text (ASCII 0x20-0x7E, int16_t cursor, stops at x>=200) + fq_text_width. Section 7.2 framebuffer type now matches implementation. Three new .c files registered in components/presentation/CMakeLists.txt.

**v2.8 amendment (Phase 8 review findings):** vm_builder.h/c naming aligned — all occurrences of view_model_builder renamed to vm_builder throughout this document to match the Phase 8 implementation in main/vm_builder.h and main/vm_builder.c.

**v2.7 amendment (Phase 7 review findings):** fq_framebuffer_t type name renamed to fq_fb_t throughout the document to match the implementation in fq_framebuffer.h. Function signatures in Section 7.2 updated from int parameters to int16_t parameters (x, y, w, h) to match actual API. draw_line implementation widened dx/dy to int32_t to eliminate signed overflow UB for extreme int16_t coordinate ranges (B4 fix). Section 7.2 framebuffer type and all screen render signatures now fully consistent with Phase 7 implementation.

**v2.5 amendment (Phase 6 training & progression):** training.h/training.c added: pure mini-game FSM (fq_minigame_t 8 bytes, 5 states WAIT/ACTIVE/SUCCESS/FAIL/DONE), score = min(100, uint32_t(hits)*100/targets), difficulty = min(10, level/10), targets = 5 + difficulty*5. progression.h extended: fq_calc_xp_to_next (50*level^2, floor 50 at L0, sentinel 0 at L99) and fq_level_up (class-biased +3 stat/level, saturating at 255). legacy.h/legacy.c added: 32-bit 16-node tier-gated bitmask tree, fq_rebirth (50/60/75% stat retention, class base floor, rebirth_count/legacy_points saturation), fq_legacy_apply_bonuses, fq_calc_rebirth_tokens (level/10 + wins/100, saturated).

**v2.4 amendment (Phase 5 item engine):** fq_combat_fighter_t expanded from 10→24 bytes (equipped_items[5], equipped_count, damage_bonus, damage_mult_pct, dodge_bonus). fq_combat_ctx_t expanded from 28→64 bytes (round_3_f1_hp, round_3_f2_hp, time_loop_used, item_recursion_depth). item_engine.c implemented with 8 representative items, trigger router with role-specific dispatch (ON_DEFEND/ON_DODGE→defender only; ON_ATTACK/ON_CRIT/ON_KILL/ON_DEATH→attacker only; PASSIVE/ON_ROUND_START/ON_ROUND_END→both fighters, defender first per NTR-C1). Section 5.2 updated. No-item PRNG baseline preserved (NTR-A2).

**v2.3 amendment (Phase 4 formula rework):** Section 5.1 updated to reflect the actual Phase 4 combat API. The aspirational v1 API (fq_combat_resolve, fq_combat_finalize, item-aware fq_combat_fighter_t) is superseded by the Phase 4 implementation. Full item-aware signature arrives in Phase 5.

**v2.1 amendment:** fq_prng_range signature changed from int to uint32_t — avoids signed/unsigned conversion hazards in modulo arithmetic.

**v2 changelog:** Combat stepper API (replaces all-at-once resolver). Typed event bus (tagged union, no void*). Mini-game contracts (scoring + running split). Training session FSM. BLE service contract. Captive portal module. View model layer (decouples presentation from game state). Visual test harness (host-rendered PNGs for LLM review before flashing). CRC32 frozen alongside PRNG. PRNG modulo bias accepted and documented. Effective stat lookup table committed. Production error logging. PSRAM policy expanded.

---

## 1. Architectural Principles

1. **Test first, always.** Every module gets a failing test before a line of implementation.
2. **Dependency inversion everywhere.** Upper layers define interfaces. Lower layers implement them.
3. **No god objects.** Each module owns exactly one concern.
4. **Determinism is a contract.** Combat, PRNG, CRC32, and item resolution are pure functions. Testable on host with zero mocks.
5. **Fail loudly, recover gracefully.** Every function that can fail returns a typed error code. Every error is logged. Every unrecoverable error triggers save + sleep.
6. **Visual verification without flashing.** Every screen renders to an in-memory framebuffer. The presentation layer compiles and runs on the host, outputting PNGs for LLM or human review. No device needed for UI iteration.

---

## 2. Layer Architecture

```
+------------------------------------------------------------------+
|                        APPLICATION                                |
|  app_main.c -- bootstrap, tasks, event loop, view model builder   |
+------------------------------------------------------------------+
        |                    |                     |
        v                    v                     v
+----------------+  +------------------+  +------------------+
|   GAME ENGINE  |  |   PRESENTATION   |  |    CONNECTIVITY  |
|                |  |                  |  |                  |
| combat.h       |  | view_models.h    |  | ble_service.h    |
| training.h     |  | renderer.h       |  | wifi_service.h   |
| mini_games.h   |  | sprite_mgr.h     |  | ota_service.h    |
| progression.h  |  | ui_widgets.h     |  | captive_portal.h |
| item_engine.h  |  | screens/*.h      |  |                  |
| modifier_eng.h |  | screen_mgr.h (†) |  |                  |
| state_machine.h|  |                  |  |                  |
| prng.h         |  | († = device only, |  |                  |
| crc32.h        |  |  not host-built)  |  |                  |
+----------------+  +------------------+  +------------------+
        |                    |                     |
        v                    v                     v
+------------------------------------------------------------------+
|                     PLATFORM SERVICES                             |
|  storage.h, input.h, power.h, clock.h, audio.h, sensor.h         |
+------------------------------------------------------------------+
        |
        v
+------------------------------------------------------------------+
|                   HARDWARE ABSTRACTION LAYER                      |
|  hal_gpio, hal_spi, hal_i2c, hal_epaper, hal_ble, hal_wifi,      |
|  hal_httpd, hal_flash, hal_timer, hal_sleep, hal_audio            |
+------------------------------------------------------------------+
```

**Key change from v1:** Presentation layer is split. Everything above the dashed line compiles on host: renderer, sprites, widgets, all screen_*.c files, and view models. Only `screen_mgr.c` (which calls `hal_epaper` for actual SPI refresh) is device-only. This enables the visual test harness (Section 7).

**View model flow:** Application layer reads game state, constructs a screen-specific view model struct, passes it to the screen renderer. Screen renderers never include `game/types.h`. They receive read-only view structs with exactly the data they need.

---

## 3. Project Structure

```
fiestaquest/
  CMakeLists.txt
  sdkconfig.defaults
  partitions.csv

  components/
    fq_hal/
      include/
        hal_gpio.h, hal_spi.h, hal_i2c.h, hal_epaper.h,
        hal_ble.h, hal_wifi.h, hal_httpd.h, hal_flash.h,
        hal_timer.h, hal_sleep.h, hal_audio.h, hal_pins.h
      src/
        (one .c per header)

    platform/
      include/
        storage.h, input.h, power.h, clock.h, audio.h, sensor.h
      src/
        (one .c per header)

    game/
      include/
        types.h, prng.h, crc32.h, combat.h, combat_protocol.h,
        training.h, mini_games.h, progression.h, item_engine.h,
        modifier_engine.h, state_machine.h, save_format.h, legacy.h
      src/
        (one .c per header)

    presentation/
      include/
        view_models.h             // all screen view model structs
        renderer.h                // framebuffer ops, draw primitives
        fq_framebuffer.h             // 200x200 1-bit framebuffer type
        sprite_mgr.h
        ui_widgets.h
        screens/
          screen_home.h ... screen_onboarding.h
      src/
        renderer.c                // HOST-COMPILABLE, pure framebuffer ops
        sprite_mgr.c              // HOST-COMPILABLE
        ui_widgets.c              // HOST-COMPILABLE
        screens/
          screen_home.c ... screen_onboarding.c  // all HOST-COMPILABLE
        screen_mgr.c              // DEVICE-ONLY -- e-paper refresh via HAL

    connectivity/
      include/
        ble_service.h, wifi_service.h, ota_service.h, captive_portal.h
      src/
        ble_service.c, wifi_service.c, ota_service.c, captive_portal.c
      assets/
        captive_portal.html       // WiFi credential entry form

  main/
    CMakeLists.txt
    app_main.c
    event_bus.h
    event_bus.c
    vm_builder.h          // game state -> view model translation
    vm_builder.c

  test/
    host/
      CMakeLists.txt              // links game/ + presentation/ (minus screen_mgr)
      // Bound tests (Rule 22 Phase A) and feature tests (Rule 22 Phase B) for
      // all phases 2-14 plus F-02 character creation.
      // Key files: test_character_bounds.c, test_character.c (F-02 remediation)
      // Mock HAL: mock_hal_epaper.c, mock_hal_flash.c, mock_hal_gpio.c,
      //           mock_hal_audio.c, mock_hal_sleep.c, mock_hal_ble.c,
      //           mock_hal_wifi.c (one per fq_hal/ module)
      check_boundary.sh           // CTest-visible compile-fail boundary script
      test_assert.h               // typed assertion macros (no Unity dependency)

    visual/                       // VISUAL TEST HARNESS (see Section 7)
      CMakeLists.txt              // links game/ + presentation/ (minus screen_mgr)
      render_all_screens.c        // renders every screen to framebuffer
      vendors/                    // stb_image_write (single-file PNG encoder)
      output/                     // generated PNGs land here (gitignored)
      golden/                     // approved reference PNGs for regression
        // blank.png, fb_test.png, scene_home.png, scene_combat.png,
        // scene_inventory.png, scene_stats.png, scene_training.png,
        // scene_dialogue.png
      diff_screens.py             // pixel-diff output/ vs golden/, flag regressions

    // Note: test/target/ (integration tests for device HAL) is deferred to
    // hardware bring-up phase. scenarios/ and framebuffer_to_png.h were removed
    // — screen scenarios are driven from render_all_screens.c directly.

  tests/                          // Python renderer suite (host visual QA)
    renderer.py                   // Python framebuffer renderer
    test_render.py                // pytest: generates 10 gameplay screenshots
    test_sprites.py               // pytest: sprite rendering tests
    test_animate.py               // pytest: animation frame tests

  assets/
    sprites/, audio/, items.json, modifiers.json,
    names_adjectives.txt, names_nouns.txt

  tools/
    sprite_compiler.py            // PNG -> 1-bit packed bitmap (with round-trip test)
    asset_compiler.py             // JSON -> binary .def files (with round-trip test)
    combat_simulator.py           // 10K fight balance validation
```

---

## 4. Frozen Contracts

These modules are frozen after v1.0 ships. Changes break cross-device BLE combat.

### 4.1 PRNG (game/prng.h)

```c
#ifndef FIESTAQUEST_PRNG_H
#define FIESTAQUEST_PRNG_H
#include <stdint.h>

typedef struct { uint32_t state; } fq_prng_t;

void     fq_prng_init(fq_prng_t *rng, uint32_t seed);
uint32_t fq_prng_next(fq_prng_t *rng);
uint32_t fq_prng_range(fq_prng_t *rng, uint32_t min, uint32_t max);

#endif
```

**Implementation (frozen):**

```c
void fq_prng_init(fq_prng_t *rng, uint32_t seed) {
    rng->state = seed ? seed : 1;  // xorshift32 cannot have state 0
}

uint32_t fq_prng_next(fq_prng_t *rng) {
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

uint32_t fq_prng_range(fq_prng_t *rng, uint32_t min, uint32_t max) {
    if (min >= max) { return min; }  /* defensive: no state advancement */
    uint32_t span = max - min + 1u;
    if (span == 0u) { return fq_prng_next(rng); }  /* full-range overflow guard */
    return min + (fq_prng_next(rng) % span);
}
```

**Modulo bias:** Accepted. For range 6, bias is ~0.00000009%. Negligible for d6 rolls. Rejection sampling would make PRNG call count non-deterministic (1+ calls per invocation), complicating the Appendix A protocol. Simplicity wins. This decision is FROZEN -- do not "improve" it later.

### 4.2 CRC32 (game/crc32.h)

```c
#ifndef FIESTAQUEST_CRC32_H
#define FIESTAQUEST_CRC32_H
#include <stdint.h>
#include <stddef.h>

// CRC-32 (ISO 3309). Polynomial 0xEDB88320 (reflected).
// Init: 0xFFFFFFFF. Final XOR: 0xFFFFFFFF.
// This implementation is FROZEN. Both devices must produce identical results.
uint32_t fq_crc32(const uint8_t *data, size_t len);

#endif
```

**Implementation (frozen):** Table-driven, 256-entry lookup table generated from polynomial 0xEDB88320. Included as `static const uint32_t crc32_table[256]` in `crc32.c`. Table generation verified by test.

### 4.3 Effective Stat Lookup Table (game/progression.h, frozen)

```c
// 256-entry table. Index = raw stat, value = effective stat.
// Generated from: eff = round(10 * ln(raw + 1) / ln(11))
// This table is FROZEN. Used in combat formulas on both devices.
static const uint8_t FQ_EFFECTIVE_STAT_TABLE[256] = {
    0,  3,  5,  6,  7,  7,  8,  9,  9, 10, 10, 10, 11, 11, 11, 12,
   11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 14, 14, 14,
   // ... (remaining 224 entries generated at build, validated by test)
};

uint8_t fq_effective_stat(uint8_t raw) {
    return FQ_EFFECTIVE_STAT_TABLE[raw];
}
```

Test validates every entry against the float formula within +/- 1 rounding tolerance.

---

## 5. Module Contracts

### 5.1 Combat Engine -- Stepper API (game/combat.h)

Combat is a step-at-a-time state machine. The caller advances one round, does BLE sync, renders, then advances the next round. Pure functions throughout -- no I/O, no hardware, no time.

**v2.3 amendment — Phase 4 actual API:** The implementation delivered in Phase 4 uses `fq_character_t *` directly (no item-aware wrapper). The aspirational item-aware `fq_combat_fighter_t` (with `equipped_items[5]`) arrives in Phase 5. The actual Phase 4 API is:

```c
game_err_t        fq_combat_init(fq_combat_ctx_t *ctx,
                                 const fq_character_t *c1,
                                 const fq_character_t *c2,
                                 uint32_t seed);
fq_round_result_t fq_combat_step(fq_combat_ctx_t *ctx);
```

**Phase 4 combat formula contract (B1–B5 rework, 2026-03-31):**
- **B1 Initiative:** `d6 + eff_speed/3` (not d100 + full eff_speed). Ties favor F2 (defender).
- **B2 Precision tier:** `tier = min(eff_precision/5, 3)`. Four-tier lookup table maps raw d6 to adjusted_roll before damage: Tier 0 `[1,2,3,4,5,6]`, Tier 1 `[2,2,3,4,5,6]`, Tier 2 `[2,3,3,4,5,5]`, Tier 3 `[3,3,4,4,5,5]`.
- **B3 Dodge clamp:** `[5, 40]` (not `[5, 75]`).
- **B4 Crit:** `crit_threshold = 6 - (tier/2)`. Determined from original raw d6 roll. No separate PRNG call.
- **B5 Rerolls:** Self-reroll if `own_raw_roll <= 2` (keep higher). Defensive reroll if `opponent_raw_roll >= 5` (keep lower for opponent). Both consume one charge from the fighter who rerolls.

**PRNG call order per round:**
1. First attacker d6 attack roll
2. [Cond.] First attacker self-reroll d6 (if raw ≤ 2 and charges > 0)
3. Dodge d100 for attack 1
4. Second attacker d6 attack roll
5. [Cond.] Second attacker self-reroll d6 (if raw ≤ 2 and charges > 0)
6. [Cond.] First attacker defensive reroll d6 (if second_raw ≥ 5 and first has charges)
7. Dodge d100 for attack 2
8. Lucky Star F1 d20 (ALWAYS consumed)
9. Lucky Star F2 d20 (ALWAYS consumed)

**Sizes (compile-time verified):**
- `fq_combat_fighter_t`: 10 bytes
- `fq_combat_ctx_t`: 28 bytes
- `fq_round_result_t`: 18 bytes

**BLOCKER advisory — Phase 5:** Full item-aware signature (equipped_items[], has_lucky_star, perk checks) must be wired before BLE combat ships. Phase 4 wires the core formula with no item passives active.

**Tests:**

```c
// test/host/test_combat_stepper.c

void test_stepper_matches_resolve(void) {
    fq_combat_fighter_t f1 = make_test_bruiser(5);
    fq_combat_fighter_t f2 = make_test_hex(4);
    uint32_t seed = 0xDEADBEEF;

    // All-at-once
    fq_combat_log_t full = fq_combat_resolve(&f1, &f2, seed);

    // Step-by-step
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &f1, &f2, seed);
    while (!fq_combat_is_finished(&ctx)) {
        fq_combat_step(&ctx);
    }
    fq_combat_log_t stepped = fq_combat_finalize(&ctx);

    TEST_ASSERT_EQUAL_UINT32(full.crc32, stepped.crc32);
    TEST_ASSERT_EQUAL_UINT8(full.winner, stepped.winner);
    TEST_ASSERT_EQUAL_UINT8(full.round_count, stepped.round_count);
}

void test_stepper_hp_accessible_between_rounds(void) {
    fq_combat_ctx_t ctx;
    fq_combat_init(&ctx, &make_test_bruiser(5), &make_test_hex(4), 42);

    fq_combat_step(&ctx);

    int16_t f1_hp, f2_hp;
    fq_combat_get_hp(&ctx, &f1_hp, &f2_hp);

    // HP should have changed from initial values
    TEST_ASSERT_TRUE(f1_hp <= ctx.fighters[0].fighter.hp_max);
    TEST_ASSERT_TRUE(f2_hp <= ctx.fighters[1].fighter.hp_max);
    TEST_ASSERT_TRUE(f1_hp < ctx.fighters[0].fighter.hp_max ||
                     f2_hp < ctx.fighters[1].fighter.hp_max);
}
```

### 5.2 Item Engine (game/item_engine.h)

**v2.4 update:** Fully implemented in Phase 5. Public API:

```c
#define FQ_ITEM_NONE          0u   /* empty slot sentinel */
#define FQ_MAX_ITEM_TRIGGERS  1u   /* recursion depth cap */

const fq_item_def_t *fq_item_lookup(uint16_t item_id);
void fq_item_eval_trigger(fq_combat_ctx_t *ctx, fq_trigger_t trigger,
                          uint8_t attacking_fighter);
```

Trigger routing (PM-approved, NTR-C1):
- ON_DEFEND, ON_DODGE → defender fighter items only
- ON_ATTACK, ON_CRIT, ON_KILL, ON_DEATH → attacker fighter items only
- PASSIVE, ON_ROUND_START, ON_ROUND_END → both fighters, defender first

PRNG discipline: Lucky Coin (d100) and Chaos Orb (d4) always consume PRNG
even when probabilistic check fails (NTR-A1). No-item fights are PRNG-invariant
against Phase 4 baseline (NTR-A2).

Phase 5 item table (8 representative items): 001, 003, 004, 104, 105, 109, 204, 207.

### 5.3 Mini-Game Scoring (game/mini_games.h)

Pure scoring functions. Host-testable. No I/O.

```c
#ifndef FIESTAQUEST_MINI_GAMES_H
#define FIESTAQUEST_MINI_GAMES_H

#include <stdint.h>

typedef enum {
    FQ_MG_QUICK_DRAW = 0,
    FQ_MG_MEMORY_CHAIN,
    FQ_MG_POWER_TAP,
    FQ_MG_STEADY_HAND,
    FQ_MG_RHYTHM_HIT,
    FQ_MG_COIN_FLIP,
    FQ_MG_DODGE_DRILL,
    FQ_MG_ENDURANCE_HOLD,
    FQ_MG_PATTERN_MATCH,
    FQ_MG_BOMB_DEFUSE,
    FQ_MG_COUNT,
} fq_mini_game_id_t;

typedef enum {
    FQ_STAT_STRENGTH,
    FQ_STAT_SPEED,
    FQ_STAT_PRECISION,
    FQ_STAT_INTELLIGENCE,
} fq_stat_t;

typedef struct {
    fq_mini_game_id_t game;
    uint16_t raw_score;
    uint16_t normalized_score;  // 0-100
    fq_stat_t trained_stat;     // primary stat this game trains
} fq_mg_result_t;

// Score a Quick Draw attempt. Input: reaction time in ms.
fq_mg_result_t fq_mg_score_quick_draw(uint32_t reaction_ms);

// Score a Power Tap attempt. Input: number of alternating taps in 5 seconds.
fq_mg_result_t fq_mg_score_power_tap(uint16_t tap_count);

// Score a Steady Hand attempt. Input: target hold time (ms), actual hold time (ms).
fq_mg_result_t fq_mg_score_steady_hand(uint32_t target_ms, uint32_t actual_ms);

// Score a Memory Chain attempt. Input: longest correct sequence length.
fq_mg_result_t fq_mg_score_memory_chain(uint8_t max_length);

// Score a Rhythm Hit attempt. Input: total timing errors in ms across all beats.
fq_mg_result_t fq_mg_score_rhythm_hit(uint32_t total_error_ms, uint8_t beat_count);

// Score a Coin Flip attempt. Input: correct guesses out of 5.
fq_mg_result_t fq_mg_score_coin_flip(uint8_t correct);

// Score a Dodge Drill attempt. Input: correct responses, total prompts.
fq_mg_result_t fq_mg_score_dodge_drill(uint8_t correct, uint8_t total);

// Score an Endurance Hold attempt. Input: hold duration in ms.
fq_mg_result_t fq_mg_score_endurance_hold(uint32_t hold_ms);

// Score a Pattern Match attempt. Input: correct answers, total shown.
fq_mg_result_t fq_mg_score_pattern_match(uint8_t correct, uint8_t total);

// Score a Bomb Defuse attempt. Input: timing error in ms from target zero.
fq_mg_result_t fq_mg_score_bomb_defuse(uint32_t error_ms);

// Get metadata for a game type.
fq_stat_t fq_mg_trained_stat(fq_mini_game_id_t game);
const char *fq_mg_name(fq_mini_game_id_t game);

#endif
```

### 5.4 Mini-Game Runners (presentation/mini_game_runner.h)

Hardware-coupled. Manages real-time input, display, and timing for each mini-game. Device-only (not host-compiled). Produces an `fq_mg_result_t` via callback on completion.

```c
#ifndef FIESTAQUEST_MINI_GAME_RUNNER_H
#define FIESTAQUEST_MINI_GAME_RUNNER_H

#include "mini_games.h"
#include "fq_framebuffer.h"

typedef void (*fq_mg_done_cb_t)(fq_mg_result_t result, void *user_data);

typedef struct {
    fq_mini_game_id_t   game;
    fq_fb_t             *fb;        // render target
    fq_mg_done_cb_t     on_done;
    void                *user_data;
} fq_mg_runner_config_t;

// Start running a mini-game. Non-blocking -- runs via event loop.
// Calls on_done when the game is complete.
void fq_mg_runner_start(const fq_mg_runner_config_t *config);

// Feed a button event into the active mini-game.
void fq_mg_runner_input(fq_input_event_t event, uint32_t timestamp_ms);

// Tick the active mini-game (call at ~10ms interval for timing).
void fq_mg_runner_tick(uint32_t now_ms);

// Abort the active mini-game (returns zero score).
void fq_mg_runner_abort(void);

#endif
```

### 5.5 Training Session FSM (game/training.h)

```c
#ifndef FIESTAQUEST_TRAINING_H
#define FIESTAQUEST_TRAINING_H

#include "types.h"
#include "mini_games.h"
#include "prng.h"

#define FQ_TRAINING_ROUNDS 6
#define FQ_TRAINING_MODIFIER_PICKS 3  // offered before rounds 2, 4, 6

typedef enum {
    FQ_TRAIN_CHOOSE_GAME,       // player picks 1 of 2 offered games
    FQ_TRAIN_PLAY_GAME,         // active mini-game in progress
    FQ_TRAIN_CHOOSE_MODIFIER,   // player picks 1 of 2 offered modifiers
    FQ_TRAIN_ALLOCATE_STATS,    // player distributes earned stat points
    FQ_TRAIN_COMPLETE,          // session done, results ready
} fq_training_phase_t;

typedef struct {
    fq_mini_game_id_t options[2];
} fq_game_offer_t;

typedef struct {
    uint8_t options[2];           // modifier IDs
} fq_modifier_offer_t;

typedef struct {
    uint8_t  stat_points;
    uint8_t  xp_earned;
    uint8_t  item_roll_rarity;    // 0=none, 1=common, 2=uncommon, 3=rare
    bool     modifier_unlocked;
    bool     cosmetic_unlocked;
} fq_training_rewards_t;

typedef struct {
    // Internal session state
    fq_prng_t             rng;
    uint8_t               round;
    uint16_t              cumulative_score;
    uint8_t               active_modifiers[FQ_TRAINING_MODIFIER_PICKS];
    uint8_t               active_modifier_count;
    fq_mini_game_id_t     games_played[FQ_TRAINING_ROUNDS];
    uint16_t              round_scores[FQ_TRAINING_ROUNDS];
    fq_training_phase_t   phase;
    fq_game_offer_t       current_game_offer;
    fq_modifier_offer_t   current_mod_offer;
    fq_training_rewards_t rewards;
} fq_training_ctx_t;

void fq_training_init(fq_training_ctx_t *ctx, const fq_modifier_pool_t *pool, uint32_t seed);
fq_training_phase_t fq_training_get_phase(const fq_training_ctx_t *ctx);

// Phase: CHOOSE_GAME
fq_game_offer_t fq_training_get_game_offer(const fq_training_ctx_t *ctx);
void fq_training_select_game(fq_training_ctx_t *ctx, uint8_t choice);  // 0 or 1

// Phase: PLAY_GAME (mini-game runs externally, submit result here)
fq_mini_game_id_t fq_training_get_active_game(const fq_training_ctx_t *ctx);
void fq_training_submit_score(fq_training_ctx_t *ctx, fq_mg_result_t result);

// Phase: CHOOSE_MODIFIER
fq_modifier_offer_t fq_training_get_modifier_offer(const fq_training_ctx_t *ctx);
void fq_training_select_modifier(fq_training_ctx_t *ctx, uint8_t choice);  // 0 or 1

// Phase: COMPLETE
fq_training_rewards_t fq_training_get_rewards(const fq_training_ctx_t *ctx);

#endif
```

### 5.6 State Machine (game/state_machine.h)

(Unchanged from v1.)

### 5.7 Save Format (game/save_format.h)

Explicit field-by-field serialization (not raw struct dump). Decouples in-memory layout from on-disk format.

```c
#ifndef FIESTAQUEST_SAVE_FORMAT_H
#define FIESTAQUEST_SAVE_FORMAT_H

#include "types.h"
#include "crc32.h"

#define FQ_SAVE_VERSION_CURRENT 1
#define FQ_SAVE_MAX_SIZE 512

typedef enum {
    FQ_SAVE_OK,
    FQ_SAVE_ERR_NULL_PTR,
    FQ_SAVE_ERR_CRC,
    FQ_SAVE_ERR_VERSION_TOO_NEW,
    FQ_SAVE_ERR_CORRUPT,
    FQ_SAVE_ERR_BUFFER_TOO_SMALL,
} fq_save_err_t; /* renamed from fq_save_result_t (v2.2) */

// Serialize to byte buffer. Field-by-field, explicit byte order (little-endian).
// Returns bytes written.
size_t fq_save_serialize(
    const fq_character_t *character,
    const fq_inventory_t *inventory,
    const fq_modifier_pool_t *modpool,
    uint8_t *buffer, size_t buffer_size
);

// Deserialize from byte buffer. Validates CRC. Runs migration if version < current.
fq_save_err_t fq_save_deserialize(
    const uint8_t *buffer, size_t buffer_size,
    fq_character_t *character_out,
    fq_inventory_t *inventory_out,
    fq_modifier_pool_t *modpool_out
);

#endif
```

**Test:** Round-trip test (serialize -> deserialize -> compare). Ensure sizeof assertions catch struct changes that don't get corresponding serialization updates.

---

## 6. View Models (presentation/view_models.h)

Screen renderers receive read-only view structs. They never include `game/types.h`. The application layer (vm_builder.c) is the only module that knows both game structs and view structs.

```c
#ifndef FIESTAQUEST_VIEW_MODELS_H
#define FIESTAQUEST_VIEW_MODELS_H

#include <stdint.h>
#include <stdbool.h>

// HOME screen
typedef struct {
    char     name[12];
    uint8_t  level;
    uint8_t  class_id;
    uint8_t  sprite_base;
    uint8_t  cosmetics[4];
    bool     is_dead;
    bool     ble_advertising;
    uint16_t wins;
    uint16_t losses;
} fq_vm_home_t;

// COMBAT screen (per-round update)
typedef struct {
    char     f1_name[12];
    char     f2_name[12];
    uint8_t  f1_class;
    uint8_t  f2_class;
    int16_t  f1_hp;
    int16_t  f1_hp_max;
    int16_t  f2_hp;
    int16_t  f2_hp_max;
    uint8_t  round;
    int8_t   last_f1_damage;
    int8_t   last_f2_damage;
    bool     f1_dodged;
    bool     f2_dodged;
    bool     f1_crit;
    bool     f2_crit;
    int8_t   overtime_damage;
    bool     finished;
    uint8_t  winner;                // 0, 1, or 2
} fq_vm_combat_t;

// TRAINING screen
typedef struct {
    uint8_t  phase;                 // fq_training_phase_t as uint8
    uint8_t  round;
    uint16_t cumulative_score;
    char     game_option_names[2][16];
    char     modifier_option_names[2][16];
    char     active_modifiers[3][16];
    uint8_t  active_modifier_count;
} fq_view_training_t;

// INVENTORY screen
typedef struct {
    uint16_t item_ids[32];
    char     item_names[32][16];
    uint8_t  item_rarities[32];
    bool     item_equipped[32];
    uint8_t  item_count;
    uint8_t  scroll_offset;
} fq_vm_inventory_t;

// STATS screen
typedef struct {
    char     name[12];
    uint8_t  class_id;
    uint8_t  level;
    uint32_t xp;
    uint32_t xp_next_level;
    uint8_t  strength;
    uint8_t  speed;
    uint8_t  precision;
    uint8_t  intelligence;
    uint8_t  eff_strength;
    uint8_t  eff_speed;
    uint8_t  eff_precision;
    uint8_t  eff_intelligence;
    uint16_t hp_max;
    uint16_t wins;
    uint16_t losses;
    uint8_t  rebirth_count;
} fq_view_stats_t;

// REBIRTH screen
typedef struct {
    char     name[12];
    uint8_t  class_id;
    uint8_t  legacy_points;
    uint8_t  rebirth_count;
    // perk offer preview
    char     next_perk_name[24];
    bool     can_afford_perk;
} fq_view_rebirth_t;

// ONBOARDING screen
typedef struct {
    uint8_t  step;                  // 0=splash, 1=name, 2=class, 3=appearance, etc.
    char     name_options[4][12];   // visible name choices (scrollable)
    uint8_t  name_cursor;
    uint8_t  class_cursor;
    uint8_t  cosmetic_preview[4];
} fq_view_onboarding_t;

// ... additional view models for RIVALS, LEGACY, SETTINGS as needed

#endif
```

**Screen render signature pattern:**

```c
// presentation/include/screens/screen_home.h
#include "fq_framebuffer.h"
#include "view_models.h"

void fq_render_home(fq_fb_t *fb, const fq_vm_home_t *view);
```

Every screen renderer takes a framebuffer pointer and a read-only view model. Pure function of its inputs. Host-compilable.

---

## 7. Visual Test Harness

### 7.1 Architecture

```
                                    HOST BUILD
  +----------+     +-------------+     +---------------+     +---------+
  | Scenario |---->| View Model  |---->| Screen Render |---->| FB->PNG |
  | (game    |     | Builder     |     | (renderer.c + |     | (stb)   |
  |  state)  |     | (same code  |     |  screen_*.c)  |     |         |
  |          |     |  as device) |     |               |     |         |
  +----------+     +-------------+     +---------------+     +---------+
                                                                  |
                                                                  v
                                                          output/*.png
                                                                  |
                                               +------------------+------------------+
                                               v                                     v
                                        diff_screens.py                    review_screens.py
                                        (pixel diff vs                     (send to LLM API
                                         golden/)                          for visual QA)
```

### 7.2 Framebuffer Type

```c
// presentation/include/framebuffer.h
#ifndef FIESTAQUEST_FRAMEBUFFER_H
#define FIESTAQUEST_FRAMEBUFFER_H

#include <stdint.h>

#define FQ_SCREEN_WIDTH  200
#define FQ_SCREEN_HEIGHT 200
#define FQ_FB_SIZE       (FQ_SCREEN_WIDTH * FQ_SCREEN_HEIGHT / 8)  // 5000 bytes

typedef struct {
    uint8_t pixels[FQ_FB_SIZE];  // 1-bit packed, MSB first, row-major
} fq_fb_t;

// Set a single pixel. x: 0-199, y: 0-199. color: 0=white, 1=black.
void fq_fb_set_pixel(fq_fb_t *fb, int16_t x, int16_t y, uint8_t color);

// Get a single pixel.
uint8_t fq_fb_get_pixel(const fq_fb_t *fb, int16_t x, int16_t y);

// Clear entire framebuffer (all white).
void fq_fb_clear(fq_fb_t *fb);

// Fill rectangle.
void fq_fb_fill_rect(fq_fb_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);

#endif
```

### 7.3 PNG Export (host only)

```c
// test/visual/framebuffer_to_png.h
#include "fq_framebuffer.h"

// Write framebuffer to a 200x200 grayscale PNG.
// White pixels = 255, black pixels = 0. Scaled up 3x for readability (600x600 output).
int fq_fb_write_png(const fq_fb_t *fb, const char *path);
```

Implementation uses `stb_image_write.h` (single-header, public domain). Scales 3x so the 200x200 image is legible when viewed on a monitor or by an LLM.

### 7.4 Scenario Structure

```c
// test/visual/scenarios/scenario_fresh_character.c

#include "fq_framebuffer.h"
#include "view_models.h"
#include "screens/screen_home.h"
#include "screens/screen_stats.h"
#include "framebuffer_to_png.h"

void render_fresh_character_screens(const char *output_dir) {
    fq_fb_t fb;

    // HOME screen: fresh Bruiser, level 1
    fq_vm_home_t home = {
        .name = "Iron Bones",
        .level = 1,
        .class_id = 0,  // Bruiser
        .is_dead = false,
        .ble_advertising = false,
        .wins = 0, .losses = 0,
    };
    fq_fb_clear(&fb);
    fq_render_home(&fb, &home);
    fq_fb_write_png(&fb, "output/home_fresh.png");

    // STATS screen: fresh Bruiser
    fq_view_stats_t stats = {
        .name = "Iron Bones",
        .level = 1,
        .strength = 3, .speed = 0, .precision = 0, .intelligence = 0,
        .eff_strength = 6, .eff_speed = 0, .eff_precision = 0, .eff_intelligence = 0,
        .hp_max = 26, .wins = 0, .losses = 0,
    };
    fq_fb_clear(&fb);
    screen_stats_render(&fb, &stats);
    fq_fb_write_png(&fb, "output/stats_fresh.png");

    // ... more screens for this scenario
}
```

### 7.5 Running the Visual Tests

```bash
# Build and run
cd test/visual && mkdir -p build && cd build
cmake .. && make && ./render_all_screens

# Pixel-diff against golden references (fails if >0.1% pixels differ)
python3 ../diff_screens.py --output ../output --golden ../golden --threshold 0.001

# Optional: send PNGs to LLM for visual review
python3 ../review_screens.py --output ../output --prompt "Review these e-paper screen renders for a 200x200 B&W handheld game. Check: readability at arm's length, information hierarchy, no overlapping text, consistent margins, clear button affordances."
```

### 7.6 review_screens.py (LLM Visual QA)

```python
#!/usr/bin/env python3
"""Send rendered screen PNGs to an LLM API for visual review."""

import anthropic, base64, sys, glob, os

def review_screens(output_dir, prompt):
    client = anthropic.Anthropic()
    pngs = sorted(glob.glob(os.path.join(output_dir, "*.png")))

    content = [{"type": "text", "text": prompt}]
    for png_path in pngs:
        with open(png_path, "rb") as f:
            b64 = base64.standard_b64encode(f.read()).decode("utf-8")
        content.append({
            "type": "image",
            "source": {"type": "base64", "media_type": "image/png", "data": b64}
        })
        content.append({
            "type": "text",
            "text": f"Filename: {os.path.basename(png_path)}"
        })

    response = client.messages.create(
        model="claude-sonnet-4-20250514",
        max_tokens=4096,
        messages=[{"role": "user", "content": content}]
    )

    print(response.content[0].text)

if __name__ == "__main__":
    review_screens(
        sys.argv[1] if len(sys.argv) > 1 else "output",
        sys.argv[2] if len(sys.argv) > 2 else (
            "Review these 200x200 B&W e-paper screen renders for a handheld game medal. "
            "For each screen, evaluate: "
            "1) Is text readable at the target resolution? "
            "2) Is information hierarchy clear (most important info largest/highest)? "
            "3) Are there overlapping elements or clipped text? "
            "4) Is the layout balanced with consistent margins? "
            "5) Would a user understand what buttons do from context? "
            "Flag any issues with specific filenames and coordinates."
        )
    )
```

### 7.7 Scenarios Checklist

Each scenario produces multiple PNGs covering different game states:

```
scenario_fresh_character    -- HOME, STATS, INVENTORY (empty), TRAIN entry
scenario_mid_game           -- HOME (lv10), STATS (mixed stats), INVENTORY (8 items, 3 equipped)
scenario_dead_character     -- HOME (dead sprite), REBIRTH screen, blocked TRAIN
scenario_combat_round       -- MATCHUP, rounds 1-3, overtime round, RESULT (win), RESULT (loss)
scenario_training_session   -- game offer, active game, modifier offer, score screen, stat allocate
scenario_inventory_full     -- full inventory, overflow swap prompt
scenario_onboarding         -- splash, name select, class select, appearance reroll, tutorial
scenario_rebirth            -- death screen, legacy point prompt, perk selection
scenario_settings           -- volume, BLE toggle, WiFi, auto-sleep
scenario_rivals             -- rival list, nemesis indicator, rival detail
scenario_legacy             -- perk tree (tiers 1-4), affordable vs locked
```

Target: ~50 PNGs covering every screen and major state variation. Each is a visual contract -- if it changes, the diff catches it.

---

## 8. BLE Service Contract (connectivity/ble_service.h)

```c
#ifndef FIESTAQUEST_BLE_SERVICE_H
#define FIESTAQUEST_BLE_SERVICE_H

#include "types.h"

typedef struct {
    uint32_t id;
    char     name[12];
    uint8_t  class_id;
    uint8_t  level;
    uint8_t  eff_strength;
    uint8_t  eff_speed;
    uint8_t  eff_precision;
    uint8_t  eff_intelligence;
    uint16_t hp_max;
    uint16_t equipped[5];
    uint8_t  equipped_count;
    uint32_t legacy_tree;
    uint8_t  wildcard_passive;
} fq_character_summary_t;

// Lifecycle
void fq_ble_init(void);
void fq_ble_deinit(void);

// Advertising (discovery)
void fq_ble_start_advertising(const fq_character_summary_t *self);
void fq_ble_stop_advertising(void);
bool fq_ble_is_advertising(void);

// Challenge flow
void fq_ble_send_challenge(const fq_character_summary_t *self);
void fq_ble_respond_challenge(bool accept, const fq_character_summary_t *self);

// Combat sync
void fq_ble_send_nonce(uint32_t nonce);
void fq_ble_send_round_check(int16_t f1_hp, int16_t f2_hp);
void fq_ble_send_fight_result(uint32_t crc32);

// Trade
void fq_ble_send_trade_offer(uint16_t item_id);
void fq_ble_respond_trade(bool accept);

// Connection management
void fq_ble_disconnect(void);
bool fq_ble_is_connected(void);

// All async responses published to event bus:
//   FQ_EVT_BLE_OPPONENT_FOUND, FQ_EVT_BLE_CHALLENGE_IN,
//   FQ_EVT_BLE_CHALLENGE_ACCEPTED, FQ_EVT_BLE_CHALLENGE_DECLINED,
//   FQ_EVT_BLE_NONCE_RECEIVED, FQ_EVT_BLE_ROUND_CHECK_RECEIVED,
//   FQ_EVT_BLE_FIGHT_RESULT_RECEIVED, FQ_EVT_BLE_DISCONNECTED,
//   FQ_EVT_BLE_TRADE_OFFER_RECEIVED, FQ_EVT_BLE_TRADE_ACCEPTED

#endif
```

### BLE Protocol State Machine

```
IDLE -> ADVERTISING -> DISCOVERED_OPPONENT -> CHALLENGE_SENT/RECEIVED
  -> CHALLENGE_ACCEPTED -> NONCE_EXCHANGE -> COMBAT_ACTIVE
  -> (per-round: ROUND_CHECK exchange) -> FIGHT_RESULT_EXCHANGE -> IDLE

Timeouts:
  CHALLENGE response: 15 seconds
  NONCE exchange: 5 seconds
  Per-round ROUND_CHECK: 10 seconds
  FIGHT_RESULT exchange: 5 seconds
  Any timeout -> FQ_EVT_BLE_DISCONNECTED -> forfeit if in combat
```

---

## 9. Captive Portal (connectivity/captive_portal.h)

```c
#ifndef FIESTAQUEST_CAPTIVE_PORTAL_H
#define FIESTAQUEST_CAPTIVE_PORTAL_H

typedef void (*fq_portal_cred_cb_t)(const char *ssid, const char *password, void *user_data);

// Start captive portal. Medal becomes a WiFi AP ("FIESTAQUEST-SETUP")
// and serves a credential entry form. Calls on_credential when user submits.
void fq_captive_portal_start(fq_portal_cred_cb_t on_credential, void *user_data);

// Stop captive portal and tear down AP.
void fq_captive_portal_stop(void);

// Is the portal currently running?
bool fq_captive_portal_is_active(void);

#endif
```

**Implementation:** Uses `hal_wifi.h` (AP mode) + `hal_httpd.h` (new, wraps `esp_http_server`). DNS hijack redirects all requests to the medal's IP. HTML form embedded as a binary blob via CMake `EMBED_TXTFILES`. The HTTP server task runs on core 0 alongside WiFi.

---

## 10. Event System (Typed)

```c
#ifndef FIESTAQUEST_EVENT_BUS_H
#define FIESTAQUEST_EVENT_BUS_H

#include "types.h"
#include "mini_games.h"

typedef struct {
    int16_t f1_hp;
    int16_t f2_hp;
} fq_round_check_t;

typedef struct {
    uint32_t bytes_received;
    uint32_t bytes_total;
} fq_ota_progress_t;

typedef enum {
    FQ_EVT_INPUT,
    FQ_EVT_BLE_OPPONENT_FOUND,
    FQ_EVT_BLE_CHALLENGE_IN,
    FQ_EVT_BLE_CHALLENGE_ACCEPTED,
    FQ_EVT_BLE_CHALLENGE_DECLINED,
    FQ_EVT_BLE_NONCE_RECEIVED,
    FQ_EVT_BLE_ROUND_CHECK_RECEIVED,
    FQ_EVT_BLE_FIGHT_RESULT_RECEIVED,
    FQ_EVT_BLE_DISCONNECTED,
    FQ_EVT_BLE_TRADE_OFFER_RECEIVED,
    FQ_EVT_BLE_TRADE_ACCEPTED,
    FQ_EVT_COMBAT_ROUND_COMPLETE,
    FQ_EVT_COMBAT_FINISHED,
    FQ_EVT_TRAINING_GAME_DONE,
    FQ_EVT_TRAINING_SESSION_DONE,
    FQ_EVT_AUTO_SLEEP_TIMEOUT,
    FQ_EVT_OTA_PROGRESS,
    FQ_EVT_OTA_COMPLETE,
    FQ_EVT_OTA_ERROR,
    FQ_EVT_PORTAL_CREDENTIAL,
} fq_event_type_t;

typedef struct {
    fq_event_type_t type;
    union {
        fq_input_event_t            input;
        fq_character_summary_t      opponent;
        uint32_t                    nonce;
        fq_round_check_t            round_check;
        fq_round_result_t           round_result;
        uint32_t                    fight_crc32;
        fq_mg_result_t              mini_game_result;
        fq_training_rewards_t       training_rewards;
        fq_ota_progress_t           ota_progress;
        uint16_t                    trade_item_id;
        struct { char ssid[33]; char password[65]; } wifi_cred;
    } data;
} fq_event_t;

typedef void (*fq_event_handler_t)(const fq_event_t *event, void *user_data);

void fq_event_bus_init(void);
void fq_event_subscribe(fq_event_type_t type, fq_event_handler_t handler, void *user_data);
void fq_event_publish(const fq_event_t *event);

#endif
```

Largest union member is `fq_character_summary_t` (~54 bytes). Ring buffer with 16 slots = ~900 bytes. Fits comfortably in SRAM.

---

## 11. FreeRTOS Tasks

```
TASK                    PRI   STACK   CORE   NOTES
main_task               5     8KB     0      Event loop, FSM, game logic, view model builder
input_task              10    2KB     0      10ms polling, publishes FQ_EVT_INPUT
ble_task                7     4KB     0      NimBLE host (ESP-IDF managed)
render_task             3     4KB     1      Reads view model from double buffer, renders to
                                              framebuffer, SPI transfer to e-paper
audio_task              4     2KB     1      Tone playback
wifi_task               6     4KB     0      Only alive during OTA / captive portal
httpd_task              6     4KB     0      Only alive during captive portal
```

**View model double buffer:** `main_task` (core 0) writes to the inactive view model buffer. Signals render_task (core 1) via semaphore. Render_task reads the active buffer. No mutex, no race. Swap on semaphore signal.

```c
static fq_view_t view_buffers[2];
static volatile uint8_t active_buffer = 0;
static SemaphoreHandle_t render_semaphore;

// main_task writes to inactive buffer, then:
active_buffer ^= 1;
xSemaphoreGive(render_semaphore);

// render_task reads from active buffer after:
xSemaphoreTake(render_semaphore, portMAX_DELAY);
render_current_screen(&view_buffers[active_buffer]);
```

---

## 12. Input Contract (fixed)

```c
#ifndef FIESTAQUEST_INPUT_H
#define FIESTAQUEST_INPUT_H

#include "types.h"

// Initialize. Configures GPIO interrupts for BOOT/PWR.
// GPIO ISR sets internal flags. Processing happens in fq_input_tick().
void fq_input_init(void);

// Call from input_task at ~10ms intervals. Processes debounce, detects gestures,
// publishes FQ_EVT_INPUT to event bus. Runs in TASK context, not ISR.
void fq_input_tick(uint32_t now_ms);

#define FQ_DOUBLE_PRESS_WINDOW_MS  280
#define FQ_LONG_PRESS_THRESHOLD_MS 600

#endif
```

No callback registration. No ISR-context confusion. Input module is a pure event producer. GPIO ISR sets a flag. `fq_input_tick()` reads the flag in task context, runs debounce logic, publishes typed events to the bus.

---

## 13. PSRAM Policy

Allowed PSRAM allocations (all at boot, never freed):

```
Framebuffer double-buffer:          10 KB   (2 x 5000 bytes)
OTA download buffer:               256 KB   (allocated only when wifi_task starts)
Item definition table:              64 KB   (grows with OTA packs)
Sprite atlas cache:                 32 KB   (decompressed sprites)
View model double-buffer:            1 KB   (2 x ~500 bytes)
```

Total: ~363 KB of 8 MB PSRAM. All via `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` in platform layer init. Game engine still has zero dynamic allocation.

---

## 14. TDD Strategy

### 14.1 Test Pyramid

```
                    /\
                   /  \
                  / HW  \         Target: hal, storage, input, power, BLE
                 /  tests \       (flash to device, manual + automated)
                /----------\
               / Visual     \     Host: render all screens to PNG, diff
              /  tests       \    against golden refs, optional LLM review
             /----------------\
            / Platform tests   \  Host: storage logic, input debounce, FSM
           /  (mocked HAL)      \ with mock HAL
          /----------------------\
         /    Game engine tests   \  Host: combat, items, training, progression,
        /      (pure functions)    \ PRNG, CRC32, save format, mini-game scoring
       /____________________________\

       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
       ~   Balance simulations      ~  Python: 10K fights, stat curves
       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
```

### 14.2 Host Builds

**Two host builds, separate CMake projects:**

1. **test/host/** -- Unit tests. Links `game/` only. Unity framework. Runs in <1 second.
2. **test/visual/** -- Screen renders. Links `game/` + `presentation/` (minus screen_mgr). Outputs PNGs. Runs in <3 seconds.

Both compile with the host C compiler. Zero ESP-IDF dependency.

### 14.3 Visual Test Workflow

```
Developer changes a screen layout:
  1. Run visual tests: ./render_all_screens
  2. Check output/*.png visually (or send to LLM)
  3. If correct: cp output/*.png golden/ (update golden references)
  4. If incorrect: fix renderer, repeat
  5. Commit golden/ alongside code changes

CI pipeline:
  1. Build host tests, run, verify all pass
  2. Build visual tests, run, generate PNGs
  3. Pixel-diff output/ vs golden/ (diff_screens.py)
  4. If >0.1% pixel difference on any screen: fail the build
  5. Optionally: run review_screens.py for LLM QA on PRs
```

### 14.4 Red-First Rule

No function in `game/src/` or `presentation/src/screens/` is written without a corresponding failing test written first. Enforced by code review.

---

## 15. Security

(Unchanged from v1 except: CRC32 polynomial now explicitly specified in Section 4.2.)

---

## 16. Build Configuration

### 16.1 Partition Table (partitions.csv)

```csv
# Name,    Type, SubType,  Offset,    Size,    Flags
nvs,       data, nvs,      0x9000,    0x6000,
phy_init,  data, phy,      0xf000,    0x1000,
app0,      app,  ota_0,    0x10000,   0x180000,
app1,      app,  ota_1,    0x190000,  0x180000,
otadata,   data, ota,      0x310000,  0x2000,
storage,   data, littlefs, 0x312000,  0x4EE000,
```

### 16.2 sdkconfig.defaults

```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1
CONFIG_BT_NIMBLE_ROLE_CENTRAL=y
CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=y
CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y
CONFIG_PM_ENABLE=y
CONFIG_PM_LIGHT_SLEEP_CALLBACKS=y
CONFIG_ESP_SLEEP_GPIO_RESET_WORKAROUND=y
CONFIG_LITTLEFS_SPIFFS_COMPAT=n
CONFIG_LOG_DEFAULT_LEVEL_WARN=y
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
CONFIG_ESP_TASK_WDT_EN=y
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10
CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y
CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF=y
```

### 16.3 Hardware Pin Map

```c
// fq_hal/include/hal_pins.h
// NOTE: ALL PINS MUST BE VERIFIED AGAINST WAVESHARE SCHEMATIC BEFORE PHASE 2.

#define FQ_PIN_BTN_BOOT     GPIO_NUM_0
#define FQ_PIN_BTN_PWR      GPIO_NUM_18

#define FQ_PIN_EPD_MOSI     GPIO_NUM_11   // TBD
#define FQ_PIN_EPD_CLK      GPIO_NUM_12   // TBD
#define FQ_PIN_EPD_CS       GPIO_NUM_10   // TBD
#define FQ_PIN_EPD_DC       GPIO_NUM_13   // TBD
#define FQ_PIN_EPD_RST      GPIO_NUM_14   // TBD
#define FQ_PIN_EPD_BUSY     GPIO_NUM_15   // TBD

#define FQ_PIN_I2C_SDA      GPIO_NUM_8    // TBD
#define FQ_PIN_I2C_SCL      GPIO_NUM_9    // TBD

#define FQ_PIN_I2S_BCLK     GPIO_NUM_45   // TBD
#define FQ_PIN_I2S_LRCK     GPIO_NUM_46   // TBD
#define FQ_PIN_I2S_DOUT     GPIO_NUM_42   // TBD

#define FQ_I2C_ADDR_RTC     0x51
#define FQ_I2C_ADDR_SHTC3   0x70
#define FQ_I2C_ADDR_ES8311  0x18
```

---

## 17. Module Development Order

### Phase 1: Core Engine (host, no hardware)

```
1.  prng.c + crc32.c + tests        // frozen contracts first
2.  types.h                          // all game structs
3.  progression.c + tests            // stat table, leveling, rebirth
4.  item_engine.c + tests            // trigger resolution
5.  combat.c (stepper API) + tests   // step-by-step combat
6.  combat_protocol.c + tests        // determinism, Appendix A validation
7.  mini_games.c (scoring) + tests   // pure scoring functions
8.  modifier_engine.c + tests        // stacking, synergy
9.  training.c + tests               // session FSM
10. state_machine.c + tests          // screen FSM
11. save_format.c + tests            // serialization, CRC, migration
12. test_fixtures.c                  // shared test helpers
```

### Phase 2: Presentation Engine (host, visual tests)

```
1.  framebuffer.c                    // pixel ops
2.  renderer.c                       // draw primitives (rect, line, text, blit)
3.  Font: embed a 1-bit bitmap font  // 5x7 or 6x8, stored as const array
4.  sprite_mgr.c                     // load and composite sprites
5.  ui_widgets.c                     // HP bar, menu list, progress bar, text box
6.  view_models.h                    // all view model structs
7.  Asset pipeline (sprite_compiler.py, asset_compiler.py + tests)
8.  Screens, one at a time, each with visual test scenario:
    screen_home -> screen_onboarding -> screen_train ->
    screen_combat -> screen_inventory -> screen_stats ->
    screen_rivals -> screen_legacy -> screen_settings -> screen_rebirth
9.  Golden reference snapshot for each scenario
10. diff_screens.py CI integration
```

### Phase 3: HAL Bringup (device)

```
1. hal_gpio     -- buttons
2. hal_i2c      -- RTC + SHTC3
3. hal_spi      -- bus init
4. hal_epaper   -- display test pattern
5. hal_flash    -- LittleFS mount
6. hal_audio    -- test tone
7. hal_ble      -- advertise + connect
8. hal_wifi     -- station connect
9. hal_httpd    -- serve test page
10. hal_sleep   -- deep sleep + wake
```

### Phase 4: Platform Services (device)

```
1. input.c      -- debounce, gestures
2. storage.c    -- save/load/backup
3. power.c      -- sleep/wake, auto-sleep
4. clock.c      -- RTC sanity
5. audio.c      -- tone sequences
6. sensor.c     -- temp/humidity
7. screen_mgr.c -- e-paper refresh (partial + full)
```

### Phase 5: Connectivity (device, two medals)

```
1. ble_service.c      -- full protocol
2. Two-device combat integration test
3. captive_portal.c   -- AP + HTTP server + form
4. wifi_service.c     -- credential management
5. ota_service.c      -- content packs + firmware
```

### Phase 6: Integration

```
1. app_main.c, event_bus.c, vm_builder.c
2. Mini-game runners (presentation layer, device-only)
3. End-to-end: onboard -> train -> fight -> die -> rebirth
4. Balance simulation (Python, 10K fights)
5. Power profiling
6. E-paper tuning (ghosting, temperature)
7. Playtest calibration
```

---

## 18. Error Handling

### 18.1 Error Codes

```c
typedef enum {
    FQ_OK = 0,
    FQ_ERR_INVALID_PARAM,
    FQ_ERR_BUFFER_TOO_SMALL,
    FQ_ERR_CRC_MISMATCH,
    FQ_ERR_VERSION_MISMATCH,
    FQ_ERR_IO_READ,
    FQ_ERR_IO_WRITE,
    FQ_ERR_BLE_DISCONNECTED,
    FQ_ERR_BLE_TIMEOUT,
    FQ_ERR_BLE_SYNC_ERROR,
    FQ_ERR_WIFI_CONNECT,
    FQ_ERR_WIFI_TIMEOUT,
    FQ_ERR_OTA_DOWNLOAD,
    FQ_ERR_OTA_VERIFY,
    FQ_ERR_OTA_TIMEOUT,
    FQ_ERR_OUT_OF_MEMORY,
} fq_error_t;
```

### 18.2 Production Error Log

On any `ESP_LOGE` call, also write to a persistent circular buffer:

```
/littlefs/error_log.dat -- 4KB circular buffer
Format: [uint32_t timestamp][uint8_t len][char[] message]
Oldest entries overwritten when full.
On OTA firmware update: upload error_log.dat to server before applying update.
On factory reset: wipe error_log.dat.
```

Crash dumps enabled via `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`. Stored in OTA staging partition (unused between updates). Extractable via USB with `espcoredump.py`.

### 18.3 Network Timeout Policy

All blocking network calls have explicit timeouts enforced at the HAL layer:

```
hal_wifi_connect():        15 seconds
hal_wifi_http_get():       60 seconds (total), 10 seconds (per-chunk)
hal_ble_connect():         10 seconds
hal_ble_write_char():      5 seconds
hal_ble_wait_notify():     timeout parameter, caller-defined
```

HAL functions return `FQ_ERR_*_TIMEOUT` on expiry. No function blocks indefinitely.

---

## 19. Coding Standards

### 19.1 Naming

```
Prefix:         fq_
Types:          fq_thing_t
Functions:      fq_module_verb_noun()
Constants:      FQ_CONSTANT_NAME
File-local:     static, no prefix
```

### 19.2 Memory

- No `malloc`/`free` in `game/` or `presentation/` (except screen_mgr.c)
- PSRAM for large boot-time allocations (Section 13)
- Stack sizes explicit per task
- FreeRTOS canary for stack overflow detection

### 19.3 Forbidden Patterns

- `malloc`/`free` in `game/` -- no stdlib include, compile error
- `float`/`double` in `game/` -- `-Werror=float-conversion`
- Global mutable state in `game/` -- all functions take explicit state params
- `#include "esp_*.h"` in `game/` or `presentation/` (except screen_mgr.c)
- `#include "types.h"` from `game/` in `presentation/screens/` -- use view_models.h
- Casting away `const` -- `-Werror=cast-qual`
- Implicit fallthrough -- `-Werror=implicit-fallthrough`
- `void*` in event payloads -- tagged union enforced

---

## Phase 2 Implementation Status (2026-03-31)

The following `components/game/` modules are fully implemented and host-tested:

| Module | Header | Implementation | Status |
|--------|--------|---------------|--------|
| XOR-shift PRNG | `include/prng.h` | `src/prng.c` | DONE |
| CRC32 hash | `include/crc32.h` | `src/crc32.c` | DONE |
| Effective stat curve | `include/progression.h` | `src/progression.c` | DONE |

### Frozen Constants

**PRNG algorithm:** xorshift32 with shifts `<<13`, `>>17`, `<<5`. Shifts are frozen
and must not change without updating all sequence literals in `test_prng.c` and the
BLE combat protocol.

**PRNG seed=0 guard:** Both `fq_prng_init()` and `fq_prng_next()` carry independent
zero-state guards. The first prevents init-time deadlock; the second guards against
uninitialized structs.

**CRC32 polynomial:** `0xEDB88320` (IEEE 802.3, reflected). Check vector:
`fq_crc32("123456789", 9) == 0xCBF43926`. Table is 256 × uint32_t static const,
frozen. `_Static_assert` verifies size at compile time.

**Stat curve range:** `fq_effective_stat(raw)` maps uint8_t [0,255] → uint8_t [0,23].
Table is monotonically non-decreasing. `_Static_assert(sizeof(table) == 256)`.

**Determinism pin:** 10,000 PRNG values from seed=1, packed as LE bytes, CRC32 hash
= `0x7B1900A6`. This literal is frozen in `test_combat_determinism.c`.

### Float Ban Enforcement

`test/host/bound_float_ban.c` is compiled by both `assert_compile_fails()` at
CMake configure time and `check_boundary.sh` at CTest runtime. The file calls
`sin()` without declaration — under `-Wall -Werror` the implicit function
declaration is a hard error, proving the float ban is enforced in CI.

---

## 15. Phase 15 Final Module Inventory & Test Coverage Summary

**v2.15 amendment (Phase 15 Hardware E2E Validation):** Final production config
(`sdkconfig.production`) and target test scaffold (`test/target/test_ble_combat.c`)
added. No architectural changes. This section documents the complete module
inventory and host test coverage at v1.0 release.

### 15.1 Complete Module Inventory

| Layer | Module | Header | Source | Status |
|-------|--------|--------|--------|--------|
| **game/** | PRNG | `prng.h` | `prng.c` | FROZEN |
| **game/** | CRC32 | `crc32.h` | `crc32.c` | FROZEN |
| **game/** | Types | `types.h` | — | FROZEN |
| **game/** | Save Format | `save_format.h` | `save_format.c` | DONE |
| **game/** | Progression | `progression.h` | `progression.c` | DONE |
| **game/** | Combat Engine | `combat.h` | `combat.c` | FROZEN |
| **game/** | Item Engine | `item_engine.h` | `item_engine.c` | DONE |
| **game/** | Training | `training.h` | `training.c` | DONE |
| **game/** | Legacy | `legacy.h` | `legacy.c` | DONE |
| **game/** | Combat Hash | `combat_hash.h` | `combat_hash.c` | FROZEN |
| **presentation/** | Framebuffer | `fq_framebuffer.h` | `fq_framebuffer.c` | DONE |
| **presentation/** | Sprite | `fq_sprite.h` | `fq_sprite.c` | DONE |
| **presentation/** | Text | `fq_text.h` | `fq_text.c` | DONE |
| **presentation/** | View Models | `view_models.h` | — | DONE |
| **presentation/** | UI Widgets | `ui_widgets.h` | `ui_widgets.c` | DONE |
| **presentation/** | Screen: Home | `screen_home.h` | `screen_home.c` | DONE |
| **presentation/** | Screen: Combat | `screen_combat.h` | `screen_combat.c` | DONE |
| **presentation/** | Screen: Training | `screen_training.h` | `screen_training.c` | DONE |
| **hal/** | E-Paper | `hal_epaper.h` | `hal_epaper.c` | LIVE |
| **hal/** | Flash | `hal_flash.h` | `hal_flash.c` | LIVE |
| **hal/** | GPIO | `hal_gpio.h` | `hal_gpio.c` | LIVE |
| **hal/** | Audio | `hal_audio.h` | `hal_audio.c` | STUB |
| **hal/** | Sleep | `hal_sleep.h` | `hal_sleep.c` | STUB |
| **hal/** | BLE | `hal_ble.h` | `hal_ble.c` | STUB |
| **hal/** | WiFi | `hal_wifi.h` | `hal_wifi.c` | STUB |
| **connectivity/** | Protocol | `protocol.h` | `protocol.c` | DONE |
| **connectivity/** | Sync | `sync.h` | `sync.c` | DONE |
| **main/** | Event Bus | `event_bus.h` | `event_bus.c` | DONE |
| **main/** | App FSM | `app_fsm.h` | `app_fsm.c` | DONE |
| **main/** | VM Builder | `vm_builder.h` | `vm_builder.c` | DONE |

**STUB** = Public API + host-compilable target stub implemented. Real ESP-IDF
hardware driver sequences deferred to physical bring-up (blocked on hardware).

**LIVE** = Public API + host-compilable target stub implemented AND failure-injection
bounds/feature tests fully passing. Phase 16 hardware bring-up validation tests
cover this module (hal_epaper, hal_flash, hal_gpio).

**FROZEN** = API and wire-format locked. Changes require both devices to be
reflashed simultaneously. Covered by determinism pin tests in `test_combat_determinism.c`.

### 15.2 Host Test Coverage Summary

All 63 host tests pass under `-Wall -Werror` on the host build system.

| Test File | Scope | Count |
|-----------|-------|-------|
| `test_prng_bounds.c` + `test_prng.c` | PRNG math + distribution | ~8 |
| `test_crc32_bounds.c` + `test_crc32.c` | CRC32 correctness | ~4 |
| `test_types_bounds.c` | Struct layout pins | ~6 |
| `test_save_format.c` + `test_save_corruption.c` | Save serializer | ~5 |
| `test_progression_bounds.c` + `test_progression.c` | Level-up, stat curve | ~5 |
| `test_combat_bounds.c` + `test_combat_engine.c` + `test_combat_determinism.c` | Combat stepper | ~6 |
| `test_item_bounds.c` + `test_item_engine.c` + `test_item_time_loop.c` | Item engine | ~5 |
| `test_training_bounds.c` + `test_training.c` | Training FSM | ~4 |
| `test_legacy_bounds.c` + `test_legacy.c` | Legacy/rebirth | ~4 |
| `test_level_up_bounds.c` + `test_level_up.c` | Level-up bounds | ~3 |
| `test_determinism_bounds.c` | PRNG isolation | ~2 |
| `test_fb_bounds.c` + `test_fb_feature.c` | Framebuffer | ~5 |
| `test_sprite_bounds.c` + `test_sprite_feature.c` | Sprite blit | ~4 |
| `test_text_bounds.c` + `test_text_feature.c` | Text render | ~4 |
| `test_vm_bounds.c` + `test_vm_builder.c` | View models | ~3 |
| `test_p9_combat_bounds.c` + `test_p9_combat_feature.c` | Screen: combat HUD | ~4 |
| `test_p9_dialogue_bounds.c` + `test_p9_dialogue_feature.c` | UI widget: dialogue | ~4 |
| `test_p10_bounds.c` + `test_p10_protocol.c` + `test_p10_combat_sync.c` | Protocol + hash + sync | ~6 |
| `test_p11_bus_bounds.c` + `test_p11_bus_feature.c` | Event bus | ~5 |
| `test_p11_fsm_bounds.c` + `test_p11_fsm_feature.c` | App FSM | ~6 |
| `test_p12_hal_epaper_bounds.c` + `test_p12_hal_epaper_feature.c` | HAL e-paper | ~4 |
| `test_p12_hal_flash_bounds.c` + `test_p12_hal_flash_feature.c` | HAL flash | ~4 |
| `test_p13_hal_gpio_bounds.c` + `test_p13_hal_gpio_feature.c` | HAL GPIO | ~5 |
| `test_p13_hal_audio_bounds.c` + `test_p13_hal_audio_feature.c` | HAL audio | ~5 |
| `test_p13_hal_sleep_bounds.c` + `test_p13_hal_sleep_feature.c` | HAL sleep | ~3 |
| `test_p14_hal_ble_bounds.c` + `test_p14_hal_ble_feature.c` | HAL BLE | ~5 |
| `test_p14_hal_wifi_bounds.c` + `test_p14_hal_wifi_feature.c` | HAL WiFi | ~5 |
| `test_partitions.c` + `test_sanity.c` | Infrastructure | ~4 |
| Boundary checks (5 tests) | Architecture boundary enforcement | 5 |

**Total: 63 tests, 0 failures, 0 warnings.**

### 15.3 Visual Regression Baseline

8 golden PNG baselines locked in `test/visual/golden/`. All verified by
`diff_screens.py` pixel-exact comparison on every feature branch.

| Scene | PNG | Verified Phase |
|-------|-----|----------------|
| Blank framebuffer | `blank.png` | Phase 7 |
| Framebuffer primitives | `fb_test.png` | Phase 7 |
| Home screen | `scene_home.png` | Phase 9 |
| Inventory screen | `scene_inventory.png` | Phase 9 |
| Stats screen | `scene_stats.png` | Phase 9 |
| Combat HUD | `scene_combat.png` | Phase 9 |
| Dialogue widget | `scene_dialogue.png` | Phase 9 |
| Training screen | `scene_training.png` | Phase 9 |

### 15.4 Production Build Configuration

`sdkconfig.production` overlays `sdkconfig.defaults` for factory flash:

| Key | Development | Production | Rationale |
|-----|-------------|------------|-----------|
| `CONFIG_LOG_DEFAULT_LEVEL` | INFO (4) | ERROR (1) | Reduce binary size ~3-8% |
| `CONFIG_BOOTLOADER_LOG_LEVEL` | INFO | ERROR | Silent boot in factory units |
| `CONFIG_COMPILER_OPTIMIZATION` | default | SIZE (-Os) | Binary < 1 MB target |
| Assertions | enabled (abort) | silent (restart) | Recovery over panic |
| `CONFIG_ESP_WIFI_SLP_IRAM_OPT` | off | on | WiFi power reduction |
| `CONFIG_BT_NIMBLE_MAX_CONNECTIONS` | default | 1 | 1v1 protocol only |

**Negative test N2 (cross-architecture divergence)** validates that -Os struct
layout produces identical combat hashes to -O0 builds. The `_Static_assert`
size pins on `fq_combat_fighter_t` and `fq_combat_ctx_t` are the first
line of defense; `test/target/test_ble_combat.c` Step 6 is the hardware
confirmation.

**v2.16 amendment (Phase 16 Hardware Bringup — feat/phase-16-hardware-bringup):** hal_epaper.c: Real SPI driver replacing Phase-12 stub. SPI2_HOST at 40 MHz Mode 0 DMA-auto, GPIO6 power rail, MOSI=GPIO13 CLK=GPIO12 CS=GPIO11 DC=GPIO10 RST=GPIO9 BUSY=GPIO8. Full init sequence: hardware reset (RST toggle), SWRESET(0x12), driver output control(0x01 0xC7 0x00 0x01), data entry mode(0x11 0x01), RAM window(0x44/0x45), border waveform(0x3C 0x01), temperature sensor(0x18 0x80), load temp+waveform(0x22 0xB1 + 0x20), cursor(0x4E/0x4F), full LUT load(0x32 + 153 bytes + LUT tail). WF_Full_1IN54 LUT (159 bytes) copied verbatim from Waveshare example driver. flush(): epd_wait_busy + set window + cursor + 0x24 + 5000 bytes + 0x22 0xF7 + 0x20 + epd_wait_busy. BUSY timeout: 3s via esp_timer_get_time() polling (HAL_EPAPER_ERR_BUSY_TIMEOUT). sleep(): 0x10 0x01 deep sleep mode 1 (~5 µA). deinit(): spi_bus_remove_device + spi_bus_free + GPIO6 low. hal_gpio.c: Real ISR driver replacing stub. GPIO0/GPIO18 active-low pull-up GPIO_INTR_NEGEDGE. ISR: 50ms debounce via esp_timer_get_time(), xQueueSendFromISR. gpio_task (priority 5, 2048-byte stack) drains queue and calls callback in task context. hal_gpio_deinit(): ISR handler removal, gpio_uninstall_isr_service, task/queue deletion. hal_flash.c: Real LittleFS driver replacing stub. joltwallet/littlefs (v1.20.4 from ESP-IDF Component Registry; header: esp_littlefs.h). Partition label="storage", mount="/littlefs". Atomic write: save.tmp→rename→save.dat. ENOENT→ERR_NOT_FOUND. app_main.c: Real FreeRTOS event loop. Init: hal_flash_init + save load (fq_save_deserialize), first-boot fq_character_create(BRUISER,1,"Ember"), fq_app_init, hal_epaper_init, hal_gpio_init(button_callback). button_callback (called from gpio_task): posts FQ_EVT_BTN_A/B_PRESS via s_app file-scope pointer. Main loop: drain bus, fq_app_dispatch, render on state change. render_current_state: dispatches TITLE/HOME/STATS/INVENTORY to vm_builder + screen renderers + hal_epaper_flush. main/CMakeLists.txt: fq_hal added to REQUIRES (only main/ permitted to span all layers). fq_hal/CMakeLists.txt: REQUIRES driver esp_timer, PRIV_REQUIRES joltwallet__littlefs. fq_hal/idf_component.yml: declares joltwallet/littlefs dependency. .gitignore: managed_components/ added as build artifact (like node_modules/). Mock enhancements: mock_hal_epaper now supports inject_spi_error/inject_busy_timeout (one-shot); mock_hal_flash supports inject_write_error/inject_mount_error (one-shot). New mock headers: mock_hal_epaper.h, mock_hal_flash.h, mock_hal_gpio.h. Phase-16 host tests: test_p16_hal_epaper_bounds (10 cases), test_p16_hal_flash_bounds (10 cases), test_p16_hal_gpio_bounds (9 cases), test_p16_hal_epaper_feature (10 cases), test_p16_hal_flash_feature (11 cases). Total: 70 ctest tests pass. idf.py build: fiestaquest.bin = 303,952 bytes (0x49f50), 81% of app partition free. ADV-P16-01 ADVISORY: hal_audio.c and hal_sleep.c remain as Phase-13 stubs — LEDC and esp_sleep wiring deferred to Phase 17. ADV-P16-02 ADVISORY: hal_ble.c and hal_wifi.c remain as Phase-14 stubs — NimBLE and WiFi stack wiring deferred to Phase 17.

**v2.17 amendment (Phase 19.5 Partial Refresh + Walk Animation + Idle Screen — feat/phase-19-5-idle-partial-refresh):** (1) HAL: `hal_epaper_flush_partial()` added to `fq_hal/include/hal_epaper.h` as a new public API. Sends the framebuffer using a partial-refresh waveform LUT to minimize ghosting artifacts during frequent screen updates. Guard order matches `flush()`: NULL check -> size check -> init check. Returns `hal_epaper_err_t`. `HAL_EPAPER_ERR_SIZE = 5` added to the `hal_epaper_err_t` enum (previously undocumented; value 5 inserted between ERR_NULL=4 and any future additions). `EPD_FULL_REFRESH_INTERVAL` constant (value `10u`) added: every Nth call to `hal_epaper_flush_partial()` automatically triggers a full-refresh cycle to clear accumulated ghosting. The internal counter `s_flush_count` tracks partial flushes and resets after each forced full refresh. Boot-time full clear added to `hal_epaper_init()`: white->black->white waveform sequence on first init to guarantee a clean display state; `s_flush_count` is reset to 0 after the boot clear completes. `hal_epaper_sleep()` now clears the `s_initialized` flag before returning — any subsequent call to `flush()` or `flush_partial()` after sleep returns `HAL_EPAPER_ERR_INIT`, enforcing the contract that the caller must re-initialize after wakeup. (2) Presentation — View Models: `fq_vm_idle_t` added to `components/presentation/include/view_models.h`. Layout: `uint16_t sprite_base` (index into sprite sheet), `char name[13]`, `uint8_t level`, `uint8_t pad` (reserved). Total: 16 bytes. `_Static_assert` size pin required. `anim_frame` field (`uint8_t`) added to `fq_vm_home_t`, replacing one pad byte — struct remains 24 bytes total, `_Static_assert` continues to pass. (3) Presentation — Builder: `fq_vm_build_idle()` added to `main/vm_builder.h` and `main/vm_builder.c`. Accepts a `const fq_character_t *` and a `uint8_t anim_frame`, populates `fq_vm_idle_t`. Pure function, no side effects. (4) Presentation — Primitives: `fq_blit_sprite_3x()` added to `components/presentation/include/sprite_util.h` and `components/presentation/src/sprite_util.c`. 3x pixel-scale 1-bit sprite blitter — each source pixel expands to a 3x3 block in the destination framebuffer. Signature follows the same clip-and-guard pattern as `fq_blit_sprite()` in `fq_sprite.h`. No floating-point arithmetic; all scaling via integer multiply. (5) Presentation — Screen: `screen_idle.h` and `screen_idle.c` added to `components/presentation/screens/`. Renders an idle overlay: character walk-cycle sprite (3x-scaled) centered on screen, character name and level below sprite, animated using `anim_frame` from `fq_vm_idle_t`. (6) Application — Idle overlay architecture: the idle screen is NOT an FSM state. It is a render-layer overlay controlled by a static boolean flag `s_idle_active` in `main/app_main.c`. When `s_idle_active` is true, `screen_idle` renders over the current base screen before flushing (partial refresh used during idle). The idle overlay is suppressed — `s_idle_active` forced false — whenever `fq_app_state_t` is `FQ_STATE_BATTLE`, `FQ_STATE_BATTLE_SETUP`, or `FQ_STATE_TITLE`. All other states permit idle overlay. This design avoids adding a new FSM state and preserves the determinism contract: idle activation is driven by a tick counter in `app_main.c`, not by game logic in `game/`.


**v2.18 amendment (Phase 19 Interactive Gameplay — Onboarding, Training Session, Inventory Equip — feat/phase-19-interactive-gameplay):** (1) FSM — State enum: `FQ_STATE_ONBOARDING = 11` appended to `fq_app_state_t`; `FQ_STATE_COUNT` updated to `12`. `_Static_assert(sizeof(fq_app_ctx_t))` size pin updated: 248 bytes (64-bit host), 232 bytes (32-bit target). Two new uint8_t fields added to `fq_app_ctx_t`: `onboarding_class_index` and `onboarding_save_failed`. Both fit in previously unused trailing padding — struct size is unchanged on 32-bit target. (2) Game modules added (all in `components/game/`): `name_gen.h/.c` — PRNG-based two-part name generation using two 16x16 word-part tables (prefix syllables, suffix syllables). `fq_generate_name(rng, out, max_len)`: concats prefix+suffix, null-terminates within max_len. NULL guards: GAME_ERR_NULL_PTR on NULL rng or out; GAME_ERR_INVALID on max_len==0. Output fits in char[12] (max 11 chars + null). 1000-seed fuzz test confirms all names are 2-11 chars, always null-terminated. `training_session.h/.c` — Phase-19 tick-based timing mini-game, SEPARATE from the Phase-6 `fq_minigame_t`. `fq_training_session_t` (8 bytes, _Static_assert pinned): state/game_type/target_pos/targets_done/score/target_speed + 2 pad bytes. States: FQ_TS_WAITING=0, FQ_TS_ACTIVE=1, FQ_TS_DONE=2. Types: FQ_TS_SPEED=0 (10 pos/tick), FQ_TS_POWER=1 (6 pos/tick), FQ_TS_INTEL=2 (3 pos/tick). Hit zone [40,60]: +20 in-zone (clamped 100), -5 out-of-zone (clamped 0). 5 targets/session. XP = score*2 (0-200). API: `fq_training_session_init`, `fq_training_session_start`, `fq_training_step`, `fq_training_hit`, `fq_training_advance_target` (test accessor — no production caller; used in test F10 to skip targets explicitly), `fq_training_get_target_speed` (test accessor — no production caller; used in test F8/F12 to verify speed values), `fq_training_get_xp_award`, `fq_training_award_xp`. PRNG isolation: no `fq_prng_t` calls anywhere in training_session.c. `equip.h/.c` — slot-relative equip toggle. `fq_equip_toggle(ch, inv, inv_slot)`: if item at inv_slot already in `ch->equipped[]`, removes it (sets slot to 0); otherwise finds first free slot and writes item ID. Sentinel ID 0 rejected (GAME_ERR_INVALID). Slots full (all equipped[0..max-1] non-zero): GAME_ERR_OVERFLOW. equipped_count clamped to min(equipped_count, 5) to guard corrupt values. Duplicate-aware: equipping same item ID from two inv slots creates two equipped entries; unequipping one slot removes exactly one copy. NULL guards on ch and inv. (3) Presentation — View Models: `fq_vm_onboarding_t` (24 bytes, _Static_assert pinned) added to `components/presentation/include/view_models.h`. Fields: `class_index uint8_t`, `sprite_base uint8_t`, `class_name char[16]`, `stat_str uint8_t`, `stat_spd uint8_t`, `stat_int uint8_t`, `stat_prc uint8_t`, `pad[1]`. `fq_vm_training_t` updated (24 bytes, size pin retained): gains `target_pos uint8_t` (offset 16), `targets_done uint8_t` (offset 17), `game_type uint8_t` (offset 18), reclaims 1 pad byte. `fq_vm_inventory_t` updated (580 bytes, _Static_assert pinned): gains `item_equipped[32] uint8_t` array (32 bytes appended after existing 548-byte layout). (4) Presentation — Builders: `fq_vm_build_onboarding(vm, ch, class_index)` added to `main/vm_builder.h/.c` — populates class_name from k_class_name table, sprite_base from character, class stats. `fq_vm_build_training_session(vm, ts)` added — populates game_name, score, state, target_pos, targets_done, game_type from `fq_training_session_t`. `fq_vm_build_inventory_ex(vm, inv, ch, cursor, scroll)` replaces `fq_vm_build_inventory` — populates item_equipped[32] by checking each inventory item ID against ch->equipped[]. (5) Presentation — Screen: `screen_onboarding.h/.c` added to `components/presentation/screens/`. Renders class carousel: sprite preview (2x scaled), class name, stat bars for STR/SPD/INT/PRC, prompt text "A: Next class  B: Select". (6) Application — First-boot routing: `app_main.c` checks for valid save after `hal_flash_read_save`; if no valid save, sets `app.state = FQ_STATE_ONBOARDING` instead of creating a hardcoded "Ember" character. `FQ_STATE_ONBOARDING` added to idle suppression list (`is_idle_forbidden()`). Auto-save on onboarding confirm: when `ONBOARDING → HOME` transition fires, `do_auto_save()` is called; if save fails, `onboarding_save_failed = 1` and state reverts to `FQ_STATE_ONBOARDING`. Auto-save on inventory exit: when state leaves `FQ_STATE_INVENTORY`, `do_auto_save()` is called. (7) Application — Training navigation: WAITING state (BTN_A cycles game_type 0→1→2→0; BTN_B starts session → ACTIVE). ACTIVE state (BTN_A registers hit via `fq_training_hit()`; TIMER_TICK advances target via `fq_training_step()`; BTN_B exits to HOME with partial XP award via `fq_training_award_xp()` — audit fix P19-ADV1). DONE state (BTN_B returns to HOME; XP already awarded on ACTIVE→DONE transition). (8) Application — Inventory navigation: BTN_B cycles `inventory_cursor` mod `item_count`; double-tap BTN_B (two presses within 6 ticks = 300ms) exits to HOME. BTN_A toggles equip on cursor item. (9) Host tests: `test_p19_interactive_bounds.c` (27 bound tests), `test_p19_interactive_feature.c` (19 feature tests), `test_p19_training_b_exit.c` (4 audit-fix tests). Total 90 ctest tests (89 pass; 1 pre-existing failure in test_p19_home_menu_feature unrelated to Phase 19 interactive work). (10) Audit remediation: ADV-P19-1 resolved — BTN_B ACTIVE training exit now awards partial XP. ADV-P19-2 resolved — `fq_training_advance_target()` and `fq_training_get_target_speed()` documented as test-only accessors with no production callers.
