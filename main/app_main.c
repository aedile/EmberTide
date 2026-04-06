/**
 * app_main.c — FiestaQuest Application Entry Point
 *
 * Bootstrap, FreeRTOS task creation, event loop, and view model builder.
 *
 * This is the ONLY module that has visibility into all layers:
 *   - Reads game state from game/ components.
 *   - Constructs view models and dispatches to presentation/.
 *   - Wires HAL inputs (buttons) to the event bus.
 *   - Calls HAL services for hardware initialization.
 *
 * Architecture constraint: game/ and presentation/ are NEVER coupled to
 * each other. The application layer is the sole orchestrator.
 *
 * Phase 16: Real FreeRTOS event loop with SPI e-paper flush, button ISR
 * wiring via hal_gpio, and LittleFS save/load via hal_flash.
 *
 * Phase 19.5: Idle screensaver, walk animation, partial e-paper refresh.
 *
 * Phase 19 Interactive:
 *   First-boot routing: if no valid save exists, state -> ONBOARDING.
 *   Remove hardcoded "Ember" character creation.
 *   ONBOARDING added to idle suppression list.
 *   Training session: TIMER_TICK posted to event bus every loop.
 *
 *   Idle design:
 *     - Idle is NOT an FSM state. s_idle_active flag overlays the renderer.
 *     - After IDLE_TIMEOUT_US (30s) of no button input, s_idle_active = 1.
 *     - Any button press clears s_idle_active; the previous FSM state resumes.
 *     - Idle is suppressed during FQ_STATE_BATTLE, FQ_STATE_BATTLE_SETUP,
 *       FQ_STATE_TITLE, and FQ_STATE_ONBOARDING.
 *     - s_last_button_us is initialized to esp_timer_get_time() at startup
 *       so idle does not fire before the first user interaction.
 *
 *   Animation design:
 *     - s_anim_frame alternates between 0 and 2 every ANIM_FRAME_US (5s).
 *     - Frame 0 = idle pose; Frame 2 = mid-step.
 *     - Animation timer resets when leaving HOME state.
 *     - Animation is suppressed while s_idle_active is set.
 *
 *   Partial refresh:
 *     - Animation redraws use hal_epaper_flush_partial() (fast, no flicker).
 *     - Onboarding class-carousel BTN_A redraws use hal_epaper_flush_partial().
 *     - HAL internally forces a full refresh every EPD_FULL_REFRESH_INTERVAL.
 *     - State-change redraws continue to use hal_epaper_flush() (full).
 *
 *   Onboarding UX fixes (hardware test):
 *     - Bug 1: BTN_A in ONBOARDING only sets needs_redraw on button events,
 *       not TIMER_TICK. TIMER_TICK fired every 50ms and caused continuous
 *       e-paper refreshes (~1.8s each) making the screen unusable.
 *     - Bug 2: Save-fail no longer forces state back to ONBOARDING. Character
 *       is valid in memory; the user stays in HOME and can play immediately.
 *     - Partial refresh: class carousel uses render_onboarding_partial().
 *     - LittleFS note: format_if_mount_failed=true in hal_flash_init() handles
 *       first-boot partition formatting automatically (no manual intervention).
 *
 * Phase 21: I2S audio init and SFX wiring.
 *   - hal_audio_init() called during HAL init.
 *   - do_sfx(id) helper: generates PCM via fq_sfx_play(), pushes to HAL.
 *   - button_callback: ONLY posts events — do_sfx() NOT called from ISR context.
 *   - SFX quiet-mode: app.sfx_enabled flag checked before each do_sfx() call.
 *
 * Phase 22: MOD music playback + audio mixing.
 *   - do_music_tick() called every main loop iteration (50ms = 1102 samples).
 *   - Music renders via fq_music_render() → mix with SFX → hal_audio_write_samples().
 *   - Mixing formula: music_scaled = (music_raw * music_vol) / 256 (int32 intermediate).
 *   - SFX ducking: when any SFX sample in the chunk is non-zero, music volume is
 *     temporarily scaled to 60% (music_vol * 153 / 256). No persistent state.
 *   - clamp16() applied to every mixed sample to prevent int16 overflow/wrap.
 *   - State transitions trigger music track changes (stop-start, no crossfade).
 *   - Idle activation stops music; idle wake resumes the current-state track.
 *   - Music mute toggle: app.music_enabled field. Checked in do_music_tick().
 *   - Track selection uses tick_count entropy — NEVER touches combat PRNG.
 *   - .mod files loaded from LittleFS via hal_flash_read_file() into s_mod_buf.
 *
 *   SPSC contract: do_music_tick() and do_sfx() are ONLY called from the
 *   main loop task. The HAL ring buffer has no mutex — sole producer model.
 *
 * Phase 23: UX Hardware Fixes + Audio Foundation.
 *   - Audio init moved BEFORE e-paper init.
 *     I2S GDMA channel must be allocated before SPI GDMA channel to avoid
 *     LoadStoreAlignment crash in gdma_install_tx_interrupt on ESP32-S3.
 *   - Real micromod (Martin Cameron, public domain) replaces Phase-22 stub.
 *     micromod_initialise() called at boot with embedded test_music.mod.
 *     micromod_get_audio() renders stereo PCM; downmixed to mono in do_music_tick().
 *   - Downmix: mono = (L + R) / 2, scaled by vol/256. Integer only, no float.
 *   - s_music_loaded_state guards against retry spam when state does not change.
 *   - ANIM_FRAME_US updated to 5s (partial refresh takes ~1.8s on real hardware).
 *   - Idle wake: ONLY on BTN_A_PRESS or BTN_B_PRESS (not TIMER_TICK).
 *   - Onboarding redraw: ONLY on button events (not TIMER_TICK — prevents 50ms
 *     continuous full-refresh loop that made the screen unusable in the field).
 *   - do_sfx_tracked uses SFX_BUF_SIZE (12000) not AUDIO_RING_BUF_SAMPLES (44100)
 *     to avoid a 88KB static buffer on the stack section.
 */

#include <stdint.h>
#include <string.h>

/* Game layer */
#include "types.h"
#include "character.h"
#include "save_format.h"
#include "sfx.h"
#include "sfxr.h"  /* for SFXR_SAMPLE_RATE_HZ compile-time guard */
#include "music.h"
#include "music_table.h"
#include "micromod.h"  /* for MAX_MOD_FILE_SIZE */

/* Application layer */
#include "event_bus.h"
#include "app_fsm.h"
#include "vm_builder.h"

/* Presentation layer */
#include "fq_framebuffer.h"
#include "fq_text.h"
#include "view_models.h"
#include "screens/screen_home.h"
#include "screens/screen_stats.h"
#include "screens/screen_inventory.h"
#include "screens/screen_training.h"
#include "screens/screen_onboarding.h"
#include "screens/screen_idle.h"
#include "screens/screen_battle_result.h"
#include "screens/screen_rebirth.h"
#include "asset_data.h"
#include "sprite_util.h"

/* HAL layer — only main/ may include hal_*.h */
#include "hal_epaper.h"
#include "hal_flash.h"
#include "hal_gpio.h"
#include "hal_audio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"  /* heap_caps_malloc for SPIRAM */

static const char *TAG = "app_main";

/* -------------------------------------------------------------------------
 * Compile-time sample-rate consistency guard.
 *
 * sfxr.h defines SFXR_SAMPLE_RATE_HZ; hal_audio.h defines AUDIO_SAMPLE_RATE_HZ.
 * These must be identical — sfxr generates PCM that is fed directly into the
 * HAL ring buffer. A mismatch would cause pitch distortion at runtime.
 * -------------------------------------------------------------------------
 */
_Static_assert(SFXR_SAMPLE_RATE_HZ == AUDIO_SAMPLE_RATE_HZ,
               "SFXR_SAMPLE_RATE_HZ must equal AUDIO_SAMPLE_RATE_HZ — "
               "sfxr PCM is fed directly into the HAL audio ring buffer");

/* -------------------------------------------------------------------------
 * Idle / animation timing constants.
 * -------------------------------------------------------------------------
 */

/** Idle timeout: 30 seconds of no button input activates the screensaver. */
#define IDLE_TIMEOUT_US    (30ULL * 1000000ULL)

/**
 * Animation frame interval: 5 seconds per walk frame.
 * Phase 23: increased from 800ms to 5s because partial refresh takes ~1.8s
 * on real hardware — the old 800ms value caused back-to-back flush storms.
 */
#define ANIM_FRAME_US      (5000000ULL)

/* BLOCKER 2 fix: Compile-time guard -- ANIM_FRAME_US must be non-zero. */
_Static_assert(ANIM_FRAME_US > 0ULL,
               "ANIM_FRAME_US must be > 0 -- zero causes runaway animation refreshes");

/* -------------------------------------------------------------------------
 * Phase-22: Music constants.
 * -------------------------------------------------------------------------
 */

/**
 * MUSIC_TICK_SAMPLES — Samples rendered per 50ms main loop tick.
 * 50ms at 22050 Hz = 22050 * 50 / 1000 = 1102 samples.
 */
#define MUSIC_TICK_SAMPLES  1102u

/**
 * SFX_BUF_SIZE — Dedicated static buffer for SFX generation in do_sfx_tracked().
 *
 * Phase 23: Using a fixed 12000-sample buffer instead of AUDIO_RING_BUF_SAMPLES
 * (44100). SFX is bounded by SFXR_MAX_DURATION_MS (500ms) at 22050Hz, yielding
 * at most ~11025 samples. 12000 provides a safe ceiling without allocating the
 * full 88KB of the enlarged ring buffer as a static array in the .bss section.
 */
#define SFX_BUF_SIZE       12000u

/* -------------------------------------------------------------------------
 * Phase-22: MOD buffer — allocated on SPIRAM on target.
 *
 * On the host test build this is a stack-region static. On the target,
 * this is the sole SPIRAM consumer for .mod data. At most one .mod file
 * is resident at a time (stop-start model, no crossfade).
 * -------------------------------------------------------------------------
 */
static uint8_t *s_mod_buf;  /* SPIRAM-allocated in do_music_load() */
static size_t  s_mod_buf_len;
static fq_music_ctx_t s_music_ctx; /* Application-layer owner of music player state. */

/**
 * s_music_loaded_state — Tracks which state we last attempted music load for.
 *
 * Phase 23: Prevents retry spam when .mod files are absent or when idle wake
 * resumes the same state. Initialised to 0xFF (sentinel "no state loaded").
 */
static fq_app_state_t s_music_loaded_state = (fq_app_state_t)0xFFu;

/* -------------------------------------------------------------------------
 * clamp16 — Clamp int32 to signed 16-bit range.
 *
 * Used for all mixing arithmetic. Prevents int16 overflow/wrap which
 * would produce harsh distortion clicks at sample boundaries.
 * -------------------------------------------------------------------------
 */
static inline int16_t clamp16(int32_t v)
{
    if (v >  32767)  { return  32767; }
    if (v < -32768)  { return -32768; }
    return (int16_t)v;
}

/* -------------------------------------------------------------------------
 * File-scope application context pointer.
 * -------------------------------------------------------------------------
 */
static fq_app_ctx_t *s_app = NULL;

/* -------------------------------------------------------------------------
 * Idle / animation state -- file-scope, reset at startup.
 * -------------------------------------------------------------------------
 */
static int64_t  s_last_button_us;
static uint8_t  s_idle_active;
static uint8_t  s_anim_frame;
static int64_t  s_last_anim_us;

/* -------------------------------------------------------------------------
 * is_idle_forbidden -- Returns 1 if idle must NOT activate in this state.
 * -------------------------------------------------------------------------
 */
static uint8_t is_idle_forbidden(fq_app_state_t state)
{
    return (state == FQ_STATE_BATTLE        ||
            state == FQ_STATE_BATTLE_SETUP  ||
            state == FQ_STATE_BATTLE_RESULT ||
            state == FQ_STATE_REBIRTH       ||
            state == FQ_STATE_TITLE         ||
            state == FQ_STATE_ONBOARDING)
           ? 1u : 0u;
}

/* -------------------------------------------------------------------------
 * music_cat_for_state — Map an FSM state to a music category.
 *
 * Returns FQ_MUSIC_CAT_COUNT if no music should play in this state.
 * -------------------------------------------------------------------------
 */
static fq_music_cat_t music_cat_for_state(fq_app_state_t state)
{
    switch (state) {
        case FQ_STATE_HOME:
        case FQ_STATE_STATS:
        case FQ_STATE_INVENTORY:
            return FQ_MUSIC_CAT_CHILL;

        case FQ_STATE_BATTLE:
        case FQ_STATE_BATTLE_SETUP:
            return FQ_MUSIC_CAT_INTENSE;

        case FQ_STATE_TITLE:
        case FQ_STATE_TRAINING:
        case FQ_STATE_ONBOARDING:
        case FQ_STATE_BATTLE_RESULT:
        case FQ_STATE_REBIRTH:
            return FQ_MUSIC_CAT_UPBEAT;

        default:
            return FQ_MUSIC_CAT_COUNT; /* No music. */
    }
}

/* -------------------------------------------------------------------------
 * do_music_load — Load and start a music track for the given FSM state.
 *
 * Picks a track using tick_count entropy (NOT combat PRNG — Priority 0).
 * Loads it from LittleFS via hal_flash_read_file() into s_mod_buf.
 * On any failure (file not found, I/O error): silent fallback, no crash.
 * -------------------------------------------------------------------------
 */
static void do_music_load(fq_app_state_t state, uint32_t tick_count)
{
    if (!s_app || !s_app->music_enabled) {
        return;
    }

    /* Don't retry music load for the same state — prevents spam when
     * .mod files are absent from the flash partition. */
    if (state == s_music_loaded_state) {
        return;
    }
    s_music_loaded_state = state;

    fq_music_cat_t cat = music_cat_for_state(state);
    if ((unsigned)cat >= (unsigned)FQ_MUSIC_CAT_COUNT) {
        /* No music for this state. */
        fq_music_stop(&s_music_ctx);
        return;
    }

    const fq_music_track_t *track = fq_music_table_pick(cat, tick_count);
    if (!track || !track->path) {
        /* Empty category — no music, no crash. */
        return;
    }

    /* Allocate SPIRAM buffer for the .mod file. */
    if (s_mod_buf) {
        heap_caps_free(s_mod_buf);
        s_mod_buf = NULL;
    }
    s_mod_buf = heap_caps_malloc(MAX_MOD_FILE_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_mod_buf) {
        ESP_LOGW(TAG, "music: SPIRAM alloc failed -- no music");
        return;
    }

    /* Load from flash. */
    s_mod_buf_len = 0u;
    hal_flash_err_t ferr = hal_flash_read_file(track->path, s_mod_buf,
                                                MAX_MOD_FILE_SIZE,
                                                &s_mod_buf_len);
    if (ferr != HAL_FLASH_OK || s_mod_buf_len == 0u) {
        ESP_LOGW(TAG, "music: file not found or load failed: %s (%d)",
                 track->path, (int)ferr);
        heap_caps_free(s_mod_buf);
        s_mod_buf = NULL;
        return; /* Graceful fallback: no music, no crash. */
    }

    /* Init and play. */
    fq_music_err_t merr = fq_music_init(&s_music_ctx, s_mod_buf, s_mod_buf_len);
    if (merr != FQ_MUSIC_OK) {
        ESP_LOGW(TAG, "music: fq_music_init failed: %d", (int)merr);
        return;
    }

    merr = fq_music_play(&s_music_ctx);
    if (merr != FQ_MUSIC_OK) {
        ESP_LOGW(TAG, "music: fq_music_play failed: %d", (int)merr);
    }
}

/* -------------------------------------------------------------------------
 * do_music_tick — Render and mix one tick of music + pending SFX.
 *
 * Phase-22 core function. Called every main loop iteration (50ms).
 * Phase-23: Rewired to use real micromod (Martin Cameron, public domain).
 *
 * Steps:
 *   1. Get volume from app context (0 if music disabled).
 *   2. Apply ducking if SFX was emitted this tick (~60% of vol).
 *   3. Render MUSIC_TICK_SAMPLES stereo pairs via micromod_get_audio().
 *   4. Downmix stereo to mono: mono = (L + R) / 2. Integer only, no float.
 *   5. Scale by vol/256. Integer only.
 *   6. Write combined mono output to HAL ring buffer.
 *
 * CONCURRENCY: ONLY called from the main loop task (SPSC producer).
 * -------------------------------------------------------------------------
 */

/* Flag: set to 1 when do_sfx() emits samples this tick; cleared each tick. */
static uint8_t s_sfx_emitted_this_tick;

/* Override of do_sfx to set the duck flag. */
static void do_sfx_tracked(fq_sfx_id_t id)
{
    if (!s_app || !s_app->sfx_enabled) {
        return;
    }
    /*
     * Phase 23: Use SFX_BUF_SIZE (12000) not AUDIO_RING_BUF_SAMPLES (44100).
     * SFX output is bounded by SFXR_MAX_DURATION_MS at 22050 Hz (~11025 max).
     * The 44100-sample ring buffer size should not leak into SFX buffer sizing.
     */
    static int16_t s_sfx_buf[SFX_BUF_SIZE];
    size_t samples_out = 0u;
    if (fq_sfx_play(id, s_sfx_buf, SFX_BUF_SIZE, &samples_out)
        == GAME_OK && samples_out > 0u) {
        hal_audio_write_samples(s_sfx_buf, samples_out);
        s_sfx_emitted_this_tick = 1u;
    }
}

static void do_music_tick(void)
{
    if (!s_app) {
        return;
    }

    static int16_t s_music_buf[MUSIC_TICK_SAMPLES];

    /* Get volume — 0 if music disabled. */
    uint8_t vol = s_app->music_enabled ? s_app->music_vol : 0u;

    /* Apply ducking if SFX was emitted this tick. */
    if (s_sfx_emitted_this_tick && vol > 0u) {
        /* Ducked vol = vol * 153 / 256 (~60%). */
        vol = (uint8_t)(((uint32_t)vol * 153u) / 256u);
    }

    /*
     * Phase 23: Use REAL micromod (Martin Cameron, public domain).
     * micromod_get_audio() returns stereo interleaved int16 (LRLRLR...).
     * We downmix to mono: mono = (L + R) / 2, then scale by vol/256.
     * Both operations use int32 intermediates — no overflow, no float.
     */
    extern void micromod_get_audio(short *output_buffer, long count);

    static int16_t s_stereo_buf[MUSIC_TICK_SAMPLES * 2];
    memset(s_stereo_buf, 0, sizeof(s_stereo_buf)); /* micromod requires zeroed buffer */
    micromod_get_audio(s_stereo_buf, (long)MUSIC_TICK_SAMPLES);

    /* Downmix stereo to mono and apply volume. */
    for (size_t i = 0u; i < MUSIC_TICK_SAMPLES; i++) {
        int32_t l    = (int32_t)s_stereo_buf[i * 2u];
        int32_t r    = (int32_t)s_stereo_buf[i * 2u + 1u];
        int32_t mono = (l + r) / 2;
        mono         = (mono * (int32_t)vol) / 256;
        s_music_buf[i] = clamp16(mono);
    }

    hal_audio_write_samples(s_music_buf, MUSIC_TICK_SAMPLES);
}

/* -------------------------------------------------------------------------
 * do_auto_save -- Serialize and write save to flash.
 * -------------------------------------------------------------------------
 */
static void do_auto_save(fq_app_ctx_t   *app,
                         fq_character_t *player,
                         fq_inventory_t *inventory)
{
    uint8_t save_buf[FQ_SAVE_MAX_SIZE];
    size_t  bytes = fq_save_serialize(player, inventory, save_buf, sizeof(save_buf));
    if (bytes == 0u) {
        ESP_LOGE(TAG, "save_serialize failed");
        if (app) { app->onboarding_save_failed = 1u; }
        return;
    }
    hal_flash_err_t err = hal_flash_write_save(save_buf, bytes);
    if (err != HAL_FLASH_OK) {
        ESP_LOGE(TAG, "hal_flash_write_save failed: %d", (int)err);
        if (app) { app->onboarding_save_failed = 1u; }
    } else {
        ESP_LOGI(TAG, "Auto-save OK (%u bytes)", (unsigned)bytes);
        if (app) { app->onboarding_save_failed = 0u; }
    }
}

/* -------------------------------------------------------------------------
 * button_callback -- Called from gpio_task context on each debounced press.
 *
 * ONLY posts events to the event bus. do_sfx() is deliberately NOT called
 * here — it runs in gpio_task (separate FreeRTOS task).
 * -------------------------------------------------------------------------
 */
static void button_callback(hal_btn_id_t btn_id)
{
    if (!s_app) {
        return;
    }
    s_last_button_us = esp_timer_get_time();

    fq_event_id_t evt_id = (btn_id == HAL_BTN_A)
                           ? FQ_EVT_BTN_A_PRESS
                           : FQ_EVT_BTN_B_PRESS;
    fq_event_bus_post(&s_app->bus, evt_id, 0u);
}

/* -------------------------------------------------------------------------
 * render_title_screen -- Draw the EmberTide title screen.
 * -------------------------------------------------------------------------
 */
static void render_title_screen(fq_fb_t *fb)
{
    const fq_font_t   *font_title = fq_get_font_title();
    const fq_font_t   *font_small = fq_get_font_small();
    const fq_sprite_t *spr        = fq_get_char_sprite(0u, 0u);

    fq_fb_fill_rect(fb,   0,   0, 200,   4, 1u);
    fq_fb_fill_rect(fb,   0, 196, 200,   4, 1u);
    fq_fb_fill_rect(fb,   0,   4,   4, 192, 1u);
    fq_fb_fill_rect(fb, 196,   4,   4, 192, 1u);

    fq_fb_draw_rect(fb, 8, 8, 184, 184, 1u);

    {
        static const char title_str[] = "EmberTide";
        int16_t w = fq_text_width(font_title, title_str);
        int16_t x = (int16_t)((200 - w) / 2);
        fq_draw_text(fb, font_title, x, 20, title_str);
    }

    fq_fb_draw_line(fb, 12, 52, 187, 52, 1u);

    if (spr != NULL) {
        int16_t sprite_x = (int16_t)((200 - 64) / 2);
        fq_blit_sprite_2x(fb, sprite_x, 60, spr);
    }

    fq_fb_draw_line(fb, 12, 130, 187, 130, 1u);

    {
        static const char prompt_str[] = "Press [PWR]";
        int16_t w = fq_text_width(font_small, prompt_str);
        int16_t x = (int16_t)((200 - w) / 2);
        fq_draw_text(fb, font_small, x, 145, prompt_str);
    }
}

/* -------------------------------------------------------------------------
 * render_current_state -- Render the current FSM state to the framebuffer.
 * -------------------------------------------------------------------------
 */
static void render_current_state(fq_app_ctx_t   *app,
                                  fq_fb_t        *fb,
                                  fq_character_t *player,
                                  fq_inventory_t *inventory)
{
    hal_epaper_err_t err;

    fq_fb_clear(fb);

    switch (app->state) {
        case FQ_STATE_TITLE: {
            render_title_screen(fb);
            break;
        }
        case FQ_STATE_HOME: {
            fq_vm_home_t vm_home;
            fq_vm_build_home(&vm_home, player);
            vm_home.menu_index = app->home_menu_index;
            vm_home.anim_frame = s_anim_frame;
            fq_render_home(fb, &vm_home);
            break;
        }
        case FQ_STATE_STATS: {
            fq_vm_stats_t vm_stats;
            fq_vm_build_stats(&vm_stats, player);
            fq_render_stats(fb, &vm_stats);
            break;
        }
        case FQ_STATE_INVENTORY: {
            fq_vm_inventory_t vm_inv;
            fq_vm_build_inventory_ex(&vm_inv, inventory, player,
                                      app->inventory_cursor, 0u);
            fq_render_inventory(fb, &vm_inv);
            break;
        }
        case FQ_STATE_TRAINING: {
            fq_vm_training_t vm_train;
            fq_vm_build_training_session(&vm_train, &app->training);
            fq_render_training(fb, &vm_train);
            break;
        }
        case FQ_STATE_ONBOARDING: {
            fq_vm_onboarding_t vm_ob;
            fq_vm_build_onboarding(&vm_ob, player, app->onboarding_class_index);
            fq_render_onboarding(fb, &vm_ob);
            break;
        }
        case FQ_STATE_BATTLE_RESULT: {
            fq_vm_battle_result_t vm_result;
            fq_vm_build_battle_result(&vm_result, player, &app->opponent,
                                       app->battle_won,
                                       app->xp_earned,
                                       app->rounds_survived,
                                       player->is_dead);
            fq_render_battle_result(fb, &vm_result);
            break;
        }
        case FQ_STATE_REBIRTH: {
            uint8_t old_stats[4] = {
                player->strength,
                player->speed,
                player->precision,
                player->intelligence
            };
            fq_vm_rebirth_t vm_rebirth;
            fq_vm_build_rebirth(&vm_rebirth, player,
                                 player->level,
                                 old_stats,
                                 (uint8_t)(app->xp_earned > 0u ? 1u : 0u));
            fq_render_rebirth(fb, &vm_rebirth);
            break;
        }
        default:
            break;
    }

    {
        uint32_t black = 0;
        for (uint32_t i = 0; i < FQ_FB_SIZE; i++) {
            uint8_t b = fb->pixels[i];
            while (b) { black += b & 1u; b >>= 1u; }
        }
        ESP_LOGI(TAG, "render: state=%d, %lu black pixels", (int)app->state, (unsigned long)black);
    }

    err = hal_epaper_flush(fb->pixels, FQ_FB_SIZE);
    if (err != HAL_EPAPER_OK) {
        ESP_LOGE(TAG, "hal_epaper_flush failed: %d", (int)err);
    }
}

/* -------------------------------------------------------------------------
 * render_home_partial -- Re-render the home screen with partial refresh.
 * -------------------------------------------------------------------------
 */
static void render_home_partial(fq_app_ctx_t   *app,
                                 fq_fb_t        *fb,
                                 fq_character_t *player)
{
    hal_epaper_err_t err;

    fq_fb_clear(fb);

    fq_vm_home_t vm_home;
    fq_vm_build_home(&vm_home, player);
    vm_home.menu_index = app->home_menu_index;
    vm_home.anim_frame = s_anim_frame;
    fq_render_home(fb, &vm_home);

    err = hal_epaper_flush_partial(fb->pixels, FQ_FB_SIZE);
    if (err != HAL_EPAPER_OK) {
        ESP_LOGE(TAG, "hal_epaper_flush_partial failed: %d -- falling back to full", (int)err);
        hal_epaper_flush(fb->pixels, FQ_FB_SIZE);
    }
}

/* -------------------------------------------------------------------------
 * render_onboarding_partial -- Re-render the onboarding screen with partial
 * refresh. Used when BTN_A cycles the class selection carousel so the display
 * updates immediately without the full-refresh flicker.
 * -------------------------------------------------------------------------
 */
static void render_onboarding_partial(fq_app_ctx_t   *app,
                                       fq_fb_t        *fb,
                                       fq_character_t *player)
{
    hal_epaper_err_t err;

    fq_fb_clear(fb);

    fq_vm_onboarding_t vm_ob;
    fq_vm_build_onboarding(&vm_ob, player, app->onboarding_class_index);
    fq_render_onboarding(fb, &vm_ob);

    err = hal_epaper_flush_partial(fb->pixels, FQ_FB_SIZE);
    if (err != HAL_EPAPER_OK) {
        ESP_LOGE(TAG, "render_onboarding_partial flush failed: %d -- falling back to full",
                 (int)err);
        hal_epaper_flush(fb->pixels, FQ_FB_SIZE);
    }
}

/* -------------------------------------------------------------------------
 * render_idle -- Render the idle screensaver with partial refresh.
 * -------------------------------------------------------------------------
 */
static void render_idle(fq_app_ctx_t   *app,
                        fq_fb_t        *fb,
                        fq_character_t *player)
{
    (void)app;

    hal_epaper_err_t err;

    fq_vm_idle_t vm_idle;
    fq_vm_build_idle(&vm_idle, player);
    fq_render_idle(fb, &vm_idle);

    err = hal_epaper_flush_partial(fb->pixels, FQ_FB_SIZE);
    if (err != HAL_EPAPER_OK) {
        ESP_LOGE(TAG, "idle flush_partial failed: %d -- falling back to full", (int)err);
        hal_epaper_flush(fb->pixels, FQ_FB_SIZE);
    }
}

/* -------------------------------------------------------------------------
 * app_main -- ESP-IDF application entry point.
 * -------------------------------------------------------------------------
 */
void app_main(void)
{
    static fq_character_t player;
    static fq_inventory_t inventory;
    static fq_app_ctx_t   app;
    static fq_fb_t        framebuffer;

    memset(&player,      0, sizeof(player));
    memset(&inventory,   0, sizeof(inventory));
    memset(&framebuffer, 0, sizeof(framebuffer));

    /* -----------------------------------------------------------------------
     * Flash init + save load.
     * ----------------------------------------------------------------------- */
    uint8_t  save_valid = 0u;
    hal_flash_err_t flash_err = hal_flash_init();
    if (flash_err != HAL_FLASH_OK) {
        ESP_LOGE(TAG, "hal_flash_init failed: %d -- first-boot fallback", (int)flash_err);
    } else {
        uint8_t save_buf[FQ_SAVE_MAX_SIZE];
        size_t  bytes_read = 0u;
        flash_err = hal_flash_read_save(save_buf, sizeof(save_buf), &bytes_read);
        if (flash_err == HAL_FLASH_OK && bytes_read >= FQ_SAVE_SERIALIZED_SIZE_V1) {
            fq_save_err_t save_err = fq_save_deserialize(save_buf, bytes_read,
                                                          &player, &inventory);
            if (save_err == FQ_SAVE_OK) {
                ESP_LOGI(TAG, "Save loaded OK (%u bytes)", (unsigned)bytes_read);
                save_valid = 1u;
            } else {
                ESP_LOGW(TAG, "Save corrupt (%d) -- first-boot fallback", (int)save_err);
                memset(&player,    0, sizeof(player));
                memset(&inventory, 0, sizeof(inventory));
            }
        }
    }

    /* -----------------------------------------------------------------------
     * App context init.
     * ----------------------------------------------------------------------- */
    fq_app_init(&app, &player, &inventory);
    s_app = &app;

    /* -----------------------------------------------------------------------
     * Phase-19 Interactive: first-boot routing.
     * ----------------------------------------------------------------------- */
    if (!save_valid) {
        app.state = FQ_STATE_ONBOARDING;
        app.onboarding_class_index = 0u;
        app.onboarding_save_failed = 0u;
        ESP_LOGI(TAG, "First boot -- entering ONBOARDING");
    }

    /* -----------------------------------------------------------------------
     * HAL init.
     *
     * Phase 23: Audio FIRST — I2S GDMA channel must be allocated before SPI
     * (e-paper) GDMA channel. Reversing this order caused LoadStoreAlignment
     * in gdma_install_tx_interrupt on ESP32-S3 when both channels are active.
     * Verified on lab hardware (ESP32-S3-PICO-1-N8R8) 2026-04-05.
     * ----------------------------------------------------------------------- */
    hal_audio_err_t audio_err = hal_audio_init();
    if (audio_err != HAL_AUDIO_OK) {
        ESP_LOGW(TAG, "hal_audio_init failed: %d -- SFX disabled", (int)audio_err);
        app.sfx_enabled    = 0u;
        app.music_enabled  = 0u;
    }

    /* -----------------------------------------------------------------------
     * Phase 23: Real micromod — play embedded .mod file.
     *
     * test_music.mod is embedded via EMBED_FILES in main/CMakeLists.txt.
     * The linker exports:
     *   _binary_test_music_mod_start  — pointer to first byte of the file
     *   _binary_test_music_mod_end    — pointer to one-past-end byte
     *
     * micromod_initialise() accepts signed char* and sampling rate.
     * Returns 0 on success; non-zero on bad header.
     * micromod_set_gain(64) = unity gain for 4-channel MOD.
     * ----------------------------------------------------------------------- */
    if (audio_err == HAL_AUDIO_OK) {
        extern const uint8_t mod_start[] asm("_binary_test_music_mod_start");
        extern const uint8_t mod_end[]   asm("_binary_test_music_mod_end");
        size_t mod_len = (size_t)(mod_end - mod_start);
        ESP_LOGI(TAG, "Phase 23: loading embedded .mod (%u bytes)", (unsigned)mod_len);

        extern long micromod_initialise(signed char *data, long sampling_rate);
        extern void micromod_set_gain(long value);

        long ret = micromod_initialise((signed char *)mod_start, (long)AUDIO_SAMPLE_RATE_HZ);
        if (ret == 0) {
            micromod_set_gain(64);  /* 64 = unity for 4-channel MOD */
            ESP_LOGI(TAG, "Phase 23: real micromod init OK — music playing");
        } else {
            ESP_LOGW(TAG, "Phase 23: real micromod init failed: %ld", ret);
        }
    }

    hal_epaper_err_t epaper_err = hal_epaper_init();
    if (epaper_err != HAL_EPAPER_OK) {
        ESP_LOGE(TAG, "hal_epaper_init failed: %d", (int)epaper_err);
    }

    hal_gpio_err_t gpio_err = hal_gpio_init(button_callback);
    if (gpio_err != HAL_GPIO_OK) {
        ESP_LOGE(TAG, "hal_gpio_init failed: %d", (int)gpio_err);
    }

    /* -----------------------------------------------------------------------
     * Phase-19.5: Initialise idle/anim state.
     * ----------------------------------------------------------------------- */
    s_last_button_us = esp_timer_get_time();
    s_last_anim_us   = s_last_button_us;
    s_idle_active    = 0u;
    s_anim_frame     = 0u;

    /* -----------------------------------------------------------------------
     * Phase-22: Load initial music track for the starting state.
     * Phase-23: Disabled — using embedded .mod loaded above via real micromod.
     * ----------------------------------------------------------------------- */
    /* do_music_load(app.state, app.tick_count); */

    /* -----------------------------------------------------------------------
     * Initial render.
     * ----------------------------------------------------------------------- */
    render_current_state(&app, &framebuffer, &player, &inventory);

    /* -----------------------------------------------------------------------
     * Main event loop -- 20 Hz poll (50 ms period).
     * ----------------------------------------------------------------------- */
    fq_event_t      evt;
    fq_app_state_t  last_state        = app.state;
    uint8_t         last_menu_index   = app.home_menu_index;
    uint8_t         needs_redraw      = 0u;

    /* Track inventory state to detect exit (for auto-save). */
    uint8_t         was_inventory     = (app.state == FQ_STATE_INVENTORY) ? 1u : 0u;

    /* Phase-21: track player level to detect level-up. */
    uint8_t         last_player_level = player.level;

    while (1) {
        int64_t now_us = esp_timer_get_time();

        /* Phase-22: clear SFX duck flag at start of each tick. */
        s_sfx_emitted_this_tick = 0u;

        /* Post TIMER_TICK to event bus (drives training target movement). */
        {
            fq_event_t tick_evt = { FQ_EVT_TIMER_TICK, 0u };
            fq_event_bus_post(&app.bus, FQ_EVT_TIMER_TICK, 0u);
            (void)tick_evt;
        }

        /* Drain the event bus. */
        while (fq_event_bus_pop(&app.bus, &evt)) {
            fq_app_state_t pre_state = app.state;
            fq_app_dispatch(&app, &evt);

            /*
             * Phase 23: Wake from idle ONLY on button press — not TIMER_TICK.
             * Previous code woke on ANY event, including TIMER_TICK which fires
             * every 50ms. This caused spurious wake-ups in the field.
             */
            if (s_idle_active &&
                (evt.id == FQ_EVT_BTN_A_PRESS || evt.id == FQ_EVT_BTN_B_PRESS)) {
                s_idle_active    = 0u;
                s_anim_frame     = 0u;
                s_last_anim_us   = now_us;
                needs_redraw     = 1u;
                last_state       = app.state;
                last_menu_index  = app.home_menu_index;

                /* Phase-22: Resume music on idle wake. Reset guard so
                 * do_music_load retries for the same state after idle.
                 * Phase-23: disabled — embedded .mod plays continuously. */
                /* s_music_loaded_state = (fq_app_state_t)0xFFu;
                do_music_load(app.state, app.tick_count); */
            }

            /* Phase-21: Button SFX — triggered in main loop task only. */
            if (evt.id == FQ_EVT_BTN_A_PRESS) {
                do_sfx_tracked(SFX_BTN_PRESS);
            } else if (evt.id == FQ_EVT_BTN_B_PRESS) {
                do_sfx_tracked(SFX_BTN_BACK);
            }

            /* Phase-21: Menu navigation SFX on home_menu_index change. */
            if (pre_state == FQ_STATE_HOME &&
                app.state  == FQ_STATE_HOME &&
                evt.id     == FQ_EVT_BTN_B_PRESS) {
                do_sfx_tracked(SFX_MENU_NAVIGATE);
            }

            /* Phase-21: Item equip SFX on inventory BTN_A press. */
            if (app.state == FQ_STATE_INVENTORY &&
                evt.id    == FQ_EVT_BTN_A_PRESS) {
                do_sfx_tracked(SFX_ITEM_EQUIP);
            }

            /* Phase-21: Combat SFX on COMBAT_ROUND_COMPLETE transition. */
            if (evt.id == FQ_EVT_COMBAT_ROUND_COMPLETE &&
                pre_state == FQ_STATE_BATTLE) {
                do_sfx_tracked(app.battle_won ? SFX_COMBAT_CRIT : SFX_COMBAT_HIT);
            }

            /* Phase-21: Level-up SFX (detect player level increase). */
            if (player.level > last_player_level) {
                do_sfx_tracked(SFX_LEVEL_UP);
                last_player_level = player.level;
            }

            /* Phase-21: Rebirth SFX on REBIRTH -> HOME transition. */
            if (pre_state == FQ_STATE_REBIRTH &&
                app.state == FQ_STATE_HOME) {
                do_sfx_tracked(SFX_REBIRTH);
            }

            /* Phase-21: Death SFX on BATTLE -> BATTLE_RESULT when player dead. */
            if (pre_state == FQ_STATE_BATTLE &&
                app.state == FQ_STATE_BATTLE_RESULT &&
                player.is_dead == 1u) {
                do_sfx_tracked(SFX_DEATH);
            }

            if (app.state != last_state) {
                /* Auto-save on inventory exit. */
                if (was_inventory && app.state != FQ_STATE_INVENTORY) {
                    do_auto_save(&app, &player, &inventory);
                }

                /* Auto-save on onboarding confirm (BTN_B -> HOME). */
                if (last_state == FQ_STATE_ONBOARDING &&
                    app.state  == FQ_STATE_HOME) {
                    do_auto_save(&app, &player, &inventory);
                    if (app.onboarding_save_failed) {
                        /* Save failed on first boot (e.g. flash error). Character
                         * is already created in memory -- stay in HOME so the user
                         * can play. A retry will occur on the next auto-save trigger.
                         * Do NOT force state back to ONBOARDING: that causes a
                         * hard-refresh loop on every TIMER_TICK event. */
                        ESP_LOGW(TAG, "Onboarding save failed -- staying HOME, will retry");
                    }
                }

                /* AC-1: Auto-save on BATTLE_RESULT -> HOME. */
                if (last_state == FQ_STATE_BATTLE_RESULT &&
                    app.state  == FQ_STATE_HOME) {
                    do_auto_save(&app, &player, &inventory);
                }

                /* AC-1: Auto-save on REBIRTH -> HOME. */
                if (last_state == FQ_STATE_REBIRTH &&
                    app.state  == FQ_STATE_HOME) {
                    do_auto_save(&app, &player, &inventory);
                }

                /* Phase-22: State transition → stop-start music (no crossfade).
                 * Phase-23: Disabled — embedded .mod plays continuously. */
                /* fq_music_stop(&s_music_ctx);
                do_music_load(app.state, app.tick_count); */

                needs_redraw     = 1u;
                last_state       = app.state;
                last_menu_index  = app.home_menu_index;
                s_anim_frame     = 0u;
                s_last_anim_us   = now_us;
            } else if (app.state == FQ_STATE_HOME &&
                       app.home_menu_index != last_menu_index) {
                needs_redraw    = 1u;
                last_menu_index = app.home_menu_index;
            } else if (app.state == FQ_STATE_INVENTORY) {
                needs_redraw = 1u;
            } else if (app.state == FQ_STATE_TRAINING) {
                needs_redraw = 1u;
            } else if (app.state == FQ_STATE_ONBOARDING &&
                       (evt.id == FQ_EVT_BTN_A_PRESS ||
                        evt.id == FQ_EVT_BTN_B_PRESS)) {
                /*
                 * Phase 23: Only redraw onboarding on BUTTON events, not TIMER_TICK.
                 * TIMER_TICK fires every 50ms and triggered continuous e-paper
                 * refreshes (~1.8s each per partial), making the carousel unusable
                 * in the field. Hardware-verified fix (lab 2026-04-05).
                 */
                needs_redraw = 1u;
            }

            was_inventory = (app.state == FQ_STATE_INVENTORY) ? 1u : 0u;
        }

        /* Idle timeout check. */
        if (!s_idle_active                                    &&
            !is_idle_forbidden(app.state)                    &&
            (now_us - s_last_button_us) > (int64_t)IDLE_TIMEOUT_US)
        {
            s_idle_active    = 1u;
            needs_redraw     = 0u;

            /* Phase-22: Stop music during idle (save power).
             * Phase-23: Disabled — embedded .mod plays continuously. */
            /* fq_music_stop(&s_music_ctx); */

            render_idle(&app, &framebuffer, &player);
        }

        /* Home screen animation tick — uses partial refresh (lab-tuned). */
        if (!s_idle_active                                    &&
            app.state == FQ_STATE_HOME                        &&
            (now_us - s_last_anim_us) > (int64_t)ANIM_FRAME_US)
        {
            s_anim_frame   = (s_anim_frame == 0u) ? 2u : 0u;
            s_last_anim_us = now_us;
            needs_redraw   = 0u;
            render_home_partial(&app, &framebuffer, &player);
        }

        /* Onboarding partial refresh: class carousel update via BTN_A.
         * Only fires when state is ONBOARDING AND it is NOT a fresh state-change
         * (state-change redraws go through render_current_state for full refresh).
         * app.state == last_state here because ONBOARDING state never changes on
         * BTN_A — only class_index changes. The needs_redraw flag is set by the
         * ONBOARDING special-case block above. */
        if (needs_redraw && !s_idle_active &&
            app.state == FQ_STATE_ONBOARDING &&
            app.state == last_state) {
            render_onboarding_partial(&app, &framebuffer, &player);
            needs_redraw = 0u;
        }

        /* Full redraw if flagged (skip if idle). */
        if (needs_redraw && !s_idle_active) {
            render_current_state(&app, &framebuffer, &player, &inventory);
            needs_redraw = 0u;
        }

        /* Phase-22: Music tick — render and push to ring buffer. */
        if (!s_idle_active) {
            do_music_tick();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
