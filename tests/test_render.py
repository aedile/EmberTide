"""
tests/test_render.py — E2E visual tests for 10 FiestaQuest 200×200 scenes.

Design principles:
- NO tile wallpapers — they destroy readability
- Characters at 2× scale (64px) so they're the visual focus
- Clear UI zones: distinct header / content / footer bars
- Classic JRPG / Game Boy RPG aesthetic: stark black & white, clean borders
- Text always on a solid panel background so it's legible
- Every scene has a clear "game state" that communicates at a glance

Run:
    pytest tests/test_render.py -v
    open /tmp/fq_scenes/
"""

import os
import pytest
from PIL import Image, ImageDraw

from tests.renderer import (
    ROOT, new_canvas, blit_char, blit_item1, blit_item2,
    blit_tile, blit_icon, draw_text, draw_rect, draw_hp_bar,
    fill_tile_grid, text_width, _colorize_glyph,
)
from tests.renderer import FONT_PATHS
import tests.renderer as R

OUT_DIR = "/tmp/fq_scenes"
os.makedirs(OUT_DIR, exist_ok=True)

# Palette (Strictly 1-bit Monochrome for E-Paper)
BLACK   = (0, 0, 0, 255)
WHITE   = (255, 255, 255, 255)


ICON_RPG  = "assets/Icons/Icons_RPG.png"
ICON_WX   = "assets/Icons/Icons_Weather.png"
ICON_CTRL = "assets/Icons/Icons_Controller.png"
ICON_MAP  = "assets/Icons/Icons_Map_Markers.png"
ICON_MEDIA = "assets/Icons/Icons_Media.png"


# ---------------------------------------------------------------------------
# Shared helpers
# ---------------------------------------------------------------------------

def _save(img: Image.Image, name: str) -> str:
    path = os.path.join(OUT_DIR, name)
    img.convert("RGB").save(path)
    return path


def _assert_not_blank(img: Image.Image, label: str) -> None:
    pixels = list(img.convert("RGB").getdata())
    white_count = sum(1 for p in pixels if p == (255, 255, 255))
    ratio = white_count / len(pixels)
    assert ratio < 0.90, f"{label}: canvas appears blank ({ratio:.0%} white)"


def _region_has_dark(img: Image.Image, x: int, y: int, w: int, h: int) -> bool:
    crop = img.crop((x, y, x + w, y + h))
    return any(
        sum(crop.getpixel((px, py))[:3]) < 384
        for px in range(crop.width) for py in range(crop.height)
    )


def hline(canvas, y, x0=0, x1=200, color=BLACK):
    draw_rect(canvas, x0, y, x1 - x0, 1, fill=color)


def vline(canvas, x, y0=0, y1=200, color=BLACK):
    draw_rect(canvas, x, y0, 1, y1 - y0, fill=color)


def panel(canvas, x, y, w, h, fill=WHITE, border=BLACK, thickness=2):
    draw_rect(canvas, x, y, w, h, fill=fill, outline=border, thickness=thickness)


def centered_text(canvas, text, font, y, color=BLACK, width=200):
    tw = text_width(font, text)
    draw_text(canvas, text, font, (width - tw) // 2, y, color=color)


def text_line(canvas, text, font, x, y, color=BLACK):
    draw_text(canvas, text, font, x, y, color=color)


def header_bar(canvas, title, font="FONT_REGS_12"):
    """Dark header strip at top."""
    draw_rect(canvas, 0, 0, 200, 20, fill=BLACK)
    hline(canvas, 20, color=BLACK)
    tw = text_width(font, title)
    draw_text(canvas, title, font, (200 - tw) // 2, 4, color=WHITE)


def footer_bar(canvas, text, font="FONT_REGS_12"):
    """Dark footer strip at bottom."""
    draw_rect(canvas, 0, 180, 200, 20, fill=BLACK)
    hline(canvas, 179, color=BLACK)
    draw_text(canvas, text, font, 6, 184, color=WHITE)


# ---------------------------------------------------------------------------
# Scene 1 — Title Screen
# ---------------------------------------------------------------------------

class TestScene01Title:
    def test_title_screen(self):
        # Clean white canvas with a dark decorative border
        canvas = new_canvas(200, 200, WHITE)
        panel(canvas, 0, 0, 200, 200, fill=WHITE, border=BLACK, thickness=4)
        panel(canvas, 8, 8, 184, 184, fill=WHITE, border=BLACK, thickness=1)

        # Big title in script font — centered
        title = "FiestaQuest"
        tw = text_width("FONT_SCRIPT_36", title)
        draw_text(canvas, title, "FONT_SCRIPT_36", (200 - tw) // 2, 14, color=BLACK)

        # Decorative separator
        hline(canvas, 52, x0=20, x1=180)

        # Hero character — DARK KNIGHT — large, center stage
        blit_char(canvas, "DARK_KNIGHT", frame=0, dest_x=68, dest_y=60, scale=2)

        # Decorative separator above footer
        hline(canvas, 148, x0=20, x1=180)

        # Sword + shield flanking title area
        blit_icon(canvas, ICON_RPG, row=0, col=0, dest_x=18, dest_y=16, scale=2)  # sword
        blit_icon(canvas, ICON_RPG, row=0, col=1, dest_x=158, dest_y=16, scale=2) # shield

        # Subtitle
        centered_text(canvas, "A HERO RISES", "FONT_REGS_12", 155, color=BLACK)

        # Press start
        centered_text(canvas, "Press  START", "FONT_REGS_12", 175, color=BLACK)

        # Three stars bottom decoration
        for i in range(3):
            blit_icon(canvas, ICON_RPG, row=5, col=4, dest_x=76 + i * 20, dest_y=166)

        path = _save(canvas, "scene_01_title.png")
        _assert_not_blank(canvas, "title")
        assert _region_has_dark(canvas, 68, 60, 64, 64), "Hero sprite missing"
        assert os.path.exists(path)


# ---------------------------------------------------------------------------
# Scene 2 — Battle Screen
# ---------------------------------------------------------------------------

class TestScene02Battle:
    def test_battle_screen(self):
        canvas = new_canvas(200, 200, WHITE)
        # Outer border
        panel(canvas, 0, 0, 200, 200, fill=WHITE, border=BLACK, thickness=2)

        # --- Enemy side (top-right) ---
        # Enemy nameplate
        panel(canvas, 96, 4, 98, 16, fill=BLACK)
        draw_text(canvas, "ARMORED BEAR", "FONT_REGS_12", 99, 7, color=WHITE)
        # Enemy HP bar
        draw_text(canvas, "HP", "FONT_REGS_12", 100, 22, color=BLACK)
        draw_hp_bar(canvas, 116, 22, 74, 8, pct=0.30, bg=WHITE, fg=BLACK, border=BLACK)
        # Enemy sprite — large, right side
        blit_char(canvas, "ARMORED_BEAR", frame=3, dest_x=128, dest_y=36, scale=2)

        # --- Player side (bottom-left) ---
        # Player sprite — large, left side
        blit_char(canvas, "DARK_KNIGHT", frame=1, dest_x=4, dest_y=60, scale=2)
        # Player nameplate
        panel(canvas, 4, 4, 88, 16, fill=BLACK)
        draw_text(canvas, "KNIGHT", "FONT_REGS_12", 7, 7, color=WHITE)
        # Player HP / MP bars
        draw_text(canvas, "HP", "FONT_REGS_12", 4,  22, color=BLACK)
        draw_hp_bar(canvas, 20, 22, 74, 8, pct=0.75, bg=WHITE, fg=BLACK, border=BLACK)
        draw_text(canvas, "MP", "FONT_REGS_12", 4,  34, color=BLACK)
        # Use an outlined bar for MP to differentiate in 1-bit
        draw_hp_bar(canvas, 20, 34, 74, 8, pct=0.50, bg=WHITE, fg=BLACK, border=BLACK)

        # --- Action menu (bottom panel) ---
        hline(canvas, 136)
        panel(canvas, 0, 137, 200, 63, fill=WHITE, border=BLACK, thickness=2)
        vline(canvas, 100, y0=137, y1=200)
        hline(canvas, 168, color=BLACK)

        draw_text(canvas, "ATTACK",  "FONT_REGS_12",  8, 144, color=BLACK)
        draw_text(canvas, "MAGIC",   "FONT_REGS_12",  8, 160, color=BLACK)
        draw_text(canvas, "ITEM",    "FONT_REGS_12",  8, 176, color=BLACK)
        draw_text(canvas, "FLEE",    "FONT_REGS_12", 108, 144, color=BLACK)

        # Icons next to actions
        blit_icon(canvas, ICON_RPG,  row=0, col=0, dest_x=64,  dest_y=144) # sword
        blit_icon(canvas, ICON_RPG,  row=2, col=4, dest_x=64,  dest_y=160) # wand
        blit_icon(canvas, ICON_RPG,  row=3, col=2, dest_x=64,  dest_y=176) # bag

        # Turn indicator
        draw_text(canvas, "> Your Turn", "FONT_REGS_12", 104, 176, color=BLACK)

        path = _save(canvas, "scene_02_battle.png")
        _assert_not_blank(canvas, "battle")
        assert _region_has_dark(canvas, 4, 60, 64, 64), "Player sprite missing"
        assert os.path.exists(path)


# ---------------------------------------------------------------------------
# Scene 3 — Inventory
# ---------------------------------------------------------------------------

class TestScene03Inventory:
    def test_inventory_screen(self):
        canvas = new_canvas(200, 200, WHITE)
        panel(canvas, 0, 0, 200, 200, fill=WHITE, border=BLACK, thickness=2)

        # Header
        header_bar(canvas, "INVENTORY")

        # 4-col item grid with labels — 2× scale
        items = [
            (0,  "Key",    0,  ICON_RPG, 0, 7),   # key
            (4,  "Potion", 1,  None,     0, 0),
            (8,  "Shield", 2,  None,     0, 0),
            (12, "Dagger", 3,  None,     0, 0),
        ]

        col_w = 44
        for i, (iid, label, col_num, icon_sheet, icon_row, icon_col) in enumerate(items):
            cx = 12 + i * col_w
            # Item cell border
            panel(canvas, cx, 30, 36, 36, fill=WHITE, border=BLACK, thickness=1)
            blit_item1(canvas, iid, dest_x=cx + 6, dest_y=36, scale=1)
            tw = text_width("FONT_REGS_12", label)
            draw_text(canvas, label, "FONT_REGS_12", cx + (36 - tw) // 2, 70, color=BLACK)

        # Second row — items2
        items2 = [(0, "Flask"), (1, "Flask2"), (4, "Chest"), (5, "Open")]
        for i, (iid, label) in enumerate(items2):
            cx = 12 + i * col_w
            panel(canvas, cx, 84, 36, 36, fill=WHITE, border=BLACK, thickness=1)
            blit_item2(canvas, iid, dest_x=cx + 6, dest_y=90, scale=1)
            tw = text_width("FONT_REGS_12", label)
            draw_text(canvas, label, "FONT_REGS_12", cx + (36 - tw) // 2, 124, color=BLACK)

        # Divider
        hline(canvas, 136, x0=4, x1=196)

        # Character preview + stats side by side
        blit_char(canvas, "VAMPIRE_ROGUE", frame=0, dest_x=4, dest_y=142, scale=2)

        panel(canvas, 72, 140, 124, 56, fill=WHITE, border=BLACK, thickness=1)
        draw_text(canvas, "HP:   88 / 100", "FONT_REGS_12", 78, 147, color=BLACK)
        draw_text(canvas, "MP:   40 / 60",  "FONT_REGS_12", 78, 163, color=BLACK)
        draw_text(canvas, "Gold: 340",      "FONT_REGS_12", 78, 179, color=BLACK)
        blit_icon(canvas, ICON_RPG, row=13, col=7, dest_x=174, dest_y=179)

        path = _save(canvas, "scene_03_inventory.png")
        _assert_not_blank(canvas, "inventory")
        assert _region_has_dark(canvas, 12, 30, 180, 80), "Item grid missing"
        assert os.path.exists(path)


# ---------------------------------------------------------------------------
# Scene 4 — Dungeon Map
# ---------------------------------------------------------------------------

class TestScene04DungeonMap:
    def test_dungeon_map(self):
        # Dark dungeon aesthetic - strict B&W
        canvas = new_canvas(200, 200, BLACK)
        panel(canvas, 0, 0, 200, 200, fill=BLACK, border=WHITE, thickness=2)

        # Title
        draw_rect(canvas, 2, 2, 196, 18, fill=BLACK)
        centered_text(canvas, "CRYPT  LEVEL  2", "FONT_REGS_12", 6, color=WHITE)

        # Map area — clean grid of dungeon tiles at 2× scale making a room
        # Draw floor only in the center room area (not full wallpaper)
        room_x, room_y = 20, 28
        for r in range(6):
            for c in range(10):
                blit_tile(canvas, "dungeon", tile_id=1, dest_x=room_x + c * 16, dest_y=room_y + r * 16)
        # Room walls
        for c in range(10):
            blit_tile(canvas, "dungeon", tile_id=0, dest_x=room_x + c * 16, dest_y=room_y)
            blit_tile(canvas, "dungeon", tile_id=0, dest_x=room_x + c * 16, dest_y=room_y + 5 * 16)
        for r in range(6):
            blit_tile(canvas, "dungeon", tile_id=0, dest_x=room_x,           dest_y=room_y + r * 16)
            blit_tile(canvas, "dungeon", tile_id=0, dest_x=room_x + 9 * 16, dest_y=room_y + r * 16)

        # Character 2× scale, center of room
        blit_char(canvas, "BONE_KNIGHT", frame=0, dest_x=80, dest_y=72, scale=2)

        # Mini-map panel top-right
        panel(canvas, 148, 24, 48, 48, fill=BLACK, border=WHITE, thickness=2)
        draw_text(canvas, "MAP", "FONT_REGS_12", 160, 26, color=WHITE)
        # Player dot on mini-map
        draw_rect(canvas, 168, 44, 4, 4, fill=WHITE)
        # Mini map room outline
        draw_rect(canvas, 152, 36, 40, 28, fill=None, outline=WHITE)

        # Bottom status bar
        draw_rect(canvas, 2, 178, 196, 20, fill=BLACK)
        blit_icon(canvas, ICON_MAP, row=0, col=7, dest_x=4, dest_y=180)
        draw_text(canvas, "Crypt LV2  |  Rm 3/8", "FONT_REGS_12", 24, 182, color=WHITE)

        path = _save(canvas, "scene_04_dungeon_map.png")
        _assert_not_blank(canvas, "dungeon_map")
        assert _region_has_dark(canvas, 80, 72, 64, 64), "Knight missing"
        assert os.path.exists(path)


# ---------------------------------------------------------------------------
# Scene 5 — Crypt Exploration
# ---------------------------------------------------------------------------

class TestScene05Crypt:
    def test_crypt_exploration(self):
        # Pure B&W
        BG = BLACK
        canvas = new_canvas(200, 200, BG)
        panel(canvas, 0, 0, 200, 200, fill=BG, border=WHITE, thickness=2)

        # Floor strip — a narrow strip of crypt tiles at bottom of play area
        for c in range(13):
            blit_tile(canvas, "crypt", tile_id=16, dest_x=c * 16, dest_y=130)
        # Wall tops above floor
        for c in range(13):
            blit_tile(canvas, "crypt", tile_id=0, dest_x=c * 16, dest_y=114)

        # Header: location name
        draw_rect(canvas, 2, 2, 196, 18, fill=BLACK)
        draw_text(canvas, "The Ancient Crypt", "FONT_REGS_12", 22, 6, color=WHITE)
        blit_icon(canvas, ICON_RPG, row=4, col=0, dest_x=4, dest_y=4)  # lantern

        # PLAYER — left, 2× scale
        blit_char(canvas, "BONE_KNIGHT", frame=0, dest_x=4, dest_y=50, scale=2)

        # ENEMY — right, 2× scale
        blit_char(canvas, "SKULL_MAGE", frame=4, dest_x=128, dest_y=46, scale=2)

        # Central lantern icon (large)  — atmospheric
        blit_icon(canvas, ICON_RPG, row=4, col=0, dest_x=88, dest_y=68, scale=3)

        # Dialogue / narration box at bottom
        panel(canvas, 2, 148, 196, 48, fill=BLACK, border=WHITE, thickness=2)
        draw_text(canvas, "A figure emerges from", "FONT_REGS_12", 8, 154, color=WHITE)
        draw_text(canvas, "the shadows...",         "FONT_REGS_12", 8, 170, color=WHITE)
        # Continue arrow
        draw_text(canvas, "v", "FONT_REGS_12", 178, 176, color=WHITE)

        path = _save(canvas, "scene_05_crypt.png")
        _assert_not_blank(canvas, "crypt")
        assert _region_has_dark(canvas, 4, 50, 64, 64), "Player missing"
        assert os.path.exists(path)


# ---------------------------------------------------------------------------
# Scene 6 — Dialogue Box
# ---------------------------------------------------------------------------

class TestScene06Dialogue:
    def test_dialogue_box(self):
        # Pure B&W outdoor
        canvas = new_canvas(200, 200, WHITE)
        panel(canvas, 0, 0, 200, 200, fill=WHITE, border=BLACK, thickness=2)

        # A single row of land tiles as ground line
        for c in range(13):
            blit_tile(canvas, "land", tile_id=0, dest_x=c*16, dest_y=110)

        # NPC character — large, center-left
        blit_char(canvas, "SKULL_MAGE", frame=0, dest_x=30, dest_y=32, scale=2)

        # Player character — small, right
        blit_char(canvas, "DARK_KNIGHT", frame=0, dest_x=142, dest_y=64)

        # NPC name tag above character
        panel(canvas, 18, 22, 60, 14, fill=BLACK, border=BLACK, thickness=1)
        draw_text(canvas, "Zara", "FONT_REGS_12", 28, 25, color=WHITE)

        # Dialogue box — bottom half
        panel(canvas, 2, 128, 196, 70, fill=WHITE, border=BLACK, thickness=3)
        panel(canvas, 4, 130, 192, 66, fill=WHITE, border=BLACK, thickness=1)

        # NPC portrait box inside dialogue
        panel(canvas, 8, 134, 36, 36, fill=WHITE, border=BLACK, thickness=2)
        blit_char(canvas, "SKULL_MAGE", frame=0, dest_x=10, dest_y=136)

        # Text
        draw_text(canvas, "Traveler! Beware the",  "FONT_REGS_12", 50, 136, color=BLACK)
        draw_text(canvas, "ancient evil that",     "FONT_REGS_12", 50, 152, color=BLACK)
        draw_text(canvas, "stirs in the crypt...", "FONT_REGS_12", 50, 168, color=BLACK)

        # ▼ continue
        draw_text(canvas, "v", "FONT_REGS_12", 186, 186, color=BLACK)

        path = _save(canvas, "scene_06_dialogue.png")
        _assert_not_blank(canvas, "dialogue")
        assert _region_has_dark(canvas, 50, 136, 140, 50), "Dialogue text missing"
        assert os.path.exists(path)


# ---------------------------------------------------------------------------
# Scene 7 — Quest Log
# ---------------------------------------------------------------------------

class TestScene07QuestLog:
    def test_quest_log(self):
        canvas = new_canvas(200, 200, WHITE)
        panel(canvas, 0, 0, 200, 200, fill=WHITE, border=BLACK, thickness=3)
        panel(canvas, 6, 6, 188, 188, fill=WHITE, border=BLACK, thickness=1)

        # Title — script font, underlined
        title = "Quest Log"
        tw = text_width("FONT_SCRIPT_36", title)
        draw_text(canvas, title, "FONT_SCRIPT_36", (200 - tw) // 2, 10, color=BLACK)
        hline(canvas, 42, x0=16, x1=184)

        # Quest entries — icon + status marker + text
        quests = [
            (True,  (0, 0),  "Find the Lost Key"),
            (True,  (0, 3),  "Slay the Bone Archer"),
            (False, (1, 0),  "Reach the Summit"),
            (False, (6, 4),  "Open the Crypt Gate"),
        ]
        for i, (done, (ir, ic), text) in enumerate(quests):
            y = 50 + i * 34
            status_icon = ICON_MAP
            blit_icon(canvas, status_icon, row=ir, col=ic, dest_x=12, dest_y=y, scale=2)
            draw_text(canvas, text, "FONT_REGS_12", 50, y + 4, color=BLACK)
            # Completion marker
            if done:
                draw_text(canvas, "[DONE]", "FONT_REGS_12", 136, y + 4, color=BLACK)
            hline(canvas, y + 28, x0=10, x1=190, color=BLACK)

        # Bottom motto
        centered_text(canvas, "Many roads, one quest.", "FONT_REGS_12", 184, color=BLACK)

        path = _save(canvas, "scene_07_quest_log.png")
        _assert_not_blank(canvas, "quest_log")
        assert _region_has_dark(canvas, (200 - tw) // 2, 10, tw, 34), "Title missing"
        assert os.path.exists(path)


# ---------------------------------------------------------------------------
# Scene 8 — Overworld Map
# ---------------------------------------------------------------------------

class TestScene08Overworld:
    def test_overworld_map(self):
        # Pure B&W Map view
        BG = WHITE
        canvas = new_canvas(200, 200, BG)
        panel(canvas, 0, 0, 200, 200, fill=BG, border=BLACK, thickness=2)

        # Organized land tile background — just 3 rows of ground at 2× scale
        for c in range(7):
            blit_tile(canvas, "land", tile_id=0, dest_x=c * 32, dest_y=96, scale=2)
            blit_tile(canvas, "land", tile_id=0, dest_x=c * 32, dest_y=128, scale=2)

        # Water patch — a clear defined region
        for c in range(3):
            blit_tile(canvas, "land", tile_id=20, dest_x=96 + c * 32, dest_y=64, scale=2)
            blit_tile(canvas, "land", tile_id=20, dest_x=96 + c * 32, dest_y=96, scale=2)

        # Title bar
        draw_rect(canvas, 2, 2, 196, 18, fill=BLACK)
        draw_text(canvas, "Overworld Map", "FONT_REGS_12", 40, 5, color=WHITE)
        blit_icon(canvas, ICON_WX, row=0, col=0, dest_x=4, dest_y=3)   # sun
        blit_icon(canvas, ICON_WX, row=0, col=0, dest_x=178, dest_y=3) # sun

        # Location markers — prominent, 2× scale
        blit_icon(canvas, ICON_MAP, row=3, col=0, dest_x=16,  dest_y=56, scale=2)  # village
        blit_icon(canvas, ICON_MAP, row=3, col=1, dest_x=80,  dest_y=30, scale=2)  # castle
        blit_icon(canvas, ICON_MAP, row=0, col=7, dest_x=144, dest_y=42, scale=2)  # pin (dungeon)

        # Location labels
        draw_text(canvas, "Elmvale",  "FONT_REGS_12", 8,   80, color=BLACK)
        draw_text(canvas, "Keep",     "FONT_REGS_12", 80,  54, color=BLACK)
        draw_text(canvas, "Crypt",    "FONT_REGS_12", 140, 66, color=BLACK)

        # Player character on map
        blit_char(canvas, "DARK_KNIGHT", frame=0, dest_x=68, dest_y=96, scale=2)

        # Coordinates bottom bar
        draw_rect(canvas, 2, 178, 196, 20, fill=BLACK)
        draw_text(canvas, "X: 14   Y: 22   Level: 3", "FONT_REGS_12", 14, 183, color=WHITE)

        path = _save(canvas, "scene_08_overworld.png")
        _assert_not_blank(canvas, "overworld")
        assert _region_has_dark(canvas, 68, 96, 64, 64), "Player on overworld missing"
        assert os.path.exists(path)


# ---------------------------------------------------------------------------
# Scene 9 — Chest Open Scene
# ---------------------------------------------------------------------------

class TestScene09ChestOpen:
    def test_chest_scene(self):
        canvas = new_canvas(200, 200, WHITE)
        panel(canvas, 0, 0, 200, 200, fill=WHITE, border=BLACK, thickness=3)

        # Glowing header (monochrome styling)
        draw_rect(canvas, 3, 3, 194, 22, fill=BLACK)
        centered_text(canvas, "Treasure Found!", "FONT_REGS_12", 8, color=WHITE)

        # Large chest (2× scale) — center, prominent
        blit_item2(canvas, item_id=5, dest_x=74, dest_y=36, scale=3)   # open chest 3×

        # Loot dropping out — items arranged around chest
        #  Key — left of chest
        panel(canvas, 10, 60, 36, 36, fill=WHITE, border=BLACK, thickness=1)
        blit_item1(canvas, item_id=0, dest_x=16, dest_y=66, scale=1)
        draw_text(canvas, "Key", "FONT_REGS_12", 14, 98, color=BLACK)

        # Potion — right of chest
        panel(canvas, 154, 60, 36, 36, fill=WHITE, border=BLACK, thickness=1)
        blit_item1(canvas, item_id=4, dest_x=160, dest_y=66, scale=1)
        draw_text(canvas, "Pot.", "FONT_REGS_12", 160, 98, color=BLACK)

        # "x1" badge on each
        draw_text(canvas, "x1", "FONT_REGS_12", 32, 86, color=BLACK)
        draw_text(canvas, "x2", "FONT_REGS_12", 168, 86, color=BLACK)

        # Divider
        hline(canvas, 112, x0=4, x1=196)

        # Character reaction
        blit_char(canvas, "FLORAL_WITCH", frame=0, dest_x=4, dest_y=118, scale=2)

        # Take / leave buttons
        panel(canvas, 72, 122, 124, 20, fill=BLACK, border=BLACK, thickness=1)
        blit_icon(canvas, ICON_CTRL, row=4, col=0, dest_x=76, dest_y=124) # A
        draw_text(canvas, "Take All", "FONT_REGS_12", 96, 126, color=WHITE)

        panel(canvas, 72, 146, 124, 20, fill=WHITE, border=BLACK, thickness=2)
        blit_icon(canvas, ICON_CTRL, row=4, col=1, dest_x=76, dest_y=148) # B
        draw_text(canvas, "Leave",    "FONT_REGS_12", 96, 150, color=BLACK)

        # Gold reward
        draw_text(canvas, "+ 120 Gold", "FONT_REGS_12", 80, 172, color=BLACK)
        blit_icon(canvas, ICON_RPG, row=13, col=7, dest_x=164, dest_y=170)

        path = _save(canvas, "scene_09_chest.png")
        _assert_not_blank(canvas, "chest")
        assert _region_has_dark(canvas, 74, 36, 72, 72), "Chest missing"
        assert os.path.exists(path)


# ---------------------------------------------------------------------------
# Scene 10 — Boss Encounter
# ---------------------------------------------------------------------------

class TestScene10Boss:
    def test_boss_encounter(self):
        # Pure B&W
        BG = BLACK
        canvas = new_canvas(200, 200, BG)

        # Ominous border
        draw_rect(canvas, 0, 0, 200, 200, fill=None, outline=WHITE, thickness=3)
        draw_rect(canvas, 6, 6, 188, 188, fill=None, outline=WHITE, thickness=1)

        # Boss name
        boss_name = "King  Ursa"
        bw = text_width("FONT_SCRIPT_36", boss_name)
        draw_text(canvas, boss_name, "FONT_SCRIPT_36", (200 - bw) // 2, 6, color=WHITE)
        hline(canvas, 42, x0=16, x1=184, color=WHITE)

        # Boss HP bar — full width, prominent
        draw_text(canvas, "BOSS", "FONT_REGS_12", 8, 46, color=WHITE)
        draw_hp_bar(canvas, 40, 44, 154, 12, pct=0.65,
                    bg=BLACK, fg=WHITE, border=WHITE)
        hline(canvas, 60, x0=8, x1=192, color=WHITE)

        # Skull flanking boss
        blit_icon(canvas, ICON_RPG, row=11, col=7, dest_x=8,  dest_y=44, scale=1)
        blit_icon(canvas, ICON_RPG, row=11, col=7, dest_x=180, dest_y=44)

        # ARMORED BEAR — very large, center
        blit_char(canvas, "ARMORED_BEAR", frame=3, dest_x=52, dest_y=60, scale=3)

        # Player HP at bottom-left
        panel(canvas, 6, 150, 96, 46, fill=BLACK, border=WHITE, thickness=2)
        draw_text(canvas, "Knight", "FONT_REGS_12", 10, 154, color=WHITE)
        draw_text(canvas, "HP", "FONT_REGS_12", 10, 168, color=WHITE)
        draw_hp_bar(canvas, 30, 168, 64, 8, pct=0.80, bg=BLACK, fg=WHITE, border=WHITE)
        draw_text(canvas, "MP", "FONT_REGS_12", 10, 180, color=WHITE)
        draw_hp_bar(canvas, 30, 180, 64, 8, pct=0.60, bg=BLACK, fg=WHITE, border=WHITE)

        # Action bar bottom-right
        panel(canvas, 106, 150, 90, 46, fill=BLACK, border=WHITE, thickness=2)
        draw_text(canvas, "ATTACK", "FONT_REGS_12", 112, 154, color=WHITE)
        draw_text(canvas, "MAGIC",  "FONT_REGS_12", 112, 170, color=WHITE)
        draw_text(canvas, "FLEE",   "FONT_REGS_12", 112, 186, color=WHITE)

        path = _save(canvas, "scene_10_boss.png")
        _assert_not_blank(canvas, "boss")
        assert _region_has_dark(canvas, 52, 60, 96, 90), "Boss sprite missing"
        assert os.path.exists(path)
