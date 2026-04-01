# Phase 18: Screen Renderers — Real Content

## Item 1: Wire Text and Sprites into Home Screen

### User Story
As a player looking at my home screen, I need to see my creature's name, level, class, HP bar with a number, win/loss record, and a character sprite — not empty rectangles.

### Acceptance Criteria
- [ ] `screen_home.c` calls `fq_draw_text()` for: character name, "Lv.XX", "W:XXX", "L:XXX"
- [ ] Calls `fq_blit_sprite()` for the character sprite (selected by `vm.sprite_base`)
- [ ] HP bar shows both the filled bar AND the text "HP XX/XX"
- [ ] All text is legible at 200×200 (uses FONT_REGS_12)
- [ ] Updated golden PNG (`scene_home.png`) shows real content

### Files to Modify
- `components/presentation/src/screens/screen_home.c`
- `test/visual/golden/scene_home.png`

---

## Item 2: Wire Text and Sprites into Combat Screen

### User Story
As a player watching combat, I need to see both fighters' names, HP bars with numbers, the round counter, action text banner, and character sprites.

### Acceptance Criteria
- [ ] `screen_combat.c` calls `fq_draw_text()` for: both names, "R:XX" round, HP values, action text
- [ ] Calls `fq_blit_sprite()` for both fighter sprites (player bottom-left, enemy top-right)
- [ ] Action text banner renders readable text, not just a white box
- [ ] Updated golden PNG

### Files to Modify
- `components/presentation/src/screens/screen_combat.c`

---

## Item 3: Wire Text into Stats, Inventory, Training, Dialogue Screens

### User Story
As a player, every screen I navigate to should display readable information — stat labels with numbers, item names, training game name and score, dialogue text.

### Acceptance Criteria
- [ ] `screen_stats.c`: "STR", "SPD", "PRC", "INT" labels + numeric values + "XP: XXXX/XXXX" + "Rebirth: X"
- [ ] `screen_inventory.c`: item name in tooltip panel, "INVENTORY" header text, slot numbers
- [ ] `screen_training.c`: game type name, "SCORE: XX", "DONE"/"ACTIVE" state label
- [ ] `ui_widgets.c` (dialogue): real text wrapping with the compiled font (already uses fq_draw_text — just needs a real font_t passed in)
- [ ] All updated golden PNGs

### Files to Modify
- `components/presentation/src/screens/screen_stats.c`
- `components/presentation/src/screens/screen_inventory.c`
- `components/presentation/src/screens/screen_training.c`
- `components/presentation/src/ui_widgets.c`
