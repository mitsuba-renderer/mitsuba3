# This file rewrites exceptions caught by PyTest and makes traces involving
# pybind11 classes more readable. It e.g. replaces "<built-in method allclose
# of PyCapsule object at 0x7ffa78041090>" by "allclose" which is arguably just
# as informative and much more compact.

import pytest
import re

re1 = re.compile(r'<built-in method (\w*) of PyCapsule object at 0x[0-9a-f]*>')
re2 = re.compile(r'<bound method PyCapsule.(\w*)[^>]*>')


def patch_line(s):
    s = re1.sub(r'\1', s)
    s = re2.sub(r'\1', s)
    return s


def patch_tb(tb):  # tb: ReprTraceback
    for entry in tb.reprentries:
        entry.lines = [patch_line(l) for l in entry.lines]


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
        except Exception:
            pytest.skip('Mitsuba variant "%s" is not enabled!' % variant)
    globals()['variant_' + variant] = fixture


for variant in ['scalar_rgb', 'scalar_spectral',
                'scalar_mono_polarized', 'packet_rgb',
                'packet_spectral', 'gpu_rgb', 'gpu_autodiff_rgb']:
    generate_fixture(variant)
del generate_fixture


@pytest.fixture(params=['packet_rgb', 'gpu_rgb'])
def variants_vec_rgb(request):
    try:
        import mitsuba
        mitsuba.set_variant(request.param)
    except Exception:
        pytest.skip('Mitsuba variant "%s" is not enabled!' % request.param)
    return request.param


@pytest.fixture(params=['scalar_rgb', 'packet_rgb'])
def variants_cpu_rgb(request):
    try:
        import mitsuba
        mitsuba.set_variant(request.param)
    except Exception:
        pytest.skip('Mitsuba variant "%s" is not enabled!' % request.param)
    return request.param


@pytest.fixture(params=['scalar_rgb', 'scalar_spectral', 'scalar_mono', 'scalar_spectral_polarized'])
def variants_scalar_all(request):
    try:
        import mitsuba
        mitsuba.set_variant(request.param)
    except Exception:
        pytest.skip('Mitsuba variant "%s" is not enabled!' % request.param)
    return request.param


@pytest.fixture(params=['scalar_rgb', 'packet_rgb', 'gpu_rgb'])
def variants_all_rgb(request):
    try:
        import mitsuba
        mitsuba.set_variant(request.param)
    except Exception:
        pytest.skip('Mitsuba variant "%s" is not enabled!' % request.param)
    return request.param


def pytest_configure(config):
    markexpr = config.getoption("markexpr", 'False')
    if not 'not slow' in markexpr:
        print("""\033[93mRunning the full test suite. To skip slow tests, please run 'pytest -m "not slow"' \033[0m""")

    config.addinivalue_line(
        "markers", "slow: marks tests as slow (deselect with -m 'not slow')"
    )
