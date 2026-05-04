"""
Test Phase 6: native Metal curves.

Renders a Cornell box with a few B-spline / linear curves in place of the
two cubes. Compares Metal against LLVM.

We write a small curves.txt file on disk (Mitsuba's curve loader format) and
load it via the bsplinecurve / linearcurve plugins.
"""

import argparse
import os
import sys
import tempfile

import mitsuba as mi
import numpy as np


def write_curves_file(path):
    """Three control-point lists separated by blank lines = three curves."""
    # Each line: x y z radius. A B-spline curve with N CPs has (N - 3) cubic
    # segments; a linear curve with N CPs has (N - 1) segments.
    lines = [
        # Curve 1 — vertical S-curve on the left
        "-0.5 -0.9 -0.2  0.06",
        "-0.6 -0.5 -0.2  0.06",
        "-0.4  0.0 -0.2  0.06",
        "-0.5  0.5 -0.2  0.06",
        "-0.6  0.9 -0.2  0.06",
        "",
        # Curve 2 — diagonal arc on the right
        " 0.5 -0.9  0.4  0.06",
        " 0.4 -0.4  0.2  0.06",
        " 0.5  0.0 -0.0  0.06",
        " 0.6  0.4 -0.2  0.06",
        " 0.5  0.9 -0.4  0.06",
        "",
        # Curve 3 — small horizontal one in the middle
        "-0.2 -0.8  0.0  0.04",
        " 0.0 -0.8  0.0  0.04",
        " 0.2 -0.8  0.0  0.04",
        " 0.4 -0.8  0.0  0.04",
        "",
    ]
    with open(path, "w") as f:
        f.write("\n".join(lines))


def make_scene_dict(curves_file, curve_type):
    scene = mi.cornell_box()
    del scene["small-box"]
    del scene["large-box"]
    scene["curves"] = {
        "type": curve_type,
        "filename": curves_file,
        "bsdf": {"type": "ref", "id": "white"},
    }
    return scene


def render_with(variant, curves_file, curve_type, out_path, spp):
    print(f"[*] {variant} ({curve_type}) -> {out_path}", flush=True)
    mi.set_variant(variant)
    scene = mi.load_dict(make_scene_dict(curves_file, curve_type))
    img = mi.render(scene, spp=spp)
    mi.util.write_bitmap(out_path, img)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--variant", default="metal_ad_rgb")
    p.add_argument("--reference", action="store_true")
    p.add_argument(
        "--curve-type", default="bsplinecurve", choices=["bsplinecurve", "linearcurve"]
    )
    p.add_argument("--spp", type=int, default=64)
    args = p.parse_args()

    avail = mi.variants()
    if args.variant not in avail:
        print(f"[!] {args.variant} not available; have: {avail}", file=sys.stderr)
        sys.exit(1)

    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as tmp:
        curves_file = tmp.name
    try:
        write_curves_file(curves_file)

        suffix = args.curve_type
        render_with(
            args.variant,
            curves_file,
            args.curve_type,
            f"cornell_{suffix}_{args.variant}.exr",
            args.spp,
        )
        if args.reference:
            for ref in ("llvm_ad_rgb", "cuda_ad_rgb", "scalar_rgb"):
                if ref in avail and ref != args.variant:
                    render_with(
                        ref,
                        curves_file,
                        args.curve_type,
                        f"cornell_{suffix}_{ref}.exr",
                        args.spp,
                    )
                    break
    finally:
        os.unlink(curves_file)


if __name__ == "__main__":
    main()
