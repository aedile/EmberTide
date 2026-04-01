#!/usr/bin/env python3
"""
font_to_c.py — FiestaQuest Phase 17 Asset Pipeline

Converts a font PNG sheet + per-glyph metrics JSON into a C header
containing packed 1-bit glyph bitmaps and the matching fq_font_t
descriptor struct.

The font PNG contains:
  - 570×150 px total, 19 cols × 5 rows, 30×30 px per cell.
  - 95 glyphs covering ASCII 0x20 (space) through 0x7E ('~').
  - White glyphs on transparent background.
    → white opaque pixel (R+G+B)/3 > 128, alpha ≥ 128 = SET (black on e-paper).
    → transparent pixel (alpha < 128) = CLEAR (white on e-paper).

The metrics JSON has the structure:
  {
    "FONT_REGS_12": [
      { "w": <int>, "h": <int>, "dx": <int>, "dy": <int> },
      ...  (95 entries, index 0 = space 0x20)
    ]
  }

  w, h   : bounding box dimensions of the glyph in pixels
  dx, dy : top-left offset of the bounding box within the 30×30 cell

Output C header contains:
  - FONTID_glyph_BITMAP[]    — concatenated bitmap data for all 95 glyphs.
                               Each glyph occupies glyph_h * ceil(glyph_max_w/8)
                               bytes (padded with zeros if its bounding box
                               width < glyph_max_w).
  - FONTID_glyph_OFFSETS[95] — byte offset into BITMAP for each glyph.
  - FONTID_WIDTHS[95]        — advance width from the JSON (w field).
  - FONTID_OFFSETS_X[95]     — dx values (int8_t).
  - FONTID_OFFSETS_Y[95]     — dy values (int8_t).
  - FONTID_FONT              — fq_font_t descriptor.

Note on glyph_max_w: the maximum w value across all 95 glyphs sets
glyph_max_w. glyph_h is the maximum h value. This ensures the blit loop
in fq_draw_text (which uses glyph_max_w to compute byte stride) always
addresses valid memory.

Usage:
  python3 tools/font_to_c.py \\
      <font_png> <metrics_json> <font_id> <fm_font_advance_index> \\
      <output_header>

Example:
  python3 tools/font_to_c.py \\
      assets/Fonts/FONT_REGS_12.png tests/font_metrics.json \\
      FONT_REGS_12 0 \\
      components/presentation/include/fonts/font_regs_12.h
"""

import sys
import os
import math
import json
import argparse


# ---------------------------------------------------------------------------
# Core helpers (same convention as sprite_to_c.py)
# ---------------------------------------------------------------------------

def pixel_stride(width):
    """Return the number of bytes per row: ceil(width / 8)."""
    return math.ceil(width / 8)


def pixel_is_set_font(r, g, b, a):
    """
    Font polarity: white glyph on transparent background.
    Bit SET when: alpha >= 128 AND (R+G+B)/3 > 128.
    """
    if a < 128:
        return 0
    brightness = (r + g + b) // 3
    return 1 if brightness > 128 else 0


def pack_row(pixel_bits, stride):
    """
    Pack a list of 0/1 bits into 'stride' bytes, MSB first.
    If len(pixel_bits) < stride*8, trailing bits are zero-padded.
    """
    result = []
    width = len(pixel_bits)
    for byte_idx in range(stride):
        byte_val = 0
        for bit_offset in range(8):
            col = byte_idx * 8 + bit_offset
            if col < width and pixel_bits[col]:
                byte_val |= (0x80 >> bit_offset)
        result.append(byte_val)
    return bytes(result)


def extract_glyph_bitmap(img_pixels, img_width,
                         cell_x, cell_y, cell_w, cell_h,
                         dx, dy, glyph_w, glyph_h,
                         out_glyph_h, out_stride):
    """
    Extract a single glyph from the font sheet.

    The glyph bounding box starts at (cell_x + dx, cell_y + dy) and has
    dimensions glyph_w × glyph_h.

    The output bitmap has out_glyph_h rows, each out_stride bytes wide
    (padded to glyph_max_w). Rows above/below the bounding box are zero.
    Within each row, pixels outside the bounding box are zero.
    """
    out = bytearray(out_glyph_h * out_stride)  # zero-filled

    for row in range(glyph_h):
        if row >= out_glyph_h:
            break
        out_row_start = row * out_stride
        row_bits = []
        for col in range(glyph_w):
            px_x = cell_x + dx + col
            px_y = cell_y + dy + row
            # Guard against reading out of the source image bounds
            if px_x < 0 or px_y < 0 or px_x >= img_width:
                row_bits.append(0)
                continue
            img_height = len(img_pixels) // img_width
            if px_y >= img_height:
                row_bits.append(0)
                continue
            idx = px_y * img_width + px_x
            r, g, b, a = img_pixels[idx]
            row_bits.append(pixel_is_set_font(r, g, b, a))

        packed = pack_row(row_bits, out_stride)
        for i, byte_val in enumerate(packed):
            out[out_row_start + i] = byte_val

    return bytes(out)


# ---------------------------------------------------------------------------
# C code generation
# ---------------------------------------------------------------------------

RODATA_WARNING = """\
/*
 * WARNING: This header contains static const arrays. Include it in exactly
 * ONE translation unit per link target to avoid RODATA duplication.
 * Multiple includes will compile without error but silently double flash usage.
 */
"""

HEADER_BANNER = """\
/**
 * {filename} — Auto-generated by tools/font_to_c.py
 *
 * Font   : {font_id}
 * Source : {source_png}
 * Metrics: {metrics_json}
 * Glyphs : 95 (ASCII 0x20–0x7E)
 * glyph_max_w : {glyph_max_w}
 * glyph_h     : {glyph_h}
 * stride      : {stride} bytes/row
 * Total bitmap: {total_bytes} bytes
 *
 * DO NOT EDIT — regenerate with:
 *   python3 tools/font_to_c.py {args_echo}
 *
 * Bit packing: MSB-first, {stride} bytes per row (padded to glyph_max_w={glyph_max_w}).
 * Polarity: white opaque pixel → SET (maps to black on e-paper).
 */
"""

ONCE_GUARD_OPEN = """\
#ifndef {guard}
#define {guard}

#include <stdint.h>
#include "fq_text.h"

"""

ONCE_GUARD_CLOSE = """\
#endif /* {guard} */
"""

NUM_GLYPHS = 95


def bytes_to_c_hex(data, indent="    "):
    """Format a bytes object as comma-separated 0xNN hex literals, 16 per line."""
    items = ["0x{:02X}U".format(b) for b in data]
    lines = []
    chunk = 16
    for i in range(0, len(items), chunk):
        lines.append(indent + ", ".join(items[i:i+chunk]))
    return ",\n".join(lines)


def generate_font_header(font_png, metrics_json_path, font_id,
                         output_path):
    """
    Main entry point: open the font PNG, extract all 95 glyphs using the
    metrics, and write the C header.
    Returns a dict with stats for reporting.
    """
    try:
        from PIL import Image
    except ImportError:
        print("ERROR: Pillow is required. Install with: pip install Pillow",
              file=sys.stderr)
        sys.exit(1)

    # --- Load metrics ---
    with open(metrics_json_path, "r") as f:
        all_metrics = json.load(f)

    if font_id not in all_metrics:
        print("ERROR: Font ID '{}' not found in {}.".format(
            font_id, metrics_json_path), file=sys.stderr)
        print("Available: {}".format(list(all_metrics.keys())), file=sys.stderr)
        sys.exit(1)

    metrics = all_metrics[font_id]
    if len(metrics) != NUM_GLYPHS:
        print("ERROR: Expected {} glyph metrics, got {}.".format(
            NUM_GLYPHS, len(metrics)), file=sys.stderr)
        sys.exit(1)

    # --- Determine glyph_max_w and glyph_h from metrics ---
    glyph_max_w = max(m["w"] for m in metrics)
    glyph_h_max = max(m["h"] for m in metrics)
    # glyph_h = max bounding-box height; this sets the uniform row count
    # per glyph in the bitmap array.
    glyph_h = glyph_h_max
    stride = pixel_stride(glyph_max_w)

    # --- Load font PNG ---
    img = Image.open(font_png).convert("RGBA")
    img_w, img_h = img.size
    pixels = list(img.getdata())

    # Font sheet constants
    cell_w = 30
    cell_h = 30
    font_cols = 19
    font_rows = 5

    # --- Extract all 95 glyph bitmaps ---
    all_bitmaps = []
    for glyph_idx in range(NUM_GLYPHS):
        col = glyph_idx % font_cols
        row = glyph_idx // font_cols
        cell_x = col * cell_w
        cell_y = row * cell_h
        m = metrics[glyph_idx]
        bitmap = extract_glyph_bitmap(
            pixels, img_w,
            cell_x, cell_y, cell_w, cell_h,
            m["dx"], m["dy"], m["w"], m["h"],
            glyph_h, stride)
        all_bitmaps.append(bitmap)

    # --- Build concatenated bitmap and offset table ---
    offsets = []
    current_offset = 0
    concat_bitmap = bytearray()
    for bm in all_bitmaps:
        offsets.append(current_offset)
        concat_bitmap.extend(bm)
        current_offset += len(bm)

    total_bytes = len(concat_bitmap)

    # --- Ensure output directory exists ---
    out_dir = os.path.dirname(output_path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    filename = os.path.basename(output_path)
    guard = filename.upper().replace(".", "_").replace("-", "_").replace("/", "_")
    args_echo = "{} {} {} {} {}".format(
        font_png, metrics_json_path, font_id, 0, output_path)

    with open(output_path, "w") as f:
        # --- File header ---
        f.write(HEADER_BANNER.format(
            filename=filename,
            font_id=font_id,
            source_png=os.path.relpath(font_png),
            metrics_json=os.path.relpath(metrics_json_path),
            glyph_max_w=glyph_max_w,
            glyph_h=glyph_h,
            stride=stride,
            total_bytes=total_bytes,
            args_echo=args_echo,
        ))

        # --- RODATA duplication warning ---
        f.write(RODATA_WARNING)

        f.write(ONCE_GUARD_OPEN.format(guard=guard))

        # --- Concatenated bitmap ---
        f.write(
            "/* ── Concatenated glyph bitmaps ─────────────────────────────── */\n"
            "/* {n} glyphs × {glyph_h} rows × {stride} bytes/row = {total} bytes */\n\n"
            "static const uint8_t {fid}_glyph_BITMAP[{total}] = {{\n".format(
                n=NUM_GLYPHS,
                glyph_h=glyph_h,
                stride=stride,
                total=total_bytes,
                fid=font_id,
            ))
        f.write(bytes_to_c_hex(bytes(concat_bitmap)))
        f.write("\n};\n\n")

        # --- Glyph offset table ---
        f.write(
            "/* ── Byte offsets into {fid}_glyph_BITMAP (one per glyph) ───── */\n\n"
            "static const uint16_t {fid}_glyph_OFFSETS[{n}] = {{\n".format(
                fid=font_id, n=NUM_GLYPHS))
        items = ["{}U".format(o) for o in offsets]
        chunk = 16
        for i in range(0, len(items), chunk):
            f.write("    " + ", ".join(items[i:i+chunk]) + ",\n")
        f.write("};\n\n")

        # --- Advance width table ---
        f.write(
            "/* ── Advance widths per glyph ─────────────────────────────────── */\n\n"
            "static const uint8_t {fid}_WIDTHS[{n}] = {{\n".format(
                fid=font_id, n=NUM_GLYPHS))
        widths = [m["w"] for m in metrics]
        items_w = ["{}U".format(w) for w in widths]
        for i in range(0, len(items_w), chunk):
            f.write("    " + ", ".join(items_w[i:i+chunk]) + ",\n")
        f.write("};\n\n")

        # --- X offsets table ---
        f.write(
            "/* ── X offsets per glyph (dx, int8_t) ─────────────────────────── */\n\n"
            "static const int8_t {fid}_OFFSETS_X[{n}] = {{\n".format(
                fid=font_id, n=NUM_GLYPHS))
        dx_vals = [m["dx"] for m in metrics]
        items_dx = ["{}".format(dx) for dx in dx_vals]
        for i in range(0, len(items_dx), chunk):
            f.write("    " + ", ".join(items_dx[i:i+chunk]) + ",\n")
        f.write("};\n\n")

        # --- Y offsets table ---
        f.write(
            "/* ── Y offsets per glyph (dy, int8_t) ─────────────────────────── */\n\n"
            "static const int8_t {fid}_OFFSETS_Y[{n}] = {{\n".format(
                fid=font_id, n=NUM_GLYPHS))
        dy_vals = [m["dy"] for m in metrics]
        items_dy = ["{}".format(dy) for dy in dy_vals]
        for i in range(0, len(items_dy), chunk):
            f.write("    " + ", ".join(items_dy[i:i+chunk]) + ",\n")
        f.write("};\n\n")

        # --- fq_font_t descriptor ---
        f.write(
            "/* ── fq_font_t descriptor ─────────────────────────────────────── */\n\n"
            "static const fq_font_t {fid}_FONT = {{\n"
            "    .bitmap      = {fid}_glyph_BITMAP,\n"
            "    .widths      = {fid}_WIDTHS,\n"
            "    .offsets_x   = {fid}_OFFSETS_X,\n"
            "    .offsets_y   = {fid}_OFFSETS_Y,\n"
            "    .glyph_h     = {glyph_h}u,\n"
            "    .glyph_max_w = {glyph_max_w}u,\n"
            "}};\n\n".format(
                fid=font_id,
                glyph_h=glyph_h,
                glyph_max_w=glyph_max_w,
            ))

        f.write(ONCE_GUARD_CLOSE.format(guard=guard))

    return {
        "glyphs": NUM_GLYPHS,
        "bytes": total_bytes,
        "glyph_h": glyph_h,
        "glyph_max_w": glyph_max_w,
        "output": output_path,
    }


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Convert a font PNG sheet to packed 1-bit C glyph arrays.")
    parser.add_argument("font_png",          help="Path to font PNG sheet")
    parser.add_argument("metrics_json",      help="Path to font_metrics.json")
    parser.add_argument("font_id",           help="Font key in JSON (e.g. FONT_REGS_12)")
    parser.add_argument("fm_font_index", type=int,
                        help="FM_FONT_ADVANCE row index (for documentation only)")
    parser.add_argument("output_header",     help="Output C header path")
    args = parser.parse_args()

    stats = generate_font_header(
        font_png=args.font_png,
        metrics_json_path=args.metrics_json,
        font_id=args.font_id,
        output_path=args.output_header,
    )
    print("font_to_c: wrote {} glyphs ({} bytes, glyph_h={}, glyph_max_w={}) → {}".format(
        stats["glyphs"], stats["bytes"],
        stats["glyph_h"], stats["glyph_max_w"],
        stats["output"]))


if __name__ == "__main__":
    main()
