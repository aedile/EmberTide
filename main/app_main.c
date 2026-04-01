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
#include "view_models.h"
#include "screens/screen_home.h"
#include "screens/screen_stats.h"
#include "screens/screen_inventory.h"

/* HAL layer — only main/ may include hal_*.h */
#include "hal_epaper.h"
#include "hal_flash.h"
#include "hal_gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "app_main";

/* -------------------------------------------------------------------------
 * File-scope application context pointer.
 *
 * button_callback is called from the gpio_task context (not ISR context —
 * the ISR posts to a queue, the task calls the callback). It needs access
 * to the event bus. Using a file-scope pointer avoids passing through
 * FreeRTOS task parameter indirection.
 * -------------------------------------------------------------------------
 */
static fq_app_ctx_t *s_app = NULL;

/* -------------------------------------------------------------------------
 * button_callback — Called from gpio_task context on each debounced press.
 *
 * Maps the HAL button ID to an FQ event ID and posts to the event bus.
 * Safe to call from task context.
 * -------------------------------------------------------------------------
 */
static void button_callback(hal_btn_id_t btn_id)
{
    if (!s_app) {
        return;
    }
    fq_event_id_t evt_id = (btn_id == HAL_BTN_A)
                           ? FQ_EVT_BTN_A_PRESS
                           : FQ_EVT_BTN_B_PRESS;
    fq_event_bus_post(&s_app->bus, evt_id, 0u);
}

/* -------------------------------------------------------------------------
 * render_current_state — Render the current FSM state to the framebuffer
 * and flush it to the e-paper display.
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
            /* Proof-of-life title screen: filled rectangle + border. */
            fq_fb_draw_rect(fb, 0, 0, 200, 200, 1);
            fq_fb_fill_rect(fb, 10, 80, 180, 40, 1);
            break;
        }
        case FQ_STATE_HOME: {
            fq_vm_home_t vm_home;
            fq_vm_build_home(&vm_home, player);
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
     * Initial render — display title then home screen.
     * ----------------------------------------------------------------------- */
    render_current_state(&app, &framebuffer, &player, &inventory);

    /* -----------------------------------------------------------------------
     * Main event loop — 20 Hz poll (50 ms period).
     * ----------------------------------------------------------------------- */
    fq_event_t      evt;
    fq_app_state_t  last_state  = app.state;
    uint8_t         needs_redraw = 0u;

    while (1) {
        /* Drain the event bus. */
        while (fq_event_bus_pop(&app.bus, &evt)) {
            fq_app_dispatch(&app, &evt);
            if (app.state != last_state) {
                needs_redraw = 1u;
                last_state   = app.state;
            }
        }

        if (needs_redraw) {
            render_current_state(&app, &framebuffer, &player, &inventory);
            needs_redraw = 0u;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
