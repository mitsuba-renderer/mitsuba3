import mitsuba as mi
import drjit as dr

import pytest


@pytest.mark.parametrize("alpha", [[1, 1, 1], [1, 0.1, 0.5]])
@pytest.mark.parametrize("angle", [dr.pi / 4, dr.pi / 2])
def test01_sampling(variants_vec_backends_once, alpha, angle):
    s = alpha + [0, 0, 0]
    w_i = dr.normalize(mi.Vector3f(0, dr.sin(angle), dr.cos(angle)))

    def sample_fun(sample):
        return mi.sggx_sample(w_i, sample, s)

    def pdf_fun(w_n):
        n_dot_wi = dr.maximum(0, dr.dot(w_n, w_i))
        return n_dot_wi * mi.sggx_pdf(w_n, s) / mi.sggx_projected_area(w_i, s)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=lambda *args: sample_fun(*args),
        pdf_func=lambda *args: pdf_fun(*args),
        sample_dim=2,
        res=128,
        ires=32
    )

    assert chi2.run()
