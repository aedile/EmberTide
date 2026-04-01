# FiestaQuest -- Game Design Document v5

**Platform:** Waveshare ESP32-S3-ePaper-1.54 (200x200 B&W, 2 buttons, BLE, WiFi, mic, speaker, RTC, temp/humidity)
**Storage:** 8MB flash (LittleFS data partition, no SD card)
**Architecture:** Fully standalone. No companion app. No external dependencies.

---

## 1. Core Data Model

### 1.1 Character

```
Character {
  save_version:   uint8         // schema version for migration (see 5.5)
  id:             uint32        // unique ID via esp_random() at creation
  name:           char[12]      // two-part generated name (see 11.6)
  class:          enum(u8)      // Bruiser, Trickster, Hex, Warden, Wildcard
  level:          uint8         // derived from XP (see 1.7)
  xp:             uint32        // lifetime XP earned

  // Base stats (grow via training sessions)
  strength:       uint8         // damage modifier
  speed:          uint8         // turn order, dodge chance
  precision:      uint8         // crit chance, opponent dodge reduction
  intelligence:   uint8         // tactical reroll charges

  hp_max:         uint16        // derived: base_hp + (strength * 2) + legacy bonuses

  // Rebirth
  rebirth_count:  uint8         // total deaths
  legacy_points:  uint8         // unspent rebirth tokens
  legacy_tree:    uint32        // bitmask, 32 possible legacy perks
  is_dead:        bool          // if true, must navigate to REBIRTH screen to revive

  // Cosmetics
  sprite_base:    uint8         // base sprite index for class
  cosmetic_slots: uint8[4]      // hat, body, accessory, aura
  title:          uint8         // earned title index ("Veteran", "Glass Jaw", etc.)

  // Equipped items
  equipped:       uint16[5]     // max 5 item slots (slot 4 requires Scavenger perk)
  equipped_count: uint8         // 4 default, 5 with Scavenger

  // Social
  wins:           uint16
  losses:         uint16
  rival_log:      RivalEntry[8] // last 8 unique opponents

  // Wildcard-only
  wildcard_passive: enum(u8)    // which class passive is active (rerolled on rebirth)
}
```

**HP is NOT persisted between fights.** Every fight starts at hp_max.

**Stat floor on rebirth:** Stats can never drop below class base values (see Section 7). A Bruiser with STR base +3 can never have STR below 3 after rebirth. The 50% penalty only strips training gains, not class identity.

### 1.2 Items (Combat Passives / "Jokers")

```
Item {
  id:             uint16        // 0-65535, supports OTA content growth
  name:           char[16]
  rarity:         enum(u8)      // Common, Uncommon, Rare, Legendary
  trigger:        enum(u8)      // WHEN does this fire?
  condition:      Condition     // IF what is true?
  effect:         Effect        // THEN what happens?
  flavor_text:    char[32]      // short description for display
}

TriggerType {
  ON_ATTACK, ON_DEFEND, ON_ROUND_START, ON_ROUND_END,
  ON_DODGE, ON_CRIT, ON_LOW_HP, ON_KILL, ON_DEATH, PASSIVE
}

Condition {
  type:           enum(u8)      // NONE, HP_BELOW, HP_ABOVE, STREAK, ROUND_NUMBER, etc.
  threshold:      uint8
}

Effect {
  type:           enum(u8)      // DAMAGE_ADD, DAMAGE_MULT, HEAL, DODGE_BONUS, REROLL, etc.
  value:          int8          // signed
  target:         enum(u8)      // SELF, OPPONENT, BOTH
}
```

**Item trigger iteration order:** Items are ALWAYS evaluated in equipped-slot order (0, 1, 2, 3, 4 if Scavenger active). Defender's items resolve before attacker's. Both BLE devices must use this exact order. See Appendix A for worked example.

### 1.3 Inventory

```
Inventory {
  items:          uint16[32]    // max 32 items owned (40 with Deep Pockets)
  count:          uint8
}
```

**Overflow:** When full and a new item is earned, player is shown new item vs current inventory. BOOT cycles owned items, PWR swaps highlighted for new, BOOT long discards new.

### 1.4 Training Session Modifiers

```
Modifier {
  id:             uint8
  name:           char[16]
  description:    char[48]
  effect_type:    enum(u8)
  value:          int8
  combo_tag:      uint8         // shared tag = synergy
}
```

### 1.5 Modifier Unlock Pool

```
ModifierPool {
  unlocked:       uint32        // bitmask, up to 32 modifiers
}
```

### 1.6 Rival Log

```
RivalEntry {
  opponent_id:    uint32
  encounters:     uint8
  wins:           uint8
  last_fight_ts:  uint32
  is_nemesis:     bool          // auto-set at 3+ encounters
}
```

### 1.7 Level Curve

```
XP for level N: 50 * N * N

Level  | Cumulative XP
-------|---------------
  1    |           50
  5    |        1,250
 10    |        5,000
 15    |       11,250
 20    |       20,000
 30    |       45,000 (soft cap)
```

XP sources: training (10-80), combat win (25 + 5 per opponent level), combat loss (10).

---

## 2. Combat Resolution

**ALL combat math uses integer arithmetic only.** See Appendix A for complete worked example.

**Canonical fighter ordering:** Both devices simulate the INITIATOR (the device that sent CHALLENGE_REQUEST) as Fighter 1 and the RESPONDER as Fighter 2, regardless of which physical device is running. All reroll heuristics, item triggers, and PRNG calls follow this canonical ordering.

### 2.1 Stat Scaling in Combat

Raw stats are compressed via log curve before use in combat formulas. Players see big numbers grow, but combat effectiveness has diminishing returns.

```
effective_stat = 10 * ln(raw_stat + 1) / ln(11)
// Integer approximation via lookup table:
// raw 0->0, 1->3, 2->5, 3->6, 5->7, 8->9, 10->10, 15->12, 20->13, 30->14, 50->16

All combat formulas use effective_stat, not raw_stat.
```

This means a player with 50 raw strength (~16 effective) is only ~1.6x as effective as a player with 10 raw strength (~10 effective), not 5x. Fights stay competitive across large level gaps.

### 2.2 Turn Order

```
initiative = rand_int(1, 6) + (eff_speed / 3)
```

Higher goes first. Ties favor defender. All division is integer truncation.

### 2.3 Attack Roll

```
raw_roll = rand_int(1, 6)

precision_tier = min(eff_precision / 5, 3)  // tiers 0-3 (capped at 3)

// Lookup table:
// Tier 0: [1,2,3,4,5,6]  (no change)
// Tier 1: [2,2,3,4,5,5]
// Tier 2: [2,3,3,4,5,5]
// Tier 3: [2,3,3,4,4,5]

adjusted_roll = PRECISION_TABLE[precision_tier][raw_roll - 1]

// Crit: uses raw_roll. Threshold = 6 at tier 0, 5 at tier 2+
crit_threshold = 6 - (precision_tier / 2)
is_crit = (raw_roll >= crit_threshold)

damage = adjusted_roll + (eff_strength / 2)
if is_crit:
  damage = damage * 3 / 2
```

### 2.4 Dodge Roll

```
dodge_chance = (eff_speed * 2) - (opponent_eff_precision / 2)
dodge_chance = clamp(dodge_chance, 5, 40)
dodge_roll = rand_int(1, 100)
if dodge_roll <= dodge_chance: attack misses
```

### 2.5 Item Trigger Resolution

```
for side in [defender, attacker]:
  for slot in [0, 1, 2, 3, 4]:  // slot 4 only if Scavenger active
    item = side.equipped[slot]
    if item != EMPTY and item.trigger matches current_event:
      if evaluate_condition(item.condition, game_state):
        apply_effect(item.effect, game_state)
```

Both devices iterate the same slots for each fighter. Include `equipped_count` in BLE character summary so both devices agree on whether slot 4 exists.

### 2.6 Tactical Rerolls

Each fight, a character gets `eff_intelligence / 4` reroll charges (min 0, max 5). Evaluated from the perspective of the fighter whose charges are being spent:

```
// Fighter N's reroll heuristic (N = 1 or 2, canonical ordering):
if fighter_N_attack_roll <= 2: reroll fighter_N attack
else if opponent_attack_roll >= 5: reroll opponent attack
```

Rerolls consume the next PRNG value. Both devices always evaluate reroll eligibility for Fighter 1 first, then Fighter 2, regardless of who attacked first this round.

### 2.7 Fight Flow

```
1. BLE handshake, exchange character data + nonces (see Section 6)
2. Display matchup screen
3. Both fighters start at hp_max
4. Loop (max 12 rounds):
   a. Roll initiative
   b. First attacker: roll attack, defender: roll dodge
   c. If hit: apply damage, trigger items
   d. Check for KO
   e. Second attacker: same sequence
   f. Check for KO
   g. End-of-round item triggers
   h. Lucky Star check: for each fighter with Lucky Star perk,
      rand_int(1, 20). If result == 1, fighter gets a bonus attack.
      This PRNG call happens every round regardless of trigger.
   i. OVERTIME: if round > 8, both fighters take (round - 8) unavoidable damage.
      Underdog bonus is HALVED during overtime rounds.
   j. Exchange HP values via BLE for mid-fight sync check.
      If HP diverges from local simulation, abort fight as SYNC ERROR.
   k. Display round summary (partial refresh)
5. If 12 rounds pass with no KO: higher remaining HP % wins
6. Apply results: XP, loot, rival log update
7. Loser's is_dead flag set to true (see Section 4)
```

### 2.8 Stat Scaling Guardrails

```
level_diff = abs(fighter_a.level - fighter_b.level)
if level_diff > 5:
  underdog_bonus = min(level_diff - 5, 5)  // capped at +5
  // Applied to underdog's ATTACK rolls only (not dodge, not initiative)
  // Halved during overtime rounds (round > 8)
```

---

## 3. Training Session Flow

### 3.1 Score Normalization

All mini-games produce a NORMALIZED score on a 0-100 scale per round:

```
normalized = (raw_score - game_floor) * 100 / (game_ceiling - game_floor)
normalized = clamp(normalized, 0, 100)
```

Game-specific floors and ceilings are defined per mini-game (Section 3.3). This ensures all games contribute equally and modifiers have consistent impact regardless of which game is played.

### 3.2 Session Flow

```
1. Player initiates training from TRAIN screen
2. System generates session: 6 rounds, each offering 2 mini-game choices
3. Before rounds 2, 4, and 6: offer modifier card pick (2 options)
4. Player completes mini-games, modifiers stack
5. End of session: sum normalized scores (0-600 base range before modifiers)
6. Score hits reward thresholds (PLACEHOLDER -- calibrate via playtest):
   - Bronze  (>150): 1 stat point + 15 XP
   - Silver  (>275): 2 stat points + 30 XP + common item roll
   - Gold    (>400): 3 stat points + 55 XP + uncommon item roll + modifier unlock
   - Platinum(>525): 4 stat points + 80 XP + rare item roll + modifier unlock + cosmetic
7. Player allocates stat points (BOOT to cycle stat, PWR to confirm)
```

### 3.3 Mini-Game Catalog

| ID | Game           | Stat      | Mechanic                                                    | Floor | Ceiling |
|----|----------------|-----------|-------------------------------------------------------------|-------|---------|
| 0  | Quick Draw     | Speed     | Visual cue on e-paper, press button ASAP. Score = 1000/reaction_ms | 3     | 8       |
| 1  | Memory Chain   | Intell.   | Simon-says binary sequence (L/R buttons), length increases  | 2     | 10      |
| 2  | Power Tap      | Strength  | Alternate buttons as fast as possible for 5s, count taps    | 15    | 50      |
| 3  | Steady Hand    | Precision | Hold button for exactly N seconds, score = accuracy         | 0     | 10      |
| 4  | Rhythm Hit     | Spd+Prc   | Visual beat pattern on e-paper, tap in sync                 | 2     | 25      |
| 5  | Coin Flip      | (Luck)    | Guess heads/tails 5 times                                   | 0     | 5       |
| 6  | Dodge Drill    | Speed     | Symbols flash on screen, press correct button before timeout| 1     | 8       |
| 7  | Endurance Hold | Strength  | Hold both buttons simultaneously, screen shows shrinking bar| 3     | 20      |
| 8  | Pattern Match  | Intell.   | Two patterns shown briefly, press L if same, R if different | 0     | 10      |
| 9  | Bomb Defuse    | Precision | Number counts down, press button at exactly 0               | 0     | 10      |

10 games. Each training round offers a choice of 2, drawn without replacement within a session. All games are visual-only; audio is supplementary.

---

## 4. Rebirth and Legacy System

### 4.1 Death and Rebirth

Instant. No timer.

```
on_death:
  character.is_dead = true
  character.rebirth_count += 1
  character.legacy_points += 1

  // Stat penalty: stats halved, but never below class base values
  for each stat in [strength, speed, precision, intelligence]:
    new_val = stat / 2
    stat = max(new_val, CLASS_BASE[class][stat])

  // Items, loadout, XP preserved.
  // Training/combat blocked until rebirth.

on_rebirth:
  character.is_dead = false
  if class == Wildcard: reroll wildcard_passive
```

### 4.2 Legacy Tree

```
Tier 1 (no prereqs, 1 point each):
  [0] Thick Skin        -- +3 max HP permanently
  [1] Quick Learner     -- +10% XP from training
  [2] Light Feet        -- +1 base dodge chance
  [3] Iron Will         -- +1 reroll charge per fight

Tier 2 (requires 1 tier-1 perk):
  [4] Second Wind       -- heal 5 HP if you survive to round 8
  [5] Scavenger         -- +1 item slot (5 total, enables equipped[4])
  [6] Soft Landing      -- on death, keep 60% of stats instead of 50%
  [7] Underdog Spirit   -- +2 to attack rolls when fighting higher-level opponent

Tier 3 (requires 2 tier-2 perks):
  [8] Phoenix Flame     -- on rebirth, keep 75% of stats (stacks with Soft Landing)
  [9] Nemesis Bond      -- +3 damage against nemesis opponents
  [10] Lucky Star       -- 5% chance per round for bonus attack (see 2.7 step h)
  [11] Deep Pockets     -- +8 inventory slots (40 total)

Tier 4 (requires 2 tier-3 perks):
  [12] Legendary Title  -- unlock "Undying" title cosmetic
  [13] Ghost Walk       -- first round of every fight: 100% dodge chance
  [14] Final Stand      -- at 1 HP, damage doubles for 3 rounds
  [15] Mentor           -- +1 XP bonus to opponents you defeat
```

---

## 5. Storage Layout (LittleFS on Flash)

### 5.1 Partition Scheme

```
ESP32-S3 8MB Flash:
  Bootloader:       0x0000 - 0x8000        (32KB)
  Partition Table:  0x8000 - 0x9000        (4KB)
  App Partition 0:  0x10000 - 0x190000     (1.5MB)
  App Partition 1:  0x190000 - 0x310000    (1.5MB) -- OTA staging
  OTA Data:         0x310000 - 0x312000    (8KB)
  LittleFS Data:    0x312000 - 0x800000    (~5MB)
```

### 5.2 File Layout

```
/littlefs/
  save.dat              // ~512 bytes. First byte = save_version.
  save.bak              // Backup copy. On CRC fail, try backup before factory reset.
  session_log.dat       // Last 10 training results (~200 bytes)
  items.def             // Item definition table (~3.3KB base)
  modifiers.def         // Modifier definition table (~2KB)
  wifi.dat              // Up to 3 stored WiFi credential sets (~300 bytes)
  sprites/
  audio/
  ota/
    manifest.dat
    packs/
```

### 5.3 Save Backup

On every successful save, write to both `save.dat` and `save.bak`. On boot, if `save.dat` CRC32 fails, try `save.bak`. If both fail, factory reset.

### 5.4 Save File Versioning

First byte is `save_version`. Migration chain runs on version mismatch.

---

## 6. BLE Protocol

### 6.1 Service and Characteristics

```
SERVICE UUID: custom 128-bit UUID

Characteristics:
  CHALLENGE_REQUEST   (write)  -- character summary
  CHALLENGE_RESPONSE  (notify) -- accept/decline + character summary
  FIGHT_SYNC          (notify) -- RNG nonce exchange + equipped_count
  ROUND_CHECK         (notify) -- HP values after each round for sync verification
  FIGHT_RESULT        (notify) -- final CRC32 of combat log
  TRADE_OFFER         (write)
  TRADE_ACCEPT        (notify)

Character Summary (~54 bytes):
  {id, name, class, level, eff_strength, eff_speed, eff_precision, eff_intelligence,
   hp_max, equipped[5], equipped_count, legacy_tree_bitmask, wildcard_passive}
```

### 6.2 Combat Determinism

See Appendix A for the full protocol spec including reference PRNG implementation.

1. Exchange 32-bit nonces via FIGHT_SYNC.
2. Shared seed = nonce_a XOR nonce_b.
3. Initialize xorshift32 PRNG (frozen algorithm, see Appendix A).
4. All random values consumed in canonical order per Appendix A.
5. After each round, exchange HP via ROUND_CHECK. Divergence = abort, SYNC ERROR displayed.
6. At fight end, exchange CRC32 of combat log. Mismatch = fight voided.

### 6.3 Connection Failure Recovery

```
TIMEOUTS:
  - 10 seconds silence = disconnect
  - Challenge phase disconnect: no penalty, return HOME
  - Combat phase disconnect: disconnector forfeits
  - On crash reboot: combat_in_progress flag -> resolve as loss

DISCOVERY UX:
  - BLE advertising is OFF by default on wake
  - PWR double on HOME = toggle "looking for fight" advertising
  - When opponent found: show name + level + class
  - BOOT cycles discovered opponents, PWR sends challenge
  - Opponent: PWR to accept, BOOT long to decline
  - 15 second challenge timeout

RF CONGESTION:
  - Connection interval: 30ms
  - Advertising interval: 1 second
  - 3 failed connections to same device = 60 second backoff
```

---

## 7. Class Definitions

| Class     | HP  | STR | SPD | PRC | INT | Passive                                              |
|-----------|-----|-----|-----|-----|-----|------------------------------------------------------|
| Bruiser   | +20 | +3  | -1  | +0  | -1  | Attacks dealing >5 dmg heal 1 HP (capped at hp_max)  |
| Trickster | +0  | -1  | +3  | +1  | +0  | 1 free dodge reroll per fight                        |
| Hex       | +0  | +0  | +0  | -1  | +3  | Each hit: -1 cumulative debuff to opponent (floor -5) |
| Warden    | +15 | -1  | -1  | +0  | +2  | Heal 1 HP end of each round (capped at hp_max)       |
| Wildcard  | +5  | +1  | +1  | +1  | +1  | Random passive from above 4, rerolled on each rebirth|

**Class base stats** (used as rebirth floor): a stat's class base = max(0, class_modifier). Bruiser STR base = 3, Bruiser SPD base = 0 (modifier is -1, floor is 0).

---

## 8. Input System

### 8.1 Gestures

| Gesture       | Detection                                     |
|---------------|-----------------------------------------------|
| Single press  | Released, no second press within 280ms        |
| Double press  | Two presses within 280ms                      |
| Long press    | Held >600ms, fires on threshold               |

### 8.2 Universal Map

```
BOOT single     = NEXT
BOOT double     = HOME (DISABLED during TRAINING_SESSION and COMBAT_ACTIVE)
BOOT long       = BACK

PWR single      = SELECT
PWR double      = SHORTCUT (context-dependent)
PWR long        = SLEEP (universal, always active)
```

---

## 9. Screen State Machine

(Unchanged from v4 except: SETTINGS comment fixed, REBIRTH screen includes Wildcard passive reroll option for 1 legacy point)

---

## 10. Power Management

(Unchanged from v4 except: BLE advertising is OFF by default on wake. Player must PWR double on HOME to enable.)

### 10.1 Auto-Sleep Timeout

5 minutes, configurable. Suspended during combat, training, WiFi OTA.

---

## 11. Item Catalog ("Jokers")

### 11.1 Synergy Philosophy

**Synergies are emergent, not tagged.** Items interact through overlapping triggers and conditions, not explicit combo mechanics. If "Haymaker" (bonus on crit) pairs well with precision builds that crit more often, that's an intended emergent synergy. There are no "set bonuses" or "combo tags" on items.

**Design constraints:** 4 equip slots (5 with Scavenger). Every item must be useful standalone. Synergies reward thoughtful loadout building but aren't required. Cursed items exist (strong upside, permanent downside while equipped). Items can be freely equipped/unequipped and discarded from inventory.

No class-locked items. Any class can equip any item.

### 11.2 Common Items (15)

| ID  | Name            | Trigger       | Condition        | Effect                        |
|-----|-----------------|---------------|------------------|-------------------------------|
| 001 | Iron Fist       | PASSIVE       | NONE             | +1 damage to all attacks      |
| 002 | Swift Boots     | PASSIVE       | NONE             | +2 to initiative rolls        |
| 003 | Tough Hide      | PASSIVE       | NONE             | -1 damage from all attacks (min 1) |
| 004 | Lucky Coin      | ON_ROUND_START| NONE             | 10% chance: +2 to next roll   |
| 005 | Bandage         | ON_ROUND_END  | HP_BELOW 50%     | Heal 1 HP                     |
| 006 | Whetstone       | ON_ATTACK     | NONE             | +1 damage on odd-numbered rounds |
| 007 | Mirror Shard    | ON_DEFEND     | NONE             | Reflect 1 damage back to attacker |
| 008 | Feather Charm   | PASSIVE       | NONE             | +5% dodge chance              |
| 009 | Focus Lens      | PASSIVE       | NONE             | +1 to crit check (lower threshold by 1) |
| 010 | War Drum        | ON_ROUND_START| ROUND_NUMBER 1   | +3 damage on first attack of fight |
| 011 | Acorn           | ON_ROUND_END  | NONE             | 15% chance: gain 1 HP         |
| 012 | Stone Fist      | ON_CRIT       | NONE             | +2 bonus crit damage          |
| 013 | Smoke Bomb      | ON_DEFEND     | ROUND_NUMBER < 4 | +15% dodge for first 3 rounds |
| 014 | Thorn Vine      | ON_DEFEND     | NONE             | If hit, attacker takes 1 damage|
| 015 | Scout Badge     | PASSIVE       | NONE             | +3 to initiative rolls        |

### 11.3 Uncommon Items (12)

| ID  | Name            | Trigger       | Condition        | Effect                        |
|-----|-----------------|---------------|------------------|-------------------------------|
| 101 | Haymaker        | ON_CRIT       | NONE             | Crit damage x2 instead of x1.5 |
| 102 | Loaded Dice     | ON_ATTACK     | NONE             | 1 free reroll per fight (doesn't use INT charges) |
| 103 | Blood Pact      | ON_ATTACK     | NONE             | +3 damage, but lose 1 HP per attack |
| 104 | Vampire Fang    | ON_KILL       | NONE             | Heal 5 HP on killing blow     |
| 105 | Adrenaline Rush | ON_LOW_HP     | HP_BELOW 25%     | +3 to all attack rolls        |
| 106 | Turtle Shell    | ON_DEFEND     | HP_BELOW 50%     | -3 damage from attacks        |
| 107 | Counter Blade   | ON_DODGE      | NONE             | After dodge, auto-attack for 3 damage |
| 108 | Hex Charm       | ON_ATTACK     | STREAK >= 2      | If hit 2+ rounds in a row, -2 to opponent next roll |
| 109 | Second Skin     | ON_ROUND_END  | HP_ABOVE 75%     | +1 damage next attack while healthy |
| 110 | Ambush Cloak    | ON_ROUND_START| ROUND_NUMBER 1   | First round: guaranteed first strike |
| 111 | Rage Shard      | ON_DEFEND     | NONE             | Each time hit, +1 cumulative damage (resets each fight) |
| 112 | Phoenix Down    | ON_DEATH      | NONE             | Once per fight: survive at 1 HP instead of dying |

### 11.4 Rare Items (8)

| ID  | Name            | Trigger       | Condition        | Effect                        |
|-----|-----------------|---------------|------------------|-------------------------------|
| 201 | Glass Cannon    | PASSIVE       | NONE             | +5 damage, +3 damage taken    |
| 202 | Fate Spinner    | ON_ROUND_START| NONE             | Reroll initiative if you lost it (1/fight) |
| 203 | Soul Leech      | ON_ATTACK     | NONE             | Heal 1 HP per hit landed      |
| 204 | Chaos Orb       | ON_ROUND_START| NONE             | Each round: randomly swap one stat with opponent for that round |
| 205 | Iron Maiden     | ON_DEFEND     | NONE             | Attacker takes 50% of damage dealt back |
| 206 | Berserker Axe   | PASSIVE       | NONE             | +2 damage per round elapsed (round 1: +2, round 5: +10). Cannot dodge. |
| 207 | Time Loop       | ON_ROUND_END  | ROUND_NUMBER 6   | At round 6: reset both fighters to round 3 HP (once per fight) |
| 208 | Nemesis Crown   | PASSIVE       | NONE             | +5 damage vs nemesis. -2 damage vs non-nemesis. |

### 11.5 Legendary Items (4)

| ID  | Name            | Trigger       | Condition        | Effect                        |
|-----|-----------------|---------------|------------------|-------------------------------|
| 301 | Cursed Crown    | PASSIVE       | NONE             | Double all stat bonuses. On death, lose 1 random item permanently. |
| 302 | Mirror Match    | ON_ROUND_START| NONE             | Copy opponent's highest stat as your own for this fight. |
| 303 | Undying Flame   | ON_DEATH      | NONE             | Revive at 25% HP, all items destroyed for rest of fight. Once ever. |
| 304 | Chaos Engine    | ON_ROUND_START| NONE             | Each round, trigger a random Common item effect you don't have equipped. |

### 11.6 Implementation Notes

- **Chaos Orb (204) and Mirror Match (302)** modify effective stats temporarily during combat. Both devices must compute the same modification because they share the PRNG. Chaos Orb's "random stat" consumes a PRNG call.
- **Time Loop (207)** requires both devices to store HP snapshots at round 3. Add to combat state.
- **Cursed Crown (301)** "lose 1 random item permanently" removes from inventory on death, not just unequips. Consumes a PRNG call for random selection. Ouch.
- **Phoenix Down (112) and Undying Flame (303)** interact: Phoenix Down fires first (slot order). If both are equipped, Phoenix Down saves you at 1 HP, Undying Flame doesn't trigger. If only Undying Flame is equipped, you revive at 25%.

---

## 12. Training Modifier Catalog

### 12.1 Synergy Rules

Modifiers have combo_tags. If two or more active modifiers share a tag, each gains +25% effectiveness per shared partner. Three modifiers with tag "RISK" each get +50% bonus. This is the "build an engine" mechanic.

```
Tags: RISK, FOCUS, GRIND, LUCK, CHAIN
```

### 12.2 Starter Modifiers (8, unlocked for all)

| ID | Name            | Tag   | Effect                                                      |
|----|-----------------|-------|-------------------------------------------------------------|
| 0  | Double Down     | RISK  | Next game score x2 or x0 (coin flip after game)             |
| 1  | Steady Pace     | FOCUS | +15 flat bonus to next game score                           |
| 2  | Warm Up         | GRIND | Each consecutive game of the same TYPE gets +10%            |
| 3  | Lucky Break     | LUCK  | 25% chance next game score x3                               |
| 4  | Combo Starter   | CHAIN | If next game scores above 60 (normalized), +20 to the one after |
| 5  | Safety Net      | FOCUS | Next game score cannot go below 30 (normalized)             |
| 6  | Adrenaline      | RISK  | Next game has 3-second time limit (normally 5s). Score x1.5 |
| 7  | Scavenger Hunt  | GRIND | +5 bonus per game completed so far this session             |

### 12.3 Unlockable Modifiers (16, earned via Gold/Platinum sessions)

| ID | Name            | Tag   | Effect                                                      |
|----|-----------------|-------|-------------------------------------------------------------|
| 8  | All In          | RISK  | Skip next game entirely. Its potential score added to the following game. |
| 9  | Echo Chamber    | CHAIN | Repeat the last mini-game instead of choosing. Score x1.3   |
| 10 | Glass Floor     | RISK  | All remaining scores in session x1.5, but any score below 25 ends session immediately |
| 11 | Momentum        | CHAIN | Each consecutive game scoring above 50 adds +10 cumulative  |
| 12 | Perfectionist   | FOCUS | If every remaining game scores above 70, bonus +50 at session end |
| 13 | Wildfire        | GRIND | All remaining games are the same type (randomly chosen)     |
| 14 | Insurance       | FOCUS | Store your current session score as a backup. If final score is lower, use the backup. |
| 15 | Jackpot         | LUCK  | 10% chance session reward tier is upgraded by one           |
| 16 | Streak Bonus    | CHAIN | Each game scoring above 50 in a row adds +5 to all subsequent games |
| 17 | Sacrifice       | RISK  | Lose 2 stat points from a random stat. Gain 30 bonus session score. |
| 18 | Deep Focus      | FOCUS | All games in the session get +5 base, but you get one fewer modifier pick |
| 19 | Lucky Sevens    | LUCK  | If session score at any point equals exactly a multiple of 77, instantly upgrade reward tier |
| 20 | Pressure Cooker | RISK  | Each modifier picked adds +10% to scores but -5 to Safety Net floor |
| 21 | Second Wind     | GRIND | Final game of the session gets x2                           |
| 22 | Tag Team        | CHAIN | If this session's modifier picks all share a tag, +30% to final score |
| 23 | Chaos Draft     | LUCK  | Get offered 3 modifiers instead of 2 at next pick           |

### 12.4 Combo Examples

**RISK engine:** Double Down (x2 or x0) + All In (skip + transfer) + Glass Floor (x1.5 but abort on low score). High risk. If you nail it, you're looking at a single monster game that hits x4.5 effective. If you whiff, session ends early with nothing. Each RISK modifier gets +50% from having 2 RISK partners, pushing the multipliers even higher.

**CHAIN engine:** Combo Starter (+20 on follow-up) + Momentum (+10 cumulative per >50) + Streak Bonus (+5 per >50). Rewards consistent above-average performance across multiple rounds. Conservative but compounds well by round 5-6. Each CHAIN modifier gets +50% from 2 partners.

**Degenerate check:** Tag Team (all same tag = +30%) + 3 RISK modifiers = each RISK modifier at +75% effectiveness (50% from 2 partners, 25% from Tag Team combo). Then Double Down becomes x2.75 on success. Glass Floor becomes x2.63 multiplier. Combined with All In concentrating two games into one, theoretical max session score is ~3x Platinum threshold. This is fine -- it requires perfect play AND lucky modifier offerings AND all-RISK commitment. The "x0" wipe from Double Down means the expected value is much lower.

---

## 13. OTA Content Delivery

### 13.1 WiFi Credential Management

Medal stores up to 3 WiFi credential sets in `wifi.dat`. Credential entry via captive portal:

```
1. Enter SETTINGS > WiFi > "Add Network"
2. Medal starts soft AP: "FIESTAQUEST-SETUP"
3. User connects phone to AP, browser opens simple form
4. User enters SSID + password, submits
5. Medal stores credentials, attempts connection
6. On success: display checkmark. On failure: display error, retry.
7. Fallback: if captive portal is unreliable (known iOS issue),
   package insert includes a QR code linking to a web page
   that generates a WiFi config payload downloadable via BLE.
   ALTERNATIVELY: pre-program a hardcoded event WiFi SSID
   ("FIESTA2026") into firmware. Medal auto-connects at events.
```

### 13.2 OTA Download Flow

```
1. SETTINGS > WiFi > "Check for Updates"
2. Medal connects to stored WiFi AP
3. HTTPS GET to pack server (TLS with pinned certificate)
4. Server returns available pack list
5. Player cycles through packs (BOOT), selects (PWR)
6. Download with progress bar on e-paper (X of Y KB)
7. On complete: verify pack CRC32/SHA-256 hash
8. Validate pack item IDs against reserved ranges (reject base game overlap)
9. Write to /littlefs/ota/packs/
10. Update manifest.dat
11. Download timeout: 60 seconds of no data = abort, delete partial, show error
```

---

## 14. Onboarding Flow

```
1. "FIESTAQUEST" splash screen
2. Name entry: two-part combinatorial names
   - 20 adjectives x 20 nouns = 400 unique names
   - BOOT single = cycle current word
   - PWR double = switch which word you're editing
   - PWR single = confirm full name
   - Examples: "Savage Bones", "Lucky Pepper", "Iron Vaquero", "Wild Luca"
3. Class selection: cycle with BOOT, confirm with PWR
4. Appearance: randomized reroll (BOOT = reroll, PWR = confirm)
5. Tutorial fight vs CPU
6. Tutorial training session (1 game, 1 modifier)
7. HOME screen
```

---

## 15. REMAINING OPEN QUESTIONS

### 15.1 Passive / Ambient Mechanics
- **Friendship bond** reward still undefined
- **Temp/humidity** sensor integration still undefined
- **Time-of-day** class bonuses still undefined

### 15.2 Display / UI Layout
200x200 pixel-level mockups needed for all screens.

### 15.3 Sound Design
SFX list, jingle count, volume levels. All supplementary to visual.

### 15.4 Playtest Calibration
Training score thresholds, mini-game floor/ceiling values, and modifier multipliers all need empirical calibration on actual hardware.

---

## Appendix A: BLE Combat Determinism Spec

### A.1 Reference PRNG

This algorithm is FROZEN. Any firmware update that changes this function breaks all cross-version combat.

```c
uint32_t xorshift32(uint32_t *state) {
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

// Bounded random integer: returns value in [min, max] inclusive
int rand_int(uint32_t *state, int min, int max) {
  uint32_t raw = xorshift32(state);
  return min + (raw % (max - min + 1));
}
```

### A.2 PRNG Call Ordering Per Round

Every round, the PRNG is called in this EXACT order. Both devices follow this sequence. Calls happen even if the result is unused (to keep sequences aligned).

```
ROUND N:
  Call 1:  Fighter 1 initiative         rand_int(1, 6)
  Call 2:  Fighter 2 initiative         rand_int(1, 6)
  -- Determine first attacker (higher initiative) --

  Call 3:  First attacker attack roll   rand_int(1, 6)
  Call 4:  Defender dodge roll          rand_int(1, 100)
  -- If hit: apply damage, run item triggers --
  -- Item triggers may consume PRNG calls (see A.3) --
  Call 5*: First attacker reroll check  (consumed only if reroll heuristic triggers)
           If reroll: replacement = rand_int(1, 6) for attack, or rand_int(1, 100) for dodge

  Call 6:  Second attacker attack roll  rand_int(1, 6)
  Call 7:  Defender dodge roll          rand_int(1, 100)
  -- If hit: apply damage, run item triggers --
  Call 8*: Second attacker reroll check (same logic as Call 5)

  -- End-of-round item triggers --

  Call 9:  Lucky Star check, Fighter 1  rand_int(1, 20) -- ALWAYS consumed
  Call 10: Lucky Star check, Fighter 2  rand_int(1, 20) -- ALWAYS consumed
  -- If Lucky Star triggers: bonus attack uses Calls 11-12 --
  Call 11*: Bonus attack roll           rand_int(1, 6)
  Call 12*: Bonus dodge roll            rand_int(1, 100)

  -- Overtime damage applied (no PRNG) --

  * = conditional call. If the condition is NOT met, the call is SKIPPED.
    This means both devices must evaluate conditions identically to stay in sync.
```

### A.3 Item-Triggered PRNG Calls

Some items consume PRNG calls:

- **Lucky Coin (004):** ON_ROUND_START, 10% chance. Consumes rand_int(1, 100) EVERY round it's equipped, regardless of trigger. Call happens after initiative but before attacks.
- **Acorn (011):** ON_ROUND_END, 15% chance. Consumes rand_int(1, 100) every round.
- **Chaos Orb (204):** ON_ROUND_START. Consumes rand_int(0, 3) to pick which stat to swap. Every round.
- **Chaos Engine (304):** ON_ROUND_START. Consumes rand_int(0, 14) to pick which Common item effect to trigger.
- **Cursed Crown (301):** ON_DEATH. Consumes rand_int(0, count-1) to pick item to destroy.

**Rule:** All probabilistic items consume their PRNG call in equipped-slot order, defender before attacker, at the start of the trigger phase. Both devices evaluate ALL probabilistic items even if the probability check fails, to keep PRNG sequences aligned.

### A.4 Worked Example: 3-Round Fight

```
SETUP:
  Fighter 1 (Initiator): "Savage Bones", Bruiser Lv5
    STR 8 (eff 9), SPD 5 (eff 7), PRC 6 (eff 8), INT 4 (eff 6)
    HP: 36. Equipped: [Iron Fist (001), Bandage (005), EMPTY, EMPTY]
    Reroll charges: 6/4 = 1

  Fighter 2 (Responder): "Iron Pepper", Hex Lv4
    STR 4 (eff 6), SPD 6 (eff 8), PRC 3 (eff 5), INT 9 (eff 9)
    HP: 20. Equipped: [Thorn Vine (014), Hex Charm (108), EMPTY, EMPTY]
    Reroll charges: 9/4 = 2

  Shared seed: 0xDEADBEEF (from nonce XOR)
  PRNG state: 0xDEADBEEF

--- ROUND 1 ---

  Call 1: F1 initiative = rand_int(1,6) -> 4. Total: 4 + (7/3) = 4 + 2 = 6
  Call 2: F2 initiative = rand_int(1,6) -> 2. Total: 2 + (8/3) = 2 + 2 = 4
  F1 goes first.

  Call 3: F1 attacks. raw_roll = rand_int(1,6) -> 5
    F1 precision_tier = min(8/5, 3) = 1. Table[1][4] = 5 (no change)
    Crit threshold = 6 - (1/2) = 6 - 0 = 6. raw_roll 5 < 6. No crit.
    Damage = 5 + (9/2) = 5 + 4 = 9
  Call 4: F2 dodge. dodge_chance = (8*2) - (8/2) = 16 - 4 = 12, clamped [5,40] = 12
    dodge_roll = rand_int(1,100) -> 67. 67 > 12. No dodge.

  F2 takes 9 damage. F2 HP: 20 - 9 = 11.
  Bruiser passive: 9 > 5, heal 1. F1 HP: 36 -> 37 (but capped at 36. stays 36).
  Thorn Vine (F2 slot 0, ON_DEFEND): attacker takes 1 damage. F1 HP: 36 - 1 = 35.
  Iron Fist (F1 slot 0, ON_ATTACK? No -- PASSIVE, already applied).
  Hex class passive: F2 landed 0 hits so far, debuff = 0.
  Hex Charm (F2 slot 1, ON_ATTACK, STREAK >= 2): F2 hasn't attacked yet. No trigger.

  Reroll check F1: attack was 5 (> 2), opponent hasn't attacked yet. No reroll.

  Call 6: F2 attacks. raw_roll = rand_int(1,6) -> 3
    F2 precision_tier = min(5/5, 3) = 1. Table[1][2] = 3 (no change)
    Crit threshold = 6 - 0 = 6. No crit.
    Damage = 3 + (6/2) = 3 + 3 = 6
  Call 7: F1 dodge. dodge_chance = (7*2) - (5/2) = 14 - 2 = 12
    dodge_roll = rand_int(1,100) -> 23. 23 > 12. No dodge.

  F1 takes 6 damage. F1 HP: 35 - 6 = 29.
  Thorn Vine doesn't trigger (F2 is attacking, not defending).
  Iron Fist (PASSIVE): already in F1's damage calc.
  Hex class passive: F2 hit F1. Cumulative debuff on F1 = -1 for next round.

  Reroll check F2: attack was 3 (> 2). F1's attack was 5 (>= 5). F2 could reroll F1's
  attack but it already resolved. Rerolls apply to CURRENT round's attacks only.
  No reroll triggered.

  End-of-round triggers:
  Bandage (F1 slot 1, ON_ROUND_END, HP_BELOW 50%): F1 at 29/36 = 81%. Not below 50%.

  Call 9: Lucky Star F1: rand_int(1,20) -> 14. No trigger.
  Call 10: Lucky Star F2: rand_int(1,20) -> 7. No trigger.

  No overtime (round 1 <= 8).

  ROUND_CHECK BLE exchange: F1 HP=29, F2 HP=11. Both devices verify match.

--- ROUND 2 ---

  Call 1: F1 initiative = rand_int(1,6) -> 1. Total: 1 + 2 = 3.
    Hex debuff from last round: -1. Adjusted: 3 - 1 = 2.
  Call 2: F2 initiative = rand_int(1,6) -> 5. Total: 5 + 2 = 7.
  F2 goes first.

  Call 3: F2 attacks. raw_roll = rand_int(1,6) -> 6
    Precision_tier 1. Table[1][5] = 5. Crit threshold 6. raw_roll 6 >= 6. CRIT!
    Damage = 5 + 3 = 8. Crit: 8 * 3 / 2 = 12.
  Call 4: F1 dodge. dodge_chance = 12.
    dodge_roll = rand_int(1,100) -> 8. 8 <= 12. DODGE!

  F1 dodges. No damage. Hex Charm: F2 hit streak was 0 (dodged), resets. No trigger.

  Reroll check F2: attack was fine (crit). F2 doesn't reroll own good result.
  But opponent dodged -- can't reroll a dodge.

  Call 6: F1 attacks. raw_roll = rand_int(1,6) -> 4
    Hex debuff -1 on F1: adjusted_roll from table = 4, then -1 = 3.
    Damage = 3 + 4 = 7. No crit (raw 4 < 6).
  Call 7: F2 dodge. dodge_chance = (8*2) - (8/2) = 12.
    dodge_roll = rand_int(1,100) -> 45. No dodge.

  F2 takes 7 damage. F2 HP: 11 - 7 = 4.
  Bruiser passive: 7 > 5, heal 1. F1 HP: 29 -> 30 (under 36 cap, OK).
  Thorn Vine: F2 defending, attacker (F1) takes 1 damage. F1 HP: 30 - 1 = 29.
  Hex passive: F2 was hit by F1. Cumulative debuff on F1 = -2 next round.

  Bandage: F1 at 29/36 = 81%. No trigger.

  Call 9: Lucky Star F1: rand_int(1,20) -> 19. No.
  Call 10: Lucky Star F2: rand_int(1,20) -> 2. No.

  ROUND_CHECK: F1 HP=29, F2 HP=4.

--- ROUND 3 ---

  Call 1: F1 init = rand_int(1,6) -> 3. + 2 = 5. Hex debuff -2 = 3.
  Call 2: F2 init = rand_int(1,6) -> 3. + 2 = 5.
  Tie: defender (F2) goes first.

  Call 3: F2 attacks. raw_roll = rand_int(1,6) -> 2
    Table[1][1] = 2. Damage = 2 + 3 = 5. No crit.
  Call 4: F1 dodge. dodge_chance = 12.
    dodge_roll = rand_int(1,100) -> 55. No dodge.

  F1 takes 5 damage. F1 HP: 29 - 5 = 24.
  Hex passive: F2 hit F1. Debuff on F1 = -3 next round.
  Hex Charm: F2 hit streak = 1. Need >= 2. No trigger yet.

  Call 6: F1 attacks. raw_roll = rand_int(1,6) -> 5
    Table[1][4] = 5. Hex debuff -2: 5 - 2 = 3. Damage = 3 + 4 = 7.
    Crit check: raw 5 < 6. No crit.
  Call 7: F2 dodge. dodge_chance = 12.
    dodge_roll = rand_int(1,100) -> 11. 11 <= 12. DODGE!

  F2 dodges.

  Bandage: F1 at 24/36 = 67%. No trigger.

  Call 9: Lucky Star F1: rand_int(1,20) -> 1. TRIGGER! Bonus attack!
  Call 10: Lucky Star F2: rand_int(1,20) -> 15. No.

  Call 11: F1 bonus attack. raw_roll = rand_int(1,6) -> 4. Debuff -2: 4-2=2. Dmg = 2+4 = 6.
  Call 12: F2 dodge. rand_int(1,100) -> 88. No dodge.

  F2 takes 6 damage. F2 HP: 4 - 6 = -2. KO!

  FIGHT RESULT: Fighter 1 wins.
  F1: +25 XP + (4*5) = 45 XP. Random item roll.
  F2: +10 XP. is_dead = true.

  Final CRC32 computed over: [29,11, 29,4, 24,-2] = HP sequence per round.
  Both devices exchange CRC. Match confirmed. Results applied.
```

### A.5 Key Determinism Invariants

1. PRNG algorithm is xorshift32 with shifts (13, 17, 5). FROZEN.
2. Fighter 1 = Initiator. Fighter 2 = Responder. ALWAYS.
3. Initiative calls: F1 first, F2 second. ALWAYS.
4. Item triggers: defender slots 0-4, then attacker slots 0-4. ALWAYS.
5. Reroll evaluation: current attacker's perspective. Charges spent from canonical fighter. ALWAYS.
6. Lucky Star: F1 checked first, F2 second. PRNG consumed regardless of perk ownership. ALWAYS.
7. Probabilistic item PRNG calls consumed even on failed probability checks. ALWAYS.
8. Mid-round HP sync via ROUND_CHECK catches divergence within 1 round.
