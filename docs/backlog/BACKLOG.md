# FiestaQuest Backlog

## Phase 1: Project Skeleton & Toolchain Initialization
* **Item 1:** Establish CMake Root, `components` directory, and `main` entrypoint
* **Item 2:** Setup `test/host` CTest suite with unity/cmock
* **Item 3:** Setup `test/visual` compilation target and `stb_image_write.h` wrapper
* **Item 4:** Configure ESP-IDF `sdkconfig` optimized for e-paper / low power

## Phase 2: Foundational Math & Determinism
* **Item 1:** Implement XOR-shift PRNG (`fq_prng_t`) and boundaries
* **Item 2:** Implement CRC32 hash generator
* **Item 3:** Implement Effective Stat Curve Lookup Table & integer interpolation logic
* **Item 4:** Implement `fq_combat_determinism_test.c` asserting exactly predictable roll sequences

## Phase 3: Core Data Structures & Storage Layout
* **Item 1:** Define Base Enums and Structs (`fq_character_t`, `fq_inventory_t`, `fq_item_def_t`)
* **Item 2:** Implement LittleFS explicit binary serializer/deserializer (`save_format.h`)
* **Item 3:** Write Host Integration Test simulating Save/Load disk corruption handling

## Phase 4: Combat Engine Core API
* **Item 1:** Define `fq_combat_state_t` and `fq_combat_ctx_t`
* **Item 2:** Implement pre-combat stat adjustments and initiative rolling
* **Item 3:** Implement `fq_combat_step()` round stepper (attack, dodge, damage apply)
* **Item 4:** Implement overtime penalty and "Lucky Star" conditional roll logic

## Phase 5: Item Engine & Synergies
* **Item 1:** Define Item evaluation loop and conditional structs
* **Item 2:** Implement specific Common/Uncommon Item Trigger Actions
* **Item 3:** Implement Rare/Legendary complex edge-case Items (e.g., Cursed Crown, Phoenix Down)
* **Item 4:** Write Host Balance Test simulating 10,000 fights with random loadouts

## Phase 6: Training & Progression
* **Item 1:** Implement Mini-game normalized scoring and thresholds
* **Item 2:** Implement Modifier Engine and Tag Synergy multipliers
* **Item 3:** Implement Level-up XP Curve and Stat Point distribution logic
* **Item 4:** Implement Rebirth penalty logic and Legacy Tree unlocking

## Phase 7: Visual Render Primitives (Host UI)
* **Item 1:** Implement `fq_framebuffer_t` geometry math and 1-bit line drawing
* **Item 2:** Implement `sprite_mgr.c` parsing 1-bit packed binary assets
* **Item 3:** Implement variable-width Font plotting using generated metrics
* **Item 4:** Write E2E Visual Test verifying primitive geometries into output PNGs

## Phase 8: View Models & UI Screens Part 1
* **Item 1:** Implement `view_model_builder` for HOME state
* **Item 2:** Implement `screen_home.c` renderer and host screenshot visual test
* **Item 3:** Implement INVENTORY view model and scrolling grid renderer
* **Item 4:** Implement STATS view model and page

## Phase 9: View Models & UI Screens Part 2
* **Item 1:** Implement COMBAT round summary renderer (HP bars, floating text)
* **Item 2:** Implement TRAINING session FSM view and selections
* **Item 3:** Implement ONBOARDING renderer (Class/Name generation)
* **Item 4:** Write comprehensive `scenario_test.c` validating all screens render properly without overlap

## Phase 10: Connectivity Data Protocol
* **Item 1:** Define BLE GATT characteristic UUIDs and Service wrappers
* **Item 2:** Implement Character Summary and Nonce exchange C structs (serialization)
* **Item 3:** Implement Round-Sync HP checker and CRC mismatch detector
* **Item 4:** Write Host Test simulating disconnected/dropping connection states

## Phase 11: Application Event Loop
* **Item 1:** Implement tagged Event Bus (`event_bus.c`) for async hardware signals
* **Item 2:** Implement Root State Machine controller (`app_main.c` orchestrator)
* **Item 3:** Implement translation layer pushing App State to View Models securely

## Phase 12: Hardware Abstraction & HAL Part 1
* **Item 1:** Implement `hal_epaper.c` (SPI display flush routines)
* **Item 2:** Implement `hal_flash.c` wrapping LittleFS
* **Item 3:** Implement `hal_wifi.c` configuration and status polling

## Phase 13: Hardware Abstraction & HAL Part 2
* **Item 1:** Implement `hal_gpio.c` button IRQ debouncing (2 buttons)
* **Item 2:** Implement `hal_audio.c` piezo buzzer PWM generation
* **Item 3:** Implement `hal_sleep.c` auto-deep-sleep / wakestub interrupts

## Phase 14: Captive Portal & OTA
* **Item 1:** Implement HTTPD captive portal for WiFi credentials (`captive_portal.c`)
* **Item 2:** Implement HTTPS downloader for `ota_service.c`
* **Item 3:** Implement Secure JSON asset patcher for OTA content upgrades

## Phase 15: Hardware E2E Validation
* **Item 1:** Write `test_hal_epaper.c` target test
* **Item 2:** Write `test_storage_integration.c` target test
* **Item 3:** Flash full game image to physical ESP32-S3 and perform user sanity checks
