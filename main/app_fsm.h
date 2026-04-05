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
 * State transition table (Phase-19 home menu navigation + onboarding):
 *   BOOT            → TITLE         (automatic on fq_app_init)
 *   TITLE           → HOME          (BTN_A_PRESS or BTN_B_PRESS)
 *   HOME            → (cycles menu)  (BTN_B_PRESS: increments home_menu_index mod 4)
 *   HOME            → TRAINING      (BTN_A_PRESS when home_menu_index == 0)
 *   HOME            → BATTLE_SETUP  (BTN_A_PRESS when home_menu_index == 1)
 *   HOME            → INVENTORY     (BTN_A_PRESS when home_menu_index == 2)
 *   HOME            → STATS         (BTN_A_PRESS when home_menu_index == 3)
 *   INVENTORY       → HOME          (double-tap BTN_B; resets home_menu_index to 0)
 *   INVENTORY cursor: BTN_B = cycle cursor; BTN_A = equip toggle
 *   STATS           → HOME          (BTN_B_PRESS; resets home_menu_index to 0)
 *   TRAINING state=WAITING: BTN_A=cycle type, BTN_B=start game
 *   TRAINING state=ACTIVE: BTN_A=hit, BTN_B=exit; auto-tick per TIMER_TICK
 *   TRAINING state=DONE: BTN_B=exit to HOME
 *   TRAINING        → HOME          (BTN_B_PRESS in DONE or B during WAITING)
 *   BATTLE_SETUP    → BATTLE        (BLE_CONNECTED; sets combat_active=1; derives shared_seed)
 *   BATTLE          → BATTLE_RESULT (COMBAT_ROUND_COMPLETE; generates hash; clears combat_active)
 *   BATTLE_RESULT   → HOME          (BTN_A_PRESS; resets home_menu_index to 0)
 *   ONBOARDING: BTN_A=cycle class, BTN_B=confirm (→HOME on save success)
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
    FQ_STATE_ONBOARDING    = 11, /**< First-boot character creation. */
    FQ_STATE_COUNT         = 12  /**< Sentinel — number of valid states. */
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
 * Phase-19: home_menu_index added.
 *
 * Phase-19 interactive: onboarding_class_index, onboarding_save_failed,
 * inventory_cursor, inv_b_press_tick added.
 *
 * Phase-19 training: training_session embedded directly.
 *
 * Phase-20 audit fix (DC-1): last_combat_hash added.
 *
 * Phase-21: sfx_enabled added. 1 = SFX active (default on boot).
 *
 * Phase-22: music_enabled and music_vol added.
 *   - music_enabled: 1 = music active (default on boot); 0 = muted.
 *   - music_vol: volume [0, 255], default 200.
 *   Both fields occupy the _p21_pad[3] bytes at the end of the struct,
 *   consuming 2 of the 3 pad bytes. One pad byte remains for alignment.
 *   Struct size is UNCHANGED: 424 bytes (64-bit), 412 bytes (32-bit).
 *
 *   music_enabled is NOT serialized to the save format wire. On load from
 *   an old save (no music field), music_enabled defaults to 1 via fq_app_init.
 *   This preserves backward compatibility (Constitution Priority 0).
 * ---------------------------------------------------------------------------*/

#include "training_session.h"

typedef struct {
    fq_app_state_t  state;                 /**< Current FSM state. */
    fq_event_bus_t  bus;                   /**< Embedded event bus. */
    uint32_t        tick_count;            /**< Monotonic tick counter. */
    /* Non-owning game state pointers — set at init, never freed by this module. */
    fq_character_t *player;                /**< Player character record. Not owned. */
    fq_inventory_t *inventory;             /**< Player inventory. Not owned. */
    /* Combat context — initialized only when entering FQ_STATE_BATTLE. */
    fq_combat_ctx_t combat;                /**< Full combat context (64 bytes). */
    uint8_t         combat_active;         /**< 1 when a battle is in progress, 0 otherwise. */
    uint8_t         home_menu_index;       /**< Home menu cursor: 0=TRAIN,1=BATTLE,2=ITEMS,3=STATS. */
    /* Onboarding state */
    uint8_t         onboarding_class_index; /**< Selected class index [0, FQ_CLASS_COUNT-1]. */
    uint8_t         onboarding_save_failed; /**< 1 = last save attempt failed; re-enter onboarding. */
    /* Inventory state */
    uint8_t         inventory_cursor;      /**< Currently highlighted inventory slot. */
    uint8_t         inv_b_press_count;     /**< Double-tap B counter for inventory exit. */
    uint32_t        inv_b_last_tick;       /**< Tick count of last B press in INVENTORY. */
    /* Training session (embedded, 8 bytes) */
    fq_training_session_t training;        /**< Active training session state. */
    /* Phase-20: BLE combat orchestration fields. */
    uint32_t        shared_seed;           /**< Derived from nonce XOR after team sync. */
    uint32_t        battle_setup_start_tick; /**< tick_count when BATTLE_SETUP was entered. */
    uint16_t        xp_earned;             /**< XP awarded at end of battle (for result screen). */
    uint8_t         my_nonce[4];           /**< Local nonce for seed derivation. */
    uint8_t         battle_won;            /**< 1=won, 0=lost (set on COMBAT_ROUND_COMPLETE). */
    uint8_t         rounds_survived;       /**< Number of rounds the fight lasted. */
    /* Phase-20: opponent character snapshot received via team_sync. */
    fq_character_t  opponent;              /**< Opponent character (received from BLE peer). */
    /* Phase-20 audit fix (DC-1): hash of combat state at the last completed round. */
    uint32_t        last_combat_hash;      /**< CRC32 of combat state at last round boundary. */
    /* Phase-21: audio quiet mode flag. */
    uint8_t         sfx_enabled;           /**< 1 = SFX active; 0 = quiet mode. */
    /* Phase-22: music control fields.
     * These two fields consume the first 2 of the 3 original _p21_pad bytes.
     * One pad byte remains to preserve the struct size. */
    uint8_t         music_enabled;         /**< 1 = music active (default 1); 0 = muted. */
    uint8_t         music_vol;             /**< Music volume [0, 255]. Default 200. */
    uint8_t         _p22_pad[1];           /**< 1-byte pad to preserve struct alignment. */
} fq_app_ctx_t;

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/**
 * fq_app_init() — Initialize the application context.
 *
 * Zeroes the combat context and event bus, wires player/inventory pointers,
 * initializes home_menu_index to 0, sfx_enabled to 1, music_enabled to 1,
 * music_vol to 200, and performs the automatic BOOT → TITLE transition.
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
 * Phase-22 layout change:
 *   Replaced _p21_pad[3] (3 bytes) with:
 *     music_enabled (uint8_t) — 1 byte
 *     music_vol     (uint8_t) — 1 byte
 *     _p22_pad[1]   (uint8_t) — 1 byte
 *   Total bytes at that location: 3 bytes — unchanged.
 *   Struct size: 424 bytes (64-bit), 412 bytes (32-bit) — UNCHANGED.
 * ---------------------------------------------------------------------------*/
#if __SIZEOF_POINTER__ == 8
_Static_assert(sizeof(fq_app_ctx_t) == 424u,
    "fq_app_ctx_t layout changed (64-bit host)");
#elif __SIZEOF_POINTER__ == 4
_Static_assert(sizeof(fq_app_ctx_t) == 412u,
    "fq_app_ctx_t layout changed (32-bit target)");
#endif

#endif /* FIESTAQUEST_MAIN_APP_FSM_H */
