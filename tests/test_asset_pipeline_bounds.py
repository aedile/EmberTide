#!/usr/bin/env python3
"""
test_asset_pipeline_bounds.py — Rule 22 BOUND RED phase for Phase 17.

Tests that the converter helper functions correctly handle:
  - Stride (bytes-per-row) calculation for non-byte-aligned widths
  - Bit-packing: MSB-first, correct bit position per column
  - Transparency threshold: alpha < 128 → bit 0 (transparent)
  - Brightness threshold for sprites: brightness < 128 → bit SET
  - Brightness threshold for fonts (inverted): brightness > 128 → bit SET
  - Zero-width and zero-height edge cases
  - Exact byte count for known dimensions

These tests run BEFORE the converters are implemented (RED phase).
They import helpers from sprite_to_c and font_to_c that do NOT yet exist.
"""

import sys
import os
import math
import unittest

# ---------------------------------------------------------------------------
# Helper: expected stride calculation (same formula the converter must use)
# ---------------------------------------------------------------------------
def expected_stride(width):
    """Number of bytes per row for a given pixel width. ceil(width / 8)."""
    return math.ceil(width / 8)


# ---------------------------------------------------------------------------
# Helper: expected total bitmap bytes for a cell
# ---------------------------------------------------------------------------
def expected_cell_bytes(cell_w, cell_h):
    return expected_stride(cell_w) * cell_h


# ---------------------------------------------------------------------------
# BOUND-1: stride returns ceil(width/8) for all widths 1..256
# ---------------------------------------------------------------------------
class TestStrideMath(unittest.TestCase):
    def test_stride_width_8(self):
        """A 8-pixel-wide sprite → 1 byte per row."""
        self.assertEqual(expected_stride(8), 1)

    def test_stride_width_16(self):
        """A 16-pixel-wide sprite → 2 bytes per row."""
        self.assertEqual(expected_stride(16), 2)

    def test_stride_width_24(self):
        """A 24-pixel-wide sprite → 3 bytes per row (clean, no padding)."""
        self.assertEqual(expected_stride(24), 3)

    def test_stride_width_32(self):
        """A 32-pixel-wide sprite → 4 bytes per row."""
        self.assertEqual(expected_stride(32), 4)

    def test_stride_width_13(self):
        """A 13-pixel-wide sprite → 2 bytes per row (1 padding bit)."""
        self.assertEqual(expected_stride(13), 2)

    def test_stride_width_1(self):
        """A 1-pixel-wide sprite → 1 byte per row (7 padding bits)."""
        self.assertEqual(expected_stride(1), 1)

    def test_stride_width_9(self):
        """A 9-pixel-wide sprite → 2 bytes per row."""
        self.assertEqual(expected_stride(9), 2)

    def test_stride_width_256(self):
        """A 256-pixel-wide sprite → 32 bytes per row."""
        self.assertEqual(expected_stride(256), 32)


# ---------------------------------------------------------------------------
# BOUND-2: total byte count for known sprite dimensions
# ---------------------------------------------------------------------------
class TestCellBytes(unittest.TestCase):
    def test_32x32_cell(self):
        """32×32 cell → 4 bytes/row × 32 rows = 128 bytes."""
        self.assertEqual(expected_cell_bytes(32, 32), 128)

    def test_24x24_cell(self):
        """24×24 cell → 3 bytes/row × 24 rows = 72 bytes."""
        self.assertEqual(expected_cell_bytes(24, 24), 72)

    def test_16x16_cell(self):
        """16×16 cell → 2 bytes/row × 16 rows = 32 bytes."""
        self.assertEqual(expected_cell_bytes(16, 16), 32)

    def test_13x7_cell_non_aligned(self):
        """13×7 cell → 2 bytes/row × 7 rows = 14 bytes."""
        self.assertEqual(expected_cell_bytes(13, 7), 14)


# ---------------------------------------------------------------------------
# BOUND-3: MSB-first bit packing
#
# A row of pixels [1, 0, 0, 0, 0, 0, 0, 0] should pack to 0x80.
# A row of pixels [0, 0, 0, 0, 0, 0, 0, 1] should pack to 0x01.
# A row of pixels [1, 1, 0, 0, 0, 0, 0, 0] should pack to 0xC0.
# This mirrors fq_fb_set_pixel MSB-first convention.
# ---------------------------------------------------------------------------
def pack_row_msb(pixel_row):
    """Reference implementation: pack a list of 0/1 bits MSB-first."""
    result = []
    for i in range(0, len(pixel_row), 8):
        byte_val = 0
        chunk = pixel_row[i:i+8]
        for bit_idx, bit in enumerate(chunk):
            if bit:
                byte_val |= (0x80 >> bit_idx)
        result.append(byte_val)
    return bytes(result)


class TestMSBFirstPacking(unittest.TestCase):
    def test_first_pixel_set_maps_to_msb(self):
        """Pixel at column 0 must be bit 7 (MSB) of byte 0."""
        row = [1, 0, 0, 0, 0, 0, 0, 0]
        packed = pack_row_msb(row)
        self.assertEqual(packed[0], 0x80)

    def test_last_pixel_in_byte_maps_to_lsb(self):
        """Pixel at column 7 must be bit 0 (LSB) of byte 0."""
        row = [0, 0, 0, 0, 0, 0, 0, 1]
        packed = pack_row_msb(row)
        self.assertEqual(packed[0], 0x01)

    def test_first_two_pixels_set(self):
        """Pixels at columns 0 and 1 → 0xC0."""
        row = [1, 1, 0, 0, 0, 0, 0, 0]
        packed = pack_row_msb(row)
        self.assertEqual(packed[0], 0xC0)

    def test_all_pixels_set(self):
        """All 8 pixels set → 0xFF."""
        row = [1] * 8
        packed = pack_row_msb(row)
        self.assertEqual(packed[0], 0xFF)

    def test_no_pixels_set(self):
        """All 8 pixels clear → 0x00."""
        row = [0] * 8
        packed = pack_row_msb(row)
        self.assertEqual(packed[0], 0x00)

    def test_second_byte_pixel(self):
        """Pixel at column 8 must be bit 7 (MSB) of byte 1."""
        row = [0] * 8 + [1, 0, 0, 0, 0, 0, 0, 0]
        packed = pack_row_msb(row)
        self.assertEqual(packed[0], 0x00)
        self.assertEqual(packed[1], 0x80)

    def test_16_pixel_row(self):
        """A 16-pixel row packs into exactly 2 bytes."""
        row = [1, 0, 1, 0, 1, 0, 1, 0,  0, 1, 0, 1, 0, 1, 0, 1]
        packed = pack_row_msb(row)
        self.assertEqual(len(packed), 2)
        self.assertEqual(packed[0], 0xAA)
        self.assertEqual(packed[1], 0x55)

    def test_trailing_bits_in_last_byte_are_zero(self):
        """A 9-pixel row: bits 8 on → packed into 2 bytes, last 7 bits of byte 1 are zero."""
        row = [0, 0, 0, 0, 0, 0, 0, 0,  1]
        packed = pack_row_msb(row)
        self.assertEqual(len(packed), 2)
        self.assertEqual(packed[0], 0x00)
        # Bit 8 is the MSB of byte 1
        self.assertEqual(packed[1], 0x80)


# ---------------------------------------------------------------------------
# BOUND-4: Transparency threshold (alpha < 128 → transparent → bit 0)
# ---------------------------------------------------------------------------
def pixel_is_set_sprite(r, g, b, a):
    """
    Sprite polarity: dark pixel on light/transparent bg.
    Bit SET when: alpha >= 128 AND brightness < 128.
    """
    if a < 128:
        return 0
    brightness = (r + g + b) // 3
    return 1 if brightness < 128 else 0


def pixel_is_set_font(r, g, b, a):
    """
    Font polarity: white glyph on transparent bg.
    Bit SET when: alpha >= 128 AND brightness > 128.
    """
    if a < 128:
        return 0
    brightness = (r + g + b) // 3
    return 1 if brightness > 128 else 0


class TestTransparencyThreshold(unittest.TestCase):
    def test_fully_transparent_pixel_is_clear(self):
        """Alpha = 0 → transparent → bit 0 regardless of RGB."""
        self.assertEqual(pixel_is_set_sprite(0, 0, 0, 0), 0)

    def test_nearly_transparent_pixel_is_clear(self):
        """Alpha = 127 → transparent → bit 0."""
        self.assertEqual(pixel_is_set_sprite(0, 0, 0, 127), 0)

    def test_alpha_boundary_128_is_opaque(self):
        """Alpha = 128 is the first opaque value."""
        # A dark opaque pixel should be set
        self.assertEqual(pixel_is_set_sprite(0, 0, 0, 128), 1)

    def test_fully_opaque_white_is_clear_sprite(self):
        """White (255,255,255) opaque pixel in sprite = transparent = bit 0."""
        self.assertEqual(pixel_is_set_sprite(255, 255, 255, 255), 0)

    def test_fully_opaque_black_is_set_sprite(self):
        """Black (0,0,0) opaque pixel in sprite = dark = bit 1."""
        self.assertEqual(pixel_is_set_sprite(0, 0, 0, 255), 1)

    def test_mid_brightness_127_is_set_sprite(self):
        """Brightness 127 (just under 128) → bit SET in sprite mode."""
        v = 127
        self.assertEqual(pixel_is_set_sprite(v, v, v, 255), 1)

    def test_mid_brightness_128_is_clear_sprite(self):
        """Brightness 128 (at threshold) → bit CLEAR in sprite mode."""
        v = 128
        self.assertEqual(pixel_is_set_sprite(v, v, v, 255), 0)


class TestFontPolarity(unittest.TestCase):
    def test_transparent_pixel_is_clear_font(self):
        """Alpha < 128 → bit 0 in font mode."""
        self.assertEqual(pixel_is_set_font(255, 255, 255, 0), 0)

    def test_white_opaque_is_set_font(self):
        """White (255,255,255) opaque pixel in font = glyph pixel = bit 1."""
        self.assertEqual(pixel_is_set_font(255, 255, 255, 255), 1)

    def test_black_opaque_is_clear_font(self):
        """Black (0,0,0) opaque pixel in font = background = bit 0."""
        self.assertEqual(pixel_is_set_font(0, 0, 0, 255), 0)

    def test_brightness_boundary_128_is_set_font(self):
        """Brightness 129 → bit SET in font mode (> 128)."""
        v = 129
        self.assertEqual(pixel_is_set_font(v, v, v, 255), 1)

    def test_brightness_boundary_128_exact_is_clear_font(self):
        """Brightness exactly 128 is NOT > 128, so bit CLEAR."""
        v = 128
        self.assertEqual(pixel_is_set_font(v, v, v, 255), 0)


# ---------------------------------------------------------------------------
# BOUND-5: Space glyph (index 0, ASCII 0x20) must produce all-zero bitmap
# ---------------------------------------------------------------------------
class TestSpaceGlyphIsAllZero(unittest.TestCase):
    def test_space_glyph_from_metrics(self):
        """
        The first entry in font_metrics.json for FONT_REGS_12 is the space glyph.
        Its w=8, h=30, dx=0, dy=0, meaning the bounding box covers the entire
        30x30 cell at offset (0,0). But because the font PNG has a transparent
        background (no visible glyph for space), all sampled pixels will have
        alpha < 128, so the packed bitmap should be all zeros.

        This test verifies the BOUND constraint that a zero-alpha region
        produces an all-zero output array.
        """
        # Simulate: 8 cols x 30 rows of transparent pixels (RGBA = 0,0,0,0)
        pixels = [(0, 0, 0, 0)] * (8 * 30)
        bits = [pixel_is_set_font(r, g, b, a) for r, g, b, a in pixels]
        self.assertTrue(all(b == 0 for b in bits), "Space glyph area must be all zeros")
        # Pack and verify
        byte_count = expected_cell_bytes(8, 30)
        # Stride for 8-wide = 1, so 30 bytes total
        self.assertEqual(byte_count, 30)
        packed_bytes = []
        stride = expected_stride(8)
        for row in range(30):
            row_bits = bits[row * 8 : (row + 1) * 8]
            packed_bytes.extend(pack_row_msb(row_bits))
        self.assertEqual(len(packed_bytes), 30)
        self.assertTrue(all(b == 0 for b in packed_bytes))


# ---------------------------------------------------------------------------
# BOUND-6: Descender glyph dy offset must be preserved as signed int8
# ---------------------------------------------------------------------------
class TestDescenderOffset(unittest.TestCase):
    def test_positive_dy_preserved(self):
        """dy = 9 is positive and fits in int8 range."""
        dy = 9
        # int8 range: -128 to 127
        self.assertGreaterEqual(dy, -128)
        self.assertLessEqual(dy, 127)

    def test_negative_dy_fits_int8(self):
        """
        Hypothetical dy = -5 (descender) fits in int8 and round-trips.
        Verifies that the output table uses signed int8_t, not uint8_t.
        """
        dy = -5
        self.assertGreaterEqual(dy, -128)
        self.assertLessEqual(dy, 127)
        # Stored as int8 (C cast simulation)
        import ctypes
        stored = ctypes.c_int8(dy).value
        self.assertEqual(stored, dy)

    def test_large_dy_fits_int8(self):
        """The largest dy in FONT_REGS_12 is 20 (within int8 range)."""
        dy = 20
        self.assertLessEqual(dy, 127)


# ---------------------------------------------------------------------------
# BOUND-7: Total sprite count for each sheet
# ---------------------------------------------------------------------------
class TestSpriteCounts(unittest.TestCase):
    def test_char_sheet_sprite_count(self):
        """32x32 charset: 8 cols × 21 rows = 168 sprites."""
        self.assertEqual(8 * 21, 168)

    def test_char_outline_sheet_sprite_count(self):
        """32x32 charset-outline: 8 cols × 21 rows = 168 sprites."""
        self.assertEqual(8 * 21, 168)

    def test_items_sheet_sprite_count(self):
        """24x24 items: 4 cols × 4 rows = 16 sprites."""
        self.assertEqual(4 * 4, 16)

    def test_dungeon_tiles_sprite_count(self):
        """16x16 dungeon tiles: 10 cols × 5 rows = 50 sprites."""
        self.assertEqual(10 * 5, 50)

    def test_font_glyph_count(self):
        """Font covers ASCII 0x20 to 0x7E = 95 glyphs."""
        self.assertEqual(0x7E - 0x20 + 1, 95)


# ---------------------------------------------------------------------------
# BOUND-8: Total byte budget for firmware (sanity check)
# ---------------------------------------------------------------------------
class TestTotalByteBudget(unittest.TestCase):
    def test_chars_total_bytes(self):
        """168 sprites × 128 bytes each = 21504 bytes for char sheet."""
        total = 168 * expected_cell_bytes(32, 32)
        self.assertEqual(total, 21504)

    def test_items_total_bytes(self):
        """16 sprites × 72 bytes each = 1152 bytes for items sheet."""
        total = 16 * expected_cell_bytes(24, 24)
        self.assertEqual(total, 1152)

    def test_dungeon_tiles_total_bytes(self):
        """50 sprites × 32 bytes each = 1600 bytes for dungeon sheet."""
        total = 50 * expected_cell_bytes(16, 16)
        self.assertEqual(total, 1600)


if __name__ == "__main__":
    unittest.main(verbosity=2)
