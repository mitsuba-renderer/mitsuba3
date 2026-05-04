"""
Test script for Phase 5 (ellipsoids). Renders a Cornell box with two
ellipsoids in place of the usual cubes — one elongated along Y, one tilted
about Z. Compares Metal vs LLVM.
"""

import argparse
import sys

import mitsuba as mi
import numpy as np


def make_scene_dict():
    T = mi.ScalarTransform4f
    scene = mi.cornell_box()
    del scene["small-box"]
    del scene["large-box"]

    # Two ellipsoids: a tall narrow one on the left (rotated 30° about Z),
    # and a wider one on the right.
    centers = np.array(
        [
            [-0.33, -0.4, -0.28],  # left
            [0.335, -0.7, 0.38],  # right
        ],
        dtype=np.float32,
    )

    scales = np.array(
        [
            [0.18, 0.55, 0.18],  # tall ellipsoid
            [0.30, 0.30, 0.30],  # round
        ],
        dtype=np.float32,
    )

    # Quaternion (x, y, z, w). 30° rotation about z-axis for the first;
    # identity for the second.
    a = np.deg2rad(30) / 2
    quats = np.array(
        [
            [0.0, 0.0, np.sin(a), np.cos(a)],
            [0.0, 0.0, 0.0, 1.0],
        ],
        dtype=np.float32,
    )

    scene["ellipsoids"] = {
        "type": "ellipsoids",
        "centers": mi.TensorXf(centers),
        "scales": mi.TensorXf(scales),
        "quaternions": mi.TensorXf(quats),
        "bsdf": {"type": "ref", "id": "white"},
    }
    return scene


def render_with(variant, out_path, spp):
    print(f"[*] {variant} -> {out_path}")
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

    avail = mi.variants()
    if args.variant not in avail:
        print(f"[!] {args.variant} not available; have: {avail}", file=sys.stderr)
        sys.exit(1)

    render_with(args.variant, f"cornell_ellipsoids_{args.variant}.exr", args.spp)
    if args.reference:
        for ref in ("llvm_ad_rgb", "cuda_ad_rgb", "scalar_rgb"):
            if ref in avail and ref != args.variant:
                render_with(ref, f"cornell_ellipsoids_{ref}.exr", args.spp)
                break


if __name__ == "__main__":
    main()
