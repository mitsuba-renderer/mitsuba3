# This file rewrites exceptions caught by PyTest and makes traces involving
# pybind11 classes more readable. It e.g. replaces "<built-in method allclose
# of PyCapsule object at 0x7ffa78041090>" by "allclose" which is arguably just
# as informative and much more compact.

import pytest
import re
import mitsuba
import gc
import enoki as ek

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
def gc_collect():
    gc.collect() # Ensure no leftover instances from other tests in registry
    gc.collect()


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


def generate_fixture(variant):
    @pytest.fixture()
    def fixture():
        try:
            import mitsuba
            mitsuba.set_variant(variant)
            if variant.startswith('llvm') or variant.startswith('cuda'):
                ek.registry_trim()
                ek.set_flags(ek.JitFlag.Default)
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
            import mitsuba
            mitsuba.set_variant(variant)
            if variant.startswith('llvm') or variant.startswith('cuda'):
                ek.registry_trim()
                ek.set_flags(ek.JitFlag.Default)
        except Exception:
            pytest.skip('Mitsuba variant "%s" is not enabled!' % variant)
        return variant
    globals()['variants_' + name] = fixture

any_scalar = next((x for x in mitsuba.variants() if x.startswith('scalar')), 'scalar_rgb')
any_llvm   = next((x for x in mitsuba.variants() if x.startswith('llvm')),   'llvm_rgb')
any_cuda   = next((x for x in mitsuba.variants() if x.startswith('cuda')),   'cuda_rgb')
any_llvm_rgb = 'llvm_rgb' if 'llvm_rgb' in mitsuba.variants() else 'llvm_ad_rgb'
any_cuda_rgb = 'cuda_rgb' if 'cuda_rgb' in mitsuba.variants() else 'cuda_ad_rgb'

variant_groups = {
    'any_scalar' : [any_scalar],
    'any_llvm' : [any_llvm],
    'any_cuda' : [any_cuda],
    'all' : mitsuba.variants(),
    'all_scalar' : [x for x in mitsuba.variants() if x.startswith('scalar')],
    'all_rgb' : [x for x in mitsuba.variants() if x.endswith('rgb')],
    'all_backends_once' : [any_scalar, any_llvm, any_cuda],
    'vec_backends_once' : [any_llvm, any_cuda],
    'vec_backends_once_rgb' : [any_llvm_rgb, any_cuda_rgb],
    'vec_rgb' : [x for x in mitsuba.variants() if x.endswith('rgb') and not x.startswith('scalar')],
    'vec_spectral' : [x for x in mitsuba.variants() if x.endswith('spectral') and not x.startswith('scalar')],
    'all_ad_rgb' : [x for x in mitsuba.variants() if x.endswith('ad_rgb')],
}

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
