#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import json
import os
import re
import shlex
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass(frozen=True)
class RunSpec:
    """One benchmark invocation shared by the current and standard builds."""

    name: str
    group: str
    description: str
    case: str
    method: str
    variant: str
    channels: int = 3
    filter_type: str = "bilinear"
    wrap_mode: str = "repeat"
    input: str = "tensor"
    field_plugin: str = ""
    out_dim: int = 3
    args_mode: str = "args"
    args_dim: int = 4
    std_supported: bool = True


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def default_output_dir() -> Path:
    return repo_root() / "benchmarks/fields/results" / time.strftime("%Y%m%d-%H%M%S")


def discover_std_build(std_root: Path) -> Path | None:
    for name in ("build-gpt-5", "build-std", "build"):
        path = std_root / name
        if (path / "setpath.sh").exists():
            return path
    for path in sorted(std_root.glob("build*")):
        if (path / "setpath.sh").exists():
            return path
    return None


def available_build_variants(build_dir: Path | None) -> list[str]:
    if build_dir is None:
        return []
    conf = build_dir / "mitsuba.conf"
    if not conf.exists():
        return []
    match = re.search(r'"enabled"\s*:\s*\[(.*?)\]', conf.read_text(errors="ignore"), re.S)
    if not match:
        return []
    return re.findall(r'"([^"]+)"', match.group(1))


def preferred_ad_rgb_variant(field_build: Path, std_build: Path | None, suite: str) -> str:
    field_variants = available_build_variants(field_build)
    std_variants = available_build_variants(std_build)
    for variant in ("cuda_ad_rgb", "llvm_ad_rgb"):
        if variant in field_variants and (suite == "field" or not std_variants or variant in std_variants):
            return variant
    for variant in ("cuda_ad_rgb", "llvm_ad_rgb"):
        if variant in field_variants:
            return variant
    raise SystemExit("No AD RGB variant found in the field build; expected cuda_ad_rgb or llvm_ad_rgb.")


def resolve_variants(raw: str, field_build: Path, std_build: Path | None, suite: str) -> list[str]:
    resolved = []
    for item in [value.strip() for value in raw.split(",") if value.strip()]:
        variant = (
            preferred_ad_rgb_variant(field_build, std_build, suite)
            if item in ("auto", "auto_ad_rgb") else item
        )
        if variant not in resolved:
            resolved.append(variant)
    return resolved


def base_specs(variants: list[str]) -> list[RunSpec]:
    specs: list[RunSpec] = []
    for variant in variants:
        specs.extend([
            RunSpec(
                name=f"{variant}_bitmap_rgb_eval3",
                group="texture",
                description="bitmap texture RGB eval_3",
                case="bitmap_eval",
                method="eval_3",
                variant=variant,
                channels=3,
            ),
            RunSpec(
                name=f"{variant}_bitmap_rgb_sample_position",
                group="texture",
                description="bitmap texture sample_position",
                case="bitmap_sampling",
                method="sample_position",
                variant=variant,
                channels=3,
            ),
            RunSpec(
                name=f"{variant}_bitmap_scalar_eval1",
                group="texture",
                description="bitmap texture scalar eval_1",
                case="bitmap_eval",
                method="eval_1",
                variant=variant,
                channels=1,
            ),
            RunSpec(
                name=f"{variant}_grid1_eval1",
                group="volume",
                description="gridvolume scalar eval_1",
                case="grid_volume_eval",
                method="eval_1",
                variant=variant,
                channels=1,
                filter_type="trilinear",
            ),
            RunSpec(
                name=f"{variant}_grid3_eval3",
                group="volume",
                description="gridvolume 3-channel eval_3",
                case="grid_volume_eval",
                method="eval_3",
                variant=variant,
                channels=3,
                filter_type="trilinear",
            ),
            RunSpec(
                name=f"{variant}_grid6_eval6",
                group="volume",
                description="gridvolume 6-channel eval_6",
                case="grid_volume_eval",
                method="eval_6",
                variant=variant,
                channels=6,
                filter_type="trilinear",
            ),
            RunSpec(
                name=f"{variant}_grid6_eval_n",
                group="volume",
                description="gridvolume 6-channel eval_n",
                case="grid_volume_eval",
                method="eval_n",
                variant=variant,
                channels=6,
                filter_type="trilinear",
            ),
        ])

    for ad_variant in [v for v in variants if v in ("cuda_ad_rgb", "llvm_ad_rgb")]:
        specs.extend([
            RunSpec(
                name=f"{ad_variant}_bitmap_field_rgb",
                group="field",
                description="bitmap Field eval_color3",
                case="field_fixed_eval",
                method="eval_color3",
                variant=ad_variant,
                channels=3,
                field_plugin="bitmap",
                std_supported=False,
            ),
            RunSpec(
                name=f"{ad_variant}_bitmap_field_scalar",
                group="field",
                description="bitmap Field eval_1",
                case="field_fixed_eval",
                method="eval_1",
                variant=ad_variant,
                channels=1,
                field_plugin="bitmap",
                std_supported=False,
            ),
            RunSpec(
                name=f"{ad_variant}_gridvolume_field6",
                group="field",
                description="gridvolume Field eval_array6",
                case="field_fixed_eval",
                method="eval_array6",
                variant=ad_variant,
                channels=6,
                filter_type="trilinear",
                field_plugin="gridvolume",
                std_supported=False,
            ),
            RunSpec(
                name=f"{ad_variant}_gridvolume_field6_eval_n",
                group="field",
                description="gridvolume Field eval_n",
                case="field_fixed_eval",
                method="eval_n",
                variant=ad_variant,
                channels=6,
                filter_type="trilinear",
                field_plugin="gridvolume",
                std_supported=False,
            ),
            RunSpec(
                name=f"{ad_variant}_neuralfield_color3",
                group="neural",
                description="neuralfield Color3 inference",
                case="neural_field_inference",
                method="eval_color3",
                variant=ad_variant,
                channels=3,
                out_dim=3,
                args_mode="no_args",
                args_dim=0,
                std_supported=False,
            ),
            RunSpec(
                name=f"{ad_variant}_neuralfield_generic_args",
                group="neural",
                description="neuralfield generic eval with args",
                case="field_generic_eval",
                method="eval",
                variant=ad_variant,
                channels=3,
                out_dim=3,
                args_mode="args",
                args_dim=4,
                std_supported=False,
            ),
            RunSpec(
                name=f"{ad_variant}_neuralbsdf_eval",
                group="bsdf",
                description="neuralbsdf diffuse eval",
                case="neuralbsdf_eval",
                method="eval",
                variant=ad_variant,
                channels=3,
                std_supported=False,
            ),
            RunSpec(
                name=f"{ad_variant}_neuralbsdf_eval_pdf",
                group="bsdf",
                description="neuralbsdf diffuse eval_pdf",
                case="neuralbsdf_eval",
                method="eval_pdf",
                variant=ad_variant,
                channels=3,
                std_supported=False,
            ),
        ])
    return specs


def select_specs(specs: list[RunSpec], suite: str) -> list[RunSpec]:
    if suite == "all":
        return specs
    if suite == "common":
        return [spec for spec in specs if spec.std_supported]
    if suite == "field":
        return [spec for spec in specs if not spec.std_supported]
    raise ValueError(f"unknown suite: {suite}")


def run_one(
    spec: RunSpec,
    implementation: str,
    build_dir: Path,
    source_root: Path,
    output_dir: Path,
    repeats: int,
    warmup: int,
    size: int,
    scalar_iterations: int,
) -> dict[str, Any]:
    raw_path = output_dir / "raw" / f"{spec.name}.{implementation}.json"
    raw_path.parent.mkdir(parents=True, exist_ok=True)

    bench = repo_root() / "benchmarks/fields/bench_fields.py"
    argv = [
        "python", str(bench),
        "--variant", spec.variant,
        "--case", spec.case,
        "--method", spec.method,
        "--channels", str(spec.channels),
        "--filter-type", spec.filter_type,
        "--wrap-mode", spec.wrap_mode,
        "--input", spec.input,
        "--repeats", str(repeats),
        "--warmup", str(warmup),
        "--size", str(size),
        "--scalar-iterations", str(scalar_iterations),
        "--json", str(raw_path),
    ]
    if spec.field_plugin:
        argv += ["--field-plugin", spec.field_plugin]
    if spec.args_mode:
        argv += ["--args-mode", spec.args_mode, "--args-dim", str(spec.args_dim)]
    if spec.out_dim:
        argv += ["--out-dim", str(spec.out_dim)]

    command = "source {} && {}".format(
        shlex.quote(str(build_dir / "setpath.sh")),
        " ".join(shlex.quote(arg) for arg in argv),
    )
    env = os.environ.copy()
    env["PYTHONNOUSERSITE"] = "1"
    env["MITSUBA_BENCH_IMPLEMENTATION"] = implementation
    env["MITSUBA_BENCH_BUILD_DIR"] = str(build_dir)
    env["MITSUBA_BENCH_SOURCE_ROOT"] = str(source_root)
    completed = subprocess.run(
        ["bash", "-lc", command],
        cwd=repo_root(),
        text=True,
        capture_output=True,
        env=env,
    )

    row: dict[str, Any] = {
        "implementation": implementation,
        "name": spec.name,
        "group": spec.group,
        "description": spec.description,
        "variant": spec.variant,
        "case": spec.case,
        "method": spec.method,
        "channels": spec.channels,
        "status": "passed" if completed.returncode == 0 else "failed",
        "raw_json": str(raw_path),
    }
    if completed.returncode != 0:
        row["stderr"] = completed.stderr[-4000:]
        row["stdout"] = completed.stdout[-4000:]
        return row

    payload = json.loads(raw_path.read_text())
    metrics = payload.get("metrics", {})
    wall_summary = payload.get("summary", {})
    kernel_summary = metrics.get("kernel_execution_summary_ms", {})
    first_history = metrics.get("first_kernel_history", {})
    repeat_operation_counts = [
        int(summary.get("operation_count") or 0)
        for summary in metrics.get("kernel_repeat_summaries", [])
        if int(summary.get("operation_count") or 0) > 0
    ]

    wall_median_ms = seconds_to_ms(wall_summary.get("median"))
    wall_mean_ms = seconds_to_ms(wall_summary.get("mean"))
    wall_stdev_ms = seconds_to_ms(wall_summary.get("stdev"))
    kernel_median_ms = kernel_summary.get("median")
    kernel_mean_ms = kernel_summary.get("mean")
    kernel_stdev_ms = kernel_summary.get("stdev")
    use_kernel_time = (
        isinstance(kernel_median_ms, (int, float)) and
        kernel_median_ms > 0 and
        kernel_summary.get("samples", 0) > 0
    )
    timing_source = "kernel_execution" if use_kernel_time else "wall"
    median_ms = float(kernel_median_ms if use_kernel_time else wall_median_ms)
    mean_ms = float(kernel_mean_ms if use_kernel_time else wall_mean_ms)
    stdev_ms = float(kernel_stdev_ms if use_kernel_time else wall_stdev_ms)

    row.update({
        "timing_source": timing_source,
        "median_ms": median_ms,
        "mean_ms": mean_ms,
        "stdev_ms": stdev_ms,
        "median_seconds": median_ms / 1000.0,
        "mean_seconds": mean_ms / 1000.0,
        "stdev_seconds": stdev_ms / 1000.0,
        "wall_median_ms": wall_median_ms,
        "wall_mean_ms": wall_mean_ms,
        "wall_stdev_ms": wall_stdev_ms,
        "kernel_median_ms": kernel_median_ms,
        "kernel_mean_ms": kernel_mean_ms,
        "kernel_stdev_ms": kernel_stdev_ms,
        "first_trace_seconds": payload.get("first_trace_seconds"),
        "first_wall_ms": seconds_to_ms(metrics.get("first_wall_seconds")),
        "first_eval_wall_ms": seconds_to_ms(metrics.get("first_eval_wall_seconds")),
        "first_setup_ms": seconds_to_ms(metrics.get("first_setup_seconds")),
        "first_kernel_execution_ms": first_history.get("execution_time_ms"),
        "first_codegen_ms": first_history.get("codegen_time_ms"),
        "first_backend_ms": first_history.get("backend_time_ms"),
        "first_kernel_total_ms": first_history.get("total_time_ms"),
        "first_cache_hits": first_history.get("cache_hits"),
        "first_cache_misses": first_history.get("cache_misses"),
        "first_operation_count": first_history.get("operation_count"),
        "operation_count_median": (
            statistics.median(repeat_operation_counts)
            if repeat_operation_counts else None
        ),
        "operation_count_max": (
            max(repeat_operation_counts) if repeat_operation_counts else None
        ),
        "launches": metrics.get("launches", {}).get("total"),
        "memory_watermark_total": metrics.get("memory_watermark_total"),
        "result": payload.get("result", {}),
        "environment": payload.get("environment", {}),
    })
    return row


def seconds_to_ms(value: Any) -> float | None:
    if isinstance(value, (int, float)):
        return float(value) * 1000.0
    return None


def flatten_signature_numbers(value: Any) -> list[float]:
    numbers: list[float] = []
    if isinstance(value, dict):
        for key in ("value", "sum", "mean", "min", "max",
                    "sample0", "sample_mid", "sample_last"):
            if isinstance(value.get(key), (int, float)):
                numbers.append(float(value[key]))
        for item in value.get("items", []):
            numbers.extend(flatten_signature_numbers(item))
    elif isinstance(value, list):
        for item in value:
            numbers.extend(flatten_signature_numbers(item))
    return numbers


def signature_max_abs_diff(a: Any, b: Any) -> float | None:
    av = flatten_signature_numbers(a)
    bv = flatten_signature_numbers(b)
    if not av or len(av) != len(bv):
        return None
    return max(abs(x - y) for x, y in zip(av, bv))


def build_comparisons(rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    by_key = {(row["implementation"], row["name"]): row for row in rows}
    comparisons = []
    for row in rows:
        if row["implementation"] != "field_based":
            continue
        std = by_key.get(("standard", row["name"]))
        if not std or std.get("status") != "passed" or row.get("status") != "passed":
            continue
        std_median = std.get("median_ms")
        field_median = row.get("median_ms")
        if not std_median or not field_median:
            continue
        comparisons.append({
            "name": row["name"],
            "group": row["group"],
            "description": row["description"],
            "variant": row["variant"],
            "method": row["method"],
            "standard_timing_source": std.get("timing_source"),
            "field_based_timing_source": row.get("timing_source"),
            "standard_median_ms": std_median,
            "field_based_median_ms": field_median,
            "field_vs_standard": field_median / std_median,
            "standard_first_wall_ms": std.get("first_wall_ms"),
            "field_based_first_wall_ms": row.get("first_wall_ms"),
            "standard_first_kernel_total_ms": std.get("first_kernel_total_ms"),
            "field_based_first_kernel_total_ms": row.get("first_kernel_total_ms"),
            "standard_operation_count_median": std.get("operation_count_median"),
            "field_based_operation_count_median": row.get("operation_count_median"),
            "operation_count_delta": (
                (row.get("operation_count_median") or 0) -
                (std.get("operation_count_median") or 0)
            ),
            "standard_first_operation_count": std.get("first_operation_count"),
            "field_based_first_operation_count": row.get("first_operation_count"),
            "launch_delta": (row.get("launches") or 0) - (std.get("launches") or 0),
            "memory_delta": ((row.get("memory_watermark_total") or 0) -
                             (std.get("memory_watermark_total") or 0)),
            "result_max_abs_diff": signature_max_abs_diff(std.get("result"), row.get("result")),
        })
    return comparisons


def write_csv(path: Path, rows: list[dict[str, Any]], fieldnames: list[str]) -> None:
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def markdown_table(headers: list[str], rows: list[list[Any]]) -> str:
    def fmt(value: Any) -> str:
        if value is None:
            return "n/a"
        if isinstance(value, float):
            return f"{value:.4g}"
        return str(value)

    out = ["| " + " | ".join(headers) + " |",
           "| " + " | ".join(["---"] * len(headers)) + " |"]
    for row in rows:
        out.append("| " + " | ".join(fmt(v) for v in row) + " |")
    return "\n".join(out)


def percent(value: Any) -> str:
    if not isinstance(value, (int, float)):
        return "n/a"
    return f"{100.0 * float(value):.3g}%"


def stdev_percent(row: dict[str, Any]) -> str:
    median = row.get("median_ms")
    stdev = row.get("stdev_ms")
    if not isinstance(median, (int, float)) or not isinstance(stdev, (int, float)) or median == 0:
        return "n/a"
    return percent(stdev / median)


def first_environment(rows: list[dict[str, Any]], implementation: str) -> dict[str, Any]:
    for row in rows:
        if row.get("implementation") == implementation and isinstance(row.get("environment"), dict):
            return row["environment"]
    return {}


def first_signature_stats(value: Any) -> dict[str, float] | None:
    if isinstance(value, dict):
        if all(isinstance(value.get(key), (int, float)) for key in ("sum", "mean", "min", "max")):
            return {
                key: float(value[key])
                for key in ("sum", "mean", "min", "max",
                            "sample0", "sample_mid", "sample_last")
                if isinstance(value.get(key), (int, float))
            }
        for item in value.get("items", []):
            result = first_signature_stats(item)
            if result is not None:
                return result
    if isinstance(value, list):
        for item in value:
            result = first_signature_stats(item)
            if result is not None:
                return result
    return None


def write_bar_svg(path: Path, title: str, rows: list[dict[str, Any]], key: str) -> None:
    values = [float(row[key]) for row in rows if isinstance(row.get(key), (int, float))]
    width = 920
    row_h = 28
    margin_l = 300
    margin_r = 40
    height = 70 + row_h * max(1, len(values))
    max_value = max(values) if values else 1.0
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">',
        '<style>text{font-family:sans-serif;font-size:12px} .title{font-size:16px;font-weight:600}</style>',
        f'<text class="title" x="20" y="28">{title}</text>',
    ]
    for i, row in enumerate(rows):
        if not isinstance(row.get(key), (int, float)):
            continue
        y = 55 + i * row_h
        value = float(row[key])
        bar_w = (width - margin_l - margin_r) * value / max_value
        label = row.get("name", row.get("description", ""))
        parts.append(f'<text x="20" y="{y + 15}">{label}</text>')
        parts.append(f'<rect x="{margin_l}" y="{y}" width="{bar_w:.2f}" height="18" fill="#4477aa"/>')
        parts.append(f'<text x="{margin_l + bar_w + 6:.2f}" y="{y + 14}">{value:.3g}</text>')
    parts.append("</svg>\n")
    path.write_text("\n".join(parts))


def write_report(
    output_dir: Path,
    rows: list[dict[str, Any]],
    comparisons: list[dict[str, Any]],
    args: argparse.Namespace,
) -> None:
    passed = [row for row in rows if row.get("status") == "passed"]
    failed = [row for row in rows if row.get("status") != "passed"]
    common_rows = [
        [
            row["variant"], row["description"], row["implementation"],
            row.get("timing_source"), row.get("median_ms"),
            row.get("wall_median_ms"), row.get("kernel_median_ms"),
            row.get("first_wall_ms"), row.get("first_kernel_total_ms"),
            row.get("operation_count_median"), row.get("launches"),
            row.get("memory_watermark_total"),
        ]
        for row in passed
    ]
    comparison_rows = [
        [
            row["variant"], row["description"],
            row["standard_timing_source"], row["field_based_timing_source"],
            row["standard_median_ms"], row["field_based_median_ms"],
            row["field_vs_standard"], (row["field_vs_standard"] - 1.0) * 100.0,
            row["standard_first_wall_ms"], row["field_based_first_wall_ms"],
            row["operation_count_delta"], row["launch_delta"], row["memory_delta"],
            row["result_max_abs_diff"],
        ]
        for row in comparisons
    ]
    detail_rows = [
        [
            row["variant"], row["description"], row["implementation"],
            row.get("timing_source"), row.get("median_ms"), row.get("mean_ms"),
            row.get("stdev_ms"), stdev_percent(row), row.get("first_wall_ms"),
            row.get("first_eval_wall_ms"), row.get("first_kernel_total_ms"),
            row.get("first_cache_hits"), row.get("first_cache_misses"),
            row.get("operation_count_median"), row.get("first_operation_count"),
            row.get("launches"), row.get("memory_watermark_total"),
        ]
        for row in passed
    ]
    field_only_rows = [
        [
            row["variant"], row["description"], row.get("timing_source"),
            row.get("median_ms"), row.get("stdev_ms"), stdev_percent(row),
            row.get("first_wall_ms"), row.get("first_kernel_total_ms"),
            row.get("operation_count_median"), row.get("launches"),
            row.get("memory_watermark_total"),
        ]
        for row in passed
        if row["implementation"] == "field_based" and
        not any(c["name"] == row["name"] for c in comparisons)
    ]
    signature_rows = []
    for row in passed:
        stats = first_signature_stats(row.get("result"))
        if stats is not None:
            signature_rows.append([
                row["variant"], row["description"], row["implementation"],
                stats["sum"], stats["mean"], stats["min"], stats["max"],
                stats.get("sample0"), stats.get("sample_mid"),
                stats.get("sample_last"),
            ])

    std_env = first_environment(rows, "standard")
    field_env = first_environment(rows, "field_based")
    env_rows = [
        ["Standard source", std_env.get("source_root"), std_env.get("git_commit"),
         std_env.get("git_dirty"), std_env.get("build_dir")],
        ["Field source", field_env.get("source_root"), field_env.get("git_commit"),
         field_env.get("git_dirty"), field_env.get("build_dir")],
    ]
    interpretation_lines: list[str] = []
    if comparisons:
        min_ratio_row = min(comparisons, key=lambda row: row["field_vs_standard"])
        max_ratio_row = max(comparisons, key=lambda row: row["field_vs_standard"])
        worst_delta_row = max(
            comparisons,
            key=lambda row: abs(row["field_vs_standard"] - 1.0),
        )
        max_diff_values = [
            row.get("result_max_abs_diff") for row in comparisons
            if isinstance(row.get("result_max_abs_diff"), (int, float))
        ]
        interpretation_lines.append(
            "- Public API median ratios range from "
            f"{min_ratio_row['field_vs_standard']:.4g}x "
            f"({min_ratio_row['description']}) to "
            f"{max_ratio_row['field_vs_standard']:.4g}x "
            f"({max_ratio_row['description']})."
        )
        interpretation_lines.append(
            "- Largest public API delta is "
            f"{(worst_delta_row['field_vs_standard'] - 1.0) * 100.0:.3g}% "
            f"for {worst_delta_row['description']} "
            f"on {worst_delta_row['variant']}."
        )
        if max_diff_values:
            interpretation_lines.append(
                "- Maximum public API result-signature difference is "
                f"{max(max_diff_values):.4g}."
            )
    noisy_jit = [
        row for row in passed
        if row.get("timing_source") == "kernel_execution" and
        isinstance(row.get("median_ms"), (int, float)) and
        isinstance(row.get("stdev_ms"), (int, float)) and
        row.get("median_ms") and
        row["stdev_ms"] / row["median_ms"] > 1.0
    ]
    if noisy_jit:
        interpretation_lines.append(
            f"- {len(noisy_jit)} JIT rows have stdev above 100% of the "
            "median; the median remains the primary steady-state metric, and "
            "raw JSON files contain the repeat histories for outlier analysis."
        )
    field_only_count = sum(
        1 for row in passed
        if row["implementation"] == "field_based" and
        not any(c["name"] == row["name"] for c in comparisons)
    )
    if field_only_count:
        interpretation_lines.append(
            f"- {field_only_count} field-only row(s) are sanity runs, not "
            "standard-vs-field comparisons."
        )

    lines = [
        "# Field vs. Standard Mitsuba Benchmarks",
        "",
        f"Generated: {time.strftime('%Y-%m-%d %H:%M:%S %Z')}",
        "",
        "## Run Configuration",
        "",
        markdown_table(
            ["Parameter", "Value"],
            [
                ["Variants", args.variants],
                ["Suite", args.suite],
                ["Repeats", args.repeats],
                ["Warmup", args.warmup],
                ["JIT sample count", args.size],
                ["Scalar inner iterations", args.scalar_iterations],
            ],
        ),
        "",
        "## Build Environment",
        "",
        markdown_table(
            ["Implementation", "Source root", "Git commit", "Dirty", "Build dir"],
            env_rows,
        ),
        "",
        markdown_table(
            ["Item", "Value"],
            [
                ["Mitsuba version", field_env.get("mitsuba_version")],
                ["Dr.Jit version", field_env.get("drjit_version")],
                ["CPU", field_env.get("cpu") or std_env.get("cpu")],
                ["GPU", field_env.get("gpu") or std_env.get("gpu")],
                ["Python", field_env.get("python")],
                ["Build type", field_env.get("build_type")],
            ],
        ),
        "",
        "## Timing Method",
        "",
        "The selected steady-state median uses Dr.Jit `kernel_history()` "
        "`execution_time` whenever JIT kernel entries are present, otherwise it "
        "falls back to wall time. Wall timings bracket the benchmark operation "
        "and then force all returned values with `dr.eval()` plus "
        "`dr.sync_thread()`. First-run columns include one-time setup, tracing, "
        "compilation, and execution costs.",
        "",
        "## Interpretation",
        "",
        "\n".join(interpretation_lines)
        if interpretation_lines else "No comparison rows were available.",
        "",
        "## Public API Comparisons",
        "",
        markdown_table(
            [
                "Variant", "Case", "Std source", "Field source", "Std ms",
                "Field ms", "Ratio", "Slowdown %", "Std first wall ms",
                "Field first wall ms", "Op delta", "Launch delta",
                "Memory delta", "Result max abs diff",
            ],
            comparison_rows,
        ) if comparison_rows else "No common comparison rows were available.",
        "",
        "## Field-Only Runs",
        "",
        markdown_table(
            [
                "Variant", "Case", "Source", "Median ms", "Stdev ms",
                "Stdev %", "First wall ms", "First JIT total ms",
                "Ops median", "Launches", "Memory watermark",
            ],
            field_only_rows,
        ) if field_only_rows else "No field-only rows were available.",
        "",
        "## All Runs",
        "",
        markdown_table(
            [
                "Variant", "Case", "Implementation", "Source", "Median ms",
                "Mean ms", "Stdev ms", "Stdev %", "First wall ms",
                "First eval wall ms", "First JIT total ms", "Cache hits",
                "Cache misses", "Ops median", "First ops", "Launches",
                "Memory watermark",
            ],
            detail_rows,
        ) if common_rows else "No successful benchmark rows were available.",
        "",
        "## Result Signatures",
        "",
        markdown_table(
            [
                "Variant", "Case", "Implementation", "Sum", "Mean", "Min",
                "Max", "Sample 0", "Sample mid", "Sample last",
            ],
            signature_rows,
        ) if signature_rows else "No numeric result signatures were available.",
        "",
        "## Artifacts",
        "",
        "- `summary.csv`: all benchmark rows",
        "- `comparisons.csv`: standard vs field-based public API ratios",
        "- `median_ms.svg`: median time chart",
        "- `first_wall_ms.svg`: first-run wall time chart",
        "- `ratio.svg`: field-based / standard ratio chart",
        "- `raw/`: raw JSON payloads from `bench_fields.py`",
    ]
    if failed:
        lines.extend(["", "## Incomplete Runs", ""])
        if any(row.get("status") == "missing_standard_build" for row in failed):
            lines.extend([
                "Rows marked `missing_standard_build` mean that no usable "
                "`setpath.sh` was found for the clean standard checkout. "
                "Configure and build `mitsuba-std` separately, then rerun with "
                "`--std-build` to populate the ratio table.",
                "",
            ])
        if any(row.get("status") == "missing_standard_variant" for row in failed):
            lines.extend([
                "Rows marked `missing_standard_variant` mean that the standard "
                "build exists but does not include the requested variant. These "
                "field-based runs are not comparative evidence for that variant.",
                "",
            ])
        lines.append(markdown_table(
            ["Implementation", "Variant", "Case", "Status"],
            [[row["implementation"], row["variant"], row["description"], row["status"]] for row in failed],
        ))

    (output_dir / "REPORT.md").write_text("\n".join(lines) + "\n")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compare field-based Mitsuba against a clean standard checkout")
    parser.add_argument("--field-build", type=Path, default=repo_root() / "build-gpt-5")
    parser.add_argument("--std-root", type=Path, default=repo_root() / "mitsuba-std")
    parser.add_argument("--std-build", type=Path)
    parser.add_argument("--output-dir", type=Path, default=default_output_dir())
    parser.add_argument("--variants", default="scalar_rgb,auto_ad_rgb")
    parser.add_argument("--suite", choices=["common", "field", "all"], default="all")
    parser.add_argument("--repeats", type=int, default=15)
    parser.add_argument("--warmup", type=int, default=4)
    parser.add_argument("--size", type=int, default=1 << 18)
    parser.add_argument("--scalar-iterations", type=int, default=20000)
    parser.add_argument("--keep-going", action="store_true")
    parser.add_argument("--allow-missing-std", action="store_true")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    field_build = args.field_build.resolve()
    std_root = args.std_root.resolve()
    std_build = args.std_build.resolve() if args.std_build else discover_std_build(std_root)

    if not (field_build / "setpath.sh").exists():
        raise SystemExit(f"Missing field build setpath.sh: {field_build}")
    std_available = std_build is not None and (std_build / "setpath.sh").exists()
    if args.suite != "field" and not std_available and not args.allow_missing_std:
        raise SystemExit(
            "Missing standard Mitsuba build. Configure/build mitsuba-std first, "
            "or pass --std-build /path/to/build."
        )

    variants = resolve_variants(args.variants, field_build, std_build, args.suite)
    args.variants = ",".join(variants)
    std_variants = available_build_variants(std_build) if std_available else []
    missing_std_variants = [
        variant for variant in variants
        if args.suite != "field" and variant not in std_variants
    ]
    if missing_std_variants and not args.allow_missing_std:
        raise SystemExit(
            "Standard Mitsuba build does not provide variant(s) "
            f"{', '.join(missing_std_variants)}. Rebuild the standard checkout "
            "with those variants for a comparison, or pass --suite field for "
            "field-only CUDA runs."
        )
    specs = select_specs(base_specs(variants), args.suite)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    rows: list[dict[str, Any]] = []
    for spec in specs:
        if spec.std_supported and args.suite != "field":
            if std_available and spec.variant in std_variants:
                row = run_one(spec, "standard", std_build, std_root,
                              args.output_dir, args.repeats, args.warmup, args.size,
                              args.scalar_iterations)
            else:
                row = {
                    "implementation": "standard",
                    "name": spec.name,
                    "group": spec.group,
                    "description": spec.description,
                    "variant": spec.variant,
                    "case": spec.case,
                    "method": spec.method,
                    "channels": spec.channels,
                    "status": (
                        "missing_standard_variant"
                        if std_available else "missing_standard_build"
                    ),
                    "raw_json": "",
                }
            rows.append(row)
            if row["status"] != "passed" and not args.keep_going:
                print(json.dumps(row, indent=2), file=sys.stderr)
                raise SystemExit(1)

        row = run_one(spec, "field_based", field_build, repo_root(),
                      args.output_dir, args.repeats, args.warmup, args.size,
                      args.scalar_iterations)
        rows.append(row)
        if row["status"] != "passed" and not args.keep_going:
            print(json.dumps(row, indent=2), file=sys.stderr)
            raise SystemExit(1)

    comparisons = build_comparisons(rows)
    write_csv(
        args.output_dir / "summary.csv",
        rows,
        [
            "implementation", "name", "group", "description", "variant",
            "case", "method", "channels", "status", "timing_source",
            "median_ms", "mean_ms", "stdev_ms", "wall_median_ms",
            "wall_mean_ms", "wall_stdev_ms", "kernel_median_ms",
            "kernel_mean_ms", "kernel_stdev_ms", "first_wall_ms",
            "first_eval_wall_ms", "first_setup_ms",
            "first_kernel_execution_ms", "first_codegen_ms",
            "first_backend_ms", "first_kernel_total_ms",
            "first_cache_hits", "first_cache_misses",
            "first_operation_count", "operation_count_median",
            "operation_count_max", "launches",
            "memory_watermark_total", "raw_json",
        ],
    )
    write_csv(
        args.output_dir / "comparisons.csv",
        comparisons,
        [
            "name", "group", "description", "variant", "method",
            "standard_timing_source", "field_based_timing_source",
            "standard_median_ms", "field_based_median_ms",
            "field_vs_standard", "standard_first_wall_ms",
            "field_based_first_wall_ms", "standard_first_kernel_total_ms",
            "field_based_first_kernel_total_ms",
            "standard_operation_count_median",
            "field_based_operation_count_median", "operation_count_delta",
            "standard_first_operation_count",
            "field_based_first_operation_count", "launch_delta", "memory_delta",
            "result_max_abs_diff",
        ],
    )
    write_bar_svg(args.output_dir / "median_ms.svg", "Median time (ms)", [
        replace_row(row)
        for row in rows if row.get("status") == "passed"
    ], "median_ms")
    write_bar_svg(args.output_dir / "first_wall_ms.svg", "First-run wall time (ms)", [
        replace_row(row)
        for row in rows if row.get("status") == "passed"
    ], "first_wall_ms")
    write_bar_svg(args.output_dir / "ratio.svg", "Field-based / standard median ratio",
                  comparisons, "field_vs_standard")
    write_report(args.output_dir, rows, comparisons, args)

    print(f"Wrote benchmark report to {args.output_dir / 'REPORT.md'}")


def replace_row(row: dict[str, Any], **updates: Any) -> dict[str, Any]:
    result = dict(row)
    result.update(updates)
    result["name"] = f"{row['name']} ({row['implementation']})"
    return result


if __name__ == "__main__":
    main()
