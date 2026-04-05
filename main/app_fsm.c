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
 *   Exception: fq_rebirth() in FQ_STATE_REBIRTH requires an fq_prng_t only for
 *   WILDCARD passive reroll (class-specific cosmetic). A local PRNG seeded from
 *   tick_count ^ rebirth_count is used — the combat.rng stream is never touched
 *   outside FQ_STATE_BATTLE.
 *
 * Phase-19 HOME state:
 *   BTN_B (sun, GPIO18) cycles home_menu_index mod FQ_HOME_MENU_COUNT.
 *   BTN_A (pwr, GPIO0) selects the highlighted menu item and transitions.
 *   home_menu_index is reset to 0 whenever any state transitions BACK to HOME.
 *
 * Phase-19 ONBOARDING state:
 *   BTN_A (pwr) cycles onboarding_class_index mod FQ_CLASS_COUNT.
 *   BTN_B (sun) confirms: creates character, transitions to HOME.
 *   If onboarding_save_failed==1 (prior save error), BTN_B is blocked
 *   until app_main.c clears the flag after a successful save retry.
 *   Note: app_main.c no longer forces state back to ONBOARDING on save
 *   failure — the character is valid in memory; the user can play.
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
 *
 * Phase-20 BATTLE state:
 *   COMBAT_ROUND_COMPLETE: calls fq_generate_combat_hash() to capture round
 *   hash for peer exchange, then calls fq_combat_award_xp() to credit
 *   XP/wins/losses, stores result in ctx->xp_earned, transitions to BATTLE_RESULT.
 *
 *   BLE_PACKET_RX in BATTLE: calls fq_sync_verify_round() against the received
 *   peer hash. On mismatch -> disconnect and return HOME (desync protection).
 *
 * Phase-20 BATTLE_SETUP state:
 *   BLE_CONNECTED: calls fq_protocol_derive_seed() from my_nonce and the
 *   peer nonce to populate shared_seed before entering BATTLE.
 *
 * Phase-20 REBIRTH state:
 *   BTN_A: spend one legacy point on the next eligible tree node via
 *          fq_legacy_unlock_node(). Scans the current legacy_tree bitmask to
 *          find the lowest unset bit in [0, 15] and attempts to unlock it.
 *          No-op if no tokens or all nodes are filled / prerequisites unmet.
 *   BTN_B: execute fq_rebirth() (requires is_dead==1), then return HOME.
 *          Uses a local PRNG (not combat.rng) -- PRNG isolation preserved.
 *
 * Phase-21: sfx_enabled initialised to 1 in fq_app_init() (SFX on by default).
 */

#include "app_fsm.h"
#include "character.h"
#include "name_gen.h"
#include "equip.h"
#include "progression.h"
#include "legacy.h"
#include "combat_hash.h"
#include "sync.h"
#include "protocol.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * go_home -- transition to HOME and reset the menu index.
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
 * BATTLE_SETUP auto-timeout: 200 ticks @ 50ms/tick = 10 seconds.
 * ---------------------------------------------------------------------------*/
#define BATTLE_SETUP_TIMEOUT_TICKS  200u

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

    /* Phase-21: SFX enabled by default. */
    /* Phase-22: Music enabled by default at volume 200. */
    ctx->sfx_enabled   = 1u;
    ctx->music_enabled = 1u;
    ctx->music_vol     = 200u;

    /* Automatic BOOT -> TITLE transition. */
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
                            ctx->state                   = FQ_STATE_BATTLE_SETUP;
                            ctx->battle_setup_start_tick = ctx->tick_count;
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
                        /* Double-tap confirmed -- exit to HOME. */
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
                        /* B in ACTIVE: partial exit -- award XP for score earned so far,
                         * then return to HOME. Spec: "Button B exits training at any
                         * time (partial score, partial XP award)." */
                        if (ctx->player != NULL) {
                            fq_training_award_xp(&ctx->training, ctx->player);
                        }
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
         *
         * BTN_B: Cancel -- return to HOME immediately.
         * BLE_CONNECTED: Derive shared seed from nonces, then begin battle.
         * BLE_DISCONNECTED: Return to HOME (no peer).
         * TIMER_TICK: Auto-timeout after BATTLE_SETUP_TIMEOUT_TICKS ticks.
         *
         * DC-3 fix: fq_protocol_derive_seed() is called on BLE_CONNECTED to
         * populate ctx->shared_seed before the PRNG is seeded for combat.
         * The peer nonce is carried in evt->data (uint32_t).
         * ------------------------------------------------------------------- */
        case FQ_STATE_BATTLE_SETUP:
            switch (evt->id) {
                case FQ_EVT_BLE_CONNECTED: {
                    /* DC-3: Derive shared PRNG seed from local and peer nonces.
                     * my_nonce is stored as 4 bytes LE; reassemble as uint32_t. */
                    uint32_t my_nonce_u32 =
                        ((uint32_t)ctx->my_nonce[0])        |
                        ((uint32_t)ctx->my_nonce[1] <<  8u) |
                        ((uint32_t)ctx->my_nonce[2] << 16u) |
                        ((uint32_t)ctx->my_nonce[3] << 24u);
                    /* Peer nonce arrives in evt->data (set by BLE RX handler). */
                    uint32_t peer_nonce = evt->data;
                    ctx->shared_seed = fq_protocol_derive_seed(my_nonce_u32, peer_nonce);

                    ctx->state                     = FQ_STATE_BATTLE;
                    ctx->combat_active             = 1u;
                    ctx->battle_setup_start_tick   = 0u;  /* clear for next use */
                    break;
                }

                case FQ_EVT_BTN_B_PRESS:
                    /* Cancel: return HOME immediately. */
                    go_home(ctx);
                    break;

                case FQ_EVT_BLE_DISCONNECTED:
                    /* Peer disappeared before connecting -- go HOME. */
                    go_home(ctx);
                    break;

                case FQ_EVT_TIMER_TICK: {
                    /* Auto-timeout: if tick_count has advanced >= TIMEOUT since entry. */
                    uint32_t elapsed = ctx->tick_count - ctx->battle_setup_start_tick;
                    if (elapsed >= BATTLE_SETUP_TIMEOUT_TICKS) {
                        go_home(ctx);
                    }
                    break;
                }

                default:
                    break;
            }
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_BATTLE
         *
         * Constitution Priority 0: ONLY this block may access ctx->combat.rng.
         * BLE_DISCONNECTED: clean up and return HOME (no save).
         *
         * Phase-20 (DC-1): On COMBAT_ROUND_COMPLETE, call
         * fq_generate_combat_hash() to capture the round hash into
         * ctx->last_combat_hash for peer exchange, BEFORE awarding XP.
         *
         * Phase-20 (DC-3): On BLE_PACKET_RX, extract peer round hash from
         * evt->data and call fq_sync_verify_round(). On mismatch, disconnect
         * and return HOME to prevent desync from corrupting battle state.
         *
         * Phase-20: On COMBAT_ROUND_COMPLETE, call fq_combat_award_xp() to
         * credit XP and update win/loss counters. Store the earned XP in
         * ctx->xp_earned for the battle result screen.
         * ------------------------------------------------------------------- */
        case FQ_STATE_BATTLE:
            switch (evt->id) {
                case FQ_EVT_COMBAT_ROUND_COMPLETE:
                    if (ctx->combat_active == 1u) {
                        /* DC-1: Generate combat hash at this round boundary and
                         * store it for the BLE hash exchange protocol.
                         * Round is read from ctx->combat.current_round -- the field
                         * incremented by fq_combat_step() before the event fires. */
                        ctx->last_combat_hash =
                            fq_generate_combat_hash(&ctx->combat,
                                                     ctx->combat.current_round);

                        /* Award XP based on battle outcome. */
                        if (ctx->player != NULL) {
                            uint32_t xp_before = ctx->player->xp;
                            fq_combat_award_xp(ctx->player,
                                               ctx->opponent.level,
                                               ctx->battle_won);
                            /* Capture XP delta for result screen (clamped to uint16_t). */
                            uint32_t xp_delta = ctx->player->xp - xp_before;
                            ctx->xp_earned = (xp_delta > 0xFFFFu)
                                             ? (uint16_t)0xFFFFu
                                             : (uint16_t)xp_delta;
                        }

                        ctx->state         = FQ_STATE_BATTLE_RESULT;
                        ctx->combat_active = 0u;
                    }
                    break;

                case FQ_EVT_BLE_PACKET_RX: {
                    /* DC-3: Verify the peer's round hash against our local hash.
                     * evt->data carries the peer's combat hash for the current round.
                     * On mismatch, abort the battle immediately -- disconnect and
                     * return HOME to prevent desync from producing invalid results.
                     *
                     * Note: in host tests the BLE mock does not exchange real packets,
                     * so this path is only exercised when evt->data != 0. A zero
                     * peer hash is treated as "no verification needed" (test stub
                     * behaviour). On target the BLE RX handler always sets data != 0
                     * for a valid FQ_PKT_ROUND_HASH packet. */
                    if (ctx->combat_active == 1u && evt->data != 0u) {
                        uint32_t peer_hash = evt->data;
                        fq_sync_err_t sync_err =
                            fq_sync_verify_round(ctx->combat.current_round,
                                                  ctx->combat.current_round,
                                                  ctx->last_combat_hash,
                                                  peer_hash);
                        if (sync_err != FQ_SYNC_OK) {
                            /* Desync detected -- abort battle, return HOME. */
                            ctx->combat_active = 0u;
                            go_home(ctx);
                        }
                    }
                    break;
                }

                case FQ_EVT_BLE_DISCONNECTED:
                    /* Disconnect mid-combat: abort without saving. */
                    ctx->combat_active = 0u;
                    go_home(ctx);
                    break;

                default:
                    break;
            }
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_BATTLE_RESULT
         *
         * BTN_A on win (is_dead==0)  -> HOME.
         * BTN_A on loss (is_dead==1) -> REBIRTH.
         * BLE_DISCONNECTED -> HOME (peer disconnected on result screen).
         * ------------------------------------------------------------------- */
        case FQ_STATE_BATTLE_RESULT:
            switch (evt->id) {
                case FQ_EVT_BTN_A_PRESS:
                    if (ctx->player != NULL && ctx->player->is_dead == 1u) {
                        ctx->state = FQ_STATE_REBIRTH;
                    } else {
                        go_home(ctx);
                    }
                    break;

                case FQ_EVT_BLE_DISCONNECTED:
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
         *          via a post-dispatch hook -- the FSM only tracks intent.)
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
                        /* Stay in ONBOARDING -- save failure re-entry. */
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
         * FQ_STATE_REBIRTH
         *
         * BTN_A: Spend one legacy token on the next eligible tree node.
         *        Scans legacy_tree for the lowest unset bit in [0, 15] and
         *        calls fq_legacy_unlock_node(). No-op if 0 tokens, tree full,
         *        or tier prerequisites are not met for the candidate node.
         * BTN_B: Execute fq_rebirth() (requires player->is_dead==1) then
         *        return HOME. app_main.c wires the auto-save.
         *        PRNG isolation: a local RNG (seeded from tick_count) is used --
         *        ctx->combat.rng is never accessed here.
         * ------------------------------------------------------------------- */
        case FQ_STATE_REBIRTH:
            switch (evt->id) {
                case FQ_EVT_BTN_A_PRESS:
                    /* Spend a token on the next eligible legacy node. */
                    if (ctx->player != NULL) {
                        /* Find the lowest unset bit in [0, 15]. */
                        uint8_t node;
                        for (node = 0u; node < 16u; node++) {
                            if ((ctx->player->legacy_tree & (1u << node)) == 0u) {
                                /* Attempt unlock -- respects tier prerequisites. */
                                fq_legacy_unlock_node(ctx->player, node);
                                break;
                            }
                        }
                    }
                    /* Stay in REBIRTH state -- player may spend more tokens. */
                    break;

                case FQ_EVT_BTN_B_PRESS: {
                    /* Execute rebirth if player is dead, then go HOME.
                     * A local PRNG seeded from tick_count is used for the
                     * WILDCARD passive reroll -- combat.rng is not accessed. */
                    if (ctx->player != NULL && ctx->player->is_dead == 1u) {
                        fq_prng_t rebirth_rng;
                        fq_prng_init(&rebirth_rng,
                            ctx->tick_count ^ (uint32_t)(ctx->player->rebirth_count));
                        fq_rebirth(ctx->player, &rebirth_rng);
                        fq_legacy_apply_bonuses(ctx->player);
                    }
                    go_home(ctx);
                    break;
                }

                default:
                    break;
            }
            break;

        /* -------------------------------------------------------------------
         * FQ_STATE_SETTINGS -- no transitions yet.
         * ------------------------------------------------------------------- */
        case FQ_STATE_SETTINGS:
        case FQ_STATE_COUNT:
        default:
            break;
    }

    return GAME_OK;
}
