#!/usr/bin/env python3
"""
diff_screens.py — Visual regression diff for FiestaQuest screen PNGs.

Compares PNGs in output/ (rendered by render_all_screens) against golden
baselines in golden/.

Phase 1 status: No golden baselines exist yet. Script exits 0 and prints a
notice. Phase 7 will populate golden/ with reference PNGs and activate the
pixel-level comparison logic below.

Usage:
    python diff_screens.py [--output-dir OUTPUT_DIR] [--golden-dir GOLDEN_DIR]

Exit codes:
    0 — all comparisons passed (or no baselines to compare against)
    1 — one or more screens differ from their golden baseline
    2 — argument / environment error
"""

import argparse
import os
import sys


def parse_args():
    parser = argparse.ArgumentParser(
        description="FiestaQuest visual regression diff"
    )
    parser.add_argument(
        "--output-dir",
        default=os.path.join(os.path.dirname(__file__), "build", "output"),
        help="Directory containing rendered PNG files (default: build/output/)",
    )
    parser.add_argument(
        "--golden-dir",
        default=os.path.join(os.path.dirname(__file__), "golden"),
        help="Directory containing golden baseline PNGs (default: golden/)",
    )
    return parser.parse_args()


def collect_pngs(directory):
    """Return sorted list of .png filenames (basename only) in directory."""
    if not os.path.isdir(directory):
        return []
    return sorted(
        f for f in os.listdir(directory) if f.lower().endswith(".png")
    )


def main():
    args = parse_args()
    golden_pngs = collect_pngs(args.golden_dir)

    # ---------------------------------------------------------------------------
    # Phase 1: No golden baselines yet — skip comparison and exit 0.
    #
    # Phase 7 will populate golden/ with reference PNGs captured after the
    # first full visual review. Once baselines exist, remove the early-return
    # block below and activate the pixel-level comparison loop.
    # ---------------------------------------------------------------------------
    if not golden_pngs:
        print(
            "diff_screens.py: No golden baselines yet — skipping diff.\n"
            "  golden dir : {}\n"
            "  output dir : {}\n"
            "To activate visual regression, add reference PNGs to golden/.".format(
                args.golden_dir, args.output_dir
            )
        )
        return 0

    # ---------------------------------------------------------------------------
    # Phase 7+: Pixel-level comparison (activated once golden/ is populated).
    #
    # Requires Pillow: pip install Pillow
    # ---------------------------------------------------------------------------
    try:
        from PIL import Image, ImageChops  # noqa: PLC0415
    except ImportError:
        print(
            "diff_screens.py: ERROR — Pillow is required for PNG comparison.\n"
            "  Install with: pip install Pillow",
            file=sys.stderr,
        )
        return 2

    output_pngs = collect_pngs(args.output_dir)
    failures = []

    for golden_name in golden_pngs:
        if golden_name not in output_pngs:
            print(
                "  [MISSING] {} — rendered output not found in {}".format(
                    golden_name, args.output_dir
                ),
                file=sys.stderr,
            )
            failures.append(golden_name)
            continue

        golden_path = os.path.join(args.golden_dir, golden_name)
        output_path = os.path.join(args.output_dir, golden_name)

        try:
            golden_img = Image.open(golden_path).convert("L")
            output_img = Image.open(output_path).convert("L")
        except Exception as exc:  # noqa: BLE001
            print(
                "  [ERROR] {} — failed to open image: {}".format(golden_name, exc),
                file=sys.stderr,
            )
            failures.append(golden_name)
            continue

        if golden_img.size != output_img.size:
            print(
                "  [FAIL] {} — size mismatch: golden={} output={}".format(
                    golden_name, golden_img.size, output_img.size
                ),
                file=sys.stderr,
            )
            failures.append(golden_name)
            continue

        diff = ImageChops.difference(golden_img, output_img)
        if diff.getbbox() is not None:
            print(
                "  [FAIL] {} — pixels differ from golden baseline".format(
                    golden_name
                ),
                file=sys.stderr,
            )
            failures.append(golden_name)
        else:
            print("  [PASS] {}".format(golden_name))

    if failures:
        print(
            "\ndiff_screens.py: {} screen(s) failed visual regression: {}".format(
                len(failures), ", ".join(failures)
            ),
            file=sys.stderr,
        )
        return 1

    print(
        "diff_screens.py: all {} golden baseline(s) matched.".format(
            len(golden_pngs)
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
