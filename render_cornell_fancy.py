"""
Fancy Cornell-box showcase: every Metal-supported shape type with a
variety of BSDFs, all in one scene.

Shapes used:
  - rectangle  (walls/floor/ceiling/light)
  - sphere     (frosted dielectric)
  - cylinder   (rough conductor / brushed metal)
  - disk       (smooth mirror, lying on the floor)
  - ellipsoids (cluster of plastic spheroids in primary colors)
  - bsplinecurve (smooth golden hair arc)
  - linearcurve  (zig-zag rust-colored polyline)
  - sdfgrid    (a torus, copper-colored)

BSDFs used:
  - diffuse, dielectric, conductor, roughconductor, plastic
  - thindielectric (for a transparent ellipsoid)

Renders with Metal first; --reference also renders with LLVM.
"""

import argparse
import os
import sys
import tempfile

import mitsuba as mi
import numpy as np


SDF_PATH = os.path.normpath(
    os.path.join(
        os.path.dirname(__file__),
        "resources/data/docs/scenes/sdfgrid/torus_sdfgrid.vol",
    )
)


def write_bspline_curves(path):
    """Smooth golden arc — a single B-spline curve high above the floor."""
    with open(path, "w") as f:
        for i in range(10):
            t = i / 9.0
            x = -0.75 + 1.5 * t
            y = 0.55 - 0.20 * np.sin(np.pi * t) ** 2
            z = -0.75 + 0.30 * np.sin(np.pi * t)
            r = 0.022
            f.write(f"{x:.4f} {y:.4f} {z:.4f} {r:.4f}\n")


def write_linear_curves(path):
    """Two thin rust polylines hugging the back walls (well clear of
    the central shapes)."""
    with open(path, "w") as f:
        # Left back wall, low → high
        for v in [
            (-0.95, -0.75, -0.85),
            (-0.85, -0.30, -0.85),
            (-0.95, 0.10, -0.85),
            (-0.85, 0.45, -0.85),
        ]:
            f.write(f"{v[0]:.3f} {v[1]:.3f} {v[2]:.3f} 0.018\n")
        f.write("\n")
        # Right back wall, low → high
        for v in [
            (0.95, 0.50, -0.85),
            (0.85, 0.10, -0.85),
            (0.95, -0.30, -0.85),
            (0.85, -0.65, -0.85),
        ]:
            f.write(f"{v[0]:.3f} {v[1]:.3f} {v[2]:.3f} 0.018\n")


def make_scene_dict(bspline_path, linear_path):
    T = mi.ScalarTransform4f
    scene = mi.cornell_box()
    del scene["small-box"]
    del scene["large-box"]

    # ---- Extra BSDFs ----------------------------------------------------
    scene["glass"] = {"type": "dielectric", "int_ior": 1.5}
    scene["mirror"] = {"type": "conductor", "material": "Al"}
    scene["brushed"] = {"type": "roughconductor", "material": "Au", "alpha": 0.18}
    scene["copper"] = {
        "type": "plastic",
        "diffuse_reflectance": {"type": "rgb", "value": [0.72, 0.40, 0.20]},
        "int_ior": 1.5,
    }
    scene["gold"] = {"type": "conductor", "material": "Au"}
    scene["rust"] = {
        "type": "diffuse",
        "reflectance": {"type": "rgb", "value": [0.55, 0.18, 0.06]},
    }
    scene["plastic_blue"] = {
        "type": "plastic",
        "diffuse_reflectance": {"type": "rgb", "value": [0.10, 0.20, 0.65]},
    }
    scene["plastic_yellow"] = {
        "type": "plastic",
        "diffuse_reflectance": {"type": "rgb", "value": [0.85, 0.70, 0.10]},
    }
    scene["plastic_pink"] = {
        "type": "plastic",
        "diffuse_reflectance": {"type": "rgb", "value": [0.85, 0.30, 0.55]},
    }

    # ---- Shapes ---------------------------------------------------------

    # Frosted-glass sphere on the floor, front-right.
    scene["sphere"] = {
        "type": "sphere",
        "center": [0.50, -0.70, 0.45],
        "radius": 0.28,
        "bsdf": {"type": "ref", "id": "glass"},
    }

    # Brushed-gold cylinder standing tall in the back-left corner.
    scene["cylinder"] = {
        "type": "cylinder",
        "to_world": (T().translate([-0.62, -1.0, -0.55]).scale([0.16, 0.16, 1.05])),
        "bsdf": {"type": "ref", "id": "brushed"},
    }

    # Mirror disk lying flat on the floor in the front-left.
    scene["disk"] = {
        "type": "disk",
        "to_world": (
            T().translate([-0.45, -0.999, 0.50]).rotate([1, 0, 0], -90).scale(0.32)
        ),
        "bsdf": {"type": "ref", "id": "mirror"},
    }

    # Cluster of three small plastic ellipsoids in the back-right corner.
    centers = np.array(
        [
            [0.55, -0.85, -0.35],
            [0.70, -0.78, -0.20],
            [0.45, -0.92, -0.55],
        ],
        dtype=np.float32,
    )
    scales = np.array(
        [
            [0.13, 0.10, 0.13],
            [0.07, 0.18, 0.07],
            [0.10, 0.10, 0.10],
        ],
        dtype=np.float32,
    )
    quats = np.array(
        [
            [0.0, 0.0, np.sin(np.pi / 12), np.cos(np.pi / 12)],
            [np.sin(np.pi / 8), 0.0, 0.0, np.cos(np.pi / 8)],
            [0.0, np.sin(np.pi / 6), 0.0, np.cos(np.pi / 6)],
        ],
        dtype=np.float32,
    )
    scene["ellipsoids"] = {
        "type": "ellipsoids",
        "centers": mi.TensorXf(centers),
        "scales": mi.TensorXf(scales),
        "quaternions": mi.TensorXf(quats),
        "bsdf": {"type": "ref", "id": "plastic_pink"},
    }

    # Smooth golden B-spline curve arching high above all the floor objects.
    scene["bspline"] = {
        "type": "bsplinecurve",
        "filename": bspline_path,
        "bsdf": {"type": "ref", "id": "gold"},
    }

    # Two jagged rust-colored polylines along the back walls.
    scene["linear"] = {
        "type": "linearcurve",
        "filename": linear_path,
        "bsdf": {"type": "ref", "id": "rust"},
    }

    # Copper torus SDF floating in the center, slightly elevated above
    # everything on the floor so it doesn't intersect anything.
    if os.path.exists(SDF_PATH):
        scene["sdf"] = {
            "type": "sdfgrid",
            "filename": SDF_PATH,
            "to_world": (
                T()
                .translate([0.0, 0.20, -0.30])
                .scale(0.85)
                .translate([-0.5, -0.5, -0.5])
            ),
            "bsdf": {"type": "ref", "id": "copper"},
        }
    else:
        print(f"[!] SDF file missing: {SDF_PATH}", file=sys.stderr)

    # Fancier sensor: bump SPP / size via the standard cornell_box setting.
    scene["sensor"]["sampler"]["sample_count"] = 256
    scene["sensor"]["film"]["width"] = 512
    scene["sensor"]["film"]["height"] = 512

    return scene


def render_with(variant, bspline_path, linear_path, out_path, spp):
    print(f"[*] {variant} -> {out_path}", flush=True)
    mi.set_variant(variant)
    scene = mi.load_dict(make_scene_dict(bspline_path, linear_path))
    img = mi.render(scene, spp=spp)
    mi.util.write_bitmap(out_path, img)


def to_png(exr_path, png_path):
    mi.set_variant("scalar_rgb")
    img = np.array(mi.Bitmap(exr_path), copy=False)
    # Reinhard-ish tone map + sRGB.
    tm = img / (1.0 + img.max(axis=-1, keepdims=True) * 0.05)
    tm = np.clip(np.power(np.clip(tm, 0, 1), 1 / 2.2), 0, 1)
    bm = mi.Bitmap(tm.astype(np.float32), pixel_format=mi.Bitmap.PixelFormat.RGB)
    bm.convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, srgb_gamma=False).write(
        png_path
    )


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--variant", default="metal_ad_rgb")
    p.add_argument("--reference", action="store_true")
    p.add_argument("--spp", type=int, default=256)
    args = p.parse_args()

    avail = mi.variants()
    if args.variant not in avail:
        print(f"[!] {args.variant} not available; have: {avail}", file=sys.stderr)
        sys.exit(1)

    bsp = tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False).name
    lin = tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False).name
    try:
        write_bspline_curves(bsp)
        write_linear_curves(lin)

        out = f"cornell_fancy_{args.variant}.exr"
        render_with(args.variant, bsp, lin, out, args.spp)
        to_png(out, out.replace(".exr", ".png"))

        if args.reference:
            for ref in ("llvm_ad_rgb", "cuda_ad_rgb"):
                if ref in avail and ref != args.variant:
                    out_ref = f"cornell_fancy_{ref}.exr"
                    render_with(ref, bsp, lin, out_ref, args.spp)
                    to_png(out_ref, out_ref.replace(".exr", ".png"))
                    break
    finally:
        os.unlink(bsp)
        os.unlink(lin)


if __name__ == "__main__":
    main()
