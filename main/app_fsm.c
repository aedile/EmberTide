/**
 * app_fsm.c — FiestaQuest Application Layer: Root State Machine Implementation
 *
 * Implements fq_app_init() and fq_app_dispatch() for the top-level FSM.
 *
 * Design principles:
 *   - One switch per state, one case per handled event. All other events fall
 *     through to the default (silent ignore) path.
 *   - No allocations. No floating point. No PRNG calls outside STATE_BATTLE.
 *   - The combat context is untouched by any handler except the BATTLE state
 *     handler, preserving the deterministic PRNG stream (Constitution P0).
 *
 * PRNG isolation contract:
 *   ctx->combat.rng MUST NOT be accessed in any state handler except the
 *   FQ_STATE_BATTLE block. This is structurally enforced by the switch layout:
 *   each case only touches the fields it needs, and only the BATTLE case is
 *   permitted to call fq_combat_step() which advances the PRNG.
 *
 * Phase-19 HOME state:
 *   BTN_B (☀ SUN, GPIO18) cycles home_menu_index mod FQ_HOME_MENU_COUNT.
 *   BTN_A (⏻ PWR, GPIO0) selects the highlighted menu item and transitions.
 *   home_menu_index is reset to 0 whenever any state transitions BACK to HOME.
 */

#include "app_fsm.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * go_home — transition to HOME and reset the menu index.
 *
 * Centralised helper used by all states that return to HOME. This ensures
 * home_menu_index is always 0 when the player arrives at the home screen,
 * regardless of which path they took to get there.
 * ---------------------------------------------------------------------------*/
static void go_home(fq_app_ctx_t *ctx)
{
    ctx->state           = FQ_STATE_HOME;
    ctx->home_menu_index = 0u;
}

/* ---------------------------------------------------------------------------
 * fq_app_init
 * ---------------------------------------------------------------------------*/
game_err_t fq_app_init(fq_app_ctx_t   *ctx,
                        fq_character_t *player,
                        fq_inventory_t *inv)
{
    if (ctx == NULL || player == NULL || inv == NULL) {
        return GAME_ERR_NULL_PTR;
    }

    /* Zero the context first to establish a clean baseline. */
    memset(ctx, 0, sizeof(*ctx));

    /* Wire non-owning pointers. */
    ctx->player    = player;
    ctx->inventory = inv;

    /* Initialize the embedded event bus. */
    fq_event_bus_init(&ctx->bus);

    /* home_menu_index starts at 0 (zeroed by memset above — explicit for clarity). */
    ctx->home_menu_index = 0u;

    /* Automatic BOOT → TITLE transition (no PRNG touched). */
    ctx->state = FQ_STATE_TITLE;

    return GAME_OK;
}

/* ---------------------------------------------------------------------------
 * fq_app_dispatch
 * ---------------------------------------------------------------------------*/
game_err_t fq_app_dispatch(fq_app_ctx_t     *ctx,
                            const fq_event_t *evt)
{
    if (ctx == NULL || evt == NULL) {
        return GAME_ERR_NULL_PTR;
    }

    /* QA P11-02: TIMER_TICK increments tick_count in ALL states.
     * This runs before the state switch so it fires regardless of current state.
     * Constitution Priority 0: no PRNG touched here. */
    if (evt->id == FQ_EVT_TIMER_TICK) {
        ctx->tick_count++;
    }

    switch (ctx->state) {

        /* -------------------------------------------------------------------
         * FQ_STATE_BOOT
         * Only the automatic init transition exits BOOT. Button presses and
         * all other runtime events are ignored — BOOT is a transient state
         * that should not be reached during normal runtime.
         * ------------------------------------------------------------------- */
        case FQ_STATE_BOOT:
            /* No transitions defined for any event in BOOT. */
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_TITLE
         * ------------------------------------------------------------------- */
        case FQ_STATE_TITLE:
            switch (evt->id) {
                case FQ_EVT_BTN_A_PRESS:
                case FQ_EVT_BTN_B_PRESS:
                    /* Either button advances past the title screen.
                     * The SUN button (GPIO18, BTN_B) and the PWR button
                     * (GPIO0, BTN_A) both work — any press is intentional. */
                    go_home(ctx);
                    break;
                default:
                    /* All other events silently ignored. */
                    break;
            }
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_HOME
         *
         * Phase-19 two-button UX:
         *   BTN_B (☀ SUN) — cycles home_menu_index mod FQ_HOME_MENU_COUNT.
         *   BTN_A (⏻ PWR) — selects current menu item, transitions to target.
         *
         * Menu mapping:
         *   0 = TRAIN  → FQ_STATE_TRAINING
         *   1 = BATTLE → FQ_STATE_BATTLE_SETUP
         *   2 = ITEMS  → FQ_STATE_INVENTORY
         *   3 = STATS  → FQ_STATE_STATS
         * ------------------------------------------------------------------- */
        case FQ_STATE_HOME:
            switch (evt->id) {
                case FQ_EVT_BTN_B_PRESS:
                    /* Cycle menu forward, wrapping at FQ_HOME_MENU_COUNT. */
                    ctx->home_menu_index =
                        (uint8_t)((ctx->home_menu_index + 1u) % FQ_HOME_MENU_COUNT);
                    break;

                case FQ_EVT_BTN_A_PRESS:
                    /* Select the currently highlighted menu item. */
                    switch (ctx->home_menu_index) {
                        case FQ_HOME_MENU_TRAIN:
                            ctx->state = FQ_STATE_TRAINING;
                            break;
                        case FQ_HOME_MENU_BATTLE:
                            ctx->state = FQ_STATE_BATTLE_SETUP;
                            break;
                        case FQ_HOME_MENU_ITEMS:
                            ctx->state = FQ_STATE_INVENTORY;
                            break;
                        case FQ_HOME_MENU_STATS:
                            ctx->state = FQ_STATE_STATS;
                            break;
                        default:
                            /* Defensive: unexpected index — silently ignore. */
                            break;
                    }
                    break;

                default:
                    break;
            }
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_INVENTORY
         * ------------------------------------------------------------------- */
        case FQ_STATE_INVENTORY:
            switch (evt->id) {
                case FQ_EVT_BTN_B_PRESS:
                    go_home(ctx);
                    break;
                default:
                    break;
            }
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_STATS
         * ------------------------------------------------------------------- */
        case FQ_STATE_STATS:
            switch (evt->id) {
                case FQ_EVT_BTN_B_PRESS:
                    go_home(ctx);
                    break;
                default:
                    break;
            }
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_TRAINING
         * ------------------------------------------------------------------- */
        case FQ_STATE_TRAINING:
            switch (evt->id) {
                case FQ_EVT_BTN_B_PRESS:
                    go_home(ctx);
                    break;
                default:
                    break;
            }
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_BATTLE_SETUP
         * ------------------------------------------------------------------- */
        case FQ_STATE_BATTLE_SETUP:
            switch (evt->id) {
                case FQ_EVT_BLE_CONNECTED:
                    ctx->state         = FQ_STATE_BATTLE;
                    ctx->combat_active = 1u;
                    /* NOTE: fq_combat_init() would be called here in a full
                     * integration (using the BLE-negotiated seed). Deferred
                     * to the BLE HAL integration phase — see Rule 8 advisory. */
                    break;
                default:
                    break;
            }
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_BATTLE
         *
         * Constitution Priority 0: this is the ONLY case block permitted to
         * access ctx->combat.rng. The guard (combat_active == 1) is checked
         * before any combat logic would run.
         * ------------------------------------------------------------------- */
        case FQ_STATE_BATTLE:
            switch (evt->id) {
                case FQ_EVT_COMBAT_ROUND_COMPLETE:
                    /* Only advance if combat is actually active (PRNG guard). */
                    if (ctx->combat_active == 1u) {
                        ctx->state         = FQ_STATE_BATTLE_RESULT;
                        ctx->combat_active = 0u;
                    }
                    break;
                default:
                    break;
            }
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_BATTLE_RESULT
         * ------------------------------------------------------------------- */
        case FQ_STATE_BATTLE_RESULT:
            switch (evt->id) {
                case FQ_EVT_BTN_A_PRESS:
                    go_home(ctx);
                    break;
                default:
                    break;
            }
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_REBIRTH, FQ_STATE_SETTINGS — no transitions yet.
         * ------------------------------------------------------------------- */
        case FQ_STATE_REBIRTH:
        case FQ_STATE_SETTINGS:
        case FQ_STATE_COUNT:
        default:
            /* Silently ignore all events in unimplemented / sentinel states. */
            break;
    }

    return GAME_OK;
}
