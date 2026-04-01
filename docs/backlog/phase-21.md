# Phase 21: Audio Engine

## Item 1: LEDC PWM Piezo Driver

### User Story
As a player, I want audible feedback when I press buttons, land hits in combat, and navigate menus — tiny 8-bit style beeps and boops from the onboard piezo speaker.

### Acceptance Criteria
- [ ] `hal_audio.c` fully wired to ESP-IDF LEDC peripheral (not stubs)
- [ ] Configures LEDC timer + channel for GPIO46 (speaker PA enable) and the audio output pin
- [ ] `hal_audio_play(freq_hz, duration_ms)` generates a hardware square wave at the given frequency
- [ ] Non-blocking: starts PWM, sets a one-shot `esp_timer` to stop after duration_ms
- [ ] Overlapping calls cancel the previous tone and start the new one (no clashing timers)
- [ ] Power pin GPIO42 driven correctly for audio amplifier enable

### Negative Test Requirements
- **Frequency 0:** Must return `HAL_AUDIO_ERR_INVALID_FREQ`, not configure a DC output
- **Duration 0:** Silent — effectively a stop command
- **Rapid sequential calls (< 5ms apart):** Timer safely cancelled and restarted, no stack overflow

### Files to Modify
- `components/fq_hal/src/hal_audio.c` (replace stub with LEDC + esp_timer)
- `components/fq_hal/CMakeLists.txt` (if additional REQUIRES needed)

---

## Item 2: Sound Effect Table

### User Story
As a game designer, I need a table of sound effects (frequency + duration pairs) for game events so the audio engine plays the right sound at the right time.

### Acceptance Criteria
- [ ] `components/game/include/sfx.h` defines `fq_sfx_id_t` enum: SFX_BTN_PRESS, SFX_BTN_BACK, SFX_MENU_NAVIGATE, SFX_COMBAT_HIT, SFX_COMBAT_MISS, SFX_COMBAT_CRIT, SFX_LEVEL_UP, SFX_ITEM_EQUIP, SFX_DEATH, SFX_REBIRTH
- [ ] `fq_sfx_play(fq_sfx_id_t id)` looks up the sound and calls `hal_audio_play(freq, duration)`
- [ ] Sound definitions are tunable `static const` arrays (not magic numbers scattered through app code)
- [ ] Each sound is a short sequence (1-3 tones with gaps) — not just a single beep

### Files to Create
- `components/game/include/sfx.h`
- `components/game/src/sfx.c`

---

## Item 3: Wire Sound Effects into Gameplay

### User Story
As a player, I want to hear a beep when I press a button, a different tone when I hit an enemy, and a distinct sound when I level up.

### Acceptance Criteria
- [ ] `app_main.c` calls `fq_sfx_play(SFX_BTN_PRESS)` on every button event
- [ ] Combat round rendering triggers `SFX_COMBAT_HIT` / `SFX_COMBAT_MISS` / `SFX_COMBAT_CRIT` based on round result
- [ ] Level-up in training triggers `SFX_LEVEL_UP`
- [ ] Rebirth triggers `SFX_REBIRTH`
- [ ] All sounds are non-blocking (gameplay continues during audio playback)

### Files to Modify
- `main/app_main.c` (wire sfx calls into event handlers and render path)
