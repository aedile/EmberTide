"""
tests/test_sprites.py — RED-GREEN unit tests for FIESTAMON sprite map data.

These tests validate that the Python equivalents of the C accessor functions
(fm_chars.h, fm_items.h, fm_tiles.h, fm_icons.h, fm_fonts.h) produce exactly
the pixel rectangles we expect — and that the source PNG files actually contain
non-transparent pixels at those locations (pixel-level sanity checks).

Run with:   pytest tests/test_sprites.py -v
"""

import os
import sys
import pytest
from PIL import Image

# ---------------------------------------------------------------------------
# Python mirrors of the C accessor math
# (These are the "executable specification" — if these pass the C will too)
# ---------------------------------------------------------------------------

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def char_rect(char_row: int, frame: int, use_alt: bool = False) -> dict:
    """Mirror of FM_CHAR_RECT in fm_chars.h."""
    row = char_row + (1 if use_alt else 0)
    return {"x": frame * 32, "y": row * 32, "w": 32, "h": 32}


# Character archetype → primary sheet row
CHAR_ROWS = {
    "DARK_KNIGHT":    0,
    "BONE_KNIGHT":    2,
    "SKULL_MAGE":     4,
    "HORNED_DEMON":   6,
    "TROLL_BEAST":    8,
    "SLIME_GOBLIN":  10,
    "FAT_BOSS":      12,
    "VAMPIRE_ROGUE": 14,
    "FLORAL_WITCH":  16,
    "SHADOW_HULK":   17,
    "OGRE_BOSS":     18,
    "BEAR_SPIRIT":   19,
    "ARMORED_BEAR":  20,
}

import json

with open(os.path.join(ROOT, "tests", "font_metrics.json")) as f:
    FONT_METRICS = json.load(f)

def glyph_rect(font_name: str, ch: str) -> dict:
    idx = ord(ch) - 0x20
    col = idx % 19
    row = idx // 19
    m = FONT_METRICS[font_name][idx]
    return {"x": col * 30 + m['dx'], "y": row * 30 + m['dy'], "w": m['w'], "h": m['h']}


def item1_rect(item_id: int) -> dict:
    """Mirror of FM_ITEM1_RECT."""
    return {"x": (item_id % 4) * 24, "y": (item_id // 4) * 24, "w": 24, "h": 24}


def item2_rect(item_id: int) -> dict:
    return {"x": (item_id % 4) * 24, "y": (item_id // 4) * 24, "w": 24, "h": 24}


def tile_rect(tile_id: int, cols: int = 8) -> dict:
    return {"x": (tile_id % cols) * 16, "y": (tile_id // cols) * 16, "w": 16, "h": 16}


def icon_rect(row: int, col: int) -> dict:
    return {"x": col * 16, "y": row * 16, "w": 16, "h": 16}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def load(rel_path: str) -> Image.Image:
    return Image.open(os.path.join(ROOT, rel_path)).convert("RGBA")


def cell_has_pixels(img: Image.Image, rect: dict, min_alpha: int = 20) -> bool:
    """Return True if at least one non-transparent pixel exists in rect."""
    crop = img.crop((rect["x"], rect["y"],
                     rect["x"] + rect["w"], rect["y"] + rect["h"]))
    for px, py in [(x, y) for x in range(crop.width) for y in range(crop.height)]:
        if crop.getpixel((px, py))[3] >= min_alpha:
            return True
    return False


def white_cell_has_pixels(img: Image.Image, rect: dict, threshold: int = 50) -> bool:
    """
    For tile sheets: return True if ≥1 opaque pixel exists.
    Accepts both white-on-black tiles AND solid black fill tiles — both are valid.
    A fully transparent cell (all alpha=0) would be a truly empty slot.
    """
    crop = img.crop((rect["x"], rect["y"],
                     rect["x"] + rect["w"], rect["y"] + rect["h"]))
    img_rgba = crop.convert("RGBA")
    for px in range(img_rgba.width):
        for py in range(img_rgba.height):
            if img_rgba.getpixel((px, py))[3] > 10:
                return True
    return False


# ---------------------------------------------------------------------------
# ══════════════════════════  CHARACTER TESTS  ══════════════════════════════
# ---------------------------------------------------------------------------

class TestCharacters:

    def setup_method(self):
        self.img = load("assets/Characters/32x32-Charset.png")

    def test_sheet_dimensions(self):
        assert self.img.width == 256, f"Expected width 256, got {self.img.width}"
        assert self.img.height == 672, f"Expected height 672, got {self.img.height}"

    @pytest.mark.parametrize("name,row", CHAR_ROWS.items())
    def test_char_idle_frame_has_pixels(self, name, row):
        r = char_rect(row, frame=0)
        assert cell_has_pixels(self.img, r), \
            f"{name} idle frame at row={row} col=0 is empty (y={r['y']})"

    @pytest.mark.parametrize("name,row", CHAR_ROWS.items())
    def test_char_walk_frames_have_pixels(self, name, row):
        for frame in range(1, 8):
            r = char_rect(row, frame=frame)
            assert cell_has_pixels(self.img, r), \
                f"{name} walk frame {frame} at row={row} is empty"

    def test_dark_knight_rect_address(self):
        r = char_rect(CHAR_ROWS["DARK_KNIGHT"], frame=0)
        assert r == {"x": 0, "y": 0, "w": 32, "h": 32}

    def test_armored_bear_rect_address(self):
        r = char_rect(CHAR_ROWS["ARMORED_BEAR"], frame=0)
        assert r == {"x": 0, "y": 640, "w": 32, "h": 32}

    def test_bone_knight_walk3_rect(self):
        r = char_rect(CHAR_ROWS["BONE_KNIGHT"], frame=3)
        assert r == {"x": 96, "y": 64, "w": 32, "h": 32}

    def test_alt_row_offset(self):
        r_primary = char_rect(CHAR_ROWS["DARK_KNIGHT"], frame=0, use_alt=False)
        r_alt     = char_rect(CHAR_ROWS["DARK_KNIGHT"], frame=0, use_alt=True)
        assert r_alt["y"] == r_primary["y"] + 32


# ---------------------------------------------------------------------------
# ══════════════════════════  ITEM TESTS  ══════════════════════════════════
# ---------------------------------------------------------------------------

class TestItems:

    def setup_method(self):
        self.img1 = load("assets/Items/Items-24x24.png")
        self.img2 = load("assets/Items/Items2-24x24.png")

    def test_sheet1_dimensions(self):
        assert self.img1.width == 96
        assert self.img1.height == 96

    def test_sheet2_dimensions(self):
        assert self.img2.width == 96
        assert self.img2.height == 48

    @pytest.mark.parametrize("item_id", range(16))
    def test_item1_all_cells_have_pixels(self, item_id):
        r = item1_rect(item_id)
        assert cell_has_pixels(self.img1, r), \
            f"Items sheet 1 cell {item_id} (row={item_id//4} col={item_id%4}) is empty"

    @pytest.mark.parametrize("item_id", range(8))
    def test_item2_all_cells_have_pixels(self, item_id):
        r = item2_rect(item_id)
        assert cell_has_pixels(self.img2, r), \
            f"Items sheet 2 cell {item_id} is empty"

    def test_key_ornate_rect(self):
        assert item1_rect(0) == {"x": 0, "y": 0, "w": 24, "h": 24}

    def test_dagger_d_rect(self):
        assert item1_rect(15) == {"x": 72, "y": 72, "w": 24, "h": 24}

    def test_chest_closed_rect(self):
        # FmItem2Id.FM_ITEM_CHEST_CLOSED = 4
        assert item2_rect(4) == {"x": 0, "y": 24, "w": 24, "h": 24}


# ---------------------------------------------------------------------------
# ══════════════════════════  TILE TESTS  ══════════════════════════════════
# ---------------------------------------------------------------------------

class TestTiles:

    SHEETS = {
        "Crypt":   ("assets/Tiles/Crypt-16x16.png",   128,  48, 24),
        "Dungeon": ("assets/Tiles/Dungeon-16x16.png",  128,  64, 32),
        "Hold":    ("assets/Tiles/Hold-16x16.png",     128,  48, 24),
        "Land":    ("assets/Tiles/Land-16x16.png",     128,  64, 32),
    }

    @pytest.mark.parametrize("name,path_w_h_count", SHEETS.items())
    def test_sheet_dimensions(self, name, path_w_h_count):
        path, w, h, _ = path_w_h_count
        img = load(path)
        assert img.width == w,  f"{name}: expected width {w}, got {img.width}"
        assert img.height == h, f"{name}: expected height {h}, got {img.height}"

    @pytest.mark.parametrize("name,path_w_h_count", SHEETS.items())
    def test_all_tiles_have_pixels(self, name, path_w_h_count):
        path, _, _, count = path_w_h_count
        img = load(path)
        for tid in range(count):
            r = tile_rect(tid)
            assert white_cell_has_pixels(img, r), \
                f"{name} tile {tid} (row={tid//8} col={tid%8}) appears blank"

    def test_crypt_tile0_rect(self):
        assert tile_rect(0) == {"x": 0, "y": 0, "w": 16, "h": 16}

    def test_crypt_tile8_rect(self):  # row 1, col 0
        assert tile_rect(8) == {"x": 0, "y": 16, "w": 16, "h": 16}

    def test_dungeon_tile17_rect(self):  # row 2, col 1
        assert tile_rect(17) == {"x": 16, "y": 32, "w": 16, "h": 16}


# ---------------------------------------------------------------------------
# ══════════════════════════  ICON TESTS  ══════════════════════════════════
# ---------------------------------------------------------------------------

class TestIcons:

    def test_rpg_sword_rect(self):
        r = icon_rect(row=0, col=0)
        assert r == {"x": 0, "y": 0, "w": 16, "h": 16}
        img = load("assets/Icons/Icons_RPG.png")
        assert cell_has_pixels(img, r), "RPG sword icon is empty"

    def test_rpg_shield_rect(self):
        r = icon_rect(row=0, col=1)
        assert r == {"x": 16, "y": 0, "w": 16, "h": 16}

    def test_weather_sun_rect(self):
        r = icon_rect(row=0, col=0)
        img = load("assets/Icons/Icons_Weather.png")
        assert cell_has_pixels(img, r), "Weather sun icon is empty"

    def test_controller_btn_a_rect(self):
        # FM_CTRL_BTN_A = 0x40 → row=4, col=0
        r = icon_rect(row=4, col=0)
        assert r == {"x": 0, "y": 64, "w": 16, "h": 16}
        img = load("assets/Icons/Icons_Controller.png")
        assert cell_has_pixels(img, r), "Controller A button is empty"

    @pytest.mark.parametrize("row,col", [(r, c) for r in range(3) for c in range(4)])
    def test_map_marker_cells(self, row, col):
        r = icon_rect(row=row, col=col)
        img = load("assets/Icons/Icons_Map_Markers.png")
        assert cell_has_pixels(img, r), \
            f"Map marker row={row} col={col} is empty"

    @pytest.mark.parametrize("row,col", [(r, c) for r in range(3) for c in range(4)])
    def test_media_icon_cells(self, row, col):
        r = icon_rect(row=row, col=col)
        img = load("assets/Icons/Icons_Media.png")
        assert cell_has_pixels(img, r), \
            f"Media icon row={row} col={col} is empty"

    def test_packed_rpg_enum_address(self):
        # FM_RPG_SWORD = 0x00 → row=0 col=0
        packed = 0x00
        row = (packed >> 4) & 0xF
        col = packed & 0xF
        assert icon_rect(row, col) == {"x": 0, "y": 0, "w": 16, "h": 16}

    def test_packed_rpg_skull_bones(self):
        # FM_RPG_SKULL_BONES = 0xB8 → row=11 col=8
        packed = 0xB8
        row = (packed >> 4) & 0xF
        col = packed & 0xF
        r = icon_rect(row, col)
        assert r == {"x": 128, "y": 176, "w": 16, "h": 16}


# ---------------------------------------------------------------------------
# ══════════════════════════  FONT TESTS  ══════════════════════════════════
# ---------------------------------------------------------------------------

class TestFonts:

    FONT_PATHS = {
        "FONT_REGS_12":   "assets/Fonts/FONT_REGS_12.png",
        "FONT_REGS_18":   "assets/Fonts/FONT_REGS_18.png",
        "FONT_REGS_24":   "assets/Fonts/FONT_REGS_24.png",
        "FONT_SCRIPT_24": "assets/Fonts/FONT_SCRIPT_24.png",
        "FONT_SCRIPT_36": "assets/Fonts/FONT_SCRIPT_36.png",
    }

    def test_all_font_sheets_load(self):
        for name, path in self.FONT_PATHS.items():
            img = load(path)
            assert img.width == 570,  f"{name}: width {img.width}"
            assert img.height == 150, f"{name}: height {img.height}"

    def test_metrics_loaded(self):
        from tests.renderer import FONT_METRICS
        assert len(FONT_METRICS["FONT_REGS_12"]) == 95

    @pytest.mark.parametrize("font_name,path", FONT_PATHS.items())
    def test_all_glyph_cells_have_pixels(self, font_name, path):
        """White-on-transparent — every printable glyph cell must have ≥1 white px."""
        img = load(path)
        missing = []
        for ch in "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789":
            idx = ord(ch) - 0x20
            col, row = idx % 19, idx // 19
            r = {"x": col * 30, "y": row * 30, "w": 30, "h": 30}
            crop = img.crop((r["x"], r["y"], r["x"]+30, r["y"]+30))
            has_px = any(
                crop.getpixel((px, py))[3] > 10
                for px in range(30) for py in range(30)
            )
            if not has_px:
                missing.append(ch)
        assert not missing, f"{font_name}: empty glyph cells for: {missing}"
