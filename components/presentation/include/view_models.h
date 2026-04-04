/**
 * view_models.h — FiestaQuest Presentation Layer: Screen View Models
 *
 * All screen-specific view model structs live here. View models are pure
 * data carriers constructed by the application layer from game state. Screen
 * renderers receive read-only view model pointers — they NEVER include
 * game/types.h directly.
 *
 * Architecture constraint: presentation/ MUST NOT include hal_*.h or
 * game/ headers (other than through the application layer constructor).
 *
 * HOST-COMPILABLE: this file compiles on the host for visual test harness.
 *
 * Phase-8 additions: fq_vm_home_t, fq_vm_inventory_t, fq_vm_stats_t.
 * Phase-9 additions: fq_vm_combat_t, fq_vm_training_t.
 * Phase-19 additions: menu_index field added to fq_vm_home_t.
 * Phase-19.5 additions:
 *   - anim_frame field added to fq_vm_home_t (replaces one _pad byte).
 *     _Static_assert pins fq_vm_home_t at 24 bytes.
 *   - fq_vm_idle_t added for idle screensaver screen.
 * Phase-19 Interactive additions:
 *   - fq_vm_onboarding_t added (new first-boot character creation screen).
 *   - fq_vm_training_t updated: target_pos, targets_done, game_type added.
 *     _Static_assert added for fq_vm_training_t.
 *   - fq_vm_inventory_t gains item_equipped[32] flags.
 *     _Static_assert updated from 548 to 580 bytes.
 */

#ifndef FIESTAQUEST_PRESENTATION_VIEW_MODELS_H
#define FIESTAQUEST_PRESENTATION_VIEW_MODELS_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Generic screen view model placeholder (Phase 1, retained for compatibility).
 * ---------------------------------------------------------------------------*/

/** Generic screen view model placeholder. */
typedef struct {
    uint8_t screen_id; /**< Which screen to render. */
} screen_view_model_t;

/* ---------------------------------------------------------------------------
 * fq_vm_home_t — Home screen view model.
 *
 * Field layout (no hidden padding on 32-bit aligned targets):
 *   uint16_t wins          (2)  offset 0
 *   uint16_t losses        (2)  offset 2
 *   char     name[13]     (13)  offset 4   — 12 chars + null terminator
 *   uint8_t  class_id      (1)  offset 17
 *   uint8_t  level         (1)  offset 18
 *   uint8_t  hp_percent    (1)  offset 19  — 0-100, pre-computed
 *   uint8_t  sprite_base   (1)  offset 20
 *   uint8_t  menu_index    (1)  offset 21  — 0=TRAIN, 1=BATTLE, 2=ITEMS, 3=STATS
 *   uint8_t  anim_frame    (1)  offset 22  — walk cycle frame index 0-7
 *   uint8_t  _pad[1]       (1)  offset 23  — explicit alignment pad
 * Total: 24 bytes.  Pinned by _Static_assert below.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint16_t wins;        /**< Total wins displayed on home screen. */
    uint16_t losses;      /**< Total losses displayed on home screen. */
    char     name[13];    /**< Display name: 12 chars + null terminator. */
    uint8_t  class_id;    /**< Character class (fq_class_t encoded as uint8_t). */
    uint8_t  level;       /**< Current character level. */
    uint8_t  hp_percent;  /**< HP bar fill: 0-100, 0 when hp_max==0. */
    uint8_t  sprite_base; /**< Base sprite index for class rendering. */
    uint8_t  menu_index;  /**< Currently highlighted menu item: 0=TRAIN, 1=BATTLE, 2=ITEMS, 3=STATS. */
    uint8_t  anim_frame;  /**< Walk cycle animation frame index 0-7. Application layer sets this. */
    uint8_t  _pad[1];     /**< Explicit alignment pad. */
} fq_vm_home_t;

_Static_assert(sizeof(fq_vm_home_t) == 24u,
               "fq_vm_home_t size changed — update layout comment and this assert");

/* ---------------------------------------------------------------------------
 * fq_vm_idle_t — Idle screensaver screen view model.
 *
 * Field layout:
 *   uint8_t  sprite_base   (1)  offset 0
 *   char     name[13]     (13)  offset 1   — 12 chars + null terminator
 *   uint8_t  level         (1)  offset 14
 *   uint8_t  _pad[1]       (1)  offset 15  — explicit alignment pad
 * Total: 16 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint8_t  sprite_base; /**< Base sprite index for 3x-scaled rendering. */
    char     name[13];    /**< Character display name: 12 chars + null. */
    uint8_t  level;       /**< Current character level shown at bottom. */
    uint8_t  _pad[1];     /**< Explicit alignment pad. */
} fq_vm_idle_t;

_Static_assert(sizeof(fq_vm_idle_t) == 16u,
               "fq_vm_idle_t size changed — update layout comment and this assert");

/* ---------------------------------------------------------------------------
 * fq_vm_onboarding_t — Character creation / onboarding screen view model.
 *
 * Carries all pre-computed display values for the first-boot class selection
 * carousel. Built by fq_vm_build_onboarding() in vm_builder.c.
 *
 * Field layout:
 *   char    class_name[16]  (16)  offset 0  — class display name + null
 *   uint8_t class_index      (1)  offset 16 — 0..FQ_CLASS_COUNT-1
 *   uint8_t sprite_base      (1)  offset 17 — base sprite for selected class
 *   uint8_t strength         (1)  offset 18 — class base strength stat
 *   uint8_t speed            (1)  offset 19 — class base speed stat
 *   uint8_t precision        (1)  offset 20 — class base precision stat
 *   uint8_t intelligence     (1)  offset 21 — class base intelligence stat
 *   uint8_t _pad[2]          (2)  offset 22 — explicit alignment pad
 * Total: 24 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    char    class_name[16];  /**< Human-readable class name (e.g. "Bruiser"). */
    uint8_t class_index;     /**< Currently selected class index [0..4]. */
    uint8_t sprite_base;     /**< Base sprite index for the selected class. */
    uint8_t strength;        /**< Base strength stat for this class. */
    uint8_t speed;           /**< Base speed stat for this class. */
    uint8_t precision;       /**< Base precision stat for this class. */
    uint8_t intelligence;    /**< Base intelligence stat for this class. */
    uint8_t _pad[2];         /**< Explicit alignment pad. */
} fq_vm_onboarding_t;

_Static_assert(sizeof(fq_vm_onboarding_t) == 24u,
               "fq_vm_onboarding_t size changed — update layout comment and this assert");

/* ---------------------------------------------------------------------------
 * fq_vm_inventory_t — Inventory grid screen view model.
 *
 * Carries up to 32 pre-resolved item display names, rarities, and equipped
 * status flags.
 *
 * Field layout:
 *   char    item_names[32][16]  (512)  offset 0
 *   uint8_t item_rarities[32]    (32)  offset 512
 *   uint8_t item_equipped[32]    (32)  offset 544  — 1=equipped, 0=not
 *   uint8_t item_count           (1)   offset 576
 *   uint8_t cursor_index         (1)   offset 577
 *   uint8_t scroll_offset        (1)   offset 578
 *   uint8_t equipped_count       (1)   offset 579  — max slot limit for display
 * Total: 580 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    char    item_names[32][16];  /**< Display name per slot (15 chars + null). */
    uint8_t item_rarities[32];   /**< Rarity per slot (fq_rarity_t as uint8_t). */
    uint8_t item_equipped[32];   /**< Equipped flag per slot: 1=equipped, 0=not. */
    uint8_t item_count;          /**< Number of items in inventory (0-32). */
    uint8_t cursor_index;        /**< Currently selected slot (0-based, clamped). */
    uint8_t scroll_offset;       /**< First visible row (for scroll). */
    uint8_t equipped_count;      /**< Max slot limit (for display, e.g. "2/4"). */
} fq_vm_inventory_t;

_Static_assert(sizeof(fq_vm_inventory_t) == 580u,
               "fq_vm_inventory_t size changed — update layout comment and this assert");

/* ---------------------------------------------------------------------------
 * fq_vm_stats_t — Character stats screen view model.
 *
 * Field layout (no hidden padding):
 *   uint32_t xp              (4)  offset 0
 *   uint32_t xp_to_next      (4)  offset 4
 *   uint16_t hp_max          (2)  offset 8
 *   char     name[13]       (13)  offset 10  — 12 chars + null terminator
 *   uint8_t  level           (1)  offset 23
 *   uint8_t  strength        (1)  offset 24
 *   uint8_t  speed           (1)  offset 25
 *   uint8_t  precision       (1)  offset 26
 *   uint8_t  intelligence    (1)  offset 27
 *   uint8_t  rebirth_count   (1)  offset 28
 *   uint8_t  _pad[3]         (3)  offset 29  — explicit alignment pad
 * Total: 32 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint32_t xp;           /**< Lifetime XP earned. */
    uint32_t xp_to_next;   /**< XP required to reach next level. */
    uint16_t hp_max;       /**< Maximum HP. */
    char     name[13];     /**< Display name: 12 chars + null terminator. */
    uint8_t  level;        /**< Current level. */
    uint8_t  strength;     /**< Raw strength stat. */
    uint8_t  speed;        /**< Raw speed stat. */
    uint8_t  precision;    /**< Raw precision stat. */
    uint8_t  intelligence; /**< Raw intelligence stat. */
    uint8_t  rebirth_count;/**< Total rebirth count. */
    uint8_t  _pad[3];      /**< Explicit alignment pad. */
} fq_vm_stats_t;

/* ---------------------------------------------------------------------------
 * fq_vm_combat_t — Combat HUD screen view model.
 *
 * Field layout (verified with _Static_assert below):
 *   char    f1_name[13]     (13)  offset 0
 *   char    f2_name[13]     (13)  offset 13
 *   int16_t f1_hp            (2)  offset 26
 *   int16_t f1_hp_max        (2)  offset 28
 *   int16_t f2_hp            (2)  offset 30
 *   int16_t f2_hp_max        (2)  offset 32
 *   uint8_t round            (1)  offset 34
 *   uint8_t f1_class_id      (1)  offset 35
 *   uint8_t f2_class_id      (1)  offset 36
 *   char    action_text[32] (32)  offset 37
 *   uint8_t finished         (1)  offset 69
 *   uint8_t winner           (1)  offset 70
 *   uint8_t _pad[1]          (1)  offset 71
 * Total: 72 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    char    f1_name[13];      /**< Player fighter name: 12 chars + null. */
    char    f2_name[13];      /**< Enemy fighter name: 12 chars + null. */
    int16_t f1_hp;            /**< Player current HP (may be negative = KO). */
    int16_t f1_hp_max;        /**< Player max HP. 0 → bar width = 0. */
    int16_t f2_hp;            /**< Enemy current HP. */
    int16_t f2_hp_max;        /**< Enemy max HP. */
    uint8_t round;            /**< Current round number (1-12). */
    uint8_t f1_class_id;      /**< Player class (fq_class_t as uint8_t). */
    uint8_t f2_class_id;      /**< Enemy class (fq_class_t as uint8_t). */
    char    action_text[32];  /**< Banner text. Empty string = no banner. */
    uint8_t finished;         /**< 1 = combat concluded this render. */
    uint8_t winner;           /**< 0=none, 1=f1 won, 2=f2 won. */
    uint8_t _pad[1];          /**< Explicit trailing pad. */
} fq_vm_combat_t;

_Static_assert(sizeof(fq_vm_combat_t) == 72u,
               "fq_vm_combat_t size changed — update layout comment and this assert");

/* ---------------------------------------------------------------------------
 * fq_vm_training_t — Training mini-game screen view model.
 *
 * Phase-19 update: added target_pos, targets_done, game_type fields.
 *
 * state:
 *   0 = waiting (pre-game type selection)
 *   1 = active  (mini-game running)
 *   2 = done    (result shown)
 *
 * Field layout:
 *   char    game_name[16]   (16)  offset 0
 *   uint8_t score            (1)  offset 16  — 0-100
 *   uint8_t difficulty       (1)  offset 17  — 0-10
 *   uint8_t state            (1)  offset 18  — 0=waiting, 1=active, 2=done
 *   uint8_t target_pos       (1)  offset 19  — 0-100 (target position on bar)
 *   uint8_t targets_done     (1)  offset 20  — 0-5
 *   uint8_t game_type        (1)  offset 21  — 0=Speed, 1=Power, 2=Intel
 *   uint8_t _pad[2]          (2)  offset 22  — explicit alignment pad
 * Total: 24 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    char    game_name[16]; /**< Mini-game name: "Speed", "Power", "Intel". */
    uint8_t score;         /**< Mini-game score: 0-100. */
    uint8_t difficulty;    /**< Difficulty level 0-10. */
    uint8_t state;         /**< 0=waiting, 1=active, 2=done. */
    uint8_t target_pos;    /**< Target position on bar: 0-100. */
    uint8_t targets_done;  /**< Number of targets completed: 0-5. */
    uint8_t game_type;     /**< Game type: 0=Speed, 1=Power, 2=Intel. */
    uint8_t _pad[2];       /**< Explicit alignment pad. */
} fq_vm_training_t;

_Static_assert(sizeof(fq_vm_training_t) == 24u,
               "fq_vm_training_t size changed — update layout comment and this assert");


/* ---------------------------------------------------------------------------
 * fq_vm_battle_result_t — Battle result screen view model.
 *
 * Phase-20 addition.
 *
 * Field layout (no hidden padding — uint16_t first to avoid alignment gap):
 *   uint16_t xp_earned          (2)  offset 0   — XP gained this fight
 *   char     winner_name[13]   (13)  offset 2   — 12 chars + null terminator
 *   uint8_t  you_won            (1)  offset 15  — 1=win, 0=loss
 *   uint8_t  rounds_survived    (1)  offset 16  — number of rounds completed
 *   uint8_t  player_sprite_base (1)  offset 17  — player sprite index
 *   uint8_t  is_dead            (1)  offset 18  — 1 = player must rebirth
 *   uint8_t  _pad[1]            (1)  offset 19  — explicit alignment pad
 * Total: 20 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint16_t xp_earned;          /**< XP awarded (0 on loss). */
    char     winner_name[13];    /**< Name of the winner (12 chars + null). */
    uint8_t  you_won;            /**< 1 = local player won, 0 = lost. */
    uint8_t  rounds_survived;    /**< Number of rounds the fight lasted. */
    uint8_t  player_sprite_base; /**< Player sprite base index for result display. */
    uint8_t  is_dead;            /**< 1 = player is dead, BTN_A routes to REBIRTH. */
    uint8_t  _pad[1];            /**< Explicit alignment pad. */
} fq_vm_battle_result_t;

_Static_assert(sizeof(fq_vm_battle_result_t) == 20u,
               "fq_vm_battle_result_t size changed — update layout comment and this assert");

/* ---------------------------------------------------------------------------
 * fq_vm_rebirth_t — Rebirth screen view model.
 *
 * Phase-20 addition.
 *
 * Field layout:
 *   uint32_t legacy_tree        (4)  offset 0   — 32-bit bitmask of unlocked nodes
 *   uint8_t  old_stats[4]       (4)  offset 4   — [STR,SPD,PRC,INT] before rebirth
 *   uint8_t  new_stats[4]       (4)  offset 8   — [STR,SPD,PRC,INT] after rebirth
 *   uint8_t  old_level          (1)  offset 12
 *   uint8_t  new_level          (1)  offset 13
 *   uint8_t  tokens_earned      (1)  offset 14
 *   uint8_t  tokens_available   (1)  offset 15
 *   uint8_t  next_node          (1)  offset 16  — index of next unset bit (0-31; 32=full)
 *   uint8_t  _pad[3]            (3)  offset 17  — explicit alignment pad
 * Total: 20 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint32_t legacy_tree;        /**< Current legacy tree bitmask. */
    uint8_t  old_stats[4];       /**< Pre-rebirth stats: [STR, SPD, PRC, INT]. */
    uint8_t  new_stats[4];       /**< Post-rebirth stats: [STR, SPD, PRC, INT]. */
    uint8_t  old_level;          /**< Character level before rebirth. */
    uint8_t  new_level;          /**< Character level after rebirth (always 1). */
    uint8_t  tokens_earned;      /**< Tokens earned this rebirth. */
    uint8_t  tokens_available;   /**< Total unspent tokens. */
    uint8_t  next_node;          /**< Index of the next available legacy node (32=full). */
    uint8_t  _pad[3];            /**< Explicit alignment pad. */
} fq_vm_rebirth_t;

_Static_assert(sizeof(fq_vm_rebirth_t) == 20u,
               "fq_vm_rebirth_t size changed — update layout comment and this assert");

#endif /* FIESTAQUEST_PRESENTATION_VIEW_MODELS_H */
