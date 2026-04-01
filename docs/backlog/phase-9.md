# Phase 9: View Models & UI Screens 2

## Item 1: Implement COMBAT HUD

### User Story
As a competitive battler, I need to see both characters clearly separated, dual HP/Stamina bars, and text popups indicating damage or item activations.

### Acceptance Criteria
- [ ] `render_combat_screen(fb, vm_combat)` divides the 200x200 screen into Top (Enemy) and Bottom (Player).
- [ ] Renders HP bars with segmented ticks so I can visually gauge fractional remaining health.
- [ ] If `vm_combat->action_text` is set, draws a solid white banner box in the middle with the text (e.g. "Bruiser used Cleave! -15").

### Negative Test Requirements (from spec-challenger)
- **0 Max HP Bar Math:** Provide a view model where `max_hp = 0` (corrupt instance). Assert the fractional HP bar width calculation does not divide by zero.
- **Stamina Negative Width:** Provide a view model where `stamina_percent` is somehow > 100%. Assert the bar width caps exactly at the pixel boundary without drawing off the canvas edge.

### Implementation Steps
1. Create `components/presentation/src/screens/combat.c`.
2. Ensure Enemy sprite has the `flip_x` attribute applied if we implement sprite flipping, otherwise orient them statically.

### Test Expectations
- `test_combat_screen.c` produces `scene_combat.png` identical to python visual test #2 and #10.

### Files to Create/Modify
- `components/presentation/include/screens/combat.h`
- `components/presentation/src/screens/combat.c`
- `test/visual/screens/test_combat_screen.c`

### Commit Messages
- `feat: implement COMBAT split-screen dual HUD`

---

## Item 2: Implement TRAINING and REBIRTH Dialogues

### User Story
As an interaction designer, I need a reusable "Dialogue Box" primitive that I can invoke for NPC text, Onboarding ("What is your name?"), and Rebirth confirmations.

### Acceptance Criteria
- [ ] `components/presentation/include/ui_widgets.h` defines `render_dialogue_box(fb, title, body, [yes/no])`.
- [ ] The widget guarantees text wrapping if a string exceeds ~25 characters per line (approx 180 pixels).
- [ ] Draws an ornate border around the panel overriding background noise.

### Negative Test Requirements (from spec-challenger)
- **Massive Word Wrap Failure:** Provide a string composed of a single 90-character unspaced word ("AAAAAAAAAAAAAAAA..."). Ensure the word-wrapper forcefully breaks the text at the panel pixel-width boundary to prevent off-screen writing, rather than searching eternally for a space character.
- **Excessive Lines:** Provide 5 paragraphs of text. Ensure the dialogue box isolates the first 4 lines and truncates with a static "..." indicator instead of writing downwards over the ESP32 hardware memory layer.

### Implementation Steps
1. Add text wrapping logic to `fq_text.c` or create a wrapper loop in `ui_widgets.c`.
2. Draw the dialogue box overlapping the bottom 80 pixels of the 200x200 display.

### Test Expectations
- `test_dialogue_widget.c` generates a frame showing wrapping mechanics logic.

### Files to Create/Modify
- `components/presentation/include/ui_widgets.h`
- `components/presentation/src/ui_widgets.c`
- `test/visual/screens/test_ui_widgets.c`

### Commit Messages
- `feat: text-wrapping dialogue box rendering widget`

---

## Item 3: Validate ALL Screens via Scenario Render Check

### User Story
As a CI/CD owner, before the PR is merged, I need a single test runner that produces exactly 10 PNGs corresponding to all Game States and asserts diff-equality against golden known-good PNGs.

### Acceptance Criteria
- [ ] `test/visual/run_scenario_tests.c` instantiates a global mock state encompassing Home, Combat, Inventory, Map, Rebirth...
- [ ] Generates `scenario_01.png` through `scenario_10.png`.
- [ ] Python `diff_screens.py` automatically exits code 1 if the pixel parity deviates more than 0.01% from `/goldens/`.

### Negative Test Requirements (from spec-challenger)
- **Pixel Fuzzer Rejection:** Deliberately alter 5 pixels in the center of `scene_01.png` outputs and run `diff_screens.py`. Confirm the pipeline accurately identifies and rejects the change with an explicit exit code 1 to halt the CI merge.

### Implementation Steps
1. Write the massive `scenario_tests.c` boilerplate mocking the view models.
2. Include the Python pixel-diffing script in the CTest phase boundaries.

### Test Expectations
- Automated checks pass. Modifying a coordinate by 1 pixel in `home.c` instantly causes the PR gate to fail.

### Files to Create/Modify
- `test/visual/run_scenario_tests.c`
- `test/visual/diff_screens.py` (review/update rules)

### Commit Messages
- `test: comprehensive 10-scene visual regression lock`
