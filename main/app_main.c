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
 *   Idle design:
 *     - Idle is NOT an FSM state. s_idle_active flag overlays the renderer.
 *     - After IDLE_TIMEOUT_US (30s) of no button input, s_idle_active = 1.
 *     - Any button press clears s_idle_active; the previous FSM state resumes.
 *     - Idle is suppressed during FQ_STATE_BATTLE, FQ_STATE_BATTLE_SETUP,
 *       and FQ_STATE_TITLE (these states must not be interrupted).
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
 *   Idle precision note:
 *     The main loop polls at 20 Hz (50ms). Idle timeout accuracy is ±50ms,
 *     which is acceptable for a 30-second timeout. No real-time guarantee.
 *
 *   esp_timer overflow note:
 *     int64_t subtraction (now - last_button) wraps safely at ~292,000 years.
 *     Not a practical concern.
 */

#include <stdint.h>
#include <string.h>

/* Game layer */
#include "types.h"
#include "character.h"
#include "save_format.h"

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
#include "screens/screen_idle.h"
#include "asset_data.h"
#include "sprite_util.h"

/* HAL layer — only main/ may include hal_*.h */
#include "hal_epaper.h"
#include "hal_flash.h"
#include "hal_gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "app_main";

/* -------------------------------------------------------------------------
 * Idle / animation timing constants.
 * -------------------------------------------------------------------------
 */

/** Idle timeout: 30 seconds of no button input activates the screensaver. */
#define IDLE_TIMEOUT_US    (30ULL * 1000000ULL)

/**
 * Animation frame interval: 0.8 seconds per walk frame.
 *
 * 0.8s = 800,000 µs. Validated interactively on hardware — faster looked
 * frantic on e-paper partial refresh (~300ms latency); slower felt dead.
 */
#define ANIM_FRAME_US      (800000ULL)

/* BLOCKER 2 fix: Compile-time guard — ANIM_FRAME_US must be non-zero.
 * A value of 0 would cause the animation to fire on every loop iteration,
 * flooding the e-paper with partial refreshes and corrupting the display. */
_Static_assert(ANIM_FRAME_US > 0ULL,
               "ANIM_FRAME_US must be > 0 — zero causes runaway animation refreshes");

/* -------------------------------------------------------------------------
 * File-scope application context pointer.
 *
 * button_callback is called from the gpio_task context (not ISR context —
 * the ISR posts to a queue, the task calls the callback). It needs access
 * to the event bus and the idle/anim state. Using a file-scope pointer
 * avoids passing through FreeRTOS task parameter indirection.
 * -------------------------------------------------------------------------
 */
static fq_app_ctx_t *s_app = NULL;

/* -------------------------------------------------------------------------
 * Idle / animation state — file-scope, reset at startup.
 * -------------------------------------------------------------------------
 */
static int64_t  s_last_button_us;  /* esp_timer timestamp of last button press */
static uint8_t  s_idle_active;     /* 1 = idle screensaver is showing */
static uint8_t  s_anim_frame;      /* current walk-cycle frame (0 or 2) */
static int64_t  s_last_anim_us;    /* timestamp of last animation frame advance */

/* -------------------------------------------------------------------------
 * is_idle_forbidden — Returns 1 if idle must NOT activate in this state.
 *
 * Idle is suppressed during active combat, BLE pairing, and the title screen:
 *   - BATTLE / BATTLE_SETUP: must not interrupt mid-combat.
 *   - TITLE: the first-boot splash should not auto-dismiss to idle.
 * -------------------------------------------------------------------------
 */
static uint8_t is_idle_forbidden(fq_app_state_t state)
{
    return (state == FQ_STATE_BATTLE       ||
            state == FQ_STATE_BATTLE_SETUP  ||
            state == FQ_STATE_TITLE)
           ? 1u : 0u;
}

/* -------------------------------------------------------------------------
 * button_callback — Called from gpio_task context on each debounced press.
 *
 * Maps the HAL button ID to an FQ event ID and posts to the event bus.
 * Also resets s_last_button_us for idle timeout tracking.
 * Safe to call from task context.
 * -------------------------------------------------------------------------
 */
static void button_callback(hal_btn_id_t btn_id)
{
    if (!s_app) {
        return;
    }
    /* Reset idle timeout on every button press. */
    s_last_button_us = esp_timer_get_time();

    fq_event_id_t evt_id = (btn_id == HAL_BTN_A)
                           ? FQ_EVT_BTN_A_PRESS
                           : FQ_EVT_BTN_B_PRESS;
    fq_event_bus_post(&s_app->bus, evt_id, 0u);
}

/* -------------------------------------------------------------------------
 * render_title_screen — Draw the EmberTide title screen.
 *
 * Layout (200x200 px, 1-bit e-paper):
 *   y=0..3    4-px thick outer border (filled rects on all four edges)
 *   y=8       1-px inner decorative border (draw_rect, inset 8px)
 *   y=20      "EmberTide" centered, script font (FONT_SCRIPT_24)
 *   y=52      horizontal separator line
 *   y=60      Dark Knight sprite (32x32) blitted at 2x -> 64x64, centered
 *   y=130     horizontal separator line
 *   y=145     "Press [PWR]" centered, small font (FONT_REGS_12)
 *   y=192     bottom of inner border
 *   y=196..199 bottom 4-px thick border
 *
 * Button note: [PWR] refers to GPIO0 (the ⏻ power icon button on the case),
 * which is the button closest to the USB-C port. This is HAL_BTN_A.
 * -------------------------------------------------------------------------
 */
static void render_title_screen(fq_fb_t *fb)
{
    const fq_font_t   *font_title = fq_get_font_title();
    const fq_font_t   *font_small = fq_get_font_small();
    const fq_sprite_t *spr        = fq_get_char_sprite(0u, 0u); /* Dark Knight, frame 0 */

    /* -- Outer 4-px thick border ------------------------------------------ */
    fq_fb_fill_rect(fb,   0,   0, 200,   4, 1u);
    fq_fb_fill_rect(fb,   0, 196, 200,   4, 1u);
    fq_fb_fill_rect(fb,   0,   4,   4, 192, 1u);
    fq_fb_fill_rect(fb, 196,   4,   4, 192, 1u);

    /* -- Inner 1-px decorative border (inset 8px from outer border) --------- */
    fq_fb_draw_rect(fb, 8, 8, 184, 184, 1u);

    /* -- "EmberTide" centered at y=20, script font -------------------------- */
    {
        static const char title_str[] = "EmberTide";
        int16_t w = fq_text_width(font_title, title_str);
        int16_t x = (int16_t)((200 - w) / 2);
        fq_draw_text(fb, font_title, x, 20, title_str);
    }

    /* -- Horizontal separator below title, y=52 ----------------------------- */
    fq_fb_draw_line(fb, 12, 52, 187, 52, 1u);

    /* -- Dark Knight sprite at 2x (64x64), centered horizontally, top at y=60 */
    if (spr != NULL) {
        int16_t sprite_x = (int16_t)((200 - 64) / 2); /* = 68 */
        fq_blit_sprite_2x(fb, sprite_x, 60, spr);
    }

    /* -- Horizontal separator above footer, y=130 --------------------------- */
    fq_fb_draw_line(fb, 12, 130, 187, 130, 1u);

    /* -- "Press [PWR]" centered at y=145, small font ------------------------ */
    {
        static const char prompt_str[] = "Press [PWR]";
        int16_t w = fq_text_width(font_small, prompt_str);
        int16_t x = (int16_t)((200 - w) / 2);
        fq_draw_text(fb, font_small, x, 145, prompt_str);
    }
}

/* -------------------------------------------------------------------------
 * render_current_state — Render the current FSM state to the framebuffer
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
            fq_vm_build_inventory(&vm_inv, inventory);
            fq_render_inventory(fb, &vm_inv);
            break;
        }
        default:
            /* All other states: clear screen — no dedicated renderer yet. */
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
 * render_home_partial — Re-render the home screen and flush using partial
 * refresh (for animation frame updates — no full flicker).
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
        ESP_LOGE(TAG, "hal_epaper_flush_partial failed: %d — falling back to full", (int)err);
        /* Fall back to full flush on partial failure. */
        hal_epaper_flush(fb->pixels, FQ_FB_SIZE);
    }
}

/* -------------------------------------------------------------------------
 * render_idle — Render the idle screensaver and flush using partial refresh.
 * -------------------------------------------------------------------------
 */
static void render_idle(fq_app_ctx_t   *app,
                        fq_fb_t        *fb,
                        fq_character_t *player)
{
    (void)app; /* reserved for future use */

    hal_epaper_err_t err;

    fq_vm_idle_t vm_idle;
    fq_vm_build_idle(&vm_idle, player);
    fq_render_idle(fb, &vm_idle);

    err = hal_epaper_flush_partial(fb->pixels, FQ_FB_SIZE);
    if (err != HAL_EPAPER_OK) {
        ESP_LOGE(TAG, "idle flush_partial failed: %d — falling back to full", (int)err);
        hal_epaper_flush(fb->pixels, FQ_FB_SIZE);
    }
}

/* -------------------------------------------------------------------------
 * app_main — ESP-IDF application entry point.
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
    hal_flash_err_t flash_err = hal_flash_init();
    if (flash_err != HAL_FLASH_OK) {
        ESP_LOGE(TAG, "hal_flash_init failed: %d — first-boot fallback", (int)flash_err);
    } else {
        uint8_t save_buf[FQ_SAVE_MAX_SIZE];
        size_t  bytes_read = 0u;
        flash_err = hal_flash_read_save(save_buf, sizeof(save_buf), &bytes_read);
        if (flash_err == HAL_FLASH_OK && bytes_read >= FQ_SAVE_SERIALIZED_SIZE_V1) {
            fq_save_err_t save_err = fq_save_deserialize(save_buf, bytes_read,
                                                          &player, &inventory);
            if (save_err == FQ_SAVE_OK) {
                ESP_LOGI(TAG, "Save loaded OK (%u bytes)", (unsigned)bytes_read);
            } else {
                ESP_LOGW(TAG, "Save corrupt (%d) — first-boot fallback", (int)save_err);
                memset(&player,    0, sizeof(player));
                memset(&inventory, 0, sizeof(inventory));
            }
        }
    }

    /* First-boot: create default character if name is empty. */
    if (player.name[0] == '\0') {
        fq_character_create(&player, FQ_CLASS_BRUISER, 1u, "Ember");
        ESP_LOGI(TAG, "First boot — created default character 'Ember'");
    }

    /* -----------------------------------------------------------------------
     * App context init.
     * ----------------------------------------------------------------------- */
    fq_app_init(&app, &player, &inventory);
    s_app = &app;

    /* -----------------------------------------------------------------------
     * HAL init.
     * Phase-19.5: hal_epaper_init() now performs a boot-time full clear
     * (white→black→white) before returning. s_flush_count reset to 0 after.
     * ----------------------------------------------------------------------- */
    hal_epaper_err_t epaper_err = hal_epaper_init();
    if (epaper_err != HAL_EPAPER_OK) {
        ESP_LOGE(TAG, "hal_epaper_init failed: %d", (int)epaper_err);
        /* Non-fatal: continue without display. */
    }

    hal_gpio_err_t gpio_err = hal_gpio_init(button_callback);
    if (gpio_err != HAL_GPIO_OK) {
        ESP_LOGE(TAG, "hal_gpio_init failed: %d", (int)gpio_err);
    }

    /* -----------------------------------------------------------------------
     * Phase-19.5: Initialise idle/anim state.
     * s_last_button_us set to now so idle does not fire before first input.
     * ----------------------------------------------------------------------- */
    s_last_button_us = esp_timer_get_time();
    s_last_anim_us   = s_last_button_us;
    s_idle_active    = 0u;
    s_anim_frame     = 0u;

    /* -----------------------------------------------------------------------
     * Initial render — display title screen.
     * ----------------------------------------------------------------------- */
    render_current_state(&app, &framebuffer, &player, &inventory);

    /* -----------------------------------------------------------------------
     * Main event loop — 20 Hz poll (50 ms period).
     * ----------------------------------------------------------------------- */
    fq_event_t      evt;
    fq_app_state_t  last_state        = app.state;
    uint8_t         last_menu_index   = app.home_menu_index;
    uint8_t         needs_redraw      = 0u;

    while (1) {
        int64_t now_us = esp_timer_get_time();

        /* ── Drain the event bus ─────────────────────────────────────────── */
        while (fq_event_bus_pop(&app.bus, &evt)) {
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

            /* Redraw on state change OR on home menu cursor change. */
            if (app.state != last_state) {
                needs_redraw     = 1u;
                last_state       = app.state;
                last_menu_index  = app.home_menu_index;
                /* Reset animation frame on any state change. */
                s_anim_frame     = 0u;
                s_last_anim_us   = now_us;
            } else if (app.state == FQ_STATE_HOME &&
                       app.home_menu_index != last_menu_index) {
                needs_redraw    = 1u;
                last_menu_index = app.home_menu_index;
            }
        }

        /* ── Idle timeout check ──────────────────────────────────────────── */
        if (!s_idle_active                                    &&
            !is_idle_forbidden(app.state)                    &&
            (now_us - s_last_button_us) > (int64_t)IDLE_TIMEOUT_US)
        {
            s_idle_active    = 1u;
            needs_redraw     = 0u;  /* suppress normal redraw */
            render_idle(&app, &framebuffer, &player);
        }

        /* ── Home screen animation tick ──────────────────────────────────── */
        if (!s_idle_active                                    &&
            app.state == FQ_STATE_HOME                        &&
            (now_us - s_last_anim_us) > (int64_t)ANIM_FRAME_US)
        {
            /* Alternate between frame 0 (idle pose) and frame 2 (mid-step).
             * Two-frame cycle minimises ghosting on e-paper partial refresh. */
            s_anim_frame   = (s_anim_frame == 0u) ? 2u : 0u;
            s_last_anim_us = now_us;
            needs_redraw   = 0u;  /* handled as partial redraw below */
            render_home_partial(&app, &framebuffer, &player);
        }

        /* ── Full redraw if state-change flagged (skip if idle) ──────────── */
        if (needs_redraw && !s_idle_active) {
            render_current_state(&app, &framebuffer, &player, &inventory);
            needs_redraw = 0u;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
