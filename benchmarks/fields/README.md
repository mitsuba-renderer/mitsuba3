# Field Benchmarks

This directory is intentionally separate from Mitsuba's ordinary test suite.
These benchmarks measure the Field API and related texture, volume, and BSDF
paths. They should not be collected by `pytest` or treated as correctness
tests.

Run from a configured build environment:

```bash
source build-{llmmodel}/setpath.sh
python benchmarks/fields/bench_fields.py --list
```

Examples:

```bash
python benchmarks/fields/bench_fields.py \
    --variant scalar_rgb \
    --case bitmap_eval \
    --method all \
    --channels 3 \
    --filter-type bilinear \
    --wrap-mode repeat \
    --scalar-iterations 20000 \
    --repeats 50 \
    --json bitmap_scalar.json

python benchmarks/fields/bench_fields.py \
    --case grid_volume_eval \
    --method eval_6 \
    --channels 6 \
    --size 1048576 \
    --coord-distribution random \
    --json grid_llvm.json

python benchmarks/fields/bench_fields.py \
    --case field_fixed_eval \
    --field-plugin gridvolume \
    --method eval_array6 \
    --channels 6

python benchmarks/fields/bench_fields.py \
    --case neural_field_inference \
    --method eval_color3 \
    --args-mode no_args \
    --args-dim 0 \
    --kernel-history \
    --json neural_no_args.json
```

Implemented baseline cases:

- `bitmap_eval`: current `bitmap` texture `eval`, `eval_1`, `eval_3`, and
  scalar-gradient paths.
- `bitmap_sampling`: current `bitmap` texture position and spectrum sampling.
- `grid_volume_eval`: current `gridvolume` fixed and variable-channel paths.

Field, neural field, and `neuralbsdf` cases use the same timing, environment,
kernel-launch, and memory-watermark machinery. They intentionally fail clearly
until the corresponding plugins are available in the active build.

The runner also stores a compact result signature (shape, width, sum, mean,
minimum, maximum, and a few sampled entries where applicable). Tensor-backed
fixtures use deterministic non-constant data so these signatures can catch
accidental constant paths and some channel/order mistakes. This is intended for
benchmark comparison reports and quick sanity checks, not as a substitute for
the ordinary test suite.

For JIT variants, benchmark cases return every value that contributes to the
workload. The runner forces these outputs with `dr.eval()` and `dr.sync_thread()`
before stopping wall-clock timers, since Dr.Jit traces lazily. It also enables
`dr.JitFlag.KernelHistory` during measurement. Raw `summary` values are forced
wall times, while `metrics.kernel_execution_summary_ms` contains the preferred
steady-state Dr.Jit device/backend execution timing when kernels are launched.
The first measured call is kept separately in `metrics.first_wall_seconds` and
`metrics.first_kernel_history` so one-time setup, code generation, backend
compilation, cache hits/misses, and first execution are visible instead of being
mixed into steady-state repeats.

Use `--compare baseline.json --fail-threshold 0.05` to compare medians against a
previous result and fail when the slowdown exceeds the threshold. Launch-count
and allocation regressions can be checked with `--fail-launch-threshold` and
`--fail-memory-watermark`. Scalar variants run a configurable inner loop via
`--scalar-iterations` so per-call allocation and dispatch costs are visible.

## Standard-vs-field comparison

`compare_field_vs_std.py` runs common public texture/volume workloads against a
clean standard Mitsuba checkout and this field-based tree, then emits CSV, raw
JSON, Markdown, and SVG summaries. It does not modify the standard checkout; it
only sources an existing build's `setpath.sh`.

```bash
python benchmarks/fields/compare_field_vs_std.py \
    --std-root mitsuba-std \
    --std-build mitsuba-std/build-std \
    --field-build build-gpt-5 \
    --variants scalar_rgb \
    --suite common
```

CUDA comparisons require both builds to include `cuda_ad_rgb`. If the standard
checkout was built with CUDA, run:

```bash
python benchmarks/fields/compare_field_vs_std.py \
    --std-root mitsuba-std \
    --std-build mitsuba-std/build-std \
    --field-build build-gpt-5 \
    --variants cuda_ad_rgb \
    --suite common
```

If only the field tree has CUDA, run a field-only sanity report instead:

```bash
python benchmarks/fields/compare_field_vs_std.py \
    --field-build build-gpt-5 \
    --variants cuda_ad_rgb \
    --suite field
```

When `--variant` is omitted in `bench_fields.py`, the runner selects
`cuda_ad_rgb` when available and falls back to `llvm_ad_rgb`. The comparison
runner resolves `auto_ad_rgb` the same way for the supplied builds.

The `common` suite only runs workloads supported by both builds. The `field`
suite only runs field-specific and neural cases on the current tree. The
default `all` suite combines both so the report contains public API ratios and
field-only absolute timings.

When the standard checkout is present but no build has been configured yet,
`--allow-missing-std --keep-going` still produces a current-tree report and
marks standard rows as incomplete. Ratio tables will remain empty until a valid
standard build is supplied.

Comparison reports include operation-count medians, first-run operation counts,
launch deltas, allocation-watermark deltas, first-kernel totals, and
result-signature differences. Treat CUDA rows as comparative evidence only when
the report contains both standard and field-based rows for `cuda_ad_rgb`; rows
under "Field-Only Runs" are sanity checks.
