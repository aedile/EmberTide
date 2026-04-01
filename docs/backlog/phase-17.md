# Phase 17: Asset Pipeline — PNG to Packed C Arrays

## Item 1: Sprite Sheet Converter

### User Story
As the build system, I need a Python script that reads PNG sprite sheets and outputs C header files containing `static const uint8_t[]` arrays in 1-bit packed MSB-first format, so the firmware can blit sprites without a runtime image decoder.

### Acceptance Criteria
- [ ] `tools/sprite_to_c.py` reads a PNG sprite sheet + grid dimensions (cell_w, cell_h, cols, rows)
- [ ] Extracts each cell, converts to 1-bit (threshold: pixel brightness < 128 = black = bit set)
- [ ] Outputs packed MSB-first `uint8_t` arrays matching `fq_sprite_t` format
- [ ] Generates a C header with one array per sprite and an index table
- [ ] Processes: Characters (32x32, 8 cols × 21 rows), Items (24x24), Tiles (16x16)
- [ ] Output files: `components/presentation/include/sprites/sprite_chars.h`, `sprite_items.h`, `sprite_tiles.h`

### Negative Test Requirements
- **Transparency handling:** Pixels with alpha < 128 in the source PNG must map to 0 (transparent/white), not 1 (black). Verify with a sprite that has transparent background.
- **Non-byte-aligned widths:** A 24px wide sprite has 3 bytes per row (24/8 = 3, clean). But a hypothetical 13px sprite must pad to 2 bytes per row. Verify the script handles non-multiple-of-8 widths.

### Files to Create
- `tools/sprite_to_c.py`
- `components/presentation/include/sprites/sprite_chars.h`
- `components/presentation/include/sprites/sprite_items.h`
- `components/presentation/include/sprites/sprite_tiles.h`

---

## Item 2: Font Bitmap Converter

### User Story
As the text renderer, I need compiled font glyph bitmaps so `fq_draw_text()` can render readable text on the e-paper display.

### Acceptance Criteria
- [ ] `tools/font_to_c.py` reads a font PNG sheet (570×150, 19 cols × 5 rows, 30×30 cells) + `tests/font_metrics.json`
- [ ] Extracts each of the 95 glyphs (ASCII 0x20–0x7E), crops to actual bounding box using metrics (w, h, dx, dy)
- [ ] Converts to 1-bit packed MSB-first format
- [ ] Outputs `static const uint8_t` glyph bitmap arrays + width/offset tables matching `fq_font_t`
- [ ] Generates: `components/presentation/include/fonts/font_regs_12.h` (and optionally font_script_24.h)
- [ ] At minimum, one working font (FONT_REGS_12 — the small readable one) must be compiled in

### Negative Test Requirements
- **Space character:** Glyph for 0x20 (space) has zero visible pixels. Verify the bitmap array is all zeros and advance width is correct (15px per fm_fonts.h).
- **Descender glyphs:** Characters like 'g', 'y', 'p' have negative dy offsets. Verify the offset table preserves these correctly.

### Files to Create
- `tools/font_to_c.py`
- `components/presentation/include/fonts/font_regs_12.h`

---

## Item 3: Verify Asset Integration

### User Story
As a visual QA gate, I need a host test that creates a framebuffer, blits a real character sprite and draws real text, and outputs a PNG proving the assets render correctly.

### Acceptance Criteria
- [ ] `test/visual/test_asset_integration.c` blits the Dark Knight sprite at (84, 60) onto a framebuffer using the compiled sprite data
- [ ] Renders "FiestaQuest!" using the compiled font at (10, 10)
- [ ] Outputs `output/asset_integration.png`
- [ ] Visual inspection confirms sprite and text are legible at 200×200

### Files to Create
- `test/visual/test_asset_integration.c`
- `test/visual/golden/asset_integration.png`
