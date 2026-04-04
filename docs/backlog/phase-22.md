# Phase 22: MOD Music Playback + Audio Mixing

## Item 1: micromod Integration — MOD Tracker Playback

### User Story
As a player, I want background music playing from the 31 tracker .mod files included with the game, giving the experience an authentic retro chiptune soundtrack.

### Architecture Decisions

**micromod** is a tiny C MOD/S3M player (~800 lines, no malloc, fixed-point math). It renders ProTracker .mod files into PCM sample buffers. Perfect for ESP32-S3 with SPIRAM.

**Library placement:** `components/game/lib/micromod.h` / `micromod.c` — vendored alongside sfxr-c.

**MOD file loading:** .mod files stored in LittleFS flash partition (the existing `storage` partition has ~4.8 MiB free). `hal_flash_read_file()` or direct LittleFS API loads the .mod data into SPIRAM. micromod operates on the in-memory buffer.

**Music API:** `components/game/include/music.h`:
- `fq_music_init(const uint8_t *mod_data, size_t len)` — load a MOD into the player
- `fq_music_play()` / `fq_music_stop()` / `fq_music_set_volume(uint8_t vol)`
- `fq_music_render(int16_t *buf, size_t samples)` — render next N samples (called by audio task)

### Acceptance Criteria
- [ ] micromod vendored in `components/game/lib/micromod.h` and `micromod.c`
- [ ] .mod files loadable from LittleFS partition into SPIRAM
- [ ] `fq_music_play()` starts continuous background playback
- [ ] `fq_music_stop()` stops playback, releases MOD data
- [ ] Music loops seamlessly (no gap at end-of-pattern)
- [ ] Volume adjustable 0-255

### Negative Test Requirements
- **NULL mod data:** Returns error
- **Truncated mod file:** Graceful error (no crash, no infinite loop in pattern parser)
- **Play before init:** Returns error

### Files to Create
- `components/game/lib/micromod.h` / `micromod.c` (vendored)
- `components/game/include/music.h`
- `components/game/src/music.c`

---

## Item 2: Audio Mixing — Music + SFX

### User Story
As a player, I want to hear both background music AND sound effects at the same time, with SFX taking priority (ducking music slightly when an effect plays).

### Architecture Decisions

**Simple additive mix with clamp.** The audio task renders both sources each DMA cycle:
1. `fq_music_render(music_buf, N)` — get N music samples
2. Copy SFX ring buffer samples into `sfx_buf`
3. Mix: `out[i] = clamp16(music_buf[i] * music_vol/256 + sfx_buf[i])`
4. Write mixed buffer to I2S

**SFX priority ducking:** When SFX is active, music volume is temporarily reduced to 60% for the duration of the effect. This prevents SFX from being drowned out.

**No separate mixing library needed.** The mix is a simple loop — ~50 lines of code in the audio task.

### Acceptance Criteria
- [ ] Audio task mixes music + SFX into a single I2S output stream
- [ ] Additive mixing with int16_t clamping (no overflow distortion)
- [ ] Music volume ducking while SFX is playing (60% of set volume)
- [ ] Music continues playing during SFX (not paused)
- [ ] Silence when both music and SFX are idle (no static/noise)

### Negative Test Requirements
- **Mix overflow:** INT16_MAX + INT16_MAX clamps to INT16_MAX, not wrap
- **SFX without music:** SFX plays cleanly with zero-filled music channel
- **Music without SFX:** Music plays cleanly with zero-filled SFX channel

### Files to Modify
- `components/fq_hal/src/hal_audio.c` (audio task mixing loop)
- `main/app_main.c` (music selection, play/stop triggers)

---

## Item 3: Music Selection + Gameplay Integration

### User Story
As a player, I want different music tracks for different game states — upbeat for the home screen, intense for combat, mellow for the inventory.

### Acceptance Criteria
- [ ] Music track table mapping FSM states to .mod file paths
- [ ] Home screen: random track from "chill" category
- [ ] Combat: random track from "intense" category
- [ ] Onboarding/menus: random track from "upbeat" category
- [ ] Track changes on state transition (crossfade or clean stop-start)
- [ ] Music stops during idle screen (save power)
- [ ] Player can mute music via a settings toggle (preserved in save)

### Files to Create/Modify
- `components/game/include/music_table.h` (track → state mapping)
- `main/app_main.c` (state-based music triggers)
