import os
import numpy as np

import pytest

def test01_create(variant_scalar_rgb, tmpdir):
    from mitsuba.core.xml import load_string
    from mitsuba.render import VolumeGrid, PhaseFunctionFlags

    tmp_file = os.path.join(str(tmpdir), "sggx.vol")
    grid = np.array([1.0, 1.0, 1.0, 0.0, 0.0, 0.0], dtype=np.float32).reshape(1, 1, 1, 6)
    VolumeGrid(grid).write(tmp_file)

    p = load_string(f"""<phase version='2.0.0' type='sggx'>
                            <volume type="gridvolume" name="S">
                                <string name="filename" value="{tmp_file}"/>
                            </volume>
                        </phase>""")

    assert p is not None
    assert p.flags() == PhaseFunctionFlags.Anisotropic | PhaseFunctionFlags.Microflake

@pytest.mark.slow
def test02_chi2_simple(variants_vec_backends_once_rgb, tmpdir):
    from mitsuba.python.chi2 import PhaseFunctionAdapter, ChiSquareTest, SphericalDomain
    from mitsuba.render import VolumeGrid

    tmp_file = os.path.join(str(tmpdir), "sggx.vol")
    grid = np.array([0.15, 1.0, 0.8, 0.0, 0.0, 0.0], dtype=np.float32).reshape(1, 1, 1, 6)
    VolumeGrid(grid).write(tmp_file)

    sample_func, pdf_func = PhaseFunctionAdapter("sggx",
         f"""<volume type="gridvolume" name="S">
                <string name="filename" value="{tmp_file}"/>
            </volume>
         """)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()

@pytest.mark.slow
def test03_chi2_skewed(variants_vec_backends_once_rgb, tmpdir):
    from mitsuba.python.chi2 import PhaseFunctionAdapter, ChiSquareTest, SphericalDomain
    from mitsuba.render import VolumeGrid

    tmp_file = os.path.join(str(tmpdir), "sggx.vol")
    grid = np.array([1.0, 0.35, 0.32, 0.52, 0.44, 0.2], dtype=np.float32).reshape(1, 1, 1, 6)
    VolumeGrid(grid).write(tmp_file)

    sample_func, pdf_func = PhaseFunctionAdapter("sggx",
         f"""<volume type="gridvolume" name="S">
                 <string name="filename" value="{tmp_file}"/>
             </volume>
         """)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()
