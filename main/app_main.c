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
 *   Training session: TIMER_TICK posted to event bus every loop (acts as
 *   tick source for target movement). Auto-save on inventory exit.
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
 *     - s_anim_frame alternates between 0 and 2 every ANIM_FRAME_US (0.8s)
 *       while in FQ_STATE_HOME and not idle.
 *     - Frame 0 = idle pose; Frame 2 = mid-step. Two frames minimize
 *       accumulated ghosting on e-paper with partial refresh.
 *     - Animation timer resets when leaving HOME state.
 *     - Animation is suppressed while s_idle_active is set.
 *
 *   Partial refresh:
 *     - Animation redraws use hal_epaper_flush_partial() (fast, no flicker).
 *     - HAL internally forces a full refresh every EPD_FULL_REFRESH_INTERVAL
 *       partial flushes to clear accumulated ghosting.
 *     - State-change redraws continue to use hal_epaper_flush() (full).
 *
 * Phase 21: I2S audio init and SFX wiring.
 *   - hal_audio_init() called during HAL init.
 *   - do_sfx(id) helper: generates PCM via fq_sfx_play(), pushes to HAL
 *     ring buffer via hal_audio_write_samples(). No-op if sfx_enabled==0.
 *   - button_callback: ONLY posts events to the event bus. do_sfx() is NOT
 *     called from button_callback — that would create a data race because
 *     button_callback runs in gpio_task (a separate FreeRTOS task) while
 *     do_sfx() uses a static buffer and an unprotected ring buffer.
 *   - Event loop: SFX_BTN_PRESS on FQ_EVT_BTN_A_PRESS, SFX_BTN_BACK on
 *     FQ_EVT_BTN_B_PRESS, SFX_MENU_NAVIGATE on home menu cycle,
 *     SFX_ITEM_EQUIP on equip toggle, SFX_COMBAT_HIT/MISS/CRIT on combat
 *     events, SFX_LEVEL_UP on level gain, SFX_REBIRTH on rebirth,
 *     SFX_DEATH on death.
 *
 *   SFX quiet-mode: app.sfx_enabled flag (set by fq_app_init to 1).
 *   do_sfx() checks the flag — zero SFX overhead when disabled.
 *
 *   Concurrency: do_sfx() is ONLY called from the main loop task. The static
 *   PCM buffer and HAL ring buffer are accessed exclusively from the main
 *   loop — no mutex required.
 */

#include <stdint.h>
#include <string.h>

/* Game layer */
#include "types.h"
#include "character.h"
#include "save_format.h"
#include "sfx.h"

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
 * Animation frame interval: 0.8 seconds per walk frame.
 *
 * 0.8s = 800,000 us. Validated interactively on hardware -- faster looked
 * frantic on e-paper partial refresh (~300ms latency); slower felt dead.
 */
#define ANIM_FRAME_US      (800000ULL)

/* BLOCKER 2 fix: Compile-time guard -- ANIM_FRAME_US must be non-zero. */
_Static_assert(ANIM_FRAME_US > 0ULL,
               "ANIM_FRAME_US must be > 0 -- zero causes runaway animation refreshes");

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
 *
 * Phase-19 Interactive: ONBOARDING added (first-boot must not auto-dismiss).
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
 * do_sfx -- Generate and play a sound effect.
 *
 * Phase-21 helper. Generates PCM samples via fq_sfx_play() into a local
 * static buffer, then pushes them to the HAL ring buffer via
 * hal_audio_write_samples(). No-op when sfx_enabled == 0 (quiet mode).
 *
 * CONCURRENCY: do_sfx() is ONLY called from the main loop task. The static
 * PCM buffer and the HAL ring buffer have no mutex because they are never
 * accessed from any other task. button_callback() runs in gpio_task and
 * does NOT call do_sfx() — it only posts events to the event bus. The main
 * loop drains the bus and calls do_sfx() from the single main-loop task
 * context, preserving this invariant.
 * -------------------------------------------------------------------------
 */
static void do_sfx(fq_sfx_id_t id)
{
    if (!s_app || !s_app->sfx_enabled) {
        return;
    }
    static int16_t s_sfx_buf[AUDIO_RING_BUF_SAMPLES];
    size_t samples_out = 0u;
    if (fq_sfx_play(id, s_sfx_buf, AUDIO_RING_BUF_SAMPLES, &samples_out)
        == GAME_OK && samples_out > 0u) {
        hal_audio_write_samples(s_sfx_buf, samples_out);
    }
}

/* -------------------------------------------------------------------------
 * do_auto_save -- Serialize and write save to flash.
 *
 * Called on inventory exit and after onboarding character creation.
 * Sets app->onboarding_save_failed based on result.
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
 * here because button_callback runs in gpio_task (a separate FreeRTOS task).
 * Calling do_sfx() from gpio_task would create a data race on the static
 * PCM buffer in do_sfx() and on the unprotected HAL ring buffer.
 *
 * Button SFX (SFX_BTN_PRESS / SFX_BTN_BACK) are triggered in the main loop
 * event drain when FQ_EVT_BTN_A_PRESS / FQ_EVT_BTN_B_PRESS are dequeued.
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
 * render_current_state -- Render the current FSM state to the framebuffer
 * and flush it to the e-paper display (full refresh).
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
            /* Capture pre-rebirth stats from opponent snapshot if available.
             * Use player's current (post-rebirth) stats as new_stats. */
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
            /* All other states: clear screen -- no dedicated renderer yet. */
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
     *
     * If no valid save: go to ONBOARDING instead of creating "Ember".
     * If valid save: stay on TITLE (fq_app_init already set TITLE).
     * ----------------------------------------------------------------------- */
    if (!save_valid) {
        app.state = FQ_STATE_ONBOARDING;
        app.onboarding_class_index = 0u;
        app.onboarding_save_failed = 0u;
        ESP_LOGI(TAG, "First boot -- entering ONBOARDING");
    }

    /* -----------------------------------------------------------------------
     * HAL init.
     * ----------------------------------------------------------------------- */
    hal_epaper_err_t epaper_err = hal_epaper_init();
    if (epaper_err != HAL_EPAPER_OK) {
        ESP_LOGE(TAG, "hal_epaper_init failed: %d", (int)epaper_err);
    }

    hal_gpio_err_t gpio_err = hal_gpio_init(button_callback);
    if (gpio_err != HAL_GPIO_OK) {
        ESP_LOGE(TAG, "hal_gpio_init failed: %d", (int)gpio_err);
    }

    /* Phase-21: Audio init. */
    hal_audio_err_t audio_err = hal_audio_init();
    if (audio_err != HAL_AUDIO_OK) {
        ESP_LOGW(TAG, "hal_audio_init failed: %d -- SFX disabled", (int)audio_err);
        app.sfx_enabled = 0u;
    }

    /* -----------------------------------------------------------------------
     * Phase-19.5: Initialise idle/anim state.
     * ----------------------------------------------------------------------- */
    s_last_button_us = esp_timer_get_time();
    s_last_anim_us   = s_last_button_us;
    s_idle_active    = 0u;
    s_anim_frame     = 0u;

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

            /* Wake from idle on any button press. */
            if (s_idle_active) {
                s_idle_active    = 0u;
                s_anim_frame     = 0u;
                s_last_anim_us   = now_us;
                needs_redraw     = 1u;
                last_state       = app.state;
                last_menu_index  = app.home_menu_index;
            }

            /* Phase-21: Button SFX — triggered here in the main loop task,
             * NOT in button_callback (which runs in gpio_task). This is the
             * only safe place to call do_sfx() without a mutex. */
            if (evt.id == FQ_EVT_BTN_A_PRESS) {
                do_sfx(SFX_BTN_PRESS);
            } else if (evt.id == FQ_EVT_BTN_B_PRESS) {
                do_sfx(SFX_BTN_BACK);
            }

            /* Phase-21: Menu navigation SFX on home_menu_index change. */
            if (pre_state == FQ_STATE_HOME &&
                app.state  == FQ_STATE_HOME &&
                evt.id     == FQ_EVT_BTN_B_PRESS) {
                do_sfx(SFX_MENU_NAVIGATE);
            }

            /* Phase-21: Item equip SFX on inventory BTN_A press. */
            if (app.state == FQ_STATE_INVENTORY &&
                evt.id    == FQ_EVT_BTN_A_PRESS) {
                do_sfx(SFX_ITEM_EQUIP);
            }

            /* Phase-21: Combat SFX on COMBAT_ROUND_COMPLETE transition. */
            if (evt.id == FQ_EVT_COMBAT_ROUND_COMPLETE &&
                pre_state == FQ_STATE_BATTLE) {
                /* Use battle_won to pick SFX -- simplified (no per-round detail). */
                do_sfx(app.battle_won ? SFX_COMBAT_CRIT : SFX_COMBAT_HIT);
            }

            /* Phase-21: Level-up SFX (detect player level increase). */
            if (player.level > last_player_level) {
                do_sfx(SFX_LEVEL_UP);
                last_player_level = player.level;
            }

            /* Phase-21: Rebirth SFX on REBIRTH -> HOME transition. */
            if (pre_state == FQ_STATE_REBIRTH &&
                app.state == FQ_STATE_HOME) {
                do_sfx(SFX_REBIRTH);
            }

            /* Phase-21: Death SFX on BATTLE -> BATTLE_RESULT when player is dead. */
            if (pre_state == FQ_STATE_BATTLE &&
                app.state == FQ_STATE_BATTLE_RESULT &&
                player.is_dead == 1u) {
                do_sfx(SFX_DEATH);
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
                        /* Save failed -- re-enter onboarding. */
                        app.state = FQ_STATE_ONBOARDING;
                        ESP_LOGW(TAG, "Onboarding save failed -- re-entering");
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
                /* Redraw on cursor movement. */
                needs_redraw = 1u;
            } else if (app.state == FQ_STATE_TRAINING) {
                /* Redraw on training tick (target moved). */
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
            render_idle(&app, &framebuffer, &player);
        }

        /* Home screen animation tick. */
        if (!s_idle_active                                    &&
            app.state == FQ_STATE_HOME                        &&
            (now_us - s_last_anim_us) > (int64_t)ANIM_FRAME_US)
        {
            s_anim_frame   = (s_anim_frame == 0u) ? 2u : 0u;
            s_last_anim_us = now_us;
            needs_redraw   = 0u;
            render_home_partial(&app, &framebuffer, &player);
        }

        /* Full redraw if flagged (skip if idle). */
        if (needs_redraw && !s_idle_active) {
            render_current_state(&app, &framebuffer, &player, &inventory);
            needs_redraw = 0u;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
