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
 *   uint8_t  _pad[3]       (3)  offset 21  — explicit alignment pad
 * Total: 24 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint16_t wins;        /**< Total wins displayed on home screen. */
    uint16_t losses;      /**< Total losses displayed on home screen. */
    char     name[13];    /**< Display name: 12 chars + null terminator. */
    uint8_t  class_id;    /**< Character class (fq_class_t encoded as uint8_t). */
    uint8_t  level;       /**< Current character level. */
    uint8_t  hp_percent;  /**< HP bar fill: 0-100, 0 when hp_max==0. */
    uint8_t  sprite_base; /**< Base sprite index for class rendering. */
    uint8_t  _pad[3];     /**< Explicit alignment pad. */
} fq_vm_home_t;

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

#endif /* FIESTAQUEST_PRESENTATION_VIEW_MODELS_H */
