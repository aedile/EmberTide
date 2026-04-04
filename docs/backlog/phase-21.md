# Phase 21: I2S Audio Engine + sfxr-c Sound Effects

## Item 1: I2S Audio HAL — ES8311 Codec Driver

### User Story
As a player, I want high-quality audio output through the onboard speaker so I can hear retro game sound effects during gameplay.

### Architecture Decisions

**I2S replaces LEDC PWM.** The original Phase 21 spec used LEDC for piezo beeps. With .mod music files planned for Phase 22, the audio HAL must output PCM samples via I2S → ES8311 codec. This supports both SFX (Phase 21) and music (Phase 22) through the same output path.

**Audio format:** 16-bit signed mono, 22050 Hz sample rate. This is sufficient for retro game audio and keeps DMA buffer sizes reasonable on ESP32-S3 (SPIRAM available for larger buffers).

**DMA buffer allocation:** I2S DMA buffers MUST be allocated with `heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)` — NOT default `malloc()` which routes to PSRAM. PSRAM is not DMA-accessible on ESP32-S3. This is documented in `sdkconfig.defaults` (A5 WARNING).

**Audio ring buffer:** Fixed-size static array `AUDIO_RING_BUF_SAMPLES = 4096` (8KB at 16-bit). SPSC (single-producer single-consumer) for Phase 21 — game loop writes, audio task reads. Phase 22 will add a mixer that pre-mixes music + SFX before writing to the ring buffer, so the SPSC model holds. Ring buffer lives in internal SRAM (not PSRAM) for DMA safety. Overflow: newest samples dropped (not oldest — preserves audio continuity for what's already queued).

**Audio task:** Dedicated FreeRTOS task (priority 5, 4KB stack) that feeds DMA buffers from the ring buffer. sfxr-c generation runs in the GAME LOOP task (not the audio task), so the 4KB audio task stack only needs to handle I2S writes.

**HAL API evolution:** `hal_audio_play(freq, duration)` is retained but internally generates a square wave into the ring buffer. Duration clamped to `AUDIO_MAX_TONE_MS = 2000` (prevents ring buffer overflow — max 44100 samples at 22050 Hz). New API: `hal_audio_write_samples(const int16_t *buf, size_t count)` for raw PCM output from sfxr-c.

**Error code ABI:** New `hal_audio_err_t` values MUST be appended after existing values (HAL_AUDIO_OK=0, ERR_INIT=1, ERR_INVALID_FREQ=2). New: ERR_NULL=3, ERR_OVERFLOW=4. Existing values pinned by `_Static_assert`.

**ES8311 codec:** I2C address 0x18 (ASEL=LOW). Init failure returns `HAL_AUDIO_ERR_INIT`. I2C address must be documented in `pin-definitions.md`.

### Acceptance Criteria
- [ ] `hal_audio.c` wired to ESP-IDF I2S driver + ES8311 codec (behind `CONFIG_BSP_AUDIO`)
- [ ] I2S configured: 16-bit mono, 22050 Hz, DMA double-buffer
- [ ] GPIO42 (Audio_PWR) and GPIO46 (PA_EN) driven correctly for amp enable
- [ ] Audio task runs at priority 5, feeds I2S from a ring buffer
- [ ] `hal_audio_play(freq, duration)` still works (generates square wave samples)
- [ ] New `hal_audio_write_samples()` for raw PCM output
- [ ] `hal_audio_init()` / `hal_audio_deinit()` manage I2S + codec lifecycle
- [ ] Host mock updated: captures written samples for test inspection

### Negative Test Requirements
- **Frequency 0:** Returns `HAL_AUDIO_ERR_INVALID_FREQ`
- **Write samples before init:** Returns `HAL_AUDIO_ERR_INIT`
- **NULL buffer to write_samples:** Returns `HAL_AUDIO_ERR_NULL`
- **Buffer overflow:** Ring buffer full → newest samples dropped (no block, no crash)
- **Max duration clamp:** `hal_audio_play(440, 65535)` clamps to `AUDIO_MAX_TONE_MS` (2000ms), no overflow
- **Error code ABI:** `HAL_AUDIO_ERR_INIT == 1`, `HAL_AUDIO_ERR_INVALID_FREQ == 2` pinned via `_Static_assert`
- **Deinit while playing:** `hal_audio_deinit()` signals audio task to stop, waits for termination, then tears down I2S
- **High frequency aliasing:** `hal_audio_play(22050, 100)` produces valid (if aliased) output, no crash

### Files to Modify
- `components/fq_hal/include/hal_audio.h` (extend API)
- `components/fq_hal/src/hal_audio.c` (I2S + ES8311 + audio task)
- `test/host/mock_hal_audio.c` (sample capture, ring buffer mock)

---

## Item 2: sfxr-c Integration — Procedural Sound Effects

### User Story
As a player, I want retro-style synthesized sound effects (hits, beeps, power-ups) that feel like a classic 8-bit game, generated procedurally without storing large audio files.

### Architecture Decisions

**sfxr-c** is a tiny C port of the sfxr synthesizer (~500 lines, no malloc, no float on output path). It generates 8-bit/16-bit PCM from parameter presets. Each SFX is defined by a `sfxr_params_t` struct (seed + envelope + waveform settings).

**Library placement:** `components/game/lib/sfxr.h` / `sfxr.c` — vendored as a single-file library within game/ since SFX definitions are game logic. The library generates PCM samples; `hal_audio_write_samples()` plays them.

**SFX table:** `components/game/include/sfx.h` defines `fq_sfx_id_t` enum and a `static const` table of sfxr parameter presets. `fq_sfx_play(id)` generates samples and pushes to the audio ring buffer.

**Architecture boundary:** `sfxr.h/c` in `game/lib/` is a pure PCM generator — it does NOT include any `hal_*.h` headers. `fq_sfx_play()` in `sfx.c` generates samples into a local buffer, then returns them. The APPLICATION LAYER (`main/app_main.c`) calls `fq_sfx_play()` to get the PCM data, then calls `hal_audio_write_samples()` to push it to the ring buffer. This preserves the game→main→HAL dependency direction.

**Return type:** `fq_sfx_play()` returns `game_err_t` (not HAL types). It writes PCM into a caller-provided buffer.

**Non-blocking:** SFX generation is fast enough (~1ms for a short effect at 22050 Hz) to run inline in the game loop. Max SFX duration clamped to 500ms (11025 samples). The generated samples are passed to the application layer which writes them to the ring buffer; the audio task plays them asynchronously.

**Quiet mode:** A `uint8_t sfx_enabled` flag in `fq_app_ctx_t` (application layer). `fq_sfx_play()` itself has no quiet-mode awareness — the caller checks the flag before calling. No cross-layer dependency.

### Acceptance Criteria
- [ ] sfxr-c vendored in `components/game/lib/sfxr.h` and `sfxr.c`
- [ ] `fq_sfx_id_t` enum: SFX_BTN_PRESS, SFX_BTN_BACK, SFX_MENU_NAVIGATE, SFX_COMBAT_HIT, SFX_COMBAT_MISS, SFX_COMBAT_CRIT, SFX_LEVEL_UP, SFX_ITEM_EQUIP, SFX_DEATH, SFX_REBIRTH
- [ ] `fq_sfx_play(id)` generates PCM via sfxr-c and writes to audio ring buffer
- [ ] Each SFX preset tuned for retro 8-bit feel (envelope attack/decay, waveform type)
- [ ] SFX presets are `static const` — no runtime allocation
- [ ] SFX generation does not block the main loop (< 5ms per effect)

### Negative Test Requirements
- **Invalid SFX ID:** `fq_sfx_play(SFX_COUNT)` returns error, no crash
- **SFX before audio init:** Returns error gracefully
- **Rapid SFX spam:** 10 rapid calls don't crash or leak memory (ring buffer handles overflow)

### Files to Create
- `components/game/lib/sfxr.h` / `sfxr.c` (vendored library)
- `components/game/include/sfx.h` (SFX enum + play API)
- `components/game/src/sfx.c` (preset table + generation glue)

---

## Item 3: Wire Sound Effects into Gameplay

### User Story
As a player, I want to hear sound effects during gameplay — button presses, combat hits, level-ups — without any gameplay lag.

### Acceptance Criteria
- [ ] `app_main.c` calls `fq_sfx_play(SFX_BTN_PRESS)` on every button event
- [ ] Combat round dispatch triggers SFX_COMBAT_HIT / SFX_COMBAT_MISS / SFX_COMBAT_CRIT based on round result
- [ ] Level-up triggers SFX_LEVEL_UP
- [ ] Rebirth triggers SFX_REBIRTH
- [ ] Item equip triggers SFX_ITEM_EQUIP
- [ ] Menu navigation triggers SFX_MENU_NAVIGATE
- [ ] All SFX calls are non-blocking (game loop continues during playback)
- [ ] SFX can be disabled via a flag (for quiet mode / testing)

### Negative Test Requirements
- **SFX in quiet mode:** All `fq_sfx_play()` calls are no-ops when disabled
- **SFX during idle screen:** No SFX triggered while idle overlay is active

### Files to Modify
- `main/app_main.c` (wire sfx calls into event handlers)
- `main/app_fsm.c` (combat SFX triggers in round dispatch)
