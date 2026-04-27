"""
Test script: render the Cornell Box with a sphere and a cylinder in place of
the two cubes. Exercises the new Metal custom-intersection code path (Phase 4).

Renders are written next to this script as EXR files. By default we render
with `metal_ad_rgb`. Pass --reference to also render with `cuda_ad_rgb`
(or `llvm_ad_rgb` if CUDA is unavailable) for a visual comparison.

NOTE: As of the time of writing, drjit-core's Metal MSL codegen still emits
`intersector<triangle_data, instancing>` for the TraceRay node. Until the
Phase 2 follow-up (`intersector<instancing>` + MTLLinkedFunctions pipeline
linking) is in place, the sphere and cylinder will not be intersected
correctly — the BLAS is built and bound, but the kernel does not yet call
the custom intersection functions. The script still serves as an end-to-end
exerciser for the BLAS-construction and library-compilation paths.
"""

import argparse
import sys

import mitsuba as mi


def make_scene_dict():
    """Cornell Box with the two cubes replaced by a sphere and a cylinder."""
    T = mi.ScalarTransform4f
    scene = mi.cornell_box()

    # Drop the two cubes.
    del scene["small-box"]
    del scene["large-box"]

    # Replace small box -> sphere.
    scene["sphere"] = {
        "type": "sphere",
        "center": [0.335, -0.7, 0.38],
        "radius": 0.3,
        "bsdf": {"type": "ref", "id": "white"},
    }

    # Replace large box -> cylinder. The cylinder primitive is z-axis aligned
    # in object space; we tilt it upright with a rotation about the X axis.
    scene["cylinder"] = {
        "type": "cylinder",
        "to_world": (
            T()
            .translate([-0.33, -1.0, -0.28])
            .rotate([1, 0, 0], -90)
            .scale([0.3, 0.3, 1.22])
        ),
        "radius": 1.0,  # radius is baked into to_world via the scale
        "bsdf": {"type": "ref", "id": "white"},
    }

    return scene


def render_with(variant, out_path, spp=None):
    print(f"[*] Rendering with variant: {variant} -> {out_path}")
    mi.set_variant(variant)
    # Scene dict construction must happen *after* set_variant since it
    # uses mi.ScalarTransform4f, which is variant-gated.
    scene_dict = make_scene_dict()
    scene = mi.load_dict(scene_dict)

    if spp is not None:
        img = mi.render(scene, spp=spp)
    else:
        img = mi.render(scene)

    mi.util.write_bitmap(out_path, img)
    print(f"    wrote {out_path}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--variant", default="metal_ad_rgb", help="Mitsuba variant to render with."
    )
    parser.add_argument(
        "--reference",
        action="store_true",
        help="Also render a CPU/CUDA reference for comparison.",
    )
    parser.add_argument("--spp", type=int, default=64, help="Samples per pixel.")
    args = parser.parse_args()

    # Set a variant once just to populate mi.variants().
    available = mi.variants()
    if args.variant not in available:
        print(
            f"[!] Variant {args.variant!r} not available. Choose from: " f"{available}",
            file=sys.stderr,
        )
        sys.exit(1)

    # Primary render (Metal by default).
    render_with(
        args.variant, f"cornell_sphere_cylinder_{args.variant}.exr", spp=args.spp
    )

    if args.reference:
        # Pick the first available reference variant.
        for ref in ("cuda_ad_rgb", "llvm_ad_rgb", "scalar_rgb"):
            if ref in available and ref != args.variant:
                render_with(ref, f"cornell_sphere_cylinder_{ref}.exr", spp=args.spp)
                break
        else:
            print("[!] No reference variant available.", file=sys.stderr)


if __name__ == "__main__":
    main()

if __name__ == "__main__":
    main()
