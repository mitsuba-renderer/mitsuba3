# Field Benchmarks

This folder is intentionally separate from Mitsuba's ordinary test suite. These
benchmarks are private performance tooling for the Field refactor and should not
be collected by `pytest` or treated as correctness tests.

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
```

Implemented baseline cases:

- `bitmap_eval`: current `bitmap` texture `eval`, `eval_1`, `eval_3`, and
  scalar-gradient paths.
- `bitmap_sampling`: current `bitmap` texture position and spectrum sampling.
- `grid_volume_eval`: current `gridvolume` fixed and variable-channel paths.

Field, neural-field, and `neuralbsdf` cases use the same timing, environment,
launch-stat, and memory-watermark machinery. They intentionally fail clearly
until the corresponding plugins land.

Use `--compare baseline.json --fail-threshold 0.05` to compare medians against a
previous result and fail when the slowdown exceeds the threshold.
