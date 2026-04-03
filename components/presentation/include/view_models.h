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
 * Carries all pre-computed display values for the home/dashboard screen.
 * No game/ types referenced here — plain integers and strings only.
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
 * Carries the minimum data needed to render the idle screen:
 * a 3x-scaled character sprite centered on screen, the game title,
 * and the character name + level at the bottom.
 *
 * Architecture: idle is a render-layer OVERLAY, NOT an FSM state.
 * app_main.c maintains an s_idle_active flag. When set, fq_render_idle()
 * is called instead of the current state's renderer. The FSM state is
 * preserved. Any button press clears s_idle_active.
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
 * fq_vm_inventory_t — Inventory grid screen view model.
 *
 * Carries up to 32 pre-resolved item display names and rarities.
 * Cursor and scroll state are held here (no game interaction during render).
 *
 * Field layout:
 *   char    item_names[32][16]  (512)  offset 0
 *   uint8_t item_rarities[32]    (32)  offset 512
 *   uint8_t item_count           (1)   offset 544
 *   uint8_t cursor_index         (1)   offset 545
 *   uint8_t scroll_offset        (1)   offset 546
 *   uint8_t equipped_count       (1)   offset 547
 * Total: 548 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    char    item_names[32][16];  /**< Display name per slot (15 chars + null). */
    uint8_t item_rarities[32];   /**< Rarity per slot (fq_rarity_t as uint8_t). */
    uint8_t item_count;          /**< Number of items in inventory (0-32). */
    uint8_t cursor_index;        /**< Currently selected slot (0-based). */
    uint8_t scroll_offset;       /**< First visible row (for scroll). */
    uint8_t equipped_count;      /**< Number of equipped items. */
} fq_vm_inventory_t;

/* ---------------------------------------------------------------------------
 * fq_vm_stats_t — Character stats screen view model.
 *
 * Carries level, four core stats, XP progress, and rebirth count.
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
 * Carries pre-computed display values for the split-screen combat view.
 * No game/ types referenced — plain integers and strings only.
 *
 * Naming convention:
 *   f1 = player fighter (bottom half of screen)
 *   f2 = enemy fighter  (top half of screen)
 *
 * HP is stored as int16_t to match fq_combat_fighter_t.hp. Negative HP
 * is valid (fighter KO'd past zero) and renders as a 0-width bar.
 *
 * action_text[0] == '\0' means no banner overlay.
 * action_text is bounded at 32 bytes; use strnlen(..., 31) for safe length.
 *
 * Field layout (verified with _Static_assert below):
 *   char    f1_name[13]     (13)  offset 0
 *   char    f2_name[13]     (13)  offset 13
 *   int16_t f1_hp            (2)  offset 26  — even, no compiler pad needed
 *   int16_t f1_hp_max        (2)  offset 28
 *   int16_t f2_hp            (2)  offset 30
 *   int16_t f2_hp_max        (2)  offset 32
 *   uint8_t round            (1)  offset 34  — wait, see note
 *   uint8_t f1_class_id      (1)  offset 35  (see note on actual offsets below)
 *   uint8_t f2_class_id      (1)  offset 36
 *   char    action_text[32] (32)  offset 37
 *   uint8_t finished         (1)  offset 69
 *   uint8_t winner           (1)  offset 70
 *   uint8_t _pad[1]          (1)  offset 71  — explicit trailing pad
 * Total: 72 bytes.  Verified by _Static_assert.
 * ---------------------------------------------------------------------------*/
typedef struct {
    char    f1_name[13];      /**< Player fighter name: 12 chars + null. */
    char    f2_name[13];      /**< Enemy fighter name: 12 chars + null. */
    int16_t f1_hp;            /**< Player current HP (may be negative = KO). */
    int16_t f1_hp_max;        /**< Player max HP. 0 → bar width = 0. */
    int16_t f2_hp;            /**< Enemy current HP. */
    int16_t f2_hp_max;        /**< Enemy max HP. 0 → bar width = 0. */
    uint8_t round;            /**< Current round number (1-12). */
    uint8_t f1_class_id;      /**< Player class (fq_class_t as uint8_t). */
    uint8_t f2_class_id;      /**< Enemy class (fq_class_t as uint8_t). */
    char    action_text[32];  /**< Banner text. Empty string = no banner. */
    uint8_t finished;         /**< 1 = combat concluded this render. */
    uint8_t winner;           /**< 0=none, 1=f1 won, 2=f2 won. */
    uint8_t _pad[1];          /**< Explicit trailing pad — makes size predictable. */
} fq_vm_combat_t;

_Static_assert(sizeof(fq_vm_combat_t) == 72u,
               "fq_vm_combat_t size changed — update layout comment and this assert");

/* ---------------------------------------------------------------------------
 * fq_vm_training_t — Training mini-game screen view model.
 *
 * Carries pre-computed display values for the training screen.
 *
 * state:
 *   0 = waiting (pre-game prompt)
 *   1 = active  (mini-game running)
 *   2 = done    (result shown)
 * ---------------------------------------------------------------------------*/
typedef struct {
    char    game_name[16]; /**< Mini-game name: "Speed", "Power", "Intel". */
    uint8_t score;         /**< Mini-game score: 0-100. */
    uint8_t difficulty;    /**< Difficulty level 0-10. */
    uint8_t state;         /**< 0=waiting, 1=active, 2=done. */
} fq_vm_training_t;

#endif /* FIESTAQUEST_PRESENTATION_VIEW_MODELS_H */
