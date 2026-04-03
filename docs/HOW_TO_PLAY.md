# How to Play EmberTide

EmberTide is played on a tiny e-paper device with two buttons. The entire game is navigated with short presses and long presses of these two buttons. There is no touchscreen, no companion app, and no internet connection required for normal play.

---

## Controls

| Input | Button | Icon on Case | GPIO |
|-------|--------|-------------|------|
| **A press** — Confirm / Select / Advance | Closest to USB-C | ⏻ (power) | GPIO0 (BOOT pin) |
| **B press** — Back / Cancel / Secondary action | Far from USB-C | ☀ (sun) | GPIO18 (PWR pin) |
| **A long hold** | Closest to USB-C | ⏻ (power) | GPIO0 |
| **B long hold** | Far from USB-C | ☀ (sun) | GPIO18 |

> **Physical layout:** Facing the front of the device, the ⏻ power-icon button is on the left (closest to the USB-C port). The ☀ sun-icon button is on the right. The title screen prompt reads "Press [PWR]" — this refers to the ⏻ button (GPIO0 / HAL_BTN_A).

---

## First Boot

When you power on a fresh device for the first time:

1. The **Title Screen** appears. Press the **⏻ button** (closest to USB-C) to continue.
2. You are taken to the **Home Screen** — your creature's dashboard.
3. A default creature is created with randomized stats and a generated two-part name.

Your save is written to flash automatically. Power cycling the device preserves your creature.

---

## Screens and Navigation

EmberTide has a hub-and-spoke navigation model. The **Home Screen** is the hub. Every other screen returns to Home when you press **B**.

```
                    ┌─────────────┐
         A long     │  TRAINING   │
        ┌──────────►│  Mini-game  │──── B ────┐
        │           └─────────────┘           │
        │                                      │
        │           ┌─────────────┐           │
        │  A press  │  INVENTORY  │           │
        ├──────────►│  Equip items│──── B ────┤
        │           └─────────────┘           │
        │                                      │
   ┌────┴────┐                           ┌────▼────┐
   │  HOME   │◄──────────────────────────│  HOME   │
   │  Screen │                           │  Screen │
   └────┬────┘                           └────▲────┘
        │                                      │
        │           ┌─────────────┐           │
        │  B press  │   BATTLE    │           │
        ├──────────►│  BLE setup  │──── A ────┤
        │           └─────────────┘           │
        │                                      │
        │           ┌─────────────┐           │
        │  B long   │   STATS     │           │
        └──────────►│  View stats │──── B ────┘
                    └─────────────┘
```

### Home Screen

Your creature's name, level, HP bar, win/loss record, and a sprite preview. This is where you decide what to do next.

### Inventory

A scrollable grid of your collected items (Jokers). Navigate with **A** (cycle forward) and **B** (back to Home). The bottom panel shows the selected item's name and effect. You can equip up to 4 items (5 with the Scavenger legacy perk). Equipped items are highlighted.

### Stats

A detailed view of your creature's four stats (Strength, Speed, Precision, Intelligence), HP, XP progress toward next level, and rebirth count. Useful for planning your training focus.

### Training

Choose a mini-game type (Speed, Power, or Intelligence) to earn XP and improve your creature. The mini-game presents a series of timed inputs — your accuracy determines your score (0-100), which translates to XP earned. Difficulty scales with your level.

After earning enough XP, your creature levels up automatically. Each level grants 3 stat points distributed according to your class:

| Class | Per Level |
|-------|-----------|
| Bruiser | +2 STR, +1 SPD |
| Trickster | +2 SPD, +1 PRC |
| Hex | +1 PRC, +2 INT |
| Warden | +1 STR, +1 SPD, +1 INT |
| Wildcard | +1 STR, +1 SPD, +1 PRC |

---

## Battle

Battles happen over Bluetooth Low Energy (BLE) between two physical devices.

### Starting a Battle

1. From Home, press **B** to enter **Battle Setup**.
2. Your device begins advertising over BLE, looking for a nearby opponent.
3. When another EmberTide device is found, the two devices exchange character summaries and agree on a shared PRNG seed (derived by XORing both devices' random nonces).
4. Combat begins automatically.

### How Combat Works

Combat is automatic — you watch it unfold. Each round:

1. **Initiative** — Both creatures roll a d6 + speed bonus. Higher goes first. Ties favor the defender.
2. **First attacker strikes** — Roll a d6, adjust through the precision tier table, add strength bonus. The defender rolls a dodge check (d100 vs. speed-derived dodge chance, capped at 40%).
3. **If the hit lands** — Damage is applied. High raw rolls may trigger critical hits (damage x1.5). Equipped items fire at their trigger points (on attack, on defend, on crit, etc.).
4. **Check for KO** — If the defender's HP hits 0, the fight ends immediately. The defender does NOT get a counter-attack.
5. **Second attacker strikes** — Same process in reverse.
6. **Overtime** — After round 8, both creatures take escalating unavoidable damage each round (1 at round 9, 2 at round 10, etc.).
7. **Round limit** — If nobody is KO'd by round 12, the creature with the higher HP percentage wins. Ties favor the defender.

Both devices execute the same deterministic logic from the shared seed. After each round, they exchange CRC32 hashes of their combat state to verify they agree. If the hashes ever diverge, the match is flagged as desynchronized.

### Rerolls

Your Intelligence stat grants tactical reroll charges (effective intelligence / 4, up to 5 charges). Rerolls trigger automatically:

- **Offensive reroll** — If your attack roll is 1 or 2, spend a charge to reroll (keep the higher result).
- **Defensive reroll** — If your opponent rolls 5 or 6, spend a charge to force them to reroll (they keep the lower result).

### Items in Combat

Equipped Jokers trigger at specific moments during each round. Defender items always resolve before attacker items. Multiple items on the same fighter trigger in slot order (0, 1, 2, 3, 4).

---

## Death and Rebirth

When your creature loses a battle and HP reaches 0, it dies. You are taken to the **Rebirth Screen**.

### What Happens on Rebirth

1. **Stat penalty** — Your stats are reduced. You keep your class base stats plus a percentage of your training gains:
   - Default: 50% retention
   - With Soft Landing (legacy perk): 60%
   - With Phoenix Flame (legacy perk): 75%

2. **Legacy tokens earned** — Based on how far you got: `(level / 10) + (wins / 100)`. These are added to your Legacy Points.

3. **Legacy Tree** — Spend Legacy Points to unlock permanent bonuses that persist across all future lives:

   | Tier | Perks | Requirement |
   |------|-------|-------------|
   | 1 | Thick Skin (+5 HP), Keen Eye (+2 PRC), Quick Feet (+2 SPD), Thick Skin II (+5 HP) | — |
   | 2 | Iron Will (+3 INT), Scavenger (5th equip slot), Soft Landing (60% retention), Deep Pockets (+8 inv) | 1 Tier 1 perk |
   | 3 | Phoenix Flame (75% retention), Veteran Mark (+10 HP), Battle Scars (+3 STR), Sixth Sense (+3 SPD) | 2 Tier 2 perks |
   | 4 | Master Mind (+5 INT), Diamond Skin (+15 HP), Godspeed (+5 SPD), Berserker (+5 STR) | 2 Tier 3 perks |

4. **Wildcard reroll** — If your class is Wildcard, your random passive ability is rerolled on rebirth.

The rebirth cycle is the long-term progression loop. Early lives are short and weak. Over many rebirths, your Legacy Tree fills out and your creature starts each life with meaningful bonuses.

---

## Items (Jokers)

Items are passive combat modifiers. You collect them from training rewards and battle victories. Each item has a rarity, a trigger condition, and an effect.

### Trigger Types

| Trigger | When it fires |
|---------|---------------|
| PASSIVE | Always active (stat modifier) |
| ON_ATTACK | When this creature attacks |
| ON_DEFEND | When this creature is attacked |
| ON_CRIT | When this creature lands a critical hit |
| ON_DODGE | When this creature dodges an attack |
| ON_KILL | When this creature KOs the opponent |
| ON_DEATH | When this creature is KO'd |
| ON_ROUND_START | At the beginning of each round |
| ON_ROUND_END | At the end of each round |
| ON_LOW_HP | When HP drops below a threshold |

### Example Items

| Name | Rarity | Trigger | Effect |
|------|--------|---------|--------|
| Iron Fist | Common | PASSIVE | +1 damage on every attack |
| Tough Hide | Common | ON_DEFEND | -1 incoming damage (min 1) |
| Lucky Coin | Common | ON_ROUND_START | 10% chance of +2 bonus damage this round |
| Vampire Fang | Uncommon | ON_KILL | Heal 5 HP |
| Haymaker | Uncommon | ON_CRIT | Crit damage x2 instead of x1.5 |
| Bandage | Uncommon | ON_ROUND_END | Heal 1 HP if below 50% |
| Chaos Orb | Rare | ON_ROUND_START | Swap a random stat with opponent for one round |
| Time Loop | Legendary | ON_ROUND_END | At round 6, restore both fighters to round 3 HP |

---

## Classes

Each class starts with different base stats and gains stats differently on level-up. All classes gain exactly 3 stat points per level.

| Class | Base HP | STR | SPD | PRC | INT | Identity |
|-------|---------|-----|-----|-----|-----|----------|
| **Bruiser** | 60 | 3 | 0 | 0 | 0 | Hits hard, takes hits. No finesse. |
| **Trickster** | 40 | 0 | 3 | 1 | 0 | Dodges everything, crits often. Glass cannon. |
| **Hex** | 45 | 0 | 0 | 1 | 3 | Controls the fight with rerolls. Denies opponent's big rolls. |
| **Warden** | 55 | 1 | 1 | 0 | 1 | Balanced. Good at everything, great at nothing. |
| **Wildcard** | 50 | 1 | 1 | 1 | 0 | Inherits one random class passive. Rerolled on each rebirth. |

Stats follow a logarithmic curve — early training gains are dramatic, but high-level characters converge. A creature with 200 raw strength is only modestly stronger than one with 100. This keeps fights competitive across level gaps and makes item/perk choices matter more than raw grinding.

---

## WiFi and OTA Updates

Hold **A** during boot to enter WiFi setup mode. Your device creates a WiFi access point named `EmberTide-AP`. Connect to it from your phone or laptop, navigate to `192.168.4.1`, and enter your home WiFi credentials. The device stores them and uses WiFi for firmware updates (OTA) when available.
