import pytest
import mitsuba

@pytest.fixture()
def variant_scalar():
    mitsuba.set_variant('scalar_rgb')


@pytest.fixture()
def variant_packet():
    try:
        mitsuba.set_variant('packet_rgb')
    except:
        pytest.skip("packet_rgb mode not enabled")


@pytest.fixture()
def variant_spectral():
    try:
        mitsuba.set_variant('scalar_spectral')
    except:
        pytest.skip("scalar_spectral mode not enabled")


@pytest.fixture()
def variant_polarized():
    try:
        mitsuba.set_variant('scalar_spectral_polarized')
    except:
        pytest.skip("scalar_spectral_polarized mode not enabled")


@pytest.fixture()
def variant_mono_polarized():
    try:
        mitsuba.set_variant('scalar_mono_polarized')
    except:
        pytest.skip("scalar_mono_polarized mode not enabled")


@pytest.fixture()
def variant_packet_spectral():
    try:
        mitsuba.set_variant('packet_spectral')
    except:
        pytest.skip("packet_spectral mode not enabled")


@pytest.fixture(params = ['packet_rgb', 'gpu_rgb'])
def variants_vec(request):
    try:
        mitsuba.set_variant(request.param)
    except:
        pytest.skip("%s mode not enabled" % request.param)
    return request.param


@pytest.fixture(params = ['scalar_rgb', 'packet_rgb'])
def variants_cpu_rgb(request):
    try:
        mitsuba.set_variant(request.param)
    except:
        pytest.skip("%s mode not enabled" % request.param)
    return request.param


@pytest.fixture(params = ['scalar_rgb', 'packet_rgb', 'gpu_rgb'])
def variants_all_rgb(request):
    try:
        mitsuba.set_variant(request.param)
    except:
        pytest.skip("%s mode not enabled" % request.param)
    return request.param


@pytest.fixture(params = mitsuba.variants())
def variants_all(request):
    try:
        mitsuba.set_variant(request.param)
    except:
        pytest.skip("%s mode not enabled" % request.param)
    return request.param