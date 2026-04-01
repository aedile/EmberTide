/**
 * types.h — FiestaQuest Game Engine: Shared Value Types
 *
 * Frozen neutral value objects consumed by multiple modules.
 * No business logic lives here — pure data carriers only.
 *
 * Architecture constraint: this header is the ONLY game header that
 * presentation/ may reference (via view_models.h adapters, never directly).
 *
 * Dependency rule: this file MUST NOT include any hal_*.h, presentation/,
 * or connectivity/ headers.
 *
 * Phase-3 additions: fq_class_t, fq_trigger_t, fq_condition_type_t,
 * fq_effect_type_t, fq_target_t, fq_rarity_t, fq_rival_entry_t,
 * fq_condition_t, fq_effect_t, fq_item_def_t, fq_character_t,
 * fq_inventory_t.
 *
 * Struct field order: uint32s first, then uint16s, then uint8s/chars.
 * This minimises padding on 32-bit aligned architectures (ESP32-S3).
 *
 * Verified sizes (host, AppleClang, x86-64; must also hold on ESP32-S3 Xtensa):
 *   sizeof(fq_rival_entry_t) = 12
 *   sizeof(fq_character_t)   = 156
 *   sizeof(fq_inventory_t)   = 66
 */

#ifndef FIESTAQUEST_GAME_TYPES_H
#define FIESTAQUEST_GAME_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * Error codes returned by all game module functions.
 * ---------------------------------------------------------------------------*/
typedef enum {
    GAME_OK            = 0,
    GAME_ERR_NULL_PTR  = 1,
    GAME_ERR_OVERFLOW  = 2,
    GAME_ERR_UNDERFLOW = 3,
    GAME_ERR_DIV_ZERO  = 4,
    GAME_ERR_INVALID   = 5
} game_err_t;

/* ---------------------------------------------------------------------------
 * Character class.
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_CLASS_BRUISER   = 0,
    FQ_CLASS_TRICKSTER = 1,
    FQ_CLASS_HEX       = 2,
    FQ_CLASS_WARDEN    = 3,
    FQ_CLASS_WILDCARD  = 4,
    FQ_CLASS_COUNT     = 5
} fq_class_t;

/* ---------------------------------------------------------------------------
 * Item trigger types — WHEN does this item fire?
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_TRIGGER_ON_ATTACK      = 0,
    FQ_TRIGGER_ON_DEFEND      = 1,
    FQ_TRIGGER_ON_ROUND_START = 2,
    FQ_TRIGGER_ON_ROUND_END   = 3,
    FQ_TRIGGER_ON_DODGE       = 4,
    FQ_TRIGGER_ON_CRIT        = 5,
    FQ_TRIGGER_ON_LOW_HP      = 6,
    FQ_TRIGGER_ON_KILL        = 7,
    FQ_TRIGGER_ON_DEATH       = 8,
    FQ_TRIGGER_PASSIVE        = 9,
    FQ_TRIGGER_COUNT          = 10
} fq_trigger_t;

/* ---------------------------------------------------------------------------
 * Item condition types — IF what is true?
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_COND_NONE         = 0,
    FQ_COND_HP_BELOW     = 1,
    FQ_COND_HP_ABOVE     = 2,
    FQ_COND_STREAK       = 3,
    FQ_COND_ROUND_NUMBER = 4,
    FQ_COND_COUNT        = 5
} fq_condition_type_t;

/* ---------------------------------------------------------------------------
 * Item effect types — THEN what happens?
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_EFFECT_DAMAGE_ADD  = 0,
    FQ_EFFECT_DAMAGE_MULT = 1,
    FQ_EFFECT_HEAL        = 2,
    FQ_EFFECT_DODGE_BONUS = 3,
    FQ_EFFECT_REROLL      = 4,
    FQ_EFFECT_COUNT       = 5
} fq_effect_type_t;

/* ---------------------------------------------------------------------------
 * Effect target.
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_TARGET_SELF     = 0,
    FQ_TARGET_OPPONENT = 1,
    FQ_TARGET_BOTH     = 2,
    FQ_TARGET_COUNT    = 3
} fq_target_t;

/* ---------------------------------------------------------------------------
 * Item rarity.
 * ---------------------------------------------------------------------------*/
typedef enum {
    FQ_RARITY_COMMON    = 0,
    FQ_RARITY_UNCOMMON  = 1,
    FQ_RARITY_RARE      = 2,
    FQ_RARITY_LEGENDARY = 3,
    FQ_RARITY_COUNT     = 4
} fq_rarity_t;

/* ---------------------------------------------------------------------------
 * fq_rival_entry_t — one entry in the rival log.
 *
 * Field order minimises padding:
 *   uint32_t  opponent_id    (4)  offset 0
 *   uint32_t  last_fight_ts  (4)  offset 4
 *   uint8_t   encounters     (1)  offset 8
 *   uint8_t   wins           (1)  offset 9
 *   uint8_t   is_nemesis     (1)  offset 10
 *   uint8_t   _pad           (1)  offset 11  — explicit pad, zero on serialize
 * Total: 12 bytes, no compiler-inserted hidden padding.
 *
 * uint8_t is used for is_nemesis (not bool) to guarantee deterministic
 * binary representation during serialization.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint32_t opponent_id;    /**< Unique opponent character ID. */
    uint32_t last_fight_ts;  /**< Timestamp of last fight (seconds since epoch). */
    uint8_t  encounters;     /**< Total number of fights against this opponent. */
    uint8_t  wins;           /**< Wins against this opponent. */
    uint8_t  is_nemesis;     /**< 1 if this is the active nemesis, 0 otherwise. */
    uint8_t  _pad;           /**< Explicit padding — written as 0 during serialization. */
} fq_rival_entry_t;

_Static_assert(sizeof(fq_rival_entry_t) == 12u,
    "fq_rival_entry_t must be exactly 12 bytes");

/* ---------------------------------------------------------------------------
 * fq_condition_t — item trigger condition.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint8_t type;       /**< fq_condition_type_t cast to uint8_t. */
    uint8_t threshold;  /**< Numeric threshold for the condition. */
} fq_condition_t;

_Static_assert(sizeof(fq_condition_t) == 2u,
    "fq_condition_t must be exactly 2 bytes");

/* ---------------------------------------------------------------------------
 * fq_effect_t — item effect.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint8_t type;    /**< fq_effect_type_t cast to uint8_t. */
    int8_t  value;   /**< Signed effect magnitude. */
    uint8_t target;  /**< fq_target_t cast to uint8_t. */
} fq_effect_t;

_Static_assert(sizeof(fq_effect_t) == 3u,
    "fq_effect_t must be exactly 3 bytes");

/* ---------------------------------------------------------------------------
 * fq_item_def_t — full item definition (OTA content, not persisted per-save).
 *
 * Layout (no hidden padding):
 *   uint16_t  id           (2)  offset 0
 *   uint8_t   rarity       (1)  offset 2
 *   uint8_t   trigger      (1)  offset 3
 *   fq_condition_t(2)           offset 4
 *   fq_effect_t(3)              offset 6
 *   uint8_t   _pad         (1)  offset 9
 *   char      name[16]    (16)  offset 10
 *   char      flavor_text[32] (32) offset 26
 * Total: 58 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint16_t       id;              /**< Item ID, 0-65535. */
    uint8_t        rarity;          /**< fq_rarity_t. */
    uint8_t        trigger;         /**< fq_trigger_t. */
    fq_condition_t condition;       /**< Trigger condition. */
    fq_effect_t    effect;          /**< Item effect. */
    uint8_t        _pad;            /**< Explicit alignment pad. */
    char           name[16];        /**< Display name. */
    char           flavor_text[32]; /**< Short flavor description. */
} fq_item_def_t;

_Static_assert(sizeof(fq_item_def_t) == 58u,
    "fq_item_def_t must be exactly 58 bytes");

/* ---------------------------------------------------------------------------
 * fq_character_t — the full character save record.
 *
 * Field order strictly: uint32s → uint16s → uint8s/chars → rival_log.
 * This minimizes compiler-inserted padding on 32-bit aligned platforms.
 *
 * Layout (verified, no hidden padding):
 *   uint32_t id            (4)   offset 0
 *   uint32_t xp            (4)   offset 4
 *   uint32_t legacy_tree   (4)   offset 8
 *   uint16_t hp_max        (2)   offset 12
 *   uint16_t wins          (2)   offset 14
 *   uint16_t losses        (2)   offset 16
 *   uint16_t equipped[5]  (10)   offset 18
 *   char     name[12]     (12)   offset 28
 *   uint8_t  save_version  (1)   offset 40
 *   uint8_t  class_id      (1)   offset 41
 *   uint8_t  level         (1)   offset 42
 *   uint8_t  strength      (1)   offset 43
 *   uint8_t  speed         (1)   offset 44
 *   uint8_t  precision     (1)   offset 45
 *   uint8_t  intelligence  (1)   offset 46
 *   uint8_t  rebirth_count (1)   offset 47
 *   uint8_t  legacy_points (1)   offset 48
 *   uint8_t  is_dead       (1)   offset 49
 *   uint8_t  sprite_base   (1)   offset 50
 *   uint8_t  cosmetic[4]   (4)   offset 51
 *   uint8_t  title         (1)   offset 55
 *   uint8_t  equipped_count(1)   offset 56
 *   uint8_t  wildcard_pass (1)   offset 57
 *   uint8_t  _pad[1]       (1)   offset 58
 *   [1 byte compiler gap to reach 4-byte align for rival_log → covered by _pad above: NO,
 *    rival_log starts at 60. _pad[1] takes to 59. Compiler inserts 1 implicit pad.
 *    RESOLUTION: use _pad[2] to cover both bytes → rival_log starts at 60, total = 156]
 *
 * Wait — re-check: wildcard_passive at 57, _pad[1] at 58, that puts us at 59.
 * rival_log needs 4-byte alignment → next boundary is 60.
 * Compiler inserts 1 implicit pad at offset 59.
 * FIX: use _pad[2] to consume offsets 58 and 59, eliminating all hidden padding.
 * Total = 60 + 96 = 156 bytes.
 * ---------------------------------------------------------------------------*/
typedef struct {
    /* 32-bit fields */
    uint32_t id;               /**< Unique character ID (esp_random() at creation). */
    uint32_t xp;               /**< Lifetime XP earned. */
    uint32_t legacy_tree;      /**< Bitmask: 32 possible legacy perk slots. */

    /* 16-bit fields */
    uint16_t hp_max;           /**< Derived max HP. NOT persisted mid-fight. */
    uint16_t wins;             /**< Total wins. */
    uint16_t losses;           /**< Total losses. */
    uint16_t equipped[5];      /**< Item IDs in equipped slots (max 5). */

    /* 8-bit and char fields */
    char    name[12];          /**< Two-part generated name + null terminator. */
    uint8_t save_version;      /**< Schema version for migration. */
    uint8_t class_id;          /**< fq_class_t. */
    uint8_t level;             /**< Derived from XP. */
    uint8_t strength;          /**< Damage modifier. */
    uint8_t speed;             /**< Turn order, dodge chance. */
    uint8_t precision;         /**< Crit chance, opponent dodge reduction. */
    uint8_t intelligence;      /**< Tactical reroll charges. */
    uint8_t rebirth_count;     /**< Total deaths (rebirths). */
    uint8_t legacy_points;     /**< Unspent rebirth tokens. */
    uint8_t is_dead;           /**< 1 if must navigate to REBIRTH screen. */
    uint8_t sprite_base;       /**< Base sprite index for class. */
    uint8_t cosmetic_slots[4]; /**< hat, body, accessory, aura. */
    uint8_t title;             /**< Earned title index. */
    uint8_t equipped_count;    /**< 4 default, 5 with Scavenger perk. */
    uint8_t wildcard_passive;  /**< Wildcard: active class passive (rerolled on rebirth). */
    uint8_t _pad[2];           /**< Explicit alignment pad — 2 bytes to reach offset 60. */

    /* Rival log (last — largest section) */
    fq_rival_entry_t rival_log[8]; /**< Last 8 unique opponents. 8 x 12 = 96 bytes. */
} fq_character_t;

_Static_assert(sizeof(fq_character_t) == 156u,
    "fq_character_t must be exactly 156 bytes");

/* ---------------------------------------------------------------------------
 * fq_inventory_t — the character's item inventory.
 * ---------------------------------------------------------------------------*/
typedef struct {
    uint16_t items[32]; /**< Item IDs owned (max 32; 40 with Deep Pockets, Phase 5). */
    uint8_t  count;     /**< Number of owned items (0-32). */
    uint8_t  _pad;      /**< Explicit alignment pad. */
} fq_inventory_t;

_Static_assert(sizeof(fq_inventory_t) == 66u,
    "fq_inventory_t must be exactly 66 bytes");

#endif /* FIESTAQUEST_GAME_TYPES_H */
