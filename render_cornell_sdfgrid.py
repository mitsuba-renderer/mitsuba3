"""
Test Phase 7: SDF grid via native Metal bbox-BLAS + custom MSL intersection.

Renders a Cornell box with the torus SDF grid (from
resources/data/docs/scenes/sdfgrid/torus_sdfgrid.vol) and compares Metal vs LLVM.
"""

import argparse
import os
import sys

import mitsuba as mi
import numpy as np


SDF_PATH = os.path.normpath(
    os.path.join(
        os.path.dirname(__file__),
        "resources/data/docs/scenes/sdfgrid/torus_sdfgrid.vol",
    )
)


def make_scene_dict():
    T = mi.ScalarTransform4f
    scene = mi.cornell_box()
    del scene["small-box"]
    del scene["large-box"]
    scene["sdf"] = {
        "type": "sdfgrid",
        "filename": SDF_PATH,
        "to_world": (
            T().translate([0.0, -0.4, 0.0]).scale(1.5).translate([-0.5, -0.5, -0.5])
        ),  # SDF grid is in [0,1]^3
        "bsdf": {"type": "ref", "id": "white"},
    }
    return scene


def render_with(variant, out_path, spp):
    print(f"[*] {variant} -> {out_path}", flush=True)
    mi.set_variant(variant)
    scene = mi.load_dict(make_scene_dict())
    img = mi.render(scene, spp=spp)
    mi.util.write_bitmap(out_path, img)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--variant", default="metal_ad_rgb")
    p.add_argument("--reference", action="store_true")
    p.add_argument("--spp", type=int, default=64)
    args = p.parse_args()

    if not os.path.exists(SDF_PATH):
        print(f"[!] SDF file not found: {SDF_PATH}", file=sys.stderr)
        sys.exit(1)

    avail = mi.variants()
    if args.variant not in avail:
        print(f"[!] {args.variant} not available; have: {avail}", file=sys.stderr)
        sys.exit(1)

    render_with(args.variant, f"cornell_sdfgrid_{args.variant}.exr", args.spp)
    if args.reference:
        for ref in ("llvm_ad_rgb", "cuda_ad_rgb", "scalar_rgb"):
            if ref in avail and ref != args.variant:
                render_with(ref, f"cornell_sdfgrid_{ref}.exr", args.spp)
                break


if __name__ == "__main__":
    main()
