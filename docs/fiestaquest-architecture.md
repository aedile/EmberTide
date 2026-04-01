# FiestaQuest -- Architectural Design Document v2

**Target:** ESP-IDF v5.x on ESP32-S3-PICO-1-N8R8
**Reference:** FiestaQuest Game Design Document v5
**Methodology:** TDD red-first, clean architecture, defense in depth

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
    hal/
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
        modifier_engine.h, state_machine.h, save_format.h
      src/
        (one .c per header)

    presentation/
      include/
        view_models.h             // all screen view model structs
        renderer.h                // framebuffer ops, draw primitives
        framebuffer.h             // 200x200 1-bit framebuffer type
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
    view_model_builder.h          // game state -> view model translation
    view_model_builder.c

  test/
    host/
      CMakeLists.txt              // links game/ + presentation/ (minus screen_mgr)
      test_prng.c
      test_crc32.c
      test_combat.c
      test_combat_determinism.c
      test_combat_stepper.c
      test_item_engine.c
      test_modifier_engine.c
      test_progression.c
      test_training.c
      test_training_scoring.c
      test_mini_game_scoring.c
      test_state_machine.c
      test_save_format.c
      test_view_models.c
      fixtures/
        test_fixtures.h           // make_test_bruiser(), etc.
        test_fixtures.c
      mocks/
        mock_storage.h, mock_input.h, mock_clock.h
        mock_storage.c, mock_input.c, mock_clock.c

    visual/                       // VISUAL TEST HARNESS (see Section 7)
      CMakeLists.txt              // links game/ + presentation/ (minus screen_mgr)
      render_all_screens.c        // renders every screen to framebuffer
      framebuffer_to_png.h        // uses stb_image_write
      framebuffer_to_png.c
      scenarios/
        scenario_fresh_character.c
        scenario_mid_game.c
        scenario_dead_character.c
        scenario_combat_round.c
        scenario_training_session.c
        scenario_inventory_full.c
        scenario_onboarding.c
        scenario_rebirth.c
      output/                     // generated PNGs land here
      golden/                     // approved reference PNGs for regression
      diff_screens.py             // pixel-diff output/ vs golden/, flag regressions
      review_screens.py           // optional: send PNGs to LLM API for review

    target/
      test_hal_epaper.c, test_hal_i2c.c, test_hal_ble.c,
      test_storage_integration.c, test_input_integration.c,
      test_power_integration.c

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
int      fq_prng_range(fq_prng_t *rng, int min, int max);

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

int fq_prng_range(fq_prng_t *rng, int min, int max) {
    uint32_t raw = fq_prng_next(rng);
    return min + (int)(raw % (uint32_t)(max - min + 1));
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
    0,  3,  5,  6,  7,  7,  8,  8,  9,  9, 10, 10, 10, 10, 11, 11,
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

```c
#ifndef FIESTAQUEST_COMBAT_H
#define FIESTAQUEST_COMBAT_H

#include "types.h"
#include "prng.h"

#define FQ_MAX_ROUNDS 12
#define FQ_OVERTIME_START 9

typedef struct {
    int16_t f1_hp;
    int16_t f2_hp;
    int8_t  f1_damage_dealt;
    int8_t  f2_damage_dealt;
    bool    f1_dodged;
    bool    f2_dodged;
    bool    f1_crit;
    bool    f2_crit;
    int8_t  overtime_damage;
    bool    f1_lucky_star;          // bonus attack triggered this round
    bool    f2_lucky_star;
} fq_round_result_t;

typedef struct {
    fq_round_result_t rounds[FQ_MAX_ROUNDS];
    uint8_t           round_count;
    uint8_t           winner;       // 1, 2, or 0 (draw)
    uint32_t          crc32;
} fq_combat_log_t;

typedef struct {
    fq_character_t  fighter;
    fq_item_def_t   equipped_items[5];
    uint8_t         equipped_count;
    bool            has_lucky_star;
    bool            has_ghost_walk;
} fq_combat_fighter_t;

// Opaque combat context. Stack-allocatable.
typedef struct {
    fq_prng_t           rng;
    fq_combat_fighter_t fighters[2];
    fq_combat_state_t   state;      // mutable HP, debuffs, item state
    fq_combat_log_t     log;
    uint8_t             current_round;
    bool                finished;
} fq_combat_ctx_t;

// Initialize combat context. Does not advance any rounds.
void fq_combat_init(
    fq_combat_ctx_t *ctx,
    const fq_combat_fighter_t *f1,   // canonical Fighter 1 (initiator)
    const fq_combat_fighter_t *f2,   // canonical Fighter 2 (responder)
    uint32_t prng_seed
);

// Advance one round. Returns the round result.
// Caller is responsible for BLE sync and rendering between calls.
fq_round_result_t fq_combat_step(fq_combat_ctx_t *ctx);

// Is the fight over?
bool fq_combat_is_finished(const fq_combat_ctx_t *ctx);

// Get current HP for both fighters (for BLE ROUND_CHECK).
void fq_combat_get_hp(const fq_combat_ctx_t *ctx, int16_t *f1_hp, int16_t *f2_hp);

// Finalize and return complete log (only valid after is_finished == true).
fq_combat_log_t fq_combat_finalize(fq_combat_ctx_t *ctx);

// Convenience: run entire fight at once (for testing / balance simulation).
fq_combat_log_t fq_combat_resolve(
    const fq_combat_fighter_t *f1,
    const fq_combat_fighter_t *f2,
    uint32_t prng_seed
);

#endif
```

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

(Unchanged from v1 except: iteration covers slots 0-4, `fq_combat_state_t` is now part of `fq_combat_ctx_t`.)

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
#include "framebuffer.h"

typedef void (*fq_mg_done_cb_t)(fq_mg_result_t result, void *user_data);

typedef struct {
    fq_mini_game_id_t   game;
    fq_framebuffer_t    *fb;        // render target
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
    FQ_SAVE_ERR_CRC,
    FQ_SAVE_ERR_VERSION_TOO_NEW,
    FQ_SAVE_ERR_CORRUPT,
    FQ_SAVE_ERR_BUFFER_TOO_SMALL,
} fq_save_result_t;

// Serialize to byte buffer. Field-by-field, explicit byte order (little-endian).
// Returns bytes written.
size_t fq_save_serialize(
    const fq_character_t *character,
    const fq_inventory_t *inventory,
    const fq_modifier_pool_t *modpool,
    uint8_t *buffer, size_t buffer_size
);

// Deserialize from byte buffer. Validates CRC. Runs migration if version < current.
fq_save_result_t fq_save_deserialize(
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

Screen renderers receive read-only view structs. They never include `game/types.h`. The application layer (view_model_builder.c) is the only module that knows both game structs and view structs.

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
} fq_view_home_t;

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
} fq_view_combat_t;

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
} fq_view_inventory_t;

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
#include "framebuffer.h"
#include "view_models.h"

void screen_home_render(fq_framebuffer_t *fb, const fq_view_home_t *view);
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
} fq_framebuffer_t;

// Set a single pixel. x: 0-199, y: 0-199. color: 0=white, 1=black.
void fq_fb_set_pixel(fq_framebuffer_t *fb, int x, int y, uint8_t color);

// Get a single pixel.
uint8_t fq_fb_get_pixel(const fq_framebuffer_t *fb, int x, int y);

// Clear entire framebuffer (all white).
void fq_fb_clear(fq_framebuffer_t *fb);

// Fill rectangle.
void fq_fb_fill_rect(fq_framebuffer_t *fb, int x, int y, int w, int h, uint8_t color);

#endif
```

### 7.3 PNG Export (host only)

```c
// test/visual/framebuffer_to_png.h
#include "framebuffer.h"

// Write framebuffer to a 200x200 grayscale PNG.
// White pixels = 255, black pixels = 0. Scaled up 3x for readability (600x600 output).
int fq_fb_write_png(const fq_framebuffer_t *fb, const char *path);
```

Implementation uses `stb_image_write.h` (single-header, public domain). Scales 3x so the 200x200 image is legible when viewed on a monitor or by an LLM.

### 7.4 Scenario Structure

```c
// test/visual/scenarios/scenario_fresh_character.c

#include "framebuffer.h"
#include "view_models.h"
#include "screens/screen_home.h"
#include "screens/screen_stats.h"
#include "framebuffer_to_png.h"

void render_fresh_character_screens(const char *output_dir) {
    fq_framebuffer_t fb;

    // HOME screen: fresh Bruiser, level 1
    fq_view_home_t home = {
        .name = "Iron Bones",
        .level = 1,
        .class_id = 0,  // Bruiser
        .is_dead = false,
        .ble_advertising = false,
        .wins = 0, .losses = 0,
    };
    fq_fb_clear(&fb);
    screen_home_render(&fb, &home);
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
// hal/include/hal_pins.h
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
1. app_main.c, event_bus.c, view_model_builder.c
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
