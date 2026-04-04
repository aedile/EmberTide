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
 *
 * Phase-19 ONBOARDING state:
 *   BTN_A (⏻ PWR) cycles onboarding_class_index mod FQ_CLASS_COUNT.
 *   BTN_B (☀ SUN) confirms: creates character, saves (if save_failed==0),
 *   transitions to HOME on success. If onboarding_save_failed==1, stays.
 *
 * Phase-19 INVENTORY state:
 *   BTN_B cycles inventory_cursor mod inventory->count.
 *   BTN_A toggles equip/unequip on the cursor item.
 *   Double-tap BTN_B (two B presses within 6 ticks = 300ms) exits to HOME.
 *
 * Phase-19 TRAINING state:
 *   WAITING: BTN_A cycles game_type, BTN_B starts game (->ACTIVE).
 *   ACTIVE:  BTN_A registers hit, TIMER_TICK advances target_pos.
 *            BTN_B exits immediately (partial score, 0 XP).
 *   DONE:    BTN_B returns to HOME (XP already awarded on DONE transition).
 */

#include "app_fsm.h"
#include "character.h"
#include "name_gen.h"
#include "equip.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * go_home — transition to HOME and reset the menu index.
 * ---------------------------------------------------------------------------*/
static void go_home(fq_app_ctx_t *ctx)
{
    ctx->state           = FQ_STATE_HOME;
    ctx->home_menu_index = 0u;
}

/* ---------------------------------------------------------------------------
 * Double-tap B threshold: 6 ticks @ 50ms/tick = 300ms.
 * ---------------------------------------------------------------------------*/
#define INV_DOUBLE_TAP_TICKS  6u

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

    memset(ctx, 0, sizeof(*ctx));

    ctx->player    = player;
    ctx->inventory = inv;

    fq_event_bus_init(&ctx->bus);

    ctx->home_menu_index = 0u;

    /* Automatic BOOT → TITLE transition. */
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

    /* TIMER_TICK increments tick_count in ALL states. */
    if (evt->id == FQ_EVT_TIMER_TICK) {
        ctx->tick_count++;
    }

    switch (ctx->state) {

        /* -------------------------------------------------------------------
         * FQ_STATE_BOOT
         * ------------------------------------------------------------------- */
        case FQ_STATE_BOOT:
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_TITLE
         * ------------------------------------------------------------------- */
        case FQ_STATE_TITLE:
            switch (evt->id) {
                case FQ_EVT_BTN_A_PRESS:
                case FQ_EVT_BTN_B_PRESS:
                    go_home(ctx);
                    break;
                default:
                    break;
            }
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_HOME
         * ------------------------------------------------------------------- */
        case FQ_STATE_HOME:
            switch (evt->id) {
                case FQ_EVT_BTN_B_PRESS:
                    ctx->home_menu_index =
                        (uint8_t)((ctx->home_menu_index + 1u) % FQ_HOME_MENU_COUNT);
                    break;

                case FQ_EVT_BTN_A_PRESS:
                    switch (ctx->home_menu_index) {
                        case FQ_HOME_MENU_TRAIN:
                            /* Enter TRAINING: init session in WAITING state. */
                            fq_training_session_init(&ctx->training, FQ_TS_SPEED);
                            ctx->state = FQ_STATE_TRAINING;
                            break;
                        case FQ_HOME_MENU_BATTLE:
                            ctx->state = FQ_STATE_BATTLE_SETUP;
                            break;
                        case FQ_HOME_MENU_ITEMS:
                            ctx->inventory_cursor  = 0u;
                            ctx->inv_b_press_count = 0u;
                            ctx->inv_b_last_tick   = ctx->tick_count;
                            ctx->state = FQ_STATE_INVENTORY;
                            break;
                        case FQ_HOME_MENU_STATS:
                            ctx->state = FQ_STATE_STATS;
                            break;
                        default:
                            break;
                    }
                    break;

                default:
                    break;
            }
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_INVENTORY
         *
         * BTN_B: cycle cursor forward (mod item_count), with double-tap exit.
         * BTN_A: toggle equip on cursor item.
         * Double-tap B (2 B presses within INV_DOUBLE_TAP_TICKS): exit to HOME.
         * ------------------------------------------------------------------- */
        case FQ_STATE_INVENTORY:
            switch (evt->id) {
                case FQ_EVT_BTN_B_PRESS: {
                    uint8_t item_count = (ctx->inventory != NULL)
                                        ? ctx->inventory->count : 0u;

                    /* Check for double-tap exit. */
                    uint32_t ticks_since = ctx->tick_count - ctx->inv_b_last_tick;
                    if (ctx->inv_b_press_count >= 1u &&
                        ticks_since <= INV_DOUBLE_TAP_TICKS) {
                        /* Double-tap confirmed — exit to HOME. */
                        ctx->inv_b_press_count = 0u;
                        go_home(ctx);
                        break;
                    }

                    /* Record this press for double-tap detection. */
                    ctx->inv_b_press_count = 1u;
                    ctx->inv_b_last_tick   = ctx->tick_count;

                    /* Cycle cursor forward. */
                    if (item_count > 0u) {
                        uint8_t next = (uint8_t)(ctx->inventory_cursor + 1u);
                        if (next >= item_count) { next = 0u; }
                        ctx->inventory_cursor = next;
                    }
                    break;
                }

                case FQ_EVT_BTN_A_PRESS: {
                    /* Toggle equip on cursor item. */
                    if (ctx->player != NULL && ctx->inventory != NULL &&
                        ctx->inventory->count > 0u) {
                        uint8_t cursor = ctx->inventory_cursor;
                        /* Clamp cursor to valid range. */
                        if (cursor >= ctx->inventory->count) {
                            cursor = (uint8_t)(ctx->inventory->count - 1u);
                            ctx->inventory_cursor = cursor;
                        }
                        /* Silently ignore GAME_ERR_OVERFLOW (caller can show FULL). */
                        fq_equip_toggle(ctx->player, ctx->inventory, cursor);
                    }
                    break;
                }

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
         *
         * WAITING (type select):
         *   BTN_A = cycle game_type forward (mod 3).
         *   BTN_B = start session (WAITING -> ACTIVE).
         *
         * ACTIVE (game running):
         *   BTN_A = register hit.
         *   BTN_B = exit immediately to HOME (no XP).
         *   TIMER_TICK = advance target position.
         *
         * DONE (score shown):
         *   BTN_B = exit to HOME (XP was awarded on DONE transition).
         * ------------------------------------------------------------------- */
        case FQ_STATE_TRAINING:
            switch (evt->id) {
                case FQ_EVT_BTN_A_PRESS:
                    if (ctx->training.state == (uint8_t)FQ_TS_WAITING) {
                        /* Cycle game type: 0->1->2->0. */
                        uint8_t next_type = (uint8_t)(ctx->training.game_type + 1u);
                        if (next_type >= 3u) { next_type = 0u; }
                        ctx->training.game_type = next_type;
                    } else if (ctx->training.state == (uint8_t)FQ_TS_ACTIVE) {
                        /* Register hit attempt. */
                        fq_training_hit(&ctx->training);
                        /* If session ended (DONE), award XP. */
                        if (ctx->training.state == (uint8_t)FQ_TS_DONE &&
                            ctx->player != NULL) {
                            fq_training_award_xp(&ctx->training, ctx->player);
                        }
                    }
                    break;

                case FQ_EVT_BTN_B_PRESS:
                    if (ctx->training.state == (uint8_t)FQ_TS_WAITING) {
                        /* B in WAITING: start the session. */
                        fq_training_session_start(&ctx->training);
                    } else if (ctx->training.state == (uint8_t)FQ_TS_ACTIVE) {
                        /* B in ACTIVE: abandon session (no XP). */
                        go_home(ctx);
                    } else {
                        /* B in DONE: return home. */
                        go_home(ctx);
                    }
                    break;

                case FQ_EVT_TIMER_TICK:
                    /* Only advance target in ACTIVE state. */
                    if (ctx->training.state == (uint8_t)FQ_TS_ACTIVE) {
                        fq_training_step(&ctx->training);
                        /* If target advance caused DONE, award XP. */
                        if (ctx->training.state == (uint8_t)FQ_TS_DONE &&
                            ctx->player != NULL) {
                            fq_training_award_xp(&ctx->training, ctx->player);
                        }
                    }
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
                    break;
                default:
                    break;
            }
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_BATTLE
         *
         * Constitution Priority 0: ONLY this block may access ctx->combat.rng.
         * ------------------------------------------------------------------- */
        case FQ_STATE_BATTLE:
            switch (evt->id) {
                case FQ_EVT_COMBAT_ROUND_COMPLETE:
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
         * FQ_STATE_ONBOARDING
         *
         * BTN_A: cycle class index forward (mod FQ_CLASS_COUNT = 5).
         * BTN_B: confirm selection.
         *        - If onboarding_save_failed == 1: stay in ONBOARDING.
         *        - Else: create character via fq_character_create(), set state
         *          HOME. (The actual save to flash is handled in app_main.c
         *          via a post-dispatch hook — the FSM only tracks intent.)
         * ------------------------------------------------------------------- */
        case FQ_STATE_ONBOARDING:
            switch (evt->id) {
                case FQ_EVT_BTN_A_PRESS:
                    /* Cycle class forward: 0->1->2->3->4->0. */
                    ctx->onboarding_class_index =
                        (uint8_t)((ctx->onboarding_class_index + 1u)
                                  % (uint8_t)FQ_CLASS_COUNT);
                    break;

                case FQ_EVT_BTN_B_PRESS: {
                    /* If save previously failed, re-enter onboarding. */
                    if (ctx->onboarding_save_failed != 0u) {
                        /* Stay in ONBOARDING — save failure re-entry. */
                        break;
                    }

                    /* Create character with selected class and generated name. */
                    if (ctx->player != NULL) {
                        fq_prng_t name_rng;
                        fq_prng_init(&name_rng,
                            (uint32_t)(ctx->tick_count ^ 0xA5A5A5A5u));
                        char new_name[12];
                        memset(new_name, 0, sizeof(new_name));
                        fq_generate_name(&name_rng, new_name, sizeof(new_name));

                        fq_character_create(ctx->player,
                                            (fq_class_t)ctx->onboarding_class_index,
                                            ctx->tick_count, /* unique ID from tick */
                                            new_name);
                    }

                    go_home(ctx);
                    break;
                }

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
            break;
    }

    return GAME_OK;
}
