# FIESTAMON Sprite Map

> **Visually confirmed** — every entry was verified by direct pixel inspection of the source PNG.  
> All offsets are **0-indexed**. Coordinate formula: `x = col * W`, `y = row * H`.  
> Coordinates given as pixel origin of the sprite's top-left corner.

---

## Characters

### `Characters/32x32-Charset.png` · `Characters/32x32-Charset-Outline.png`

| Property      | Value            |
|---------------|------------------|
| Sheet size    | 256 × 672 px     |
| Sprite size   | 32 × 32 px       |
| Grid          | 8 cols × 21 rows |
| Total sprites | 168              |

The `-Outline.png` variant is pixel-identical in layout — swap sheets to switch between filled and outlined styles.

**Offset formula:**
```
x = col * 32,   y = row * 32
```

### Animation Frame Convention

Each row contains **8 frames** of one character. The walk cycle is a looping sprite animation — frame 0 is the idle/base pose; frames 1-7 cycle through the walk.

| Frame (col) | Action |
|-------------|--------|
| 0 | Idle stand / base pose |
| 1 | Walk step 1 (leaning left) |
| 2 | Walk step 2 |
| 3 | Walk step 3 (lean forward) |
| 4 | Walk step 4 / alternate pose |
| 5 | Walk step 5 |
| 6 | Walk step 6 |
| 7 | Walk step 7 |

> Adjacent rows (even + odd) form **one full character archetype**: even row = front/darker palette, odd row = rear/lighter palette (or inverted color scheme variant of the same character).

### Character Roster (visually confirmed)

| Row(s) | Archetype ID | Description |
|--------|--------------|-------------|
| 0–1 | `DARK_KNIGHT` | Cloaked warrior with broadsword & shield; heavy dark armor, round shield on left arm |
| 2–3 | `BONE_KNIGHT` | Skeletal armored fighter; exposed ribs, bony limbs, dark cape |
| 4–5 | `SKULL_MAGE` | Heavy robed figure with skull face; wide garment, glowing eyes, no visible weapon |
| 6–7 | `HORNED_DEMON` | Large demonic brute with prominent curved horns, muscular; transitions to larger silhouette from col 4 onward |
| 8–9 | `TROLL_BEAST` | Massive horned troll/ogre; broader than Horned Demon, four horn nubs visible at top; row 9 shifts to slightly more organic/goblinoid shape |
| 10–11 | `SLIME_GOBLIN` | Short squat goblin/slug creature; round belly, stubby limbs; row 11 shifts to a variant with wider hat/crown detail |
| 12–13 | `FAT_BOSS` | Very wide boss character; top hat or crown, enormous belly, frog-like grin; row 13 transitions to ornate wide-brimmed version (Witch or Hexmage) |
| 14–15 | `VAMPIRE_ROGUE` | Tall cloaked vampire/rogue; upturned collar, dark shroud; row 15 shifts to gem-knight variant (ornate chestpiece with large gem) |
| 16–17 | `FLORAL_WITCH` | Ornate plant/flower-decorated figure; large decorative crown; frames 4-7 shift to armed variant holding staff/weapon. Row 17 = `SHADOW_HULK`: dark brooding muscle-creature with skull face, round frame |
| 18–19 | `OGRE_BOSS` | Wide grinning ogre/troll boss; large gaping smile; row 18 frames 4-7 transition to spiked variant. Row 19 = `BEAR_SPIRIT`: bear-faced creature, natural animal body, mid-row transitions to armored bear |
| 20    | `ARMORED_BEAR` | Spiked-crown armored bear; very large bulky frame, crown of spikes on head; armored shoulder pads |

### Quick-Access Pixel Offsets

```c
// Example: DARK_KNIGHT idle frame
// Row 0, Col 0 → x=0, y=0

// BONE_KNIGHT walk frame 3
// Row 2, Col 3 → x=96, y=64

// ARMORED_BEAR idle
// Row 20, Col 0 → x=0, y=640
```

---

## Items

### `Items/Items-24x24.png` · `Items/Items-24x24-outline.png`

| Property      | Value          |
|---------------|----------------|
| Sheet size    | 96 × 96 px     |
| Sprite size   | 24 × 24 px     |
| Grid          | 4 cols × 4 rows |
| Total sprites | 16             |

**Offset formula:** `x = col * 24`, `y = row * 24`

| col→      | 0 | 1 | 2 | 3 |
|-----------|---|---|---|---|
| **row 0** | Key (ornate, solid) | Key (square bow) | Key (rounded bow) | Key (chunky, beveled) |
| **row 1** | Potion vial (sm, dark liquid) | Potion vial (md, dark) | Potion vial (lg, dark) | Potion vial (lg, dark, narrow neck) |
| **row 2** | Shield (heraldic divided) | Shield (heraldic outlined) | Shield (heraldic white) | Shield (thin-line) |
| **row 3** | Dagger (sheathed/handle left) | Dagger (blade right, tilted) | Dagger (lighter blade) | Dagger (thin, dark pommel) |

> Visual notes: Row 0 = four key variants (same key form, different grip/bow shapes). Row 2 = four shield variants sharing a quartered heraldic pattern. Row 3 = four dagger variants at ~45° angle.

---

### `Items/Items2-24x24.png` · `Items/Items2-24x24-outline.png`

| Property      | Value          |
|---------------|----------------|
| Sheet size    | 96 × 48 px     |
| Sprite size   | 24 × 24 px     |
| Grid          | 4 cols × 2 rows |
| Total sprites | 8              |

**Offset formula:** `x = col * 24`, `y = row * 24`

| col→      | 0 | 1 | 2 | 3 |
|-----------|---|---|---|---|
| **row 0** | Potion flask (round belly, dark) | Potion flask (outlined) | Potion flask (lighter) | Potion flask (darkest) |
| **row 1** | Chest (closed, 3-bar front) | Chest (open, lid up, outlined) | Chest (open, darker shading) | Chest (open, lightest) |

---

## Tiles

All tile sheets use **16 × 16 px** sprites. Offset formula: `x = col * 16`, `y = row * 16`.  
Tiles are **white-on-black** (inverted from normal display); render with color inversion for e-paper black-on-white output.

---

### `Tiles/Crypt-16x16.png`

| Property   | Value           |
|------------|-----------------|
| Sheet size | 128 × 48 px     |
| Grid       | 8 cols × 3 rows |
| Total tiles | 24             |

Tight geometric patterns — angular connectors, maze walls, corner pieces.

| Row | Col 0–7 (left→right) |
|-----|----------------------|
| 0 | Horizontal/vertical wall segments + T-junctions + cross junction |
| 1 | Diagonal corner walls, angled rubble, entrance arch fragments |
| 2 | Star/dot floor accents, diamond patterns, scattered rubble, column bases |

---

### `Tiles/Dungeon-16x16.png`

| Property   | Value           |
|------------|-----------------|
| Sheet size | 128 × 64 px     |
| Grid       | 8 cols × 4 rows |
| Total tiles | 32             |

Rounded cell/bubble patterns — organic dungeon walls with jewel-like nodes at intersections.

| Row | Col 0–7 (left→right) |
|-----|----------------------|
| 0 | Solid fill + outlined cell + large round-corner rooms |
| 1 | Cross-wall junctions with circular nodes, half-walls |
| 2 | Arch/gate segments, floor transitions |
| 3 | Chevron/arrow floor markings, decorative diamond floor tiles |

---

### `Tiles/Hold-16x16.png`

| Property   | Value           |
|------------|-----------------|
| Sheet size | 128 × 48 px     |
| Grid       | 8 cols × 3 rows |
| Total tiles | 24             |

Sharp geometric patterns with a fortress/ship-hold aesthetic — triangles, angular arches.

| Row | Col 0–7 (left→right) |
|-----|----------------------|
| 0 | Solid fill + brick outline + triangular ceiling/floor transitions |
| 1 | Angular arch tops, sawtooth floor, triangular corner connectors |
| 2 | Horizontal band walls, decorative crosshatch patterns, palm/leaf motif accent |

---

### `Tiles/Land-16x16.png`

| Property   | Value           |
|------------|-----------------|
| Sheet size | 128 × 64 px     |
| Grid       | 8 cols × 4 rows |
| Total tiles | 32             |

Organic swirling patterns — terrain texture for overworld maps.

| Row | Col 0–7 (left→right) |
|-----|----------------------|
| 0 | Flowing wave/grass terrain fills (light, medium, dark variants) |
| 1 | Bow-tie/hourglass terrain connectors + transition edge pieces |
| 2 | Arrow/chevron directional tiles, fish-scale water texture |
| 3 | Large central motifs (flower, armor crest, diamond), scattered dot patterns |

---

## Icons

All icon sheets use **16 × 16 px** sprites. Offset: `x = col * 16`, `y = row * 16`.

---

### `Icons/Icons_Controller.png`

| Property   | Value             |
|------------|-------------------|
| Sheet size | 256 × 128 px (approx) |
| Grid       | ~16 cols × 8 rows |

**Visually confirmed row contents:**

| Row | Contents |
|-----|----------|
| 0 | Circle buttons: empty → filled → half → Pac-Man → directional arrows (8 variants) → RT/LT/R2/L2 shoulder buttons (labeled rectangles) |
| 1 | Circle buttons (alt fills) → D-pad cross (5 states) → LB/FB/L1/F1 shoulder labels |
| 2 | `R`/`L`/`R̃`/`L̃` bumpers (circle style) → D-pad variants → gamepad silhouettes (NES, SNES style) → joystick → arcade stick → Switch controller |
| 3 | `O`/`L`/`O̤`/`L̤` variants → D-pad (4 sizes/fills) → *(row ends)* |
| 4 | `A`/`B`/`X`/`Y` XBOX style → D-pad (decorative fills, 6 variants) |
| 5 | `A`/`B`/`X`/`Y` alt style → D-pad (ornate/checkerboard fills) |
| 6 | `△`/`✕`/`○`/`□` PlayStation style → D-pad (ornate variants) |
| 7 | `▶`/`◀`/`■`/`☰` media/menu buttons → camera icon → D-pad (final variants) |

---

### `Icons/Icons_Map_Markers.png`

| Property   | Value             |
|------------|-------------------|
| Sheet size | varies            |
| Grid       | ~13 cols × 7 rows |

**Visually confirmed row contents:**

| Row | Contents |
|-----|----------|
| 0 | Compass rose variants (4) → star/snowflake pins → location drop-pins (2 styles) |
| 1 | Mountain/peak → volcano → crown → city block → grid/map → window → terrain profile → bar charts → cave entrance |
| 2 | Trees (pine, round, multi-leaf, palm, umbrella, cactus) → rainbow → lucky cat |
| 3 | House → fortress/castle → ruined castle → city block → windmill → vertical lines (water/prison) → fenced area → bridge → cat/dog icon → buildings row → keystone arch → person figure |
| 4 | Shop/market stall → market (alt) → flower shop → decorated shop → tent/pyramids → ruined arch → temple/Parthenon → barn → hospital → city building |
| 5 | Bag (tag) → shopping bag → capsule/pill → road sign (2 styles) → battle axe on pedestal → fork/knife pedestal → lantern → crossed tools → scissors → bandage cross |
| 6 | Signpost (arrow, 2 styles) → bulletin board → traffic light → chess pawn → bishop → knight → rook → windrose → crossed arrows → target cross |
| 7 | Pennant flag → waving flag → cross marker → shield+cross → tombstone → bottle → angel statue → ornate cross monument |

---

### `Icons/Icons_Media.png`

| Property   | Value             |
|------------|-------------------|
| Sheet size | varies            |
| Grid       | ~11 cols × 8 rows |

**Visually confirmed row contents:**

| Row | Contents |
|-----|----------|
| 0 | Headphones → headset (boom mic) → earphones → speaker (sm/lg) → megaphone → horn (3 sizes) |
| 1 | Ear variants (hearing, muted, sound) → speaker volume icons (7 levels from muted to loud+) |
| 2 | Play window → filmstrip → film clapboard → ticker tape → film reel → movie camera → camera → camera (alt) → disco/lens → fan/shutter |
| 3 | Equalizer/mixing board → turntable → microphone variants (stand, handheld, stick) → boom mic → podium mic |
| 4 | Music note (quarter) → beamed notes → eighth note → music player display → cassette player → cassette tape → vinyl record → disco ball → bar graph equalizer → speaker grill |
| 5 | Piano keys → grand piano → banjo → acoustic guitar → electric guitar → bass guitar → tube/horn instrument → saxophone → trumpet/bugle |
| 6 | Harp → lyre → pan flute → pencil → eraser → crayon → warning triangle → tuning fork → music stand crossed → sound board → film reel (lg) → filmstrip (lg) |
| 7 | Drum kit → drum with sticks → cymbal → bowl/bongo → pot drum → flower/splash |

---

### `Icons/Icons_RPG.png`

| Property   | Value              |
|------------|--------------------|
| Sheet size | 256 × 256+ px      |
| Grid       | ~10 cols × 14 rows |

**Visually confirmed row contents:**

| Row | Contents |
|-----|----------|
| 0 | Sword (straight) → shield → hammer → crossed hammers → crossed swords/scissor → bow+arrow → boomerang → key → scepter → bone wand |
| 1 | Sword (broad) → sword (thick) → spiked mace → star mace → spear → ornate sword → bullet/projectile → *(empty)* |
| 2 | Dagger → throwing star → feather wand → needle/pin → magic wand → skull on stick → circular spell → snowflake/explosion |
| 3 | Helmet → feather/wing → battle angel → crossed tools → person silhouette → torso armor → eye/bullseye → amulet → chain/link → ankh |
| 4 | Lamp/lantern → lightning bolt → locked chest → *(3 cells empty)* |
| 5 | Flower (4-petal) → swirl → heart+arrow → heart → star → 4-star burst → frame/border → clover (4-leaf) → flower (5-petal) → cross → snowflake → running figure |
| 6 | ◎ target → ⊙ bullseye → horseshoe → heart (outline) → star (outline) → fist → square tile → yin-yang → paw print → 4-leaf clover → cross clover → crescent |
| 7 | Happy face → skull face → 4-arm star → person standing → figure running → directional cross → frame → horseshoe magnet → eye |
| 8 | Sun/radial burst → feathers → leaf (lg) → crossed swords (lg) → starburst crossed → crossed axes |
| 9 | Key (ornate) → crossed wrenches → book → crossed hearts → ornate heart → ornate key → *(empty)* |
| 10 | Coin (face) → coin (striped) → coin (dotted) → leaf coin → skull coin → flower coin → arrow+sword → electric sword → crossed electric |
| 11 | Person silhouette → bird/phoenix → wings spread → bird up → face with wings → bottle/elixir → cake → hand cursor → skull → skull+bones → skull (outline) → skull+wand |
| 12 | Snake → network/nodes → cat face → wolf face → devil face |
| 13 | Dollar bill → money bag (text) → coin purse → coin (O) → coin (S) → coin (blank) → stacked coins → stacked bills → bag (S) → bag (draw) |
| 14 | Gavel/hammer → crown badge → scales → ornate scales → heart shield → gift box |

---

### `Icons/Icons_Weather.png`

| Property   | Value          |
|------------|----------------|
| Sheet size | ~128 × 112 px  |
| Grid       | 5 cols × 7 rows |

**Visually confirmed row contents:**

| Row | Contents |
|-----|----------|
| 0 | Sun (rays) → crescent moon → biohazard → nuclear/radioactive → cloud → rain cloud → storm cloud (lightning) |
| 1 | Sun (ring) → crescent (alt) → skull/deathmask → saturn/planet → partial cloud → cloud+rain dots → umbrella open |
| 2 | Flame (ornate) → wind swirl → wave/water → snowflake → water drop |
| 3 | Candle flame (open) → leaf (wind) → maple leaf → simple leaf → water drop (outline) |
| 4 | Thermometer (empty) → thermometer (filled) |
| 5 | Snowflake (6-arm) → crystal snowflake → diamond/rhombus → flower crystal |

---

## Fonts

All font bitmaps: **570 × 150 px** — **19 cols × 5 rows**, **30 × 30 px** per cell = **95 glyphs**.

Glyph map (ASCII 0x20–0x7E, row-major order):
```
Row 0:  [sp] ! " # $ % & ' ( ) * + , - . / 0 1 2
Row 1:  3 4 5 6 7 8 9 : ; < = > ? @ A B C D E
Row 2:  F G H I J K L M N O P Q R S T U V W X
Row 3:  Y Z [ \ ] ^ _ ` a b c d e f g h i j k
Row 4:  l m n o p q r s t u v w x y z { | } ~
```

**Cell address formula:**
```c
uint8_t idx = char_code - 0x20;   // 0..94
uint8_t col = idx % 19;
uint8_t row = idx / 19;
int x = col * 30;
int y = row * 30;
// Blit: src rect = {x, y, advance_width, 30}
```

> ⚠️ All font PNGs use **white glyphs on a transparent background**.  
> For the e-paper display, transparent → white pixel, white glyph pixel → black pixel.

---

### Per-Glyph Advance Widths

Advance width = rightmost non-transparent pixel column + 1 px gap.  
Values measured by pixel analysis of each 30×30 cell.

#### `FONT_REGS_12.png` — Press Start 2P @ 12 px

Fixed-width bitmap font. All printable glyphs advance **20 px** except:

| Char | Advance |
|------|---------|
| ` ` (space) | 15 |
| `j` | 19 |
| *all others* | 20 |

#### `FONT_REGS_18.png` — Press Start 2P @ 18 px

| Char | Advance |
|------|---------|
| ` ` (space) | 15 |
| `j` | 20 |
| *all others* | 23 |

#### `FONT_REGS_24.png` — Press Start 2P @ 24 px

| Char | Advance |
|------|---------|
| ` ` (space) | 15 |
| `j` | 22 |
| *all others* | 25 |

#### `FONT_SCRIPT_24.png` — Jacquard 12 Script @ 24 px (variable width)

| Char | Adv | Char | Adv | Char | Adv | Char | Adv |
|------|-----|------|-----|------|-----|------|-----|
| ` ` | 15  | `!`  | 12  | `"`  | 17  | `#`  | 22  |
| `0` | 20  | `1`  | 18  | `2`  | 20  | `3`  | 19  |
| `4` | 20  | `5`  | 19  | `6`  | 19  | `7`  | 20  |
| `8` | 20  | `9`  | 19  | `A`  | 24  | `B`  | 24  |
| `C` | 23  | `D`  | 22  | `E`  | 23  | `F`  | 24  |
| `G` | 23  | `H`  | 24  | `I`  | 23  | `J`  | 23  |
| `K` | 24  | `L`  | 23  | `M`  | 24  | `N`  | 24  |
| `O` | 24  | `P`  | 23  | `Q`  | 24  | `R`  | 24  |
| `S` | 23  | `T`  | 25  | `U`  | 24  | `V`  | 25  |
| `W` | 24  | `X`  | 24  | `Y`  | 24  | `Z`  | 23  |
| `a` | 21  | `b`  | 21  | `c`  | 20  | `d`  | 20  |
| `e` | 20  | `f`  | 20  | `g`  | 21  | `h`  | 21  |
| `i` | 19  | `j`  | 19  | `k`  | 21  | `l`  | 19  |
| `m` | 24  | `n`  | 21  | `o`  | 21  | `p`  | 21  |
| `q` | 21  | `r`  | 20  | `s`  | 20  | `t`  | 19  |
| `u` | 22  | `v`  | 21  | `w`  | 24  | `x`  | 21  |
| `y` | 21  | `z`  | 21  |      |     |      |     |

#### `FONT_SCRIPT_36.png` — Jacquard 12 Script @ 36 px (variable width)

| Char | Adv | Char | Adv | Char | Adv | Char | Adv |
|------|-----|------|-----|------|-----|------|-----|
| ` ` | 15  | `0`  | 22  | `1`  | 19  | `2`  | 22  |
| `3` | 20  | `4`  | 23  | `5`  | 20  | `6`  | 20  |
| `7` | 23  | `8`  | 22  | `9`  | 20  | `A`  | 28  |
| `B` | 28  | `C`  | 26  | `D`  | 25  | `E`  | 26  |
| `F` | 28  | `G`  | 27  | `H`  | 28  | `I`  | 26  |
| `J` | 26  | `K`  | 29  | `L`  | 27  | `M`  | 29  |
| `N` | 28  | `O`  | 28  | `P`  | 27  | `Q`  | 29  |
| `R` | 29  | `S`  | 27  | `T`  | 30  | `U`  | 29  |
| `V` | 30  | `W`  | 29  | `X`  | 28  | `Y`  | 28  |
| `Z` | 26  | `a`  | 23  | `b`  | 23  | `c`  | 22  |
| `d` | 23  | `e`  | 23  | `f`  | 22  | `g`  | 23  |
| `h` | 23  | `i`  | 20  | `j`  | 20  | `k`  | 23  |
| `l` | 20  | `m`  | 28  | `n`  | 24  | `o`  | 23  |
| `p` | 24  | `q`  | 23  | `r`  | 23  | `s`  | 23  |
| `t` | 20  | `u`  | 25  | `v`  | 24  | `w`  | 28  |
| `x` | 24  | `y`  | 24  | `z`  | 23  |      |     |

---

## Summary Table

| Sheet | Sprite Size | Grid | Total | Sheet Px |
|-------|-------------|------|-------|----------|
| `Characters/32x32-Charset.png` | 32×32 | 8×21 | 168 | 256×672 |
| `Characters/32x32-Charset-Outline.png` | 32×32 | 8×21 | 168 | 256×672 |
| `Items/Items-24x24.png` | 24×24 | 4×4 | 16 | 96×96 |
| `Items/Items-24x24-outline.png` | 24×24 | 4×4 | 16 | 96×96 |
| `Items/Items2-24x24.png` | 24×24 | 4×2 | 8 | 96×48 |
| `Items/Items2-24x24-outline.png` | 24×24 | 4×2 | 8 | 96×48 |
| `Tiles/Crypt-16x16.png` | 16×16 | 8×3 | 24 | 128×48 |
| `Tiles/Dungeon-16x16.png` | 16×16 | 8×4 | 32 | 128×64 |
| `Tiles/Hold-16x16.png` | 16×16 | 8×3 | 24 | 128×48 |
| `Tiles/Land-16x16.png` | 16×16 | 8×4 | 32 | 128×64 |
| `Icons/Icons_RPG.png` | 16×16 | ~10×14 | ~140 | 256×256 |
| `Icons/Icons_Weather.png` | 16×16 | 5×7 | ~35 | 128×112 |
| `Icons/Icons_Controller.png` | 16×16 | ~16×8 | ~128 | 256×128 |
| `Icons/Icons_Map_Markers.png` | 16×16 | ~13×7 | ~91 | 256×128 |
| `Icons/Icons_Media.png` | 16×16 | ~11×8 | ~88 | 256×128 |
| `Fonts/FONT_REGS_12.png` | cell 30×30 | 19×5 | 95 glyphs | 570×150 |
| `Fonts/FONT_REGS_18.png` | cell 30×30 | 19×5 | 95 glyphs | 570×150 |
| `Fonts/FONT_REGS_24.png` | cell 30×30 | 19×5 | 95 glyphs | 570×150 |
| `Fonts/FONT_SCRIPT_24.png` | cell 30×30 | 19×5 | 95 glyphs | 570×150 |
| `Fonts/FONT_SCRIPT_36.png` | cell 30×30 | 19×5 | 95 glyphs | 570×150 |
| `Icons/Sprites/` *(individual)* | 16×16 | — | 1,476 | varies |
| `Icons/Sprites_Cropped/` *(individual)* | varies | — | 1,476 | tight-cropped |
