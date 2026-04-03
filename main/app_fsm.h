/**
 * app_fsm.h — FiestaQuest Application Layer: Root State Machine
 *
 * Defines the top-level application states and the dispatch function that
 * advances the FSM by one event.
 *
 * Architecture placement: main/ (application layer).
 *   - This is the orchestration layer — the ONLY module that may include
 *     both game/ headers (types.h, combat.h) AND the event bus.
 *   - hal_*.h is NEVER included here.
 *
 * Constitution Priority 0 — PRNG isolation contract:
 *   fq_app_dispatch() MUST NOT access ctx->combat.rng unless the current
 *   state is FQ_STATE_BATTLE AND ctx->combat_active == 1. This preserves
 *   the deterministic PRNG stream used by the combat engine.
 *
 * State transition table (Phase-19 home menu navigation):
 *   BOOT            → TITLE         (automatic on fq_app_init)
 *   TITLE           → HOME          (BTN_A_PRESS or BTN_B_PRESS)
 *   HOME            → (cycles menu)  (BTN_B_PRESS: increments home_menu_index mod 4)
 *   HOME            → TRAINING      (BTN_A_PRESS when home_menu_index == 0)
 *   HOME            → BATTLE_SETUP  (BTN_A_PRESS when home_menu_index == 1)
 *   HOME            → INVENTORY     (BTN_A_PRESS when home_menu_index == 2)
 *   HOME            → STATS         (BTN_A_PRESS when home_menu_index == 3)
 *   INVENTORY       → HOME          (BTN_B_PRESS; resets home_menu_index to 0)
 *   STATS           → HOME          (BTN_B_PRESS; resets home_menu_index to 0)
 *   TRAINING        → HOME          (BTN_B_PRESS; resets home_menu_index to 0)
 *   BATTLE_SETUP    → BATTLE        (BLE_CONNECTED; sets combat_active=1)
 *   BATTLE          → BATTLE_RESULT (COMBAT_ROUND_COMPLETE; clears combat_active)
 *   BATTLE_RESULT   → HOME          (BTN_A_PRESS; resets home_menu_index to 0)
 *
 * All other events in any state are silently ignored — GAME_OK is returned
 * and the state is unchanged.
 *
 * Host-compilable: no hal_*.h included.
 */

#ifndef FIESTAQUEST_MAIN_APP_FSM_H
#define FIESTAQUEST_MAIN_APP_FSM_H

#include <stdint.h>
#include "types.h"
#include "combat.h"
#include "event_bus.h"

/* ---------------------------------------------------------------------------
 * fq_app_state_t — application screen states.
 *
 * Values are pinned — must not be reordered. New states append before
 * FQ_STATE_COUNT.
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_STATE_BOOT          = 0,  /**< Power-on bootstrap (transient — exits on init). */
    FQ_STATE_TITLE         = 1,  /**< Title / splash screen. */
    FQ_STATE_HOME          = 2,  /**< Home screen (hub). */
    FQ_STATE_BATTLE_SETUP  = 3,  /**< BLE pairing / opponent discovery. */
    FQ_STATE_BATTLE        = 4,  /**< Active combat screen. */
    FQ_STATE_BATTLE_RESULT = 5,  /**< Post-battle results. */
    FQ_STATE_INVENTORY     = 6,  /**< Inventory management. */
    FQ_STATE_STATS         = 7,  /**< Character stats view. */
    FQ_STATE_TRAINING      = 8,  /**< Training mini-game. */
    FQ_STATE_REBIRTH       = 9,  /**< Rebirth / permadeath screen. */
    FQ_STATE_SETTINGS      = 10, /**< Settings / preferences. */
    FQ_STATE_COUNT         = 11  /**< Sentinel — number of valid states. */
} fq_app_state_t;

/* ---------------------------------------------------------------------------
 * Home menu indices (used with ctx->home_menu_index).
 * ---------------------------------------------------------------------------*/
#define FQ_HOME_MENU_TRAIN   0u  /**< TRAIN — navigates to FQ_STATE_TRAINING. */
#define FQ_HOME_MENU_BATTLE  1u  /**< BATTLE — navigates to FQ_STATE_BATTLE_SETUP. */
#define FQ_HOME_MENU_ITEMS   2u  /**< ITEMS — navigates to FQ_STATE_INVENTORY. */
#define FQ_HOME_MENU_STATS   3u  /**< STATS — navigates to FQ_STATE_STATS. */
#define FQ_HOME_MENU_COUNT   4u  /**< Total number of menu entries (wrap boundary). */

/* ---------------------------------------------------------------------------
 * fq_app_ctx_t — root application context.
 *
 * All game state is owned by separate static allocations in app_main.c and
 * referenced here via non-owning pointers (player, inventory).
 *
 * Constitution Priority 0: combat.rng MUST NOT be read/written by dispatch
 * unless state == FQ_STATE_BATTLE && combat_active == 1.
 *
 * Phase-19: home_menu_index added. Tracks the currently highlighted home
 * screen menu entry. Reset to 0 whenever the FSM enters FQ_STATE_HOME.
 * ---------------------------------------------------------------------------*/
typedef struct {
    fq_app_state_t  state;            /**< Current FSM state. */
    fq_event_bus_t  bus;              /**< Embedded event bus. */
    uint32_t        tick_count;       /**< Monotonic tick counter (incremented per TIMER_TICK). */
    /* Non-owning game state pointers — set at init, never freed by this module. */
    fq_character_t *player;           /**< Player character record. Not owned. */
    fq_inventory_t *inventory;        /**< Player inventory. Not owned. */
    /* Combat context — initialized only when entering FQ_STATE_BATTLE. */
    fq_combat_ctx_t combat;           /**< Full combat context (64 bytes). */
    uint8_t         combat_active;    /**< 1 when a battle is in progress, 0 otherwise. */
    uint8_t         home_menu_index;  /**< Home menu cursor: 0=TRAIN,1=BATTLE,2=ITEMS,3=STATS. */
} fq_app_ctx_t;

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/**
 * fq_app_init() — Initialize the application context.
 *
 * Zeroes the combat context and event bus, wires player/inventory pointers,
 * initializes home_menu_index to 0, and performs the automatic BOOT → TITLE
 * transition.
 *
 * @param ctx    Application context to initialize. Must not be NULL.
 * @param player Player character (non-owning). Must not be NULL.
 * @param inv    Player inventory (non-owning). Must not be NULL.
 * @return       GAME_OK on success, GAME_ERR_NULL_PTR if any pointer is NULL.
 */
game_err_t fq_app_init(fq_app_ctx_t  *ctx,
                        fq_character_t *player,
                        fq_inventory_t *inv);

/**
 * fq_app_dispatch() — Process one event and advance the FSM.
 *
 * Applies the event to the current state, performing any state transition
 * defined in the table above. Unknown events are silently ignored.
 *
 * Constitution Priority 0: MUST NOT touch ctx->combat.rng unless
 * ctx->state == FQ_STATE_BATTLE && ctx->combat_active == 1.
 *
 * Phase-19 HOME state behaviour:
 *   BTN_B_PRESS: increments home_menu_index modulo FQ_HOME_MENU_COUNT (wraps 3→0).
 *   BTN_A_PRESS: transitions to the state for the current home_menu_index.
 *   Any transition away from HOME that later returns to HOME resets
 *   home_menu_index to 0.
 *
 * @param ctx  Application context. Returns GAME_ERR_NULL_PTR if NULL.
 * @param evt  Event to process. Returns GAME_ERR_NULL_PTR if NULL.
 * @return     GAME_OK on success, GAME_ERR_NULL_PTR if ctx or evt is NULL.
 */
game_err_t fq_app_dispatch(fq_app_ctx_t      *ctx,
                            const fq_event_t  *evt);

/* ---------------------------------------------------------------------------
 * A4 (Architecture P11): fq_app_ctx_t layout invariant pinned at compile time.
 *
 * Phase-19: home_menu_index (uint8_t) added after combat_active (uint8_t).
 * Both bytes pack into the trailing padding of the struct — total size is
 * unchanged on both host (64-bit) and target (32-bit).
 *
 * Layout varies by pointer width:
 *   Host (x86-64, 8-byte pointers): 232 bytes
 *   Target (Xtensa ESP32-S3, 4-byte pointers): 216 bytes
 *
 * The struct contains two pointers (player, inventory) whose size differs
 * between host and target. Both sizes are pinned below.
 * ---------------------------------------------------------------------------*/
#if __SIZEOF_POINTER__ == 8
_Static_assert(sizeof(fq_app_ctx_t) == 232u,
    "fq_app_ctx_t layout changed (64-bit host)");
#elif __SIZEOF_POINTER__ == 4
_Static_assert(sizeof(fq_app_ctx_t) == 216u,
    "fq_app_ctx_t layout changed (32-bit target)");
#endif

#endif /* FIESTAQUEST_MAIN_APP_FSM_H */
