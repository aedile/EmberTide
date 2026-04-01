"""
tests/renderer.py — Python blit engine mirroring the C fm_*.h accessor logic.

Used by:
  - test_sprites.py  (imports advance tables for font tests)
  - test_render.py   (calls draw_* helpers to composite 200×200 scenes)

All coordinate math must stay identical to the C accessors in include/*.h.
"""

from __future__ import annotations
import os
from typing import Optional
from PIL import Image, ImageDraw
import json

# ---------------------------------------------------------------------------
# Project root
# ---------------------------------------------------------------------------
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------------------
# Asset loader cache
# ---------------------------------------------------------------------------
_cache: dict[str, Image.Image] = {}

def _load(rel: str) -> Image.Image:
    if rel not in _cache:
        path = os.path.join(ROOT, rel)
        _cache[rel] = Image.open(path).convert("RGBA")
    return _cache[rel]

# ---------------------------------------------------------------------------
# Advance-width tables (95 glyphs, index = char_code - 0x20)
# Mirror of FM_FONT_ADVANCE in fm_fonts.h
# ---------------------------------------------------------------------------

with open(os.path.join(ROOT, "tests", "font_metrics.json")) as f:
    FONT_METRICS = json.load(f)

FONT_PATHS = {
    "FONT_REGS_12":   "assets/Fonts/FONT_REGS_12.png",
    "FONT_REGS_18":   "assets/Fonts/FONT_REGS_18.png",
    "FONT_REGS_24":   "assets/Fonts/FONT_REGS_24.png",
    "FONT_SCRIPT_24": "assets/Fonts/FONT_SCRIPT_24.png",
    "FONT_SCRIPT_36": "assets/Fonts/FONT_SCRIPT_36.png",
}

# ---------------------------------------------------------------------------
# Character archetypes → primary sheet row (mirror of FM_CHARS table)
# ---------------------------------------------------------------------------
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

# ---------------------------------------------------------------------------
# Core blit helpers
# ---------------------------------------------------------------------------

def blit_char(
    canvas: Image.Image,
    name: str,
    frame: int,
    dest_x: int,
    dest_y: int,
    scale: int = 1,
    use_alt: bool = False,
    outline: bool = False,
) -> None:
    """Blit one 32×32 character frame onto canvas."""
    path = (
        "assets/Characters/32x32-Charset-Outline.png" if outline
        else "assets/Characters/32x32-Charset.png"
    )
    sheet = _load(path)
    row = CHAR_ROWS[name] + (1 if use_alt else 0)
    sx, sy = frame * 32, row * 32
    sprite = sheet.crop((sx, sy, sx + 32, sy + 32))
    if scale != 1:
        sprite = sprite.resize((32 * scale, 32 * scale), Image.NEAREST)
    # Alpha-composite directly: black lines show, white fills show, transparent bg is skipped
    canvas.paste(sprite, (dest_x, dest_y), sprite)


def blit_item1(
    canvas: Image.Image, item_id: int, dest_x: int, dest_y: int,
    scale: int = 1, outline: bool = False,
) -> None:
    path = ("assets/Items/Items-24x24-outline.png" if outline
            else "assets/Items/Items-24x24.png")
    sheet = _load(path)
    col, row = item_id % 4, item_id // 4
    sx, sy = col * 24, row * 24
    sprite = sheet.crop((sx, sy, sx + 24, sy + 24))
    if scale != 1:
        sprite = sprite.resize((24 * scale, 24 * scale), Image.NEAREST)
    canvas.paste(sprite, (dest_x, dest_y), sprite)


def blit_item2(
    canvas: Image.Image, item_id: int, dest_x: int, dest_y: int,
    scale: int = 1, outline: bool = False,
) -> None:
    path = ("assets/Items/Items2-24x24-outline.png" if outline
            else "assets/Items/Items2-24x24.png")
    sheet = _load(path)
    col, row = item_id % 4, item_id // 4
    sx, sy = col * 24, row * 24
    sprite = sheet.crop((sx, sy, sx + 24, sy + 24))
    if scale != 1:
        sprite = sprite.resize((24 * scale, 24 * scale), Image.NEAREST)
    canvas.paste(sprite, (dest_x, dest_y), sprite)


def blit_tile(
    canvas: Image.Image,
    tileset: str,    # "crypt" | "dungeon" | "hold" | "land"
    tile_id: int,
    dest_x: int,
    dest_y: int,
    scale: int = 1,
) -> None:
    paths = {
        "crypt":   "assets/Tiles/Crypt-16x16.png",
        "dungeon": "assets/Tiles/Dungeon-16x16.png",
        "hold":    "assets/Tiles/Hold-16x16.png",
        "land":    "assets/Tiles/Land-16x16.png",
    }
    sheet = _load(paths[tileset]).convert("RGB")
    col, row = tile_id % 8, tile_id // 8
    sx, sy = col * 16, row * 16
    tile = sheet.crop((sx, sy, sx + 16, sy + 16))
    if scale != 1:
        tile = tile.resize((16 * scale, 16 * scale), Image.NEAREST)
    # Tiles are white-on-black; place as-is on white canvas (they'll invert naturally)
    # Convert to RGBA so paste works
    tile_rgba = tile.convert("RGBA")
    canvas.paste(tile_rgba, (dest_x, dest_y), tile_rgba)


def blit_icon(
    canvas: Image.Image,
    sheet_path: str,
    row: int,
    col: int,
    dest_x: int,
    dest_y: int,
    scale: int = 1,
) -> None:
    sheet = _load(sheet_path)
    sx, sy = col * 16, row * 16
    sprite = sheet.crop((sx, sy, sx + 16, sy + 16))
    if scale != 1:
        sprite = sprite.resize((16 * scale, 16 * scale), Image.NEAREST)
    canvas.paste(sprite, (dest_x, dest_y), sprite)


def draw_text(
    canvas: Image.Image,
    text: str,
    font_name: str,
    x: int,
    y: int,
    color: tuple = (0, 0, 0, 255),
    max_width: Optional[int] = None,
) -> int:
    """
    Draw text onto canvas using the sprite font.
    Returns the x position after the last character.
    Glyphs are white-on-transparent → rendered as `color` on the canvas.
    """
    sheet = _load(FONT_PATHS[font_name])
    metrics = FONT_METRICS[font_name]
    cursor_x = x
    for ch in text:
        if max_width and (cursor_x - x) >= max_width:
            break
        if ch == '\n':
            y += 32
            cursor_x = x
            continue
        idx = ord(ch) - 0x20
        if idx < 0 or idx >= 95:
            cursor_x += metrics[0]['w']
            continue
        col_s = idx % 19
        row_s = idx // 19
        sx, sy = col_s * 30, row_s * 30
        
        m = metrics[idx]
        w, h, dx, dy = m['w'], m['h'], m['dx'], m['dy']
        
        # Only crop the tight visible bounds of the character
        glyph = sheet.crop((sx + dx, sy + dy, sx + dx + w, sy + dy + h))
        # Recolor: white glyph pixels → color
        colored = _colorize_glyph(glyph, color)
        
        # Paste securely positioned incorporating individual vertical offset
        canvas.paste(colored, (cursor_x, y + dy), colored)
        
        cursor_x += w
    return cursor_x


def text_width(font_name: str, text: str) -> int:
    metrics = FONT_METRICS[font_name]
    return sum(metrics[ord(c) - 0x20]['w'] for c in text if 0x20 <= ord(c) <= 0x7E)


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def new_canvas(w: int = 200, h: int = 200, bg: tuple = (255, 255, 255, 255)) -> Image.Image:
    return Image.new("RGBA", (w, h), bg)


def _invert_for_display(sprite: Image.Image) -> Image.Image:
    """
    No-op: sprites are already black+white on transparent background.
    Black pixels show on white canvas; white pixels blend in to white background.
    Just returns the sprite unchanged.
    """
    return sprite


def _colorize_glyph(glyph: Image.Image, color: tuple) -> Image.Image:
    """Replace white glyph pixels with `color`, keep alpha."""
    result = Image.new("RGBA", glyph.size, (0, 0, 0, 0))
    src = glyph.load()
    dst = result.load()
    cr, cg, cb, ca = color
    for py in range(glyph.height):
        for px in range(glyph.width):
            r, g, b, a = src[px, py]
            if a > 10:
                dst[px, py] = (cr, cg, cb, a)
    return result


def draw_rect(
    canvas: Image.Image,
    x: int, y: int, w: int, h: int,
    fill: Optional[tuple] = None,
    outline: Optional[tuple] = None,
    thickness: int = 1,
) -> None:
    d = ImageDraw.Draw(canvas)
    if fill:
        d.rectangle([x, y, x + w - 1, y + h - 1], fill=fill)
    if outline:
        for t in range(thickness):
            d.rectangle([x + t, y + t, x + w - 1 - t, y + h - 1 - t],
                        outline=outline)


def draw_hp_bar(
    canvas: Image.Image,
    x: int, y: int, w: int, h: int,
    pct: float,          # 0.0–1.0
    bg: tuple  = (200, 200, 200, 255),
    fg: tuple  = (0, 0, 0, 255),
    border: tuple = (0, 0, 0, 255),
) -> None:
    draw_rect(canvas, x, y, w, h, fill=bg, outline=border)
    filled = max(1, int(w * pct))
    draw_rect(canvas, x + 1, y + 1, filled - 2, h - 2, fill=fg)


def fill_tile_grid(
    canvas: Image.Image,
    tileset: str,
    tile_id: int,
    x0: int, y0: int,
    cols: int, rows: int,
    scale: int = 1,
) -> None:
    """Fill a rectangular region with a repeating tile."""
    sz = 16 * scale
    for r in range(rows):
        for c in range(cols):
            blit_tile(canvas, tileset, tile_id, x0 + c * sz, y0 + r * sz, scale)
