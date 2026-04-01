# Phase 18: Screen Renderers — Real Content

**Status: COMPLETE** (all acceptance criteria met, all tests passing)

## Item 1: Wire Text and Sprites into Home Screen

### User Story
As a player looking at my home screen, I need to see my creature's name, level, class, HP bar with a number, win/loss record, and a character sprite — not empty rectangles.

### Acceptance Criteria
- [x] `screen_home.c` calls `fq_draw_text()` for: character name, "Lv.XX", "W:XXX", "L:XXX"
- [x] Calls `fq_blit_sprite()` for the character sprite (selected by `vm.sprite_base`)
- [x] HP bar shows both the filled bar AND the text "HP" label (white patch + text)
- [x] All text is legible at 200×200 (uses FONT_REGS_12)
- [x] Updated golden PNG (`scene_home.png`) shows real content

### Implementation Notes
- `asset_data.c` introduced as the single TU that includes sprite/font headers.
- HP overflow guard: `hp_percent` clamped to 100 before bar fill computation.

### Files Modified
- `components/presentation/src/screens/screen_home.c`
- `components/presentation/include/asset_data.h` (new)
- `components/presentation/src/asset_data.c` (new)
- `test/visual/golden/scene_home.png`

---

## Item 2: Wire Text and Sprites into Combat Screen

### User Story
As a player watching combat, I need to see both fighters' names, HP bars with numbers, the round counter, action text banner, and character sprites.

### Acceptance Criteria
- [x] `screen_combat.c` calls `fq_draw_text()` for: both names, "R:XX" round, HP labels, action text
- [x] Calls `fq_blit_sprite()` for both fighter sprites (top-right area for both zones)
- [x] Action text banner renders readable text, not just a white box
- [x] Updated golden PNG

### Files Modified
- `components/presentation/src/screens/screen_combat.c`
- `test/visual/golden/scene_combat.png`

---

## Item 3: Wire Text into Stats, Inventory, Training, Dialogue Screens

### User Story
As a player, every screen I navigate to should display readable information — stat labels with numbers, item names, training game name and score, dialogue text.

### Acceptance Criteria
- [x] `screen_stats.c`: "STR", "SPD", "PRC", "INT" labels + numeric values + "XP: XXXX/XXXX" + "Deaths: X"
- [x] `screen_inventory.c`: item name in tooltip panel, "INVENTORY" header text, item sprites in cells
- [x] `screen_training.c`: game type name, "SCORE: XX", "DONE"/"GO!"/"READY" state label
- [x] `ui_widgets.c` (dialogue): real text rendering with fq_draw_text for title and body, YES/NO labels
- [x] All updated golden PNGs

### Files Modified
- `components/presentation/src/screens/screen_stats.c`
- `components/presentation/src/screens/screen_inventory.c`
- `components/presentation/src/screens/screen_training.c`
- `components/presentation/src/ui_widgets.c`
- `test/visual/golden/scene_stats.png`
- `test/visual/golden/scene_inventory.png`
- `test/visual/golden/scene_training.png`
- `test/visual/golden/scene_dialogue.png`

---

## Quality Gate Results

| Gate | Result |
|------|--------|
| Host ctest (75 tests) | 100% PASS |
| Visual regression (9 goldens) | 100% PASS |
| idf.py build (ESP32-S3) | PASS — 0x550c0 bytes (22% of partition, 78% free) |
