from importlib import import_module as _import

def test01_import_mitsuba_variants():
    import mitsuba

    # Initially the mitsuba variant should be None
    assert mitsuba.variant() is None

    # However mitsuba.current should fall back to scalar_rgb
    assert mitsuba.current.variant() == 'scalar_rgb'

    variant = mitsuba.variants()[-1]

    # Importing specific variant for the first time should change the current variant
    m = _import(f'mitsuba.{variant}')
    assert m.variant() == variant
    assert mitsuba.variant() == variant
    assert mitsuba.current.variant() == variant

    # Change the current variant
    mitsuba.set_variant('scalar_rgb')
    assert mitsuba.variant() == 'scalar_rgb'
    assert mitsuba.current.variant() == 'scalar_rgb'
    assert m.variant() == variant


def test02_import_submodules():
    import mitsuba
    variant = mitsuba.variants()[-1]
    m = _import(f'mitsuba.{variant}')

    # Check C++ submodules
    assert m.math.ShadowEpsilon is not None
    assert m.warp.square_to_uniform_disk is not None

    # Check Python submodules
    assert m.chi2.ChiSquareTest is not None

    # Import nested submodules
    _import(f'mitsuba.{variant}.test.util')
    assert m.test.util.fresolver_append_path is not None


def test03_import_from_submodules():
    from mitsuba.current import Float
    assert Float == float

    from mitsuba.current.warp import square_to_uniform_disk
    assert square_to_uniform_disk is not None

    from mitsuba.current.chi2 import ChiSquareTest
    assert ChiSquareTest is not None

    from mitsuba.current.test.util import fresolver_append_path
    assert fresolver_append_path is not None


def test04_check_all_variants():
    import mitsuba
    for v in mitsuba.variants():
        mitsuba.set_variant(v)