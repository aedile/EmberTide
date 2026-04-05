# Phase 22: MOD Music Playback + Audio Mixing

## Item 1: micromod Integration — MOD Tracker Playback

### User Story
As a player, I want background music playing from the 31 tracker .mod files included with the game, giving the experience an authentic retro chiptune soundtrack.

### Architecture Decisions (from spec-challenger)

**micromod** is a tiny C MOD/S3M player (~800 lines, no malloc, fixed-point math). It renders ProTracker .mod files into PCM sample buffers. Perfect for ESP32-S3 with SPIRAM.

**Library placement:** `components/game/lib/micromod.h` / `micromod.c` — vendored alongside sfxr-c. micromod is a pure PCM renderer — it does NOT include any `hal_*.h` headers.

**MOD file loading:** Full file load into SPIRAM (no streaming — flash read latency would cause underruns). New HAL API: `hal_flash_read_file(const char *path, uint8_t *buf, size_t buf_size, size_t *bytes_read)` added to `hal_flash.h`. `MAX_MOD_FILE_SIZE = 524288u` (512 KiB) — caps SPIRAM usage. Files exceeding this are rejected.

**SPIRAM allocation:** `.mod` buffer allocated with `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. NULL return → error, no crash. Buffer freed on `fq_music_stop()`.

**Pattern loop guard:** micromod render capped at `MAX_MOD_RENDER_SAMPLES` (22050 * 600 = ~10 minutes). If a malformed .mod loops infinitely, render stops after the cap.

**Sample rate:** micromod initialized at 22050 Hz to match `AUDIO_SAMPLE_RATE_HZ`. `_Static_assert` enforces match.

### Acceptance Criteria
- [x] micromod vendored in `components/game/lib/micromod.h` and `micromod.c`
- [x] `hal_flash_read_file()` added to HAL API with NULL/path/size guards
- [x] .mod files loadable from LittleFS partition into SPIRAM (`MAX_MOD_FILE_SIZE = 512 KiB`)
- [x] `fq_music_init(const uint8_t *mod_data, size_t len)` loads a MOD into the player
- [x] `fq_music_play()` starts continuous background playback
- [x] `fq_music_stop()` stops playback, frees MOD data buffer
- [x] `fq_music_render(int16_t *buf, size_t samples)` renders next N samples
- [x] Music loops seamlessly (no gap at end-of-pattern)
- [x] Volume adjustable 0-255
- [x] Pattern loop guard: max 10 minutes of render per load

### Negative Test Requirements
- **NULL mod data:** Returns error
- **Truncated mod file:** Graceful error (no crash, no infinite loop)
- **File exceeds MAX_MOD_FILE_SIZE:** Rejected with error
- **SPIRAM allocation failure:** Returns error, no crash
- **hal_flash_read_file NULL path:** Returns ERR_NULL
- **hal_flash_read_file non-existent path:** Returns ERR_NOT_FOUND
- **hal_flash_read_file buf_size=0:** Returns ERR_SIZE
- **Play before init:** Returns error
- **micromod sample rate mismatch:** Compile-time assertion

### Files to Create
- `components/game/lib/micromod.h` / `micromod.c` (vendored)
- `components/game/include/music.h`
- `components/game/src/music.c`
- `components/fq_hal/include/hal_flash.h` (extend with `hal_flash_read_file`)
- `components/fq_hal/src/hal_flash.c` (implement file read)
- `test/host/mock_hal_flash.c` (extend with file read mock)

---

## Item 2: Audio Mixing — Music + SFX

### User Story
As a player, I want to hear both background music AND sound effects at the same time, with SFX taking priority (ducking music slightly when an effect plays).

### Architecture Decisions (from spec-challenger)

**Mixing location: APPLICATION LAYER (`main/app_main.c`), NOT hal_audio.c.** The audio task in `hal_audio.c` is the I2S consumer — it drains the ring buffer and feeds I2S DMA. The mix loop runs in the MAIN LOOP task, which calls `fq_music_render()` to get music samples, retrieves any pending SFX samples, mixes them, and writes the result to the ring buffer via `hal_audio_write_samples()`. This preserves:
- SPSC ring buffer model (main loop = sole producer, audio task = sole consumer)
- Architecture boundary (hal/ never includes game/ headers)
- Single-threaded mixing (no mutexes needed)

**Mixing runs every main loop tick (50ms).** Each tick renders 1102 samples of music (50ms at 22050 Hz). If SFX is pending, it's mixed additively. Combined output is written to the ring buffer. If the ring buffer is near-full, the music render is shortened to fit.

**Clamp function:** `static inline int16_t clamp16(int32_t v)` — clamps to [-32768, 32767]. Used for all mixing arithmetic.

**Volume:** `uint8_t music_vol` (0-255). Scaling: `(int32_t)sample * music_vol / 256`. Volume 255 ≈ 99.6% — documented as max, not unity.

**SFX ducking:** When SFX samples are non-zero in the current mix chunk, music volume is temporarily scaled to 60% (`music_vol * 153 / 256`). Ducking disengages when no SFX samples remain in the chunk. No persistent state needed — checked per-chunk.

**Ring buffer underrun:** Audio task reads from empty buffer → outputs silence (zero-filled DMA buffer). No stale data.

### Acceptance Criteria
- [x] Mix loop in `main/app_main.c` renders music + SFX each tick
- [x] `clamp16()` prevents int16 overflow/wrap in all mixing paths
- [x] Music volume scaling: `sample * music_vol / 256` with int32 intermediate
- [x] Music volume ducking to 60% while SFX samples are non-zero
- [x] Ring buffer underrun → silence (not stale data)
- [x] Combined mix output per tick never exceeds `AUDIO_RING_BUF_SAMPLES`
- [x] SFX without music plays cleanly (zero-filled music channel)
- [x] Music without SFX plays cleanly (zero-filled SFX channel)

### Negative Test Requirements
- **Mix overflow:** INT16_MAX + INT16_MAX clamps to INT16_MAX, not wrap
- **Mix underflow:** INT16_MIN + INT16_MIN clamps to INT16_MIN
- **Volume 0:** Music channel is silence
- **Volume 255 + max SFX:** Clamps correctly
- **SFX without music:** Clean output
- **Music without SFX:** Clean output
- **Rapid-fire SFX ducking:** Ducking does not permanently reduce volume

### Files to Modify
- `main/app_main.c` (mix loop, do_music_tick helper)
- `components/fq_hal/src/hal_audio.c` (underrun → silence behavior)

---

## Item 3: Music Selection + Gameplay Integration

### User Story
As a player, I want different music tracks for different game states — upbeat for the home screen, intense for combat, mellow for the inventory.

### Architecture Decisions

**Track selection uses `esp_random()` or `tick_count` for randomness — NOT combat PRNG.** Constitution Priority 0: combat PRNG must not be touched outside FQ_STATE_BATTLE.

**State transition → clean stop-start (no crossfade).** Crossfade requires two simultaneous .mod buffers (1 MiB SPIRAM) and double CPU cost. Clean stop-start is simpler and sufficient for a retro aesthetic.

**Music mute toggle:** `uint8_t music_enabled` field in `fq_app_ctx_t`. Saved to flash. Save format backward compatibility: if deserializing an older save (no music_enabled field), default to `music_enabled = 1`.

**Idle screen:** `fq_music_stop()` on idle activation. On wake, resume the track for the current FSM state.

### Acceptance Criteria
- [x] Music track table mapping FSM states to .mod file paths (categories: chill, intense, upbeat)
- [x] Home screen: random track from "chill" category
- [x] Combat: random track from "intense" category
- [x] Onboarding/menus: random track from "upbeat" category
- [x] Track changes on state transition (clean stop-start)
- [x] Music stops during idle screen (save power)
- [x] Music resumes correct track on idle wake
- [x] Player can mute music via settings toggle (preserved in save)
- [x] Save format backward compatible (missing field → music_enabled=1)
- [x] Track randomization does NOT touch combat PRNG

### Negative Test Requirements
- **Missing .mod file on filesystem:** Graceful fallback (no music, no crash)
- **Empty category (no tracks mapped):** No music for that state, no crash
- **PRNG isolation:** Combat PRNG state unchanged after track selection
- **Save backward compatibility:** Old save (no music field) deserializes with music_enabled=1

### Files to Create/Modify
- `components/game/include/music_table.h` (new — track → state mapping)
- `components/game/src/music_table.c` (new — category arrays)
- `main/app_main.c` (state-based music triggers, idle stop/resume)
- `main/app_fsm.h` (add music_enabled, music_vol, current_track fields; update _Static_assert)
- `components/game/include/save_format.h` (extend for music_enabled — backward compatible)
