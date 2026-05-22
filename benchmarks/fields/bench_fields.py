#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import math
import os
import platform
import statistics
import subprocess
import tempfile
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Callable

import drjit as dr
import mitsuba as mi


@dataclass
class BenchmarkConfig:
    variant: str
    case: str
    repeats: int
    warmup: int
    size: int
    scalar_iterations: int
    method: str
    channels: int
    filter_type: str
    wrap_mode: str
    raw: bool
    storage_format: str
    input: str
    use_grid_bbox: bool
    coord_distribution: str
    inactive_fraction: float
    args_mode: str
    field_plugin: str
    encoding: str
    decoder: str
    out_dim: int
    args_dim: int
    kernel_history: bool
    fail_threshold: float | None
    fail_launch_threshold: int | None
    fail_memory_watermark: int | None


@dataclass
class BenchmarkResult:
    config: BenchmarkConfig
    environment: dict[str, Any]
    first_trace_seconds: float
    timings: list[float]
    summary: dict[str, float]
    metrics: dict[str, Any]
    result: dict[str, Any]


class BenchmarkContext:
    def __init__(self, config: BenchmarkConfig):
        self.config = config
        self.cache: dict[str, Any] = {}
        self.setup_seconds = 0.0
        self._tempdir: tempfile.TemporaryDirectory[str] | None = None

    @property
    def sample_count(self) -> int:
        if self.config.variant.startswith("scalar_"):
            return 1
        return self.config.size

    @property
    def iterations(self) -> int:
        if self.config.variant.startswith("scalar_"):
            return max(1, self.config.scalar_iterations)
        return 1

    def get(self, key: str, setup: Callable[[], Any]) -> Any:
        if key not in self.cache:
            start = time.perf_counter()
            self.cache[key] = setup()
            self.setup_seconds += time.perf_counter() - start
        return self.cache[key]

    def temp_path(self, name: str) -> Path:
        if self._tempdir is None:
            self._tempdir = tempfile.TemporaryDirectory(prefix="mitsuba-field-bench-")
        return Path(self._tempdir.name) / name


CaseFn = Callable[[BenchmarkContext], Any]


@dataclass(frozen=True)
class CaseInfo:
    name: str
    group: str
    description: str
    implemented: bool = True


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def source_root() -> Path:
    return Path(os.environ.get("MITSUBA_BENCH_SOURCE_ROOT", repo_root())).resolve()


def build_dir(config: BenchmarkConfig) -> Path:
    env_build = os.environ.get("MITSUBA_BENCH_BUILD_DIR")
    if env_build:
        return Path(env_build).resolve()
    return repo_root() / f"build-{config.variant}"


def git_output(*args: str) -> str | None:
    try:
        return subprocess.check_output(
            ["git", *args],
            cwd=source_root(),
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except Exception:
        return None


def command_output(args: list[str]) -> str | None:
    try:
        return subprocess.check_output(args, text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        return None


def cpu_model() -> str | None:
    path = Path("/proc/cpuinfo")
    if path.exists():
        for line in path.read_text(errors="ignore").splitlines():
            if line.lower().startswith("model name"):
                return line.split(":", 1)[1].strip()
    return platform.processor() or None


def gpu_summary() -> str | None:
    return command_output([
        "nvidia-smi",
        "--query-gpu=name,driver_version,memory.total",
        "--format=csv,noheader",
    ])


def collect_environment(config: BenchmarkConfig) -> dict[str, Any]:
    build_type = None
    cache = build_dir(config) / "CMakeCache.txt"
    if cache.exists():
        for line in cache.read_text(errors="ignore").splitlines():
            if line.startswith("CMAKE_BUILD_TYPE:"):
                build_type = line.split("=", 1)[-1]
                break

    return {
        "mitsuba_version": mi.MI_VERSION,
        "drjit_version": getattr(dr, "__version__", None),
        "git_commit": git_output("rev-parse", "--short", "HEAD"),
        "git_dirty": bool(git_output("status", "--short")),
        "source_root": str(source_root()),
        "build_dir": str(build_dir(config)),
        "implementation": os.environ.get("MITSUBA_BENCH_IMPLEMENTATION"),
        "variant": config.variant,
        "available_variants": list(mi.variants()),
        "platform": platform.platform(),
        "cpu": cpu_model(),
        "gpu": gpu_summary(),
        "python": platform.python_version(),
        "debug_build": bool(getattr(mi, "DEBUG", False)),
        "build_type": build_type,
        "cuda_enabled": bool(getattr(mi, "MI_ENABLE_CUDA", False)),
        "llvm_available": dr.has_backend(dr.JitBackend.LLVM),
        "cuda_available": dr.has_backend(dr.JitBackend.CUDA),
        "kernel_history_enabled": True,
        "kernel_history_payload_enabled": config.kernel_history,
    }


def force_eval(value: Any = None) -> None:
    """Force evaluation of benchmark outputs before stopping a timer.

    Dr.Jit traces lazily for JIT variants, so measuring only the Python call
    would omit compilation and kernel execution.  Benchmark cases return every
    value that should contribute to the measurement; this helper evaluates the
    returned PyTree and synchronizes the backend before wall-clock timing stops.
    """

    if value is None:
        dr.eval()
        dr.sync_thread()
        return

    try:
        dr.eval(value)
        dr.sync_thread()
        return
    except Exception:
        pass

    if isinstance(value, dict):
        for item in value.values():
            force_eval(item)
        return

    if isinstance(value, (tuple, list)):
        for item in value:
            force_eval(item)
        return

    try:
        dr.eval(dr.ravel(value))
    except Exception:
        pass
    dr.sync_thread()


def summarize(timings: list[float]) -> dict[str, float]:
    if not timings:
        return {}
    result = {
        "min": min(timings),
        "median": statistics.median(timings),
        "mean": statistics.mean(timings),
        "max": max(timings),
        "samples": float(len(timings)),
    }
    if len(timings) > 1:
        result["stdev"] = statistics.stdev(timings)
    else:
        result["stdev"] = 0.0
    return result


def kernel_history_summary(history: list[dict[str, Any]]) -> dict[str, Any]:
    """Summarize Dr.Jit kernel history entries in milliseconds.

    `execution_time` is the primary steady-state timing metric for JIT variants.
    The first run additionally reports code generation and backend compilation
    time, which are expected one-time costs when kernels are not already cached.
    """

    summary: dict[str, Any] = {
        "entries": len(history),
        "jit_entries": 0,
        "kernel_entries": 0,
        "cache_hits": 0,
        "cache_disk_hits": 0,
        "cache_misses": 0,
        "soft_misses": 0,
        "hard_misses": 0,
        "execution_time_ms": 0.0,
        "codegen_time_ms": 0.0,
        "backend_time_ms": 0.0,
        "total_time_ms": 0.0,
        "operation_count": 0,
    }

    for entry in history:
        if "execution_time" in entry:
            summary["kernel_entries"] += 1
            summary["execution_time_ms"] += float(entry.get("execution_time") or 0.0)

        if "codegen_time" not in entry and "backend_time" not in entry:
            continue

        summary["jit_entries"] += 1
        cache_hit = bool(entry.get("cache_hit"))
        cache_disk = bool(entry.get("cache_disk"))
        if cache_hit:
            summary["cache_hits"] += 1
        else:
            summary["cache_misses"] += 1
            if cache_disk:
                summary["soft_misses"] += 1
            else:
                summary["hard_misses"] += 1
        if cache_disk:
            summary["cache_disk_hits"] += 1

        summary["codegen_time_ms"] += float(entry.get("codegen_time") or 0.0)
        summary["backend_time_ms"] += float(entry.get("backend_time") or 0.0)
        summary["operation_count"] += int(entry.get("operation_count") or 0)

    summary["total_time_ms"] = (
        summary["execution_time_ms"] +
        summary["codegen_time_ms"] +
        summary["backend_time_ms"]
    )
    return summary


def malloc_watermark() -> dict[str, int]:
    memory = {}
    for alloc_type in [
        dr.detail.AllocType.Host,
        dr.detail.AllocType.HostAsync,
        dr.detail.AllocType.HostPinned,
        dr.detail.AllocType.Device,
    ]:
        try:
            usage = dr.detail.malloc_watermark(alloc_type)
        except Exception:
            usage = 0
        if usage:
            memory[alloc_type.name] = int(usage)
    return memory


def launch_delta(start: tuple[int, int, int], end: tuple[int, int, int]) -> dict[str, int]:
    return {
        "total": int(end[0] - start[0]),
        "soft_misses": int(end[1] - start[1]),
        "hard_misses": int(end[2] - start[2]),
    }


def memory_total(memory: dict[str, int]) -> int:
    return int(sum(memory.values()))


def _scalar_float(value: Any) -> float:
    dr.eval(value)
    dr.sync_thread()
    return float(value[0] if hasattr(value, "__getitem__") else value)


def result_signature(value: Any) -> dict[str, Any]:
    """Return compact output statistics for benchmark result comparison.

    The benchmark suite remains timing-oriented, but these signatures make it
    easy to detect large disagreements between standard Mitsuba and the
    field-based tree when both execute the same public texture/volume workload.
    """

    if value is None:
        return {"kind": "none"}

    if isinstance(value, (tuple, list)):
        return {
            "kind": type(value).__name__,
            "items": [result_signature(item) for item in value],
        }

    if isinstance(value, (bool, int, float)):
        return {"kind": type(value).__name__, "value": float(value)}

    try:
        flat = dr.ravel(value)
        dr.eval(flat)
        dr.sync_thread()
        return {
            "kind": type(value).__name__,
            "shape": list(dr.shape(value)),
            "width": int(dr.width(value)),
            "sum": _scalar_float(dr.sum(flat, axis=None)),
            "mean": _scalar_float(dr.mean(flat, axis=None)),
            "min": _scalar_float(dr.min(flat, axis=None)),
            "max": _scalar_float(dr.max(flat, axis=None)),
        }
    except Exception:
        return {
            "kind": type(value).__name__,
            "repr": repr(value)[:160],
        }


def measure_case(ctx: BenchmarkContext, fn: CaseFn) -> BenchmarkResult:
    def run_iterations() -> Any:
        last_value = None
        for _ in range(ctx.iterations):
            last_value = fn(ctx)
        force_eval(last_value)
        return last_value

    # Kernel history is always enabled while measuring. The optional CLI flag
    # only controls whether representative raw entries are serialized.
    with dr.scoped_set_flag(dr.JitFlag.KernelHistory, True):
        dr.kernel_history_clear()
        setup_before = ctx.setup_seconds
        start = time.perf_counter()
        value = fn(ctx)
        force_eval(value)
        first_elapsed = time.perf_counter() - start
        first_history = dr.kernel_history()
        setup_delta = ctx.setup_seconds - setup_before
        first_eval = max(0.0, first_elapsed - setup_delta)
        first_history_summary = kernel_history_summary(first_history)

        for _ in range(ctx.config.warmup):
            dr.kernel_history_clear()
            run_iterations()
            dr.kernel_history()

        timings = []
        kernel_execution_times_ms = []
        kernel_total_times_ms = []
        repeat_kernel_summaries = []
        repeat_history_preview = []
        dr.detail.malloc_clear_statistics()
        launch_start = dr.detail.launch_stats()

        for _ in range(ctx.config.repeats):
            dr.kernel_history_clear()
            start = time.perf_counter()
            value = run_iterations()
            timings.append(time.perf_counter() - start)
            history = dr.kernel_history()
            history_summary = kernel_history_summary(history)
            repeat_kernel_summaries.append(history_summary)
            if history_summary["kernel_entries"]:
                kernel_execution_times_ms.append(history_summary["execution_time_ms"])
            if history_summary["kernel_entries"] or history_summary["jit_entries"]:
                kernel_total_times_ms.append(history_summary["total_time_ms"])
            if ctx.config.kernel_history and not repeat_history_preview and history:
                repeat_history_preview = [str(entry) for entry in history[:3]]

        launch_end = dr.detail.launch_stats()

    memory = malloc_watermark()
    metrics = {
        "timing_method": (
            "Wall time uses perf_counter around traced work followed by dr.eval() "
            "and dr.sync_thread(); JIT timing uses dr.kernel_history() execution_time."
        ),
        "launches": launch_delta(launch_start, launch_end),
        "memory_watermark": memory,
        "memory_watermark_total": memory_total(memory),
        "sample_count_per_iteration": ctx.sample_count,
        "iterations_per_repeat": ctx.iterations,
        "effective_sample_count": ctx.sample_count * ctx.iterations,
        "setup_seconds": ctx.setup_seconds,
        "first_setup_seconds": setup_delta,
        "first_wall_seconds": first_elapsed,
        "first_eval_wall_seconds": first_eval,
        "first_kernel_history": first_history_summary,
        "kernel_execution_summary_ms": summarize(kernel_execution_times_ms),
        "kernel_total_summary_ms": summarize(kernel_total_times_ms),
        "kernel_repeat_summaries": repeat_kernel_summaries,
    }
    if ctx.config.kernel_history:
        metrics["kernel_history_count"] = (
            len(first_history) + sum(int(s["entries"]) for s in repeat_kernel_summaries)
        )
        metrics["kernel_history_first"] = [str(entry) for entry in first_history[:3]]
        metrics["kernel_history_repeat"] = repeat_history_preview

    return BenchmarkResult(
        config=ctx.config,
        environment=collect_environment(ctx.config),
        first_trace_seconds=first_eval,
        timings=timings,
        summary=summarize(timings),
        metrics=metrics,
        result=result_signature(value),
    )


def make_uvs(ctx: BenchmarkContext) -> mi.SurfaceInteraction3f:
    n = ctx.sample_count
    si = dr.zeros(mi.SurfaceInteraction3f, n)
    t = mi.Float(0.0) if n == 1 else dr.arange(mi.Float, n) / max(n, 1)

    if ctx.config.coord_distribution == "coherent":
        u = t
        v = dr.fma(t, 0.5, 0.25)
    elif ctx.config.coord_distribution == "out_of_range":
        u = dr.fma(t, 4.0, -1.5)
        v = dr.fma(t, -3.0, 2.0)
    else:
        u = dr.sin(t * 12.9898) * 43758.5453
        v = dr.sin(t * 78.233) * 24634.6345
        u = u - dr.floor(u)
        v = v - dr.floor(v)

    si.uv = mi.Point2f(u, v)
    si.p = mi.Point3f(u, v, 0)
    si.n = mi.Normal3f(0, 0, 1)
    si.wi = mi.Vector3f(0, 0, 1)
    si.sh_frame = mi.Frame3f(si.n)
    return si


def make_positions(ctx: BenchmarkContext) -> mi.Interaction3f:
    n = ctx.sample_count
    it = dr.zeros(mi.Interaction3f, n)
    t = mi.Float(0.0) if n == 1 else dr.arange(mi.Float, n) / max(n, 1)

    if ctx.config.coord_distribution == "coherent":
        p = mi.Point3f(t, dr.fma(t, 0.5, 0.25), dr.fma(t, 0.25, 0.5))
    elif ctx.config.coord_distribution == "out_of_range":
        p = mi.Point3f(dr.fma(t, 3.0, -1.0), dr.fma(t, -2.0, 1.5), dr.fma(t, 4.0, -2.0))
    else:
        x = dr.sin(t * 12.9898) * 43758.5453
        y = dr.sin(t * 78.233) * 24634.6345
        z = dr.sin(t * 39.425) * 95731.1357
        p = mi.Point3f(x - dr.floor(x), y - dr.floor(y), z - dr.floor(z))

    it.p = p
    return it


def active_mask(ctx: BenchmarkContext):
    if ctx.config.inactive_fraction <= 0:
        return True
    n = ctx.sample_count
    t = mi.Float(0.0) if n == 1 else dr.arange(mi.Float, n) / max(n, 1)
    return t >= ctx.config.inactive_fraction


def tensor2(channels: int, resolution: int = 128):
    return dr.full(mi.TensorXf, 0.5, shape=[resolution, resolution, channels])


def tensor3(channels: int, resolution: int = 64):
    return dr.full(mi.TensorXf, 0.5, shape=[resolution, resolution, resolution, channels])


def bitmap_filename(channels: int) -> str:
    if channels == 1:
        return str(repo_root() / "resources/data/common/textures/noise_02.png")
    if channels == 3:
        return str(repo_root() / "resources/data/common/textures/carrot.png")
    raise ValueError("--input file for bitmap cases only supports --channels 1 or 3")


def volume_filename(ctx: BenchmarkContext) -> str:
    path = ctx.temp_path(f"volume_{ctx.config.channels}_{ctx.config.size}.vol")
    if not path.exists():
        mi.VolumeGrid(tensor3(ctx.config.channels)).write(str(path))
    return str(path)


def bitmap_texture_config(ctx: BenchmarkContext) -> dict[str, Any]:
    filter_type = "bilinear" if ctx.config.filter_type == "trilinear" else ctx.config.filter_type
    config: dict[str, Any] = {
        "type": "bitmap",
        "raw": ctx.config.raw,
        "filter_type": filter_type,
        "wrap_mode": ctx.config.wrap_mode,
    }
    if ctx.config.input == "file":
        config["filename"] = bitmap_filename(ctx.config.channels)
    else:
        config["data"] = tensor2(ctx.config.channels)
    if ctx.config.storage_format != "auto":
        config["format"] = ctx.config.storage_format
    return config


def grid_volume_config(ctx: BenchmarkContext) -> dict[str, Any]:
    filter_type = "trilinear" if ctx.config.filter_type == "bilinear" else ctx.config.filter_type
    config: dict[str, Any] = {
        "type": "gridvolume",
        "raw": ctx.config.raw,
        "filter_type": filter_type,
        "wrap_mode": ctx.config.wrap_mode,
        "use_grid_bbox": ctx.config.use_grid_bbox,
    }
    if ctx.config.input == "file":
        config["filename"] = volume_filename(ctx)
    elif ctx.config.use_grid_bbox:
        config["grid"] = mi.VolumeGrid(tensor3(ctx.config.channels))
    else:
        config["data"] = tensor3(ctx.config.channels)
    return config


def bitmap_field_config(ctx: BenchmarkContext) -> dict[str, Any]:
    filter_type = "bilinear" if ctx.config.filter_type == "trilinear" else ctx.config.filter_type
    config: dict[str, Any] = {
        "type": ctx.config.field_plugin or "bitmapfield",
        "raw": ctx.config.raw,
        "filter_type": filter_type,
        "wrap_mode": ctx.config.wrap_mode,
    }
    if ctx.config.input == "file":
        config["filename"] = bitmap_filename(ctx.config.channels)
    else:
        config["data"] = tensor2(ctx.config.channels)
    return config


def grid_field_config(ctx: BenchmarkContext) -> dict[str, Any]:
    filter_type = "trilinear" if ctx.config.filter_type == "bilinear" else ctx.config.filter_type
    config: dict[str, Any] = {
        "type": ctx.config.field_plugin or "gridfield",
        "raw": ctx.config.raw,
        "filter_type": filter_type,
        "wrap_mode": ctx.config.wrap_mode,
    }
    if ctx.config.input == "file":
        config["filename"] = volume_filename(ctx)
    else:
        config["data"] = tensor3(ctx.config.channels)
    return config


def encoding_config(ctx: BenchmarkContext) -> dict[str, Any]:
    plugin = {
        "hashgrid": "hashgridencoding",
        "permuto": "permutoencoding",
        "sinusoidal": "sinusoidalencoding",
    }[ctx.config.encoding]
    return {
        "type": plugin,
        "input_dim": 2,
        "out_dim": max(ctx.config.out_dim, 8),
        "n_levels": 8,
        "n_features_per_level": 2,
        "base_resolution": 8,
        "per_level_scale": 1.5,
        "hashmap_size": 1 << 14,
    }


def neural_field_config(ctx: BenchmarkContext) -> dict[str, Any]:
    args_dim = 0 if ctx.config.args_mode == "no_args" else ctx.config.args_dim
    return {
        "type": ctx.config.field_plugin or "neuralfield",
        "domain": "Surface",
        "out_type": "Color3" if ctx.config.out_dim == 3 else "Features",
        "out_dim": ctx.config.out_dim,
        "args_dim": args_dim,
        "encoding": encoding_config(ctx),
        "decoder": ctx.config.decoder,
        "hidden_size": 64,
        "num_layers": 3,
    }


def call_bitmap_eval(texture, si, method: str, active, channels: int):
    if method == "eval":
        return texture.eval(si, active)
    if method == "eval_1":
        return texture.eval_1(si, active)
    if method == "eval_3":
        return texture.eval_3(si, active)
    if method == "eval_1_grad":
        return texture.eval_1_grad(si, active)
    if method == "mean":
        return texture.mean()
    if method == "resolution":
        return texture.resolution()
    if method == "all":
        values = [
            texture.eval(si, active),
            texture.eval_1(si, active),
        ]
        if channels == 3:
            values.append(texture.eval_3(si, active))
        if channels == 1:
            values.append(texture.eval_1_grad(si, active))
        values.append(texture.mean())
        values.append(texture.resolution())
        return values
    raise ValueError(f"Unsupported bitmap eval method: {method}")


def case_bitmap_eval(ctx: BenchmarkContext) -> Any:
    texture, si = ctx.get("bitmap_eval", lambda: (
        mi.load_dict(bitmap_texture_config(ctx)),
        make_uvs(ctx),
    ))
    return call_bitmap_eval(texture, si, ctx.config.method, active_mask(ctx), ctx.config.channels)


def case_bitmap_sampling(ctx: BenchmarkContext) -> Any:
    texture, si, sample = ctx.get("bitmap_sampling", lambda: (
        mi.load_dict(bitmap_texture_config(ctx)),
        make_uvs(ctx),
        mi.Point2f(0.37, 0.73),
    ))
    method = ctx.config.method
    if method == "all":
        return [
            texture.sample_position(sample, active_mask(ctx)),
            texture.pdf_position(sample, active_mask(ctx)),
            texture.sample_spectrum(si, mi.sample_shifted(mi.Float(0.5)), active_mask(ctx)),
            texture.pdf_spectrum(si, active_mask(ctx)),
        ]
    if method in ("sample_position", "all"):
        value = texture.sample_position(sample, active_mask(ctx))
    if method in ("pdf_position", "all"):
        value = texture.pdf_position(sample, active_mask(ctx))
    if method in ("sample_spectrum", "all"):
        value = texture.sample_spectrum(si, mi.sample_shifted(mi.Float(0.5)), active_mask(ctx))
    if method in ("pdf_spectrum", "all"):
        value = texture.pdf_spectrum(si, active_mask(ctx))
    if method not in ("sample_position", "pdf_position", "sample_spectrum", "pdf_spectrum", "all"):
        raise ValueError(f"Unsupported bitmap sampling method: {method}")
    return value


def case_grid_volume_eval(ctx: BenchmarkContext) -> Any:
    volume, it = ctx.get("grid_volume_eval", lambda: (
        mi.load_dict(grid_volume_config(ctx)),
        make_positions(ctx),
    ))
    active = active_mask(ctx)
    method = ctx.config.method
    if method == "eval":
        return volume.eval(it, active)
    elif method == "eval_1":
        return volume.eval_1(it, active)
    elif method == "eval_3":
        return volume.eval_3(it, active)
    elif method == "eval_6":
        return volume.eval_6(it, active)
    elif method == "eval_n":
        return volume.eval_n(it, active)
    elif method == "eval_gradient":
        return volume.eval_gradient(it, active)
    elif method == "max_per_channel":
        return volume.max_per_channel()
    elif method == "max":
        return volume.max()
    elif method == "bbox":
        return volume.bbox()
    elif method == "resolution":
        return volume.resolution()
    elif method == "channel_count":
        return volume.channel_count()
    elif method == "all":
        values = []
        if ctx.config.channels == 1:
            values.extend([
                volume.eval(it, active),
                volume.eval_1(it, active),
                volume.eval_gradient(it, active),
            ])
        elif ctx.config.channels == 3:
            values.extend([
                volume.eval(it, active),
                volume.eval_3(it, active),
            ])
        elif ctx.config.channels == 6:
            values.append(volume.eval_6(it, active))
        values.extend([
            volume.eval_n(it, active),
            volume.max_per_channel(),
            volume.max(),
            volume.bbox(),
            volume.resolution(),
            volume.channel_count(),
        ])
        return values
    else:
        raise ValueError(f"Unsupported grid volume method: {method}")


def call_field_fixed(field, interaction_record, method: str, active):
    if method == "eval":
        return field.eval(interaction_record, active=active)
    if method == "eval_1":
        return field.eval_1(interaction_record, active=active)
    if method == "eval_color3":
        return field.eval_color3(interaction_record, active=active)
    if method == "eval_array2":
        return field.eval_array2(interaction_record, active=active)
    if method == "eval_array3":
        return field.eval_array3(interaction_record, active=active)
    if method == "eval_spec":
        return field.eval_spec(interaction_record, active=active)
    if method == "eval_array6":
        return field.eval_array6(interaction_record, active=active)
    if method == "eval_n":
        return field.eval_n(interaction_record, field.out_dim(), active=active)
    raise ValueError(f"Unsupported field method: {method}")


def case_field_fixed_eval(ctx: BenchmarkContext) -> Any:
    field, record = ctx.get("field_fixed_eval", lambda: (
        mi.load_dict(bitmap_field_config(ctx) if ctx.config.field_plugin != "gridfield" else grid_field_config(ctx)),
        make_positions(ctx) if ctx.config.field_plugin == "gridfield" else make_uvs(ctx),
    ))
    return call_field_fixed(field, record, ctx.config.method, active_mask(ctx))


def case_field_generic_eval(ctx: BenchmarkContext) -> Any:
    field, si, args = ctx.get("field_generic_eval", lambda: (
        mi.load_dict(neural_field_config(ctx)),
        make_uvs(ctx),
        None if ctx.config.args_mode == "no_args" else [0.0] * ctx.config.args_dim,
    ))
    if args is None:
        return field.eval(si, active=active_mask(ctx))
    else:
        return field.eval(si, args=args, active=active_mask(ctx))


def case_neural_field_inference(ctx: BenchmarkContext) -> Any:
    field, si, args = ctx.get("neural_field_inference", lambda: (
        mi.load_dict(neural_field_config(ctx)),
        make_uvs(ctx),
        None if ctx.config.args_mode == "no_args" else [0.0] * ctx.config.args_dim,
    ))
    if ctx.config.method in ("eval_color3", "all"):
        if args is None:
            return field.eval_color3(si, active=active_mask(ctx))
        else:
            return field.eval_color3(si, args=args, active=active_mask(ctx))
    elif ctx.config.method in ("eval", "generic"):
        if args is None:
            return field.eval(si, active=active_mask(ctx))
        else:
            return field.eval(si, args=args, active=active_mask(ctx))
    else:
        return call_field_fixed(field, si, ctx.config.method, active_mask(ctx))


def case_neural_field_training(ctx: BenchmarkContext) -> Any:
    field, si, args, params = ctx.get("neural_field_training", lambda: (
        mi.load_dict(neural_field_config(ctx)),
        make_uvs(ctx),
        None if ctx.config.args_mode == "no_args" else [0.0] * ctx.config.args_dim,
        None,
    ))
    if params is None:
        params = mi.traverse(field)
        for key in params.keys():
            if "weight" in key or "encoding" in key:
                dr.enable_grad(params[key])
        ctx.cache["neural_field_training"] = (field, si, args, params)

    if args is None:
        value = field.eval_color3(si, active=active_mask(ctx))
    else:
        value = field.eval_color3(si, args=args, active=active_mask(ctx))
    loss = dr.mean(dr.sqr(value))
    dr.backward(loss)
    return loss


def case_neuralbsdf_eval(ctx: BenchmarkContext) -> Any:
    bsdf, si, ctx_bsdf, wo = ctx.get("neuralbsdf_eval", lambda: (
        mi.load_dict({
            "type": "neuralbsdf",
            "reflectance": bitmap_field_config(ctx),
        }),
        make_uvs(ctx),
        mi.BSDFContext(),
        mi.Vector3f(0.3, 0.2, math.sqrt(1.0 - 0.3 * 0.3 - 0.2 * 0.2)),
    ))
    active = active_mask(ctx)
    method = ctx.config.method
    if method == "eval":
        return bsdf.eval(ctx_bsdf, si, wo, active)
    elif method == "pdf":
        return bsdf.pdf(ctx_bsdf, si, wo, active)
    elif method == "eval_pdf":
        return bsdf.eval_pdf(ctx_bsdf, si, wo, active)
    elif method == "sample":
        return bsdf.sample(ctx_bsdf, si, mi.Float(0.37), mi.Point2f(0.41, 0.73), active)
    elif method == "all":
        return [
            bsdf.eval(ctx_bsdf, si, wo, active),
            bsdf.pdf(ctx_bsdf, si, wo, active),
            bsdf.eval_pdf(ctx_bsdf, si, wo, active),
            bsdf.sample(ctx_bsdf, si, mi.Float(0.37), mi.Point2f(0.41, 0.73), active),
        ]
    else:
        raise ValueError(f"Unsupported neuralbsdf method: {method}")


CASES: dict[str, CaseFn] = {
    "bitmap_eval": case_bitmap_eval,
    "bitmap_sampling": case_bitmap_sampling,
    "grid_volume_eval": case_grid_volume_eval,
    "field_fixed_eval": case_field_fixed_eval,
    "field_generic_eval": case_field_generic_eval,
    "neural_field_inference": case_neural_field_inference,
    "neural_field_training": case_neural_field_training,
    "neuralbsdf_eval": case_neuralbsdf_eval,
}


CASE_INFO = {
    "bitmap_eval": CaseInfo("bitmap_eval", "texture", "Bitmap eval/eval_1/eval_3/eval_1_grad baseline"),
    "bitmap_sampling": CaseInfo("bitmap_sampling", "texture", "Bitmap position and spectrum sampling baseline"),
    "grid_volume_eval": CaseInfo("grid_volume_eval", "volume", "Grid volume fixed and variable channel baseline"),
    "field_fixed_eval": CaseInfo("field_fixed_eval", "field", "Fixed-output field evaluation"),
    "field_generic_eval": CaseInfo("field_generic_eval", "field", "Dynamic field evaluation with optional arguments"),
    "neural_field_inference": CaseInfo("neural_field_inference", "neural", "Neural field inference"),
    "neural_field_training": CaseInfo("neural_field_training", "neural", "Neural field forward and backward pass"),
    "neuralbsdf_eval": CaseInfo("neuralbsdf_eval", "bsdf", "neuralbsdf eval/pdf/sample methods"),
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Field benchmark runner")
    parser.add_argument("--variant", default="scalar_rgb")
    parser.add_argument("--case", choices=sorted(CASES))
    parser.add_argument("--repeats", type=int, default=10)
    parser.add_argument("--warmup", type=int, default=3)
    parser.add_argument("--size", type=int, default=1 << 16)
    parser.add_argument("--scalar-iterations", type=int, default=10000)
    parser.add_argument("--method", default="all")
    parser.add_argument("--channels", type=int, default=3)
    parser.add_argument("--filter-type", choices=["nearest", "bilinear", "trilinear"], default="bilinear")
    parser.add_argument("--wrap-mode", choices=["repeat", "clamp", "mirror"], default="repeat")
    parser.add_argument("--raw", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--format", dest="storage_format", default="auto")
    parser.add_argument("--input", choices=["tensor", "file"], default="tensor")
    parser.add_argument("--use-grid-bbox", action="store_true")
    parser.add_argument("--coord-distribution", choices=["coherent", "random", "out_of_range"], default="coherent")
    parser.add_argument("--inactive-fraction", type=float, default=0.0)
    parser.add_argument("--args-mode", choices=["args", "no_args"], default="args")
    parser.add_argument("--field-plugin", default="")
    parser.add_argument("--encoding", choices=["hashgrid", "permuto", "sinusoidal"], default="hashgrid")
    parser.add_argument("--decoder", choices=["neural", "linear"], default="neural")
    parser.add_argument("--out-dim", type=int, default=3)
    parser.add_argument("--args-dim", type=int, default=4)
    parser.add_argument("--kernel-history", action="store_true")
    parser.add_argument("--json", type=Path)
    parser.add_argument("--compare", type=Path)
    parser.add_argument("--fail-threshold", type=float)
    parser.add_argument("--fail-launch-threshold", type=int)
    parser.add_argument("--fail-memory-watermark", type=int)
    parser.add_argument("--list", action="store_true")
    return parser.parse_args()


def selected_median_seconds(summary: dict[str, Any], metrics: dict[str, Any]) -> tuple[float | None, str]:
    kernel = metrics.get("kernel_execution_summary_ms", {})
    kernel_median = kernel.get("median")
    if isinstance(kernel_median, (int, float)) and kernel_median > 0 and kernel.get("samples", 0) > 0:
        return float(kernel_median) / 1000.0, "kernel_execution"
    median = summary.get("median")
    if isinstance(median, (int, float)):
        return float(median), "wall"
    return None, "missing"


def compare_results(result: BenchmarkResult, baseline_path: Path) -> dict[str, Any]:
    baseline = json.loads(baseline_path.read_text())
    baseline_median, baseline_source = selected_median_seconds(
        baseline.get("summary", {}),
        baseline.get("metrics", {}),
    )
    current_median, current_source = selected_median_seconds(result.summary, result.metrics)
    baseline_launches = baseline.get("metrics", {}).get("launches", {}).get("total")
    current_launches = result.metrics.get("launches", {}).get("total")
    baseline_memory = baseline.get("metrics", {}).get("memory_watermark_total")
    current_memory = result.metrics.get("memory_watermark_total")
    if baseline_median is None or current_median is None:
        return {"error": "missing median timing in current result or baseline"}
    ratio = current_median / baseline_median if baseline_median else float("inf")
    comparison = {
        "baseline": str(baseline_path),
        "baseline_timing_source": baseline_source,
        "current_timing_source": current_source,
        "baseline_median": baseline_median,
        "current_median": current_median,
        "ratio": ratio,
        "slowdown_percent": (ratio - 1.0) * 100.0,
    }
    if baseline_launches is not None and current_launches is not None:
        comparison["baseline_launches"] = baseline_launches
        comparison["current_launches"] = current_launches
        comparison["launch_delta"] = current_launches - baseline_launches
    if baseline_memory is not None and current_memory is not None:
        comparison["baseline_memory_watermark_total"] = baseline_memory
        comparison["current_memory_watermark_total"] = current_memory
        comparison["memory_watermark_delta"] = current_memory - baseline_memory
    return comparison


def main() -> None:
    args = parse_args()

    if args.list:
        for name in sorted(CASE_INFO):
            info = CASE_INFO[name]
            print(f"{info.name:24s} {info.group:8s} {info.description}")
        return

    if not args.case:
        raise SystemExit("--case is required unless --list is specified")

    if args.inactive_fraction < 0 or args.inactive_fraction >= 1:
        raise SystemExit("--inactive-fraction must be in [0, 1)")
    if args.scalar_iterations < 1:
        raise SystemExit("--scalar-iterations must be positive")
    if args.args_mode == "no_args" and args.args_dim != 0:
        # Reflect the no-argument configuration in serialized results.
        args.args_dim = 0

    mi.set_variant(args.variant)
    config = BenchmarkConfig(
        variant=args.variant,
        case=args.case,
        repeats=args.repeats,
        warmup=args.warmup,
        size=args.size,
        scalar_iterations=args.scalar_iterations,
        method=args.method,
        channels=args.channels,
        filter_type=args.filter_type,
        wrap_mode=args.wrap_mode,
        raw=args.raw,
        storage_format=args.storage_format,
        input=args.input,
        use_grid_bbox=args.use_grid_bbox,
        coord_distribution=args.coord_distribution,
        inactive_fraction=args.inactive_fraction,
        args_mode=args.args_mode,
        field_plugin=args.field_plugin,
        encoding=args.encoding,
        decoder=args.decoder,
        out_dim=args.out_dim,
        args_dim=args.args_dim,
        kernel_history=args.kernel_history,
        fail_threshold=args.fail_threshold,
        fail_launch_threshold=args.fail_launch_threshold,
        fail_memory_watermark=args.fail_memory_watermark,
    )
    result = measure_case(BenchmarkContext(config), CASES[args.case])

    payload = asdict(result)
    if args.compare:
        payload["metrics"]["comparison"] = compare_results(result, args.compare)
        comparison = payload["metrics"]["comparison"]
        ratio = comparison.get("ratio")
        if args.fail_threshold is not None and ratio is not None and ratio > 1.0 + args.fail_threshold:
            print(json.dumps(payload, indent=2))
            raise SystemExit(
                f"Benchmark exceeded threshold: ratio={ratio:.4f}, "
                f"threshold={1.0 + args.fail_threshold:.4f}"
            )
        launch_delta_value = comparison.get("launch_delta")
        if (args.fail_launch_threshold is not None and launch_delta_value is not None and
                launch_delta_value > args.fail_launch_threshold):
            print(json.dumps(payload, indent=2))
            raise SystemExit(
                f"Benchmark launch delta exceeded threshold: delta={launch_delta_value}, "
                f"threshold={args.fail_launch_threshold}"
            )
        memory_delta = comparison.get("memory_watermark_delta")
        if (args.fail_memory_watermark is not None and memory_delta is not None and
                memory_delta > args.fail_memory_watermark):
            print(json.dumps(payload, indent=2))
            raise SystemExit(
                f"Benchmark memory watermark delta exceeded threshold: delta={memory_delta}, "
                f"threshold={args.fail_memory_watermark}"
            )

    print(json.dumps(payload, indent=2))
    if args.json:
        args.json.write_text(json.dumps(payload, indent=2) + "\n")


if __name__ == "__main__":
    main()
