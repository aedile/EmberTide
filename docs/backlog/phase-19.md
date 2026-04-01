# Phase 19: Interactive Gameplay — Training, Items, Onboarding

## Item 1: Character Creation / Onboarding Screen

### User Story
As a new player on first boot, I want to pick my class and see my creature before the game starts, instead of being handed a hardcoded Bruiser named "Ember."

### Acceptance Criteria
- [ ] New FSM state `FQ_STATE_ONBOARDING` added, entered on first boot (when no save exists)
- [ ] `screen_onboarding.c` renders a class selection carousel: 5 classes with name, sprite preview, and stat summary
- [ ] Button A cycles through classes, Button B confirms selection
- [ ] After class selection, a two-part name is randomly generated (using PRNG seeded from `esp_random()`)
- [ ] Character is created via `fq_character_create()` with the chosen class and generated name
- [ ] Character is saved to flash immediately after creation
- [ ] FSM transitions to HOME after onboarding completes
- [ ] Subsequent boots skip onboarding (save file exists)

### Negative Test Requirements
- **Back-button during onboarding:** Button B on the first class (index 0) must not underflow to class 255.
- **Power loss during onboarding:** If the device loses power before the save completes, next boot should re-enter onboarding (no half-created character).

### Files to Create/Modify
- `components/presentation/include/screens/screen_onboarding.h`
- `components/presentation/src/screens/screen_onboarding.c`
- `main/app_fsm.h` / `main/app_fsm.c` (add FQ_STATE_ONBOARDING)
- `main/app_main.c` (route first-boot to onboarding instead of hardcoded creation)

---

## Item 2: Training Session Input Handling

### User Story
As a player in the training screen, I want to play a timing-based mini-game where I press Button A at the right moment to score hits, earning XP for my creature.

### Acceptance Criteria
- [ ] `FQ_STATE_TRAINING` handles `FQ_EVT_BTN_A_PRESS` as a "hit" input
- [ ] The training screen shows a moving target indicator (simple animation: a filled rect that moves across a timing bar)
- [ ] Pressing A when the target is in the "hit zone" scores a hit; pressing at the wrong time scores a miss
- [ ] After all targets are presented, the final score (0-100) is shown and XP is awarded via `fq_level_up()` if threshold met
- [ ] Button B exits training at any time (partial score, partial XP)
- [ ] The mini-game type (Speed/Power/Intel) is selectable from a pre-training menu

### Files to Create/Modify
- `main/app_fsm.c` (training state button handling)
- `components/presentation/src/screens/screen_training.c` (animated target + score display)
- `main/app_main.c` (XP award after training)

---

## Item 3: Inventory Equip/Unequip

### User Story
As a player in the inventory screen, I want to press Button A to equip or unequip the currently highlighted item, so I can customize my combat loadout.

### Acceptance Criteria
- [ ] `FQ_STATE_INVENTORY` handles `FQ_EVT_BTN_A_PRESS` as equip/unequip toggle
- [ ] Equipped items are visually distinguished (inverted cell or border marker)
- [ ] Equipping when all slots are full shows a "FULL" indicator (no crash, no silent overwrite)
- [ ] `equipped_count` is updated in `fq_character_t` and saved to flash after changes
- [ ] Button B returns to home (saving automatically)

### Files to Create/Modify
- `main/app_fsm.c` (inventory button handling)
- `components/presentation/src/screens/screen_inventory.c` (equipped visual indicator)
- `main/app_main.c` (save after equip change)
