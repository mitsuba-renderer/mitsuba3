"""
PyTest configuration for Mitsuba 3.

This module configures the test environment for Mitsuba, providing:
- Variant-specific fixtures for running tests with different rendering backends
- Test metrics collection (memory, time, kernel launches)
- Test cleanup between runs to prevent resource leaks
- Random number generator fixtures with fixed seeds for reproducibility
"""

import time
import pytest
import drjit as dr
import mitsuba as mi

# ===== RNG Seedinng Fixture =====


@pytest.fixture
def np_rng():
    import numpy as np

    return np.random.default_rng(seed=12345)


# ===== Variant Fixture Generation =====


def _variant_fixture_impl(request, variant):
    """
    Set variant and clean up after test. Ensures no leftover instances
    from other tests are still in registry, waits for all running kernels
    to finish and resets the JitFlag to default. Also frees the malloc cache to
    prevent tests from hogging all system memory.
    """
    mi.set_variant(variant)
    yield variant

    # Clean up after test
    dr.sync_thread()
    dr.flush_kernel_cache()
    dr.kernel_history_clear()
    dr.flush_malloc_cache()
    dr.detail.malloc_clear_statistics()
    dr.detail.clear_registry()
    dr.set_flag(dr.JitFlag.Default, True)


def generate_fixture(variant):
    @pytest.fixture()
    def fixture(request):
        yield from _variant_fixture_impl(request, variant)

    globals()["variant_" + variant] = fixture


def generate_fixture_group(name, variants):
    @pytest.fixture(params=variants)
    def fixture(request):
        yield from _variant_fixture_impl(request, request.param)

    globals()["variants_" + name] = fixture


class VariantFilter(list):
    """Helper class for filtering Mitsuba variants with a chainable interface."""

    def all(self, *patterns):
        """Return a new filter with variants containing all specified patterns."""
        return VariantFilter([v for v in self if all(p in v for p in patterns)])

    def exclude(self, *patterns):
        """Return a new filter excluding variants containing any of the patterns."""
        return VariantFilter([v for v in self if not any(p in v for p in patterns)])

    def one(self):
        """Return a list with the first variant, or empty list if none."""
        return self[:1]


# Get all compiled variants and filter by backend availability
available = []
for variant in mi.variants():
    if variant.startswith("cuda") and not dr.has_backend(dr.JitBackend.CUDA):
        continue
    if variant.startswith("llvm") and not dr.has_backend(dr.JitBackend.LLVM):
        continue
    available.append(variant)

# Create the variant filter helper
v = VariantFilter(available)

# Generate all possible variant names for fixture creation
suffixes = ["_mono", "_mono_polarized", "_rgb", "_spectral", "_spectral_polarized"]
suffix_variants = [a + b for a in suffixes for b in ["", "_double"]]
all_possible_variants = ["scalar" + s for s in suffix_variants] + [
    a + b + c for a in ["llvm", "cuda"] for b in ["", "_ad"] for c in suffix_variants]

# Create single variant fixtures for all possible variants
for variant in all_possible_variants:
    generate_fixture(variant)

# Create variant groups using the filter helper
variant_groups = {
    "any_scalar": v.all("scalar").one(),
    "any_llvm": v.all("llvm").one(),
    "any_cuda": v.all("cuda").one(),
    "all": v,
    "all_scalar": v.all("scalar"),
    "all_rgb": v.all("rgb"),
    "all_spectral": v.all("spectral"),
    "all_backends_once": v.all("scalar").one()
    + v.all("llvm").one()
    + v.all("cuda").one(),
    "vec_backends_once": v.all("llvm").one() + v.all("cuda").one(),
    "vec_backends_once_rgb": v.all("llvm", "rgb").one() + v.all("cuda", "rgb").one(),
    "vec_backends_once_spectral": v.all("llvm", "spectral").one()
    + v.all("cuda", "spectral").one(),
    "vec_rgb": v.all("rgb").exclude("scalar"),
    "vec_spectral": v.all("spectral").exclude("scalar"),
    "all_ad_rgb": v.all("ad", "rgb"),
    "all_ad_spectral": v.all("ad", "spectral"),
}

# Create parameterized group fixtures (only for non-empty groups)
for name, variant_list in variant_groups.items():
    generate_fixture_group(name, variant_list)


class TestMetricsPlugin:
    """
    Pytest plugin that collects and reports performance metrics for tests.

    Supports tracking:
    - memory: Peak memory usage with breakdown by allocation type
    - time: Test execution duration
    - launches: Number of kernel launches

    Usage: pytest --collect-metrics=memory,time,launches
    """

    def __init__(self, metrics_to_collect):
        self.data = {}  # {test_name: {metric: value}}
        self.enabled = set(metrics_to_collect)
        self.start_times = {}  # For timing tests
        self.start_launch_stats = {}  # For kernel launch tracking

    @pytest.hookimpl
    def pytest_runtest_setup(self, item):
        """Called before each test setup"""
        test_name = item.nodeid
        if "time" in self.enabled:
            self.start_times[test_name] = time.perf_counter()
        if "launches" in self.enabled:
            self.start_launch_stats[test_name] = dr.detail.launch_stats()
        if "memory" in self.enabled:
            dr.detail.malloc_clear_statistics()

    @pytest.hookimpl
    def pytest_runtest_teardown(self, item):
        """Called after each test teardown"""
        test_name = item.nodeid
        metrics = {}

        # Collect memory usage with breakdown
        if "memory" in self.enabled:
            memory_stats = {}
            total = 0
            for alloc_type in [
                dr.detail.AllocType.Host,
                dr.detail.AllocType.HostAsync,
                dr.detail.AllocType.HostPinned,
                dr.detail.AllocType.Device,
            ]:
                usage = dr.detail.malloc_watermark(alloc_type)
                if usage > 0:
                    memory_stats[alloc_type.name] = usage
                    total += usage

            if total > 0:
                metrics["memory"] = {"total": total, "breakdown": memory_stats}

        # Collect execution time
        if "time" in self.enabled:
            if test_name in self.start_times:
                metrics["time"] = time.perf_counter() - self.start_times.pop(test_name)

        # Collect kernel launches (relative count)
        if "launches" in self.enabled:
            if test_name in self.start_launch_stats:
                launches, soft_misses, hard_misses = dr.detail.launch_stats()
                start_launches, start_soft, start_hard = self.start_launch_stats.pop(
                    test_name
                )
                metrics["launches"] = {
                    "total": launches - start_launches,
                    "soft_misses": soft_misses - start_soft,
                    "hard_misses": hard_misses - start_hard,
                }

        if metrics:
            self.data[test_name] = metrics

    @pytest.hookimpl
    def pytest_sessionfinish(self, session, exitstatus):
        """Called after the entire test session"""
        if not self.data:
            return

        print("\n=== Test Metrics Report ===")

        # Report top 100 for each enabled metric
        for metric in sorted(self.enabled):
            # Get tests that have this metric
            tests_with_metric = []
            for name, data in self.data.items():
                if metric in data:
                    tests_with_metric.append((name, data[metric]))

            if not tests_with_metric:
                continue

            # Sort by metric value (descending) and take top 100
            key_func = (
                lambda x: x[1]["total"] if metric in ("memory", "launches") else x[1]
            )
            tests_with_metric.sort(key=key_func, reverse=True)
            top_tests = tests_with_metric[:100]

            print(f"\nTop tests by {metric}:")
            for test_name, value in top_tests:
                print(f"  {test_name}")
                if metric == "memory":
                    print(f"    Total: {mi.misc.mem_string(value['total'])}")
                    for alloc_type, usage in value["breakdown"].items():
                        if usage > 0:
                            print(
                                f"    - {alloc_type.lower()}: {mi.misc.mem_string(usage)}"
                            )
                elif metric == "time":
                    print(f"    {value:.1f} seconds")
                elif metric == "launches":
                    total = value["total"]
                    soft = value["soft_misses"]
                    hard = value["hard_misses"]
                    print(f"    {total:,} kernel launches")
                    if soft > 0 or hard > 0:
                        print(f"    - cache misses: {soft:,} soft, {hard:,} hard")


def pytest_addoption(parser, pluginmanager):
    parser.addoption(
        "--generate_ref",
        action="store_true",
        help="Generate reference images for regression tests",
    )
    parser.addoption(
        "--collect-metrics",
        help="Comma-separated metrics to collect: memory,time,launches",
    )


def pytest_configure(config):
    import sys
    # Show warning only if pytest was run without any arguments (full test suite)
    # sys.argv will be just the pytest executable when no args provided
    if len(sys.argv) <= 1:
        print("""\033[93mRunning the full test suite. To skip slow tests, please run 'pytest -m "not slow"' \033[0m""")

    config.addinivalue_line(
        "markers", "slow: marks tests as slow (deselect with -m 'not slow')"
    )

    # Conditionally register the test metrics plugin
    metrics_arg = config.getoption("--collect-metrics", "")
    if metrics_arg:
        requested_metrics = set(m.strip() for m in metrics_arg.split(","))
        valid_metrics = {"memory", "time", "launches"}
        invalid_metrics = requested_metrics - valid_metrics

        if invalid_metrics:
            raise pytest.UsageError(
                f"Invalid metrics specified: {', '.join(sorted(invalid_metrics))}. "
                f"Valid metrics are: {', '.join(sorted(valid_metrics))}"
            )

        # Register the plugin
        metrics_plugin = TestMetricsPlugin(requested_metrics)
        config.pluginmanager.register(metrics_plugin, "test_metrics")


@pytest.fixture(scope="session")
def regression_test_options(request):
    return {"generate_ref": request.config.option.generate_ref}


def pytest_collection_modifyitems(config, items):
    """
    Modify test collection to:
    1. Filter out tests for non-existent variants
    2. Sort tests by variant to minimize variant switching
    """
    import re

    pattern = re.compile(r"\[((cuda|llvm|scalar)_[^,-\]]*)")

    variant_items = []
    for item in items:
        # Skip tests with empty variant groups
        if 'NOTSET' in item.name:
            continue

        # Try to extract variant from test name
        match = pattern.search(item.name)
        if match:
            variant = match.group(1)
        else:
            # Try to extract from fixture names
            variant = "z"
            for fixname in item.fixturenames:
                if fixname.startswith("variant_"):
                    variant = fixname[8:]
                    break

        # Skip unavailable variants
        if variant != "z" and variant not in v:
            continue

        variant_items.append((variant, item.location, item))

    variant_items.sort()

    if len(items) != len(variant_items):
        print(
            f"\033[93m-- Filtered tests with unavailable variants ({len(items)} →  {len(variant_items)})\033[0m"
        )

    items[:] = [item[2] for item in variant_items]
