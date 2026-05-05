"""
Benchmark Metal vs LLVM rendering at 1024 spp on the fancy Cornell scene.

We render twice per variant: a small warmup pass to trigger JIT compilation
+ AS build, then the timed pass at 1024 spp / 512x512.
"""

import os
import sys
import tempfile
import time

# Reuse the scene constructors from the fancy demo.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mitsuba as mi
import numpy as np
import render_cornell_fancy as fancy  # type: ignore


def time_render(variant, bsp, lin, spp, label):
    print(f"=== {variant} ({label}) ===", flush=True)
    mi.set_variant(variant)
    scene = mi.load_dict(fancy.make_scene_dict(bsp, lin))

    # Warmup: triggers MSL compile / pipeline link / IFT build / TLAS build.
    print(f"  warmup (8 spp)...", flush=True)
    t0 = time.perf_counter()
    img_warm = mi.render(scene, spp=8)
    # Force the underlying eval to complete on the GPU before stopping the timer.
    np.array(img_warm)
    t_warm = time.perf_counter() - t0
    print(f"  warmup: {t_warm:.2f} s", flush=True)

    # Timed pass.
    print(f"  timed render ({spp} spp)...", flush=True)
    t0 = time.perf_counter()
    img = mi.render(scene, spp=spp)
    np.array(img)  # block until done
    elapsed = time.perf_counter() - t0
    print(f"  >>> {variant}: {elapsed:.3f} s for {spp} spp", flush=True)

    return elapsed, img


def main():
    spp = 1024

    bsp = tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False).name
    lin = tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False).name
    try:
        fancy.write_bspline_curves(bsp)
        fancy.write_linear_curves(lin)

        results = {}
        for variant, label in [
            ("metal_ad_rgb", "Apple GPU"),
            ("llvm_ad_rgb", "CPU SIMD"),
        ]:
            if variant not in mi.variants():
                print(f"[!] {variant} not available; skipping", file=sys.stderr)
                continue
            elapsed, img = time_render(variant, bsp, lin, spp, label)
            results[variant] = (elapsed, img)
            mi.util.write_bitmap(f"bench_{variant}.exr", img)

        # Summary
        print("\n=== Summary ===")
        for variant, (elapsed, _) in results.items():
            mrays_per_sec = (512 * 512 * spp) / elapsed / 1e6
            print(
                f"  {variant:<14}  {elapsed:7.3f} s   "
                f"({mrays_per_sec:6.1f} M primary samples/s)"
            )

        if len(results) == 2:
            t_metal = results["metal_ad_rgb"][0]
            t_llvm = results["llvm_ad_rgb"][0]
            speedup = t_llvm / t_metal
            print(f"\n  Metal speedup vs LLVM: {speedup:.2f}x")

            # Sanity: pixel-mean check.
            m = np.array(results["metal_ad_rgb"][1])
            l = np.array(results["llvm_ad_rgb"][1])
            d = np.abs(m - l)
            print(
                f"  Image agreement: |Metal-LLVM| mean={d.mean():.4f} "
                f"max={d.max():.4f}"
            )
    finally:
        os.unlink(bsp)
        os.unlink(lin)


if __name__ == "__main__":
    main()
