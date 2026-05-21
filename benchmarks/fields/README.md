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
    --variant llvm_ad_rgb \
    --case grid_volume_eval \
    --method eval_6 \
    --channels 6 \
    --size 1048576 \
    --coord-distribution random \
    --json grid_llvm.json

python benchmarks/fields/bench_fields.py \
    --variant llvm_ad_rgb \
    --case field_fixed_eval \
    --field-plugin gridfield \
    --method eval_array6 \
    --channels 6

python benchmarks/fields/bench_fields.py \
    --variant llvm_ad_rgb \
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

Use `--compare baseline.json --fail-threshold 0.05` to compare medians against a
previous result and fail when the slowdown exceeds the threshold. Launch-count
and allocation regressions can be checked with `--fail-launch-threshold` and
`--fail-memory-watermark`. Scalar variants run a configurable inner loop via
`--scalar-iterations` so per-call allocation and dispatch costs are visible.
