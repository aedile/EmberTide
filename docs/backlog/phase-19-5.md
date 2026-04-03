# Phase 19.5: Idle Screen, Walk Animation & Partial E-Paper Refresh

**Origin:** Rapid iteration lab session with human operator (2026-04-03).
Features were prototyped interactively on branch `lab/idle-screen-partial-refresh`
to evaluate UI/UX feel on real hardware before committing to a TDD build.
This spec formalizes the validated design for proper implementation.

---

## Item 1: Partial E-Paper Refresh

### User Story
As a user watching animations or returning from idle, I want screen updates
to happen quickly and without a full black-white-black flicker, so the device
feels responsive rather than sluggish.

### Acceptance Criteria
- [ ] `hal_epaper.c` contains a partial-refresh waveform LUT (`k_wf_partial_1in54`, 159 bytes)
- [ ] New function `hal_epaper_flush_partial()` sends framebuffer data using the partial LUT (no full black-white cycle)
- [ ] A full refresh is forced every `EPD_FULL_REFRESH_INTERVAL` flushes (default: 10) to clear accumulated ghosting
- [ ] `hal_epaper_init()` performs a boot-time full clear (white → black → white) to establish a clean baseline regardless of prior screen state
- [ ] Flush counter (`s_flush_count`) tracks total flushes since init; resets on `hal_epaper_init()`
- [ ] Partial mode enter/exit is internal to the HAL — callers see only `flush` vs `flush_partial`

### Negative Test Requirements
- **Partial flush before init:** Must return `HAL_EPAPER_ERR_INIT`
- **Guard order for flush_partial:** Must match `flush()` — NULL → size → init. Tests must verify this order.
- **Flush counter overflow:** Use modulo (`s_flush_count % EPD_FULL_REFRESH_INTERVAL == 0`). uint32_t wrap is well-defined in C; modulo handles all values safely. `_Static_assert(EPD_FULL_REFRESH_INTERVAL > 0, ...)` at compile time to prevent division by zero.
- **Flush counter reset timing:** `s_flush_count` must be reset to 0 AFTER the boot-time full clear completes, so boot clears don't count toward the interval.
- **NULL framebuffer to flush_partial:** Must return `HAL_EPAPER_ERR_NULL`
- **Wrong-size buffer to flush_partial:** Must return `HAL_EPAPER_ERR_SIZE`
- **flush_partial after sleep:** Must return `HAL_EPAPER_ERR_INIT` — `hal_epaper_sleep()` must clear `s_initialized`.

### Files to Create/Modify
- `components/fq_hal/include/hal_epaper.h` (add `hal_epaper_flush_partial` declaration)
- `components/fq_hal/src/hal_epaper.c` (partial LUT, flush_partial impl, boot-time clear, flush counter)
- `test/host/mock_hal_epaper.c` (add `mock_epaper_flush_partial`, flush counter tracking)
- `test/host/test_p19_5_epaper_partial_bounds.c` (bound tests)
- `test/host/test_p19_5_epaper_partial_feature.c` (feature tests)

---

## Item 2: Home Screen Walk Cycle Animation

### User Story
As a player on the home screen, I want my creature sprite to alternate between
two walk frames so it feels alive and animated, not frozen in place.

### Acceptance Criteria
- [ ] `fq_vm_home_t` gains an `anim_frame` field (uint8_t, replaces one pad byte). `_Static_assert(sizeof(fq_vm_home_t) == 24u, ...)` must be added to pin struct size.
- [ ] `screen_home.c` uses `vm->anim_frame` (clamped 0–7) to select the sprite frame instead of hardcoded frame 0. Must handle NULL return from `fq_get_char_sprite()` for any frame value.
- [ ] `app_main.c` alternates `anim_frame` between frame 0 and frame 2 on a timer (0.8s per tick) while in `FQ_STATE_HOME`
- [ ] Animation uses only frames 0 (idle) and 2 (mid-step) — two-frame cycle minimizes e-paper ghosting artifacts
- [ ] Animation timer resets when leaving and re-entering HOME state
- [ ] `hal_epaper_flush_partial()` is used for animation redraws (not full refresh)

### Negative Test Requirements
- **anim_frame out of range:** Values ≥ 8 must clamp to 0 in the renderer (defensive guard)
- **Animation while not in HOME:** Timer must not advance `anim_frame` when state != `FQ_STATE_HOME`
- **Zero-duration frame interval:** `ANIM_FRAME_US` set to 0 must not cause infinite loop or division by zero

### Files to Create/Modify
- `components/presentation/include/view_models.h` (add `anim_frame` to `fq_vm_home_t`)
- `components/presentation/src/screens/screen_home.c` (use `anim_frame` for sprite selection)
- `main/app_main.c` (animation timer, frame alternation logic)
- `test/host/test_p19_5_home_anim_bounds.c` (bound tests)
- `test/host/test_p19_5_home_anim_feature.c` (feature tests)
- `test/visual/golden/scene_home.png` (updated baseline with frame 0)

---

## Item 3: Idle Screen with Timeout

### User Story
As a user who hasn't pressed a button in 30 seconds, I want the device to
show a screensaver-style idle screen with a large character sprite and the
game title, so the display doesn't burn in a static image and the device
looks alive when sitting on a desk.

### Architecture Decision: Idle is a render-layer overlay, NOT an FSM state

Idle does NOT add `FQ_STATE_IDLE` to `fq_app_state_t`. Instead, `app_main.c`
maintains a `s_idle_active` flag. When set, the main loop calls `fq_render_idle()`
instead of the current state's renderer. The underlying FSM state is preserved.
On any button press, `s_idle_active` is cleared and the normal renderer resumes.
This avoids FSM state explosion and preserves the "return to previous screen"
behavior trivially.

### Architecture Decision: Idle renderer and blit_sprite_3x in presentation/

`fq_blit_sprite_3x()` is a rendering primitive and belongs in
`components/presentation/src/sprite_util.c` (declared in `sprite_util.h`),
alongside the existing `fq_blit_sprite_2x()`.

The idle screen renderer (`fq_render_idle()`) belongs in
`components/presentation/src/screens/screen_idle.c` with a view model
`fq_vm_idle_t` defined in `view_models.h`.

### Acceptance Criteria
- [ ] After 30 seconds of no button input (`IDLE_TIMEOUT_US`), the idle screen activates via `s_idle_active` flag (NOT an FSM state transition)
- [ ] `fq_vm_idle_t` defined in `view_models.h` with fields: `sprite_base` (uint8_t), `name` (char[13]), `level` (uint8_t), `_pad` as needed. `fq_vm_build_idle()` added to `vm_builder.h`.
- [ ] `fq_render_idle()` in `screen_idle.c` renders: character sprite at 3x scale (96x96 pixels, centered), "EmberTide" in title font below, character name + level in small font at bottom
- [ ] Any button press immediately clears `s_idle_active` and the normal renderer resumes (previous FSM state preserved)
- [ ] Idle timeout counter resets on every button event
- [ ] Idle screen uses `hal_epaper_flush_partial()` for the transition (fast, no flicker)
- [ ] `fq_blit_sprite_3x()` in `sprite_util.c` renders 1-bit sprites at 3x pixel scale
- [ ] Idle must NOT activate during `FQ_STATE_BATTLE`, `FQ_STATE_BATTLE_SETUP`, or `FQ_STATE_TITLE`
- [ ] Animation timer is paused while `s_idle_active` is set

### Negative Test Requirements
- **Idle during combat/title:** Idle must NOT activate during `FQ_STATE_BATTLE`, `FQ_STATE_BATTLE_SETUP`, or `FQ_STATE_TITLE`
- **NULL sprite in 3x blit:** Must not crash — guard on NULL sprite, NULL data, or zero dimensions
- **Rapid button mashing at idle boundary:** Pressing a button at exactly the 30s mark must not show idle for one frame then immediately dismiss it (debounce the transition)
- **esp_timer overflow:** `int64_t` subtraction for `(now - last_button)` is safe — esp_timer wraps at ~292,000 years. Document as not a practical concern.
- **Idle reactivation:** After waking from idle, timeout counter resets — idle must activate again after another 30s of inactivity
- **Concurrent animation + idle boundary:** If both timers fire in the same poll cycle, idle takes priority and suppresses the animation redraw

### Files to Create/Modify
- `components/presentation/include/view_models.h` (add `fq_vm_idle_t`)
- `components/presentation/include/screens/screen_idle.h` (new)
- `components/presentation/src/screens/screen_idle.c` (new — `fq_render_idle()`)
- `components/presentation/include/sprite_util.h` (add `fq_blit_sprite_3x` declaration)
- `components/presentation/src/sprite_util.c` (add `fq_blit_sprite_3x` implementation)
- `components/presentation/include/vm_builder.h` (add `fq_vm_build_idle` declaration)
- `main/app_main.c` (idle flag, timeout logic, call `fq_render_idle` via view model)
- `test/host/test_p19_5_idle_bounds.c` (bound tests)
- `test/host/test_p19_5_idle_feature.c` (feature tests)

---

## Design Notes (from rapid iteration lab)

These decisions were validated interactively with the human operator on real
hardware before being formalized:

1. **Two-frame animation (0, 2) not four-frame:** E-paper partial refresh
   causes visible ghosting. Fewer unique frames = less accumulated ghost
   artifacts. Frames 0 and 2 give a natural idle-step-idle feel.

2. **3x sprite scale on idle:** At 200x200 resolution, 32x32 sprites are
   too small to be a visual centerpiece. 3x (96x96) fills the screen nicely.

3. **Partial refresh with periodic full clear:** Pure partial refresh
   accumulates ghost pixels. Every 10th flush does a full cycle to reset.
   This was tuned by watching the display during rapid iteration.

4. **0.8s animation interval:** Faster looked frantic on e-paper (partial
   refresh still has ~300ms latency). Slower felt dead. 0.8s was the sweet
   spot confirmed by human operator.

5. **Boot-time full clear:** Eliminates whatever was on screen from prior
   firmware or factory test image. Clean first impression.
