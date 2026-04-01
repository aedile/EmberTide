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
 * State transition table:
 *   BOOT            → TITLE         (automatic on fq_app_init)
 *   TITLE           → HOME          (BTN_A_PRESS)
 *   HOME            → INVENTORY     (BTN_A_PRESS)
 *   HOME            → BATTLE_SETUP  (BTN_B_PRESS)
 *   HOME            → TRAINING      (BTN_A_LONG)
 *   HOME            → STATS         (BTN_B_LONG)
 *   INVENTORY       → HOME          (BTN_B_PRESS)
 *   STATS           → HOME          (BTN_B_PRESS)
 *   TRAINING        → HOME          (BTN_B_PRESS)
 *   BATTLE_SETUP    → BATTLE        (BLE_CONNECTED; sets combat_active=1)
 *   BATTLE          → BATTLE_RESULT (COMBAT_ROUND_COMPLETE; clears combat_active)
 *   BATTLE_RESULT   → HOME          (BTN_A_PRESS)
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
 * fq_app_ctx_t — root application context.
 *
 * All game state is owned by separate static allocations in app_main.c and
 * referenced here via non-owning pointers (player, inventory).
 *
 * Constitution Priority 0: combat.rng MUST NOT be read/written by dispatch
 * unless state == FQ_STATE_BATTLE && combat_active == 1.
 * ---------------------------------------------------------------------------*/
typedef struct {
    fq_app_state_t  state;          /**< Current FSM state. */
    fq_event_bus_t  bus;            /**< Embedded event bus. */
    uint32_t        tick_count;     /**< Monotonic tick counter (incremented per TIMER_TICK). */
    /* Non-owning game state pointers — set at init, never freed by this module. */
    fq_character_t *player;         /**< Player character record. Not owned. */
    fq_inventory_t *inventory;      /**< Player inventory. Not owned. */
    /* Combat context — initialized only when entering FQ_STATE_BATTLE. */
    fq_combat_ctx_t combat;         /**< Full combat context (64 bytes). */
    uint8_t         combat_active;  /**< 1 when a battle is in progress, 0 otherwise. */
} fq_app_ctx_t;

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/**
 * fq_app_init() — Initialize the application context.
 *
 * Zeroes the combat context and event bus, wires player/inventory pointers,
 * and performs the automatic BOOT → TITLE transition.
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
 * @param ctx  Application context. Returns GAME_ERR_NULL_PTR if NULL.
 * @param evt  Event to process. Returns GAME_ERR_NULL_PTR if NULL.
 * @return     GAME_OK on success, GAME_ERR_NULL_PTR if ctx or evt is NULL.
 */
game_err_t fq_app_dispatch(fq_app_ctx_t      *ctx,
                            const fq_event_t  *evt);

/* ---------------------------------------------------------------------------
 * A4 (Architecture P11): fq_app_ctx_t layout invariant pinned at compile time.
 *
 * Layout (host x86-64 / Xtensa LP64-equivalent with 8-byte pointer alignment):
 *   [0..3]    fq_app_state_t  state          (4 bytes, enum-as-int)
 *   [4..135]  fq_event_bus_t  bus            (132 bytes)
 *   [136..139] uint32_t       tick_count     (4 bytes)
 *   [140..143] padding                       (4 bytes, pointer alignment)
 *   [144..151] fq_character_t *player        (8 bytes, pointer)
 *   [152..159] fq_inventory_t *inventory     (8 bytes, pointer)
 *   [160..223] fq_combat_ctx_t combat        (64 bytes)
 *   [224]      uint8_t         combat_active (1 byte)
 *   [225..231] padding                       (7 bytes, struct end alignment)
 *   total = 232 bytes
 *
 * If fq_combat_ctx_t, fq_event_bus_t, or pointer width changes, this assert
 * will fire. Update the layout comment and pinned value together.
 * ---------------------------------------------------------------------------*/
_Static_assert(sizeof(fq_app_ctx_t) == 232u,
    "fq_app_ctx_t layout changed — update A4 static assert and this comment");

#endif /* FIESTAQUEST_MAIN_APP_FSM_H */
