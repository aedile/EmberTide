# Phase 8: View Models & UI Screens 1

## Item 1: View Model Translation Layer

### User Story
As a presentation layer developer, I need cleanly decoupled "View Models" containing ONLY the text strings, integer bars, and sprite IDs so I don't have to directly query `fq_character_t` from the game core inside my drawing loops.

### Acceptance Criteria
- [ ] `components/presentation/include/view_models.h` defines `fq_vm_combat_t`, `fq_vm_home_t`, `fq_vm_inventory_t`.
- [ ] `components/presentation/src/vm_builder.c` translates a raw game state pointer into the isolated view model structs.

### Negative Test Requirements (from spec-challenger)
- **Null Safety:** Pass `NULL` state pointers to the View Model builder. Assert it safely populates the View Model with "ERROR" defaults strings and `0` values rather than dereferencing cleanly off a cliff.
- **Buffer Overflow on Name Mapping:** Map a base character with a maliciously modified name string missing a null terminator. Assert the `vm_builder` uses `strncpy(vm.name, src.name, 15)` to guarantee view model strings never clobber adjacent layout data.

### Implementation Steps
1. Define the structures with explicit `char name[16]` buffers and `uint8_t hp_percent` pre-calculated.
2. The core game invokes the builder to pass a safe snapshot of data to the rendering layer.

### Test Expectations
- `test_vm_builder.c` takes a character with 10/20 HP, generates a View Model, and asserts the View Model `hp_percent` is exactly 50%.

### Files to Create/Modify
- `components/presentation/include/view_models.h`
- `components/presentation/src/vm_builder.c`
- `test/host/test_vm_builder.c`

### Commit Messages
- `feat: abstract presentation data into disjoint view models`

---

## Item 2: Implement HOME Screen Renderer

### User Story
As a player, the home screen is my dashboard. It must draw my beast, my current level, gold, and contextual icons (WiFi status, Battery).

### Acceptance Criteria
- [ ] `components/presentation/include/screens/home.h` defines `render_home_screen(fb, vm_home)`.
- [ ] Blits the large character sprite centrally.
- [ ] Draws the Class Name, Level, and Gold at the top.
- [ ] Draws the main action carousel (Inventory, Battle, Train, System) at the bottom.

### Negative Test Requirements (from spec-challenger)
- **Missing Asset Fallback:** Provide a View Model requesting a `sprite_id` that does not exist in the lookup table (e.g. `999`). Assert the screen renderer falls back to a "Missing/Glitch" default sprite rather than crashing.
- **Number String Overflow:** Force the character to have `99,999,999` gold. Ensure `snprintf` is strictly used when generating the drawing text so it truncates to `9999999+` instead of overrunning the 200x200 string bounding limits.

### Implementation Steps
1. Use purely `fq_fb_*` and `fq_draw_text` primitives.
2. Layout coordinates strictly hardcoded for the 200x200 canvas.

### Test Expectations
- `test/visual/screens/test_home_screen.c` produces `scene_home.png` matching the design reference.

### Files to Create/Modify
- `components/presentation/include/screens/home.h`
- `components/presentation/src/screens/home.c`
- `test/visual/screens/test_home_screen.c`

### Commit Messages
- `feat: implement HOME screen rendering logic`

---

## Item 3: Implement INVENTORY Grids

### User Story
As an item hoarder, I need a clear 2x2 or 3x3 scrolling grid representing my equipped Jokers and my bag so I can make loadout decisions.

### Acceptance Criteria
- [ ] `render_inventory_screen(fb, vm_inv)` handles both grid drawing and a selection cursor box.
- [ ] Selected item prints its full name and +stat modifiers in the bottom text panel.

### Negative Test Requirements (from spec-challenger)
- **Cursor Out of Bounds:** Provide a view model where `cursor_index = 45` on an inventory sized for `32`. Verify the renderer clamps the box drawing safely inside the screen bounds.
- **Empty Description Box:** Hover a null inventory slot. Ensure the tooltip text renderer cleanly clears the bottom panel text box instead of leaving stale pixel artefacts from the previously hovered item.

### Implementation Steps
1. Compute grid offsets computationally: `x = col * 40; y = row * 40`.
2. Draw thick inverted boxes to represent selection cursors (E-paper high contrast).

### Test Expectations
- `test_inventory_screen.c` produces `scene_inventory.png` showing 5 items and the cursor hovering slot 2.

### Files to Create/Modify
- `components/presentation/include/screens/inventory.h`
- `components/presentation/src/screens/inventory.c`
- `test/visual/screens/test_inventory_screen.c`

### Commit Messages
- `feat: inventory cursor grid and tooltip renderer`
