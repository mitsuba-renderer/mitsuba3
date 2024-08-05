from importlib import import_module as _import
import pytest

def test01_import_mitsuba_variants():
    import mitsuba as mi

    # Set an arbitrary variant
    variant = mi.variants()[-1]
    mi.set_variant(variant)
    assert mi.variant() == variant

    # Importing specific variant
    mi_var = _import(f'mitsuba.{variant}')
    assert mi_var.__name__ == f"mitsuba"

    # Should be able to access the variants from mitsuba itself
    mi_var2 = getattr(mi, variant)
    assert mi_var2.__name__ == f"mitsuba"

    # Change the current variant
    mi.set_variant('scalar_rgb')
    assert mi.variant() == 'scalar_rgb'
    assert mi_var.__name__ == f"mitsuba"


def test02_import_submodules():
    import mitsuba as mi
    mi.set_variant(mi.variants()[-1])

    # Check C++ submodules
    assert mi.math.ShadowEpsilon is not None
    assert mi.warp.square_to_uniform_disk is not None

    # Check Python submodules
    assert mi.math_py.rlgamma is not None
    assert mi.chi2.ChiSquareTest is not None

    # Import nested submodules
    import mitsuba.test.util
    assert mi.test.util.fresolver_append_path is not None


def test03_import_from_submodules():
    import mitsuba as mi
    mi.set_variant('scalar_rgb')

    Float = mi.Float
    assert Float == float

    from mitsuba.chi2 import ChiSquareTest
    assert ChiSquareTest is not None

    from mitsuba.test.util import fresolver_append_path
    assert fresolver_append_path is not None


def test04_python_extensions():
    import mitsuba as mi
    mi.set_variant('scalar_rgb')

    # Check that python/python/__init__.py is executed properly
    assert mi.traverse is not None
    assert mi.SceneParameters is not None


def test05_check_all_variants():
    import mitsuba as mi
    for v in mi.variants():
        mi.set_variant(v)

    # Test set_variant when passing multiple variant names
    mi.set_variant('foo', 'scalar_rgb')
    assert mi.variant() == 'scalar_rgb'


def test06_register_ad_integrators():
    import mitsuba as mi

    for variant in mi.variants():
        if not '_ad_' in variant:
            continue
        mi.set_variant(variant)
        integrator = mi.load_dict({'type': 'prb'})
        assert integrator.class_().variant() == variant

def test07_reload():
    from importlib import reload

    import mitsuba
    mitsuba.set_variant('scalar_rgb')
    mitsuba.Float()

    reload(mitsuba)

    mitsuba.set_variant('scalar_rgb')
    mitsuba.Float()
    assert mitsuba.variant() == 'scalar_rgb'


def test08_sys_module_size():
    # Make sure the size of sys.modules doesn't change when importing from mitsuba
    import sys

    import mitsuba as mi
    mi.set_variant("scalar_rgb")

    for k, v in sys.modules.items():
        getattr(v, "foo", False)

    assert True


@pytest.mark.parametrize('order', [0, 1, 2])
def test09_import_torch_order(order):
    if order == 0:
        pytest.importorskip("torch")
        import mitsuba as mi
        mi.set_variant(mi.variants()[0])
    if order == 1:
        import mitsuba as mi
        pytest.importorskip("torch")
        mi.set_variant(mi.variants()[0])
    if order == 2:
        import mitsuba as mi
        mi.set_variant(mi.variants()[0])
        pytest.importorskip("torch")

    bmp = mi.Bitmap(mi.TensorXf([0.0, 0.0, 0.0, 0.0], (2, 2, 1)))
    bsdf = mi.load_dict({
        'type': 'diffuse',
        'reflectance': {
            'type': 'bitmap',
            'bitmap': bmp,
        },
    })

    print(bsdf)
