import pytest
import drjit as dr
import mitsuba as mi
import os


def test01_create(variant_scalar_rgb, tmpdir):
    tmp_file = os.path.join(str(tmpdir), "sggx.vol")
    grid = mi.TensorXf([1.0, 1.0, 1.0, 0.0, 0.0, 0.0], (1, 1, 1, 6))
    mi.VolumeGrid(grid).write(tmp_file)

    p = mi.load_string(f"""<phase version='2.0.0' type='sggx'>
                            <volume type="gridvolume" name="S">
                                <string name="filename" value="{tmp_file}"/>
                            </volume>
                        </phase>""")

    assert p is not None
    assert p.flags() == mi.PhaseFunctionFlags.Anisotropic | mi.PhaseFunctionFlags.Microflake


@pytest.mark.slow
def test02_chi2_simple(variants_vec_backends_once_rgb, tmpdir):
    tmp_file = os.path.join(str(tmpdir), "sggx.vol")
    grid = mi.TensorXf([0.15, 1.0, 0.8, 0.0, 0.0, 0.0], (1, 1, 1, 6))
    mi.VolumeGrid(grid).write(tmp_file)

    sample_func, pdf_func = mi.chi2.PhaseFunctionAdapter("sggx",
         f"""<volume type="gridvolume" name="S">
                <string name="filename" value="{tmp_file}"/>
            </volume>
         """)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()


@pytest.mark.slow
def test03_chi2_skewed(variants_vec_backends_once_rgb, tmpdir):
    tmp_file = os.path.join(str(tmpdir), "sggx.vol")
    grid = mi.TensorXf([1.0, 0.35, 0.32, 0.52, 0.44, 0.2], (1, 1, 1, 6))
    mi.VolumeGrid(grid).write(tmp_file)

    sample_func, pdf_func = mi.chi2.PhaseFunctionAdapter("sggx",
         f"""<volume type="gridvolume" name="S">
                 <string name="filename" value="{tmp_file}"/>
             </volume>
         """)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=sample_func,
        pdf_func=pdf_func,
        sample_dim=3
    )

    assert chi2.run()
