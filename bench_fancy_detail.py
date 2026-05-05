"""
Detailed benchmark with the pbdr `wrap_function` utility — Metal vs LLVM at
256 spp on the fancy scene. Reports kernel-history-based breakdowns
(jitting / codegen / backend / execution) plus async-vs-sync overhead.
"""

import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _bench_util as bench  # type: ignore
import mitsuba as mi
import render_cornell_fancy as fancy  # type: ignore


def main():
    spp = 256

    bsp = tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False).name
    lin = tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False).name
    try:
        fancy.write_bspline_curves(bsp)
        fancy.write_linear_curves(lin)

        dataframes = []
        for variant in ("metal_ad_rgb", "llvm_ad_rgb"):
            if variant not in mi.variants():
                print(f"[!] {variant} not available; skipping", file=sys.stderr)
                continue
            mi.set_variant(variant)
            scene = mi.load_dict(fancy.make_scene_dict(bsp, lin))

            @bench.wrap_function(
                label=f"render-{variant}",
                dataframes=dataframes,
                nb_runs=4,
                nb_dry_runs=1,  # warmup once before timing
                log_level=2,
                clear_cache=False,  # keep kernel cache to compare hot times
                no_async=False,
            )
            def render(scene_, spp_):
                return mi.render(scene_, spp=spp_, seed=0)

            print(f"\n========== {variant} (spp={spp}) ==========")
            render(scene, spp, label=f"{variant}, {spp}spp")

        # Compact summary table.
        print("\n========== SUMMARY ==========")
        print(
            f"{'variant':<14} {'sync(ms)':>10} {'async(ms)':>10} "
            f"{'codegen':>9} {'backend':>9} {'exec':>9} {'jit':>8}"
        )
        for df in dataframes:
            v = df["label"].replace("render-", "")
            print(
                f"{v:<14} "
                f"{df['sync_total_time']:10.2f} "
                f"{df.get('async_total_time', 0):10.2f} "
                f"{df['codegen_time']:9.2f} "
                f"{df['backend_time']:9.2f} "
                f"{df['execution_time']:9.2f} "
                f"{df['jitting_time']:8.2f}"
            )

        if len(dataframes) == 2:
            t_metal = dataframes[0]["async_total_time"]
            t_llvm = dataframes[1]["async_total_time"]
            print(f"\nMetal speedup (async total): {t_llvm / t_metal:.2f}x")

            e_metal = dataframes[0]["execution_time"]
            e_llvm = dataframes[1]["execution_time"]
            if e_metal > 0:
                print(
                    f"Metal speedup (kernel execution only): "
                    f"{e_llvm / e_metal:.2f}x"
                )
            else:
                print(
                    "(Metal `execution_time` from kernel_history() is 0 — "
                    "the Metal backend doesn't populate this field, so the "
                    "wall-clock 'async total' is the reliable number.)"
                )
    finally:
        os.unlink(bsp)
        os.unlink(lin)


if __name__ == "__main__":
    main()
