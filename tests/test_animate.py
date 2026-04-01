"""
tests/test_animate.py — Animated GIF test: Game Boy style walking scene.

Produces /tmp/fq_scenes/anim_walk.gif

Scene: Dark Knight walks across an overworld path from left to right,
then enters a dungeon door. Turn-based movement: character walks a tile,
pauses, walks next tile. Classic Game Boy RPG pacing.

Animation structure:
  - Overworld walk: character moves R→R across 5 tiles at 2× scale
  - Each "step" = 4 frames of walk animation while sliding 8px right
  - Brief pause frame between steps (idle, foot planted)
  - Final frames: fade into doorway (character walks off-screen to door)
"""

import os
import pytest
from PIL import Image

from tests.renderer import (
    ROOT, new_canvas, blit_char, blit_tile, blit_icon,
    draw_text, draw_rect, fill_tile_grid, text_width,
)

OUT_DIR = "/tmp/fq_scenes"
os.makedirs(OUT_DIR, exist_ok=True)

# Palette (Strictly 1-bit Monochrome)
BLACK  = (0,   0,   0,   255)
WHITE  = (255, 255, 255, 255)

ICON_MAP = "assets/Icons/Icons_Map_Markers.png"
ICON_WX  = "assets/Icons/Icons_Weather.png"


def _hline(canvas, y, x0=0, x1=200, color=BLACK):
    draw_rect(canvas, x0, y, x1 - x0, 1, fill=color)


def _draw_overworld_bg(canvas: Image.Image, x_offset: int = 0) -> None:
    """
    Clean Game Boy DMG-style overworld in strict 1-bit B&W:
    - Solid sky band (white)
    - Simple silhouette hills (black outlines)
    - Solid white ground
    - Single tile-row dirt path the character walks on
    """
    # Sky and ground are all white
    draw_rect(canvas, 0, 0, 200, 200, fill=WHITE)

    # Sun
    blit_icon(canvas, ICON_WX, row=0, col=0, dest_x=170, dest_y=22, scale=2)

    # Hill silhouettes — black outlines to separate from white sky
    draw_rect(canvas,   0,  36,  70, 26, fill=WHITE, outline=BLACK)
    draw_rect(canvas,  55,  44,  90, 18, fill=WHITE, outline=BLACK)
    draw_rect(canvas, 130,  40,  70, 22, fill=WHITE, outline=BLACK)

    # Ground separator lines
    _hline(canvas, 62, color=BLACK)
    _hline(canvas, 164, color=BLACK)

    # One row of scrolling tiles on the path strip
    for c in range(14):
        blit_tile(canvas, "land", tile_id=3,
                  dest_x=(c * 16 - (x_offset % 16)), dest_y=108)


def _draw_dungeon_bg(canvas: Image.Image) -> None:
    """Dark dungeon interior background."""
    draw_rect(canvas, 0, 0, 200, 200, fill=BLACK)
    # Wall strip
    for c in range(14):
        blit_tile(canvas, "dungeon", tile_id=0, dest_x=c * 16, dest_y=60)
    # Floor
    for r in range(8):
        for c in range(14):
            blit_tile(canvas, "dungeon", tile_id=1, dest_x=c * 16, dest_y=76 + r * 16)


def _draw_hud(canvas: Image.Image, location: str) -> None:
    """Consistent HUD strip at top and bottom."""
    draw_rect(canvas, 0, 0, 200, 18, fill=BLACK)
    _hline(canvas, 18, color=WHITE)
    tw = text_width("FONT_REGS_12", location)
    draw_text(canvas, location, "FONT_REGS_12", (200 - tw) // 2, 2, color=WHITE)

    draw_rect(canvas, 0, 182, 200, 18, fill=BLACK)
    _hline(canvas, 181, color=WHITE)
    draw_text(canvas, "FiestaQuest", "FONT_REGS_12", 4, 185, color=WHITE)


def _draw_step_indicator(canvas: Image.Image, step: int, total: int) -> None:
    """Small footstep dots at the bottom."""
    dot_x = 140
    for i in range(total):
        # outlined vs filled dots for B&W
        draw_rect(canvas, dot_x + i * 8, 187, 5, 5, fill=(WHITE if i <= step else BLACK), outline=WHITE, thickness=1)


def build_frames() -> list[Image.Image]:
    """Build all animation frames. Returns list of PIL Images."""
    frames = []

    # ── Phase 1: Character walks R across overworld (5 steps × 4 sub-frames) ──
    WALK_FRAMES   = [0, 1, 2, 3]   # animation cols from sheet (0-3 are walk, 4 is alt stand)
    STEP_PIXELS   = 16             # pixels moved per step
    SUBSTEPS      = 4              # sub-frames per step (smooth movement)
    PIXEL_PER_SUB = STEP_PIXELS // SUBSTEPS
    CHAR_Y        = 86             # vertical position (on path)
    START_X       = 10             # initial character x

    for step in range(6):          # 6 walking steps across screen
        for sub in range(SUBSTEPS):
            char_x   = START_X + step * STEP_PIXELS + sub * PIXEL_PER_SUB
            scroll   = step * STEP_PIXELS + sub * PIXEL_PER_SUB
            anim_col = WALK_FRAMES[sub % len(WALK_FRAMES)]

            canvas = new_canvas(200, 200, WHITE)
            _draw_overworld_bg(canvas, x_offset=scroll // 2)
            _draw_hud(canvas, "Elmvale Road")
            _draw_step_indicator(canvas, step, 6)

            blit_char(canvas, "DARK_KNIGHT", frame=anim_col,
                      dest_x=char_x, dest_y=CHAR_Y, scale=2)
            frames.append(canvas.convert("RGB"))

        # Pause frame — idle pose after each step lands
        char_x = START_X + (step + 1) * STEP_PIXELS
        scroll = char_x
        canvas = new_canvas(200, 200, WHITE)
        _draw_overworld_bg(canvas, x_offset=scroll // 2)
        _draw_hud(canvas, "Elmvale Road")
        _draw_step_indicator(canvas, step, 6)
        blit_char(canvas, "DARK_KNIGHT", frame=0,
                  dest_x=char_x, dest_y=CHAR_Y, scale=2)
        frames.append(canvas.convert("RGB"))
        frames.append(canvas.convert("RGB"))  # hold 2 frames

    # ── Phase 2: Door approach — character near dungeon entrance ──
    door_x = START_X + 6 * STEP_PIXELS  # ~106
    for pause in range(4):
        canvas = new_canvas(200, 200, WHITE)
        _draw_overworld_bg(canvas, x_offset=60)
        # Dungeon entrance — represented by map marker + black rectangle
        draw_rect(canvas, 158, 60, 32, 56, fill=BLACK)
        draw_rect(canvas, 162, 64, 24, 52, fill=WHITE)   # doorway interior
        blit_icon(canvas, ICON_MAP, row=0, col=7, dest_x=158, dest_y=52, scale=2)  # location pin
        draw_text(canvas, "Crypt", "FONT_REGS_12", 156, 40, color=BLACK)

        _draw_hud(canvas, "Elmvale Road")
        blit_char(canvas, "DARK_KNIGHT", frame=0, dest_x=door_x, dest_y=CHAR_Y, scale=2)
        frames.append(canvas.convert("RGB"))

    # ── Phase 3: Character walks INTO the door ──
    for sub in range(6):
        enter_x  = door_x + sub * 8
        anim_col = WALK_FRAMES[sub % len(WALK_FRAMES)]
        canvas = new_canvas(200, 200, WHITE)
        _draw_overworld_bg(canvas, x_offset=60)
        draw_rect(canvas, 158, 60, 32, 56, fill=BLACK)
        draw_rect(canvas, 162, 64, 24, 52, fill=WHITE)
        blit_icon(canvas, ICON_MAP, row=0, col=7, dest_x=158, dest_y=52, scale=2)
        draw_text(canvas, "Crypt", "FONT_REGS_12", 156, 40, color=BLACK)
        _draw_hud(canvas, "Entering Crypt...")

        # Draw character with progressive clip — they "enter" the door
        sprite_canvas = new_canvas(200, 200, (0, 0, 0, 0))
        blit_char(sprite_canvas, "DARK_KNIGHT", frame=anim_col,
                  dest_x=enter_x, dest_y=CHAR_Y, scale=2)
        # Composite — only show portion left of doorway threshold
        visible_w = max(0, 162 - enter_x)
        if visible_w > 0:
            canvas.paste(sprite_canvas.crop((enter_x, CHAR_Y, enter_x + visible_w, CHAR_Y + 64)),
                         (enter_x, CHAR_Y))
        frames.append(canvas.convert("RGB"))

    # ── Phase 4: Screen transition — fade to black ──
    last_outdoor = frames[-1].copy()
    for i in range(8):
        overlay = Image.new("RGB", (200, 200), (0, 0, 0))
        alpha   = int(255 * (i / 7))
        blended = Image.blend(last_outdoor, overlay, alpha / 255)
        frames.append(blended)

    # ── Phase 5: Dungeon — character enters from left ──
    for step in range(6):
        for sub in range(SUBSTEPS):
            char_x   = -64 + step * STEP_PIXELS + sub * PIXEL_PER_SUB
            anim_col = WALK_FRAMES[sub % len(WALK_FRAMES)]
            canvas = new_canvas(200, 200, BLACK)
            _draw_dungeon_bg(canvas)
            _draw_hud(canvas, "The Ancient Crypt")

            if char_x >= -60:
                blit_char(canvas, "DARK_KNIGHT", frame=anim_col,
                          dest_x=max(0, char_x), dest_y=86, scale=2)
            frames.append(canvas.convert("RGB"))

        # Pause after each dungeon step
        char_x  = -64 + (step + 1) * STEP_PIXELS
        canvas  = new_canvas(200, 200, BLACK)
        _draw_dungeon_bg(canvas)
        _draw_hud(canvas, "The Ancient Crypt")
        if char_x >= -60:
            blit_char(canvas, "DARK_KNIGHT", frame=0,
                      dest_x=max(0, char_x), dest_y=86, scale=2)
        frames.append(canvas.convert("RGB"))
        frames.append(canvas.convert("RGB"))

    # ── Phase 6: Encounter flash ──
    final_char_x = -64 + 6 * STEP_PIXELS   # ~32
    for flash in range(6):
        canvas = new_canvas(200, 200, BLACK)
        _draw_dungeon_bg(canvas)
        _draw_hud(canvas, "The Ancient Crypt")
        blit_char(canvas, "DARK_KNIGHT", frame=0,
                  dest_x=max(0, final_char_x), dest_y=86, scale=2)

        # Skull mage appears on the right, flashing
        if flash % 2 == 0:
            blit_char(canvas, "SKULL_MAGE", frame=0,
                      dest_x=128, dest_y=82, scale=2)
        frames.append(canvas.convert("RGB"))

    return frames


class TestAnimateWalk:
    def test_animated_walk_gif(self):
        frames = build_frames()

        # Convert to palette (GIF requires indexed color)
        pal_frames = []
        for f in frames:
            pal = f.convert("P", palette=Image.ADAPTIVE, colors=32)
            pal_frames.append(pal)

        out_path = os.path.join(OUT_DIR, "anim_walk.gif")
        pal_frames[0].save(
            out_path,
            save_all=True,
            append_images=pal_frames[1:],
            loop=0,
            duration=80,   # ms per frame (~12 fps)
            optimize=False,
        )
        assert os.path.exists(out_path)
        assert os.path.getsize(out_path) > 10_000, "GIF seems too small to be valid"

        # Verify frame count
        result = Image.open(out_path)
        frame_count = 0
        try:
            while True:
                frame_count += 1
                result.seek(result.tell() + 1)
        except EOFError:
            pass
        assert frame_count >= 30, f"Too few frames: {frame_count}"
        print(f"\n✓ Animated GIF: {frame_count} frames → {out_path}")
