# This file rewrites exceptions caught by PyTest and makes traces involving
# pybind11 classes more readable. It e.g. replaces "<built-in method allclose
# of PyCapsule object at 0x7ffa78041090>" by "allclose" which is arguably just
# as informative and much more compact.

import pytest
import re
import gc
import drjit as dr
import mitsuba as mi

re1 = re.compile(r'<built-in method (\w*) of PyCapsule object at 0x[0-9a-f]*>')
re2 = re.compile(r'<bound method PyCapsule.(\w*)[^>]*>')


def patch_line(s):
    s = re1.sub(r'\1', s)
    s = re2.sub(r'\1', s)
    return s


def patch_tb(tb):  # tb: ReprTraceback
    for entry in tb.reprentries:
        entry.lines = [patch_line(l) for l in entry.lines]



@pytest.fixture
def np_rng():
    import numpy as np
    return np.random.default_rng(seed=12345)


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    rep = outcome.get_result()
    if rep.outcome == 'failed':
        if hasattr(rep.longrepr, 'chain'):
            for entry in rep.longrepr.chain:
                patch_tb(entry[0])
    return rep


def clean_up():
    '''
    Ensure no leftover instances from other tests are still in registry, wait
    for all running kernels to finish and reset the JitFlag to default. Also
    periodically frees the malloc cache to prevent the testcases from hogging
    all system memory.
    '''
    gc.collect()
    gc.collect()

    dr.kernel_history_clear()
    dr.flush_malloc_cache()
    dr.flush_kernel_cache()

    if hasattr(dr, 'sync_thread'):
        dr.sync_thread()
        dr.detail.clear_registry()
        dr.set_flag(dr.JitFlag.Default, True)


def generate_fixture(variant):
    @pytest.fixture()
    def fixture():
        try:
            clean_up()
            mi.set_variant(variant)
        except Exception:
            pytest.skip('Mitsuba variant "%s" is not enabled!' % variant)
    globals()['variant_' + variant] = fixture


# Compute set of valid variants
suffix_variants = [a + b for a in ["_mono", "_mono_polarized", "_rgb", "_spectral", "_spectral_polarized"] for b in ["", "_double"]]
scalar_variants = ["scalar" + s for s in suffix_variants]
other_variants  = [a + b + c for a in ["llvm", "cuda"] for b in ["", "_ad"] for c in suffix_variants]

for variant in scalar_variants + other_variants:
    generate_fixture(variant)
del generate_fixture


def generate_fixture_group(name, variants):
    @pytest.fixture(params=variants)
    def fixture(request):
        variant = request.param
        try:
            clean_up()
            mi.set_variant(variant)
        except Exception:
            pytest.skip('Mitsuba variant "%s" is not enabled!' % variant)
        return variant
    globals()['variants_' + name] = fixture

variants = mi.variants()

any_scalar = next((x for x in variants if x.startswith('scalar')), 'scalar_rgb')
any_llvm   = next((x for x in variants if x.startswith('llvm')),   'llvm_rgb')
any_cuda   = next((x for x in variants if x.startswith('cuda')),   'cuda_rgb')
any_llvm_rgb = 'llvm_rgb' if 'llvm_rgb' in variants else 'llvm_ad_rgb'
any_cuda_rgb = 'cuda_rgb' if 'cuda_rgb' in variants else 'cuda_ad_rgb'
any_llvm_spectral = 'llvm_spectral' if 'llvm_spectral' in variants else 'llvm_ad_spectral'
any_cuda_spectral = 'cuda_spectral' if 'cuda_spectral' in variants else 'cuda_ad_spectral'

variant_groups = {
    'any_scalar' : [any_scalar],
    'any_llvm' : [any_llvm],
    'any_cuda' : [any_cuda],
    'all' : variants,
    'all_scalar' : [x for x in variants if x.startswith('scalar')],
    'all_rgb' : [x for x in variants if x.endswith('rgb')],
    'all_spectral' : [x for x in variants if x.endswith('spectral')],
    'all_backends_once' : [any_scalar, any_llvm, any_cuda],
    'vec_backends_once' : [any_llvm, any_cuda],
    'vec_backends_once_rgb' : [any_llvm_rgb, any_cuda_rgb],
    'vec_backends_once_spectral' : [any_llvm_spectral, any_cuda_spectral],
    'vec_rgb' : [x for x in variants if x.endswith('rgb') and not x.startswith('scalar')],
    'vec_spectral' : [x for x in variants if x.endswith('spectral') and not x.startswith('scalar')],
    'all_ad_rgb' : [x for x in variants if x.endswith('ad_rgb')],
    'all_ad_spectral' : [x for x in variants if x.endswith('ad_spectral')],
}

del variants

for name, variants in variant_groups.items():
    generate_fixture_group(name, variants)
del generate_fixture_group


def pytest_configure(config):
    markexpr = config.getoption("markexpr", 'False')
    if not 'not slow' in markexpr:
        print("""\033[93mRunning the full test suite. To skip slow tests, please run 'pytest -m "not slow"' \033[0m""")

    config.addinivalue_line(
        "markers", "slow: marks tests as slow (deselect with -m 'not slow')"
    )
