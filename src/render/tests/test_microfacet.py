import pytest
import drjit as dr
import mitsuba as mi
import numpy as np


def test01_construct(variant_scalar_rgb):
    md = mi.MicrofacetDistribution(mi.MicrofacetType.Beckmann, 0.1, True)
    assert md.type() == mi.MicrofacetType.Beckmann
    assert dr.allclose(md.alpha_u(), 0.1)
    assert dr.allclose(md.alpha_v(), 0.1)
    assert md.sample_visible()


def test02_eval_pdf_beckmann(variants_vec_backends_once):
    # Compare against data obtained from previous Mitsuba v0.6 implementation
    mdf   = mi.MicrofacetDistribution(mi.MicrofacetType.Beckmann, 0.1, 0.3, False)
    mdf_i = mi.MicrofacetDistribution(mi.MicrofacetType.Beckmann, 0.1, False)
    assert not mdf.is_isotropic()
    assert mdf.is_anisotropic()
    assert mdf_i.is_isotropic()
    assert not mdf_i.is_anisotropic()

    steps = 20
    theta = dr.linspace(mi.Float, 0, dr.pi, steps)
    phi = dr.full(mi.Float, dr.pi / 2, steps)
    cos_theta, sin_theta = dr.cos(theta), dr.sin(theta)
    cos_phi, sin_phi = dr.cos(phi), dr.sin(phi)
    v = [cos_phi * sin_theta, sin_phi * sin_theta, cos_theta]

    wi = mi.Vector3f(0, 0, 1)

    assert dr.allclose(mdf.eval(v),
              [  1.06103287e+01,   8.22650051e+00,   3.57923722e+00,
                 6.84863329e-01,   3.26460004e-02,   1.01964230e-04,
                 5.87322635e-10,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00])

    assert dr.allclose(mdf.pdf(wi, v),
               [  1.06103287e+01,   8.11430168e+00,   3.38530421e+00,
                 6.02319300e-01,   2.57622823e-02,   6.90584930e-05,
                 3.21235011e-10,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00])

    assert dr.allclose(mdf_i.eval(v),
               [  3.18309879e+01,   2.07673073e+00,   3.02855828e-04,
                 1.01591990e-11,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00])

    assert dr.allclose(mdf_i.pdf(wi, v),
               [  3.18309879e+01,   2.04840684e+00,   2.86446273e-04,
                 8.93474877e-12,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00])

    theta = dr.full(mi.Float, 0.1, steps)
    phi = dr.linspace(mi.Float, 0, 2*dr.pi, steps)
    cos_theta, sin_theta = dr.cos(theta), dr.sin(theta)
    cos_phi, sin_phi = dr.cos(phi), dr.sin(phi)
    v = [cos_phi * sin_theta, sin_phi * sin_theta, cos_theta]

    assert dr.allclose(mdf.eval(v),
               [ 3.95569706,  4.34706259,  5.54415846,  7.4061389 ,  9.17129803,
                 9.62056446,  8.37803268,  6.42071199,  4.84459257,  4.05276537,
                 4.05276537,  4.84459257,  6.42071199,  8.37803268,  9.62056446,
                 9.17129803,  7.4061389 ,  5.54415846,  4.34706259,  3.95569706 ])

    assert dr.allclose(mdf.pdf(wi, v),
               mi.Float([ 3.95569706,  4.34706259,  5.54415846,  7.4061389 ,  9.17129803,
                          9.62056446,  8.37803268,  6.42071199,  4.84459257,  4.05276537,
                          4.05276537,  4.84459257,  6.42071199,  8.37803268,  9.62056446,
                          9.17129803,  7.4061389 ,  5.54415846,  4.34706259,  3.95569706 ]) * dr.cos(0.1))

    assert dr.allclose(mdf_i.eval(v), dr.full(mi.Float, 11.86709118, steps))
    assert dr.allclose(mdf_i.pdf(wi, v), dr.full(mi.Float, 11.86709118 * dr.cos(0.1), steps))


def test03_smith_g1_beckmann(variants_vec_backends_once):
    # Compare against data obtained from previous Mitsuba v0.6 implementation
    mdf   = mi.MicrofacetDistribution(mi.MicrofacetType.Beckmann, 0.1, 0.3, False)
    mdf_i = mi.MicrofacetDistribution(mi.MicrofacetType.Beckmann, 0.1, False)
    steps = 20
    theta = dr.linspace(mi.Float, dr.pi/3, dr.pi/2, steps)

    phi = dr.full(mi.Float, dr.pi/2, steps)
    cos_theta, sin_theta = dr.cos(theta), dr.sin(theta)
    cos_phi, sin_phi = dr.cos(phi), dr.sin(phi)
    v = [cos_phi * sin_theta, sin_phi * sin_theta, cos_theta]
    wi = mi.Vector3f(0, 0, 1)

    assert dr.allclose(mdf.smith_g1(v, wi),
                       [1.0000000e+00, 1.0000000e+00, 1.0000000e+00, 1.0000523e+00, 9.9941480e-01,
                        9.9757767e-01, 9.9420297e-01, 9.8884594e-01, 9.8091525e-01, 9.6961778e-01,
                        9.5387781e-01, 9.3222123e-01, 9.0260512e-01, 8.6216795e-01, 8.0686140e-01,
                        7.3091686e-01, 6.2609726e-01, 4.8074335e-01, 2.7883825e-01, 1.9197471e-06], atol=1e-5)

    assert dr.allclose(mdf_i.smith_g1(v, wi),
               [1.0000000e+00, 1.0000000e+00, 1.0000000e+00, 1.0000000e+00, 1.0000000e+00,
                1.0000000e+00, 1.0000000e+00, 1.0000000e+00, 1.0000000e+00, 1.0000000e+00,
                1.0000000e+00, 1.0000000e+00, 1.0000000e+00, 1.0000000e+00, 9.9828446e-01,
                9.8627287e-01, 9.5088160e-01, 8.5989666e-01, 6.2535185e-01, 5.7592310e-06], atol=1e-5)

    steps = 20
    theta = dr.full(mi.Float, dr.pi / 2 * 0.98, steps)
    phi = dr.linspace(mi.Float, 0, 2 * dr.pi, steps)
    cos_theta, sin_theta = dr.cos(theta), dr.sin(theta)
    cos_phi, sin_phi = dr.cos(phi), dr.sin(phi)
    v = [cos_phi * sin_theta, sin_phi * sin_theta, cos_theta]

    assert dr.allclose(mdf.smith_g1(v, wi),
               [ 0.67333597,  0.56164336,  0.42798978,  0.35298213,  0.31838724,
                 0.31201753,  0.33166203,  0.38421196,  0.48717275,  0.63746351,
                 0.63746351,  0.48717275,  0.38421196,  0.33166203,  0.31201753,
                 0.31838724,  0.35298213,  0.42798978,  0.56164336,  0.67333597])

    assert dr.allclose(mdf_i.smith_g1(v, wi), dr.full(mi.Float, 0.67333597, steps))


def test04_sample_beckmann(variants_vec_backends_once):
    mdf = mi.MicrofacetDistribution(mi.MicrofacetType.Beckmann, 0.1, 0.3, False)

    # Compare against data obtained from previous Mitsuba v0.6 implementation
    steps = 6
    u = dr.linspace(mi.Float, 0, 1, steps, endpoint=False)
    u1, u2 = dr.meshgrid(u, u)
    u = [u1, u2]
    wi = mi.Vector3f(0, 0, 1)

    result = mdf.sample(wi, u)

    ref = (np.array([[ 0.00000000e+00,  0.00000000e+00,  1.00000000e+00],
                     [ 4.26597558e-02,  0.00000000e+00,  9.99089658e-01],
                     [ 6.35476336e-02,  0.00000000e+00,  9.97978806e-01],
                     [ 8.29685107e-02,  0.00000000e+00,  9.96552169e-01],
                     [ 1.04243755e-01,  0.00000000e+00,  9.94551778e-01],
                     [ 1.32673502e-01,  0.00000000e+00,  9.91159797e-01],
                     [ 0.00000000e+00,  0.00000000e+00,  1.00000000e+00],
                     [ 2.12147459e-02,  1.10235065e-01,  9.93679106e-01],
                     [ 3.13956439e-02,  1.63136587e-01,  9.86103833e-01],
                     [ 4.06531654e-02,  2.11240083e-01,  9.76588428e-01],
                     [ 5.05014807e-02,  2.62413442e-01,  9.63633120e-01],
                     [ 6.30887374e-02,  3.27818751e-01,  9.42631781e-01],
                     [-0.00000000e+00,  0.00000000e+00,  1.00000000e+00],
                     [-2.12147497e-02,  1.10235058e-01,  9.93679106e-01],
                     [-3.13956514e-02,  1.63136572e-01,  9.86103833e-01],
                     [-4.06531766e-02,  2.11240068e-01,  9.76588428e-01],
                     [-5.05014956e-02,  2.62413412e-01,  9.63633120e-01],
                     [-6.30887523e-02,  3.27818722e-01,  9.42631781e-01],
                     [-0.00000000e+00, -0.00000000e+00,  1.00000000e+00],
                     [-4.26597558e-02, -1.11883027e-08,  9.99089658e-01],
                     [-6.35476336e-02, -1.66665313e-08,  9.97978806e-01],
                     [-8.29685107e-02, -2.17600107e-08,  9.96552169e-01],
                     [-1.04243755e-01, -2.73398335e-08,  9.94551778e-01],
                     [-1.32673502e-01, -3.47960558e-08,  9.91159797e-01],
                     [-0.00000000e+00, -0.00000000e+00,  1.00000000e+00],
                     [-2.12147422e-02, -1.10235058e-01,  9.93679106e-01],
                     [-3.13956402e-02, -1.63136572e-01,  9.86103833e-01],
                     [-4.06531580e-02, -2.11240068e-01,  9.76588428e-01],
                     [-5.05014732e-02, -2.62413412e-01,  9.63633120e-01],
                     [-6.30887300e-02, -3.27818722e-01,  9.42631781e-01],
                     [ 0.00000000e+00, -0.00000000e+00,  1.00000000e+00],
                     [ 2.12147627e-02, -1.10235050e-01,  9.93679106e-01],
                     [ 3.13956700e-02, -1.63136557e-01,  9.86103833e-01],
                     [ 4.06531990e-02, -2.11240053e-01,  9.76588428e-01],
                     [ 5.05015217e-02, -2.62413412e-01,  9.63633120e-01],
                     [ 6.30887896e-02, -3.27818722e-01,  9.42631781e-01]]),
    np.array([10.610329,   8.866132,  7.1166167,  5.360419,   3.5952191,  1.816128,
              10.610329,   9.01175,   7.3768272,  5.695923,   3.9525049,  2.1113062,
              10.610329,   9.01175,   7.3768272,  5.695923,   3.9525049,  2.1113062,
              10.610329,   8.866132,  7.1166167,  5.360419,   3.5952191,  1.816128,
              10.610329,   9.01175,   7.3768272,  5.695923,   3.9525049,  2.1113062,
              10.610329,   9.01175,   7.3768272,  5.695923,   3.9525049,  2.1113062]))

    assert dr.allclose(ref[0].transpose(), result[0], atol=5e-4)
    assert dr.allclose(ref[1], result[1], atol=1e-4)


def test03_smith_g1_ggx(variants_vec_backends_once):
    # Compare against data obtained from previous Mitsuba v0.6 implementation
    mdf   = mi.MicrofacetDistribution(mi.MicrofacetType.GGX, 0.1, 0.3, False)
    mdf_i = mi.MicrofacetDistribution(mi.MicrofacetType.GGX, 0.1, False)
    steps = 20
    theta = dr.linspace(mi.Float, dr.pi/3, dr.pi/2, steps)
    phi = dr.full(mi.Float, dr.pi/2, steps)
    cos_theta, sin_theta = dr.cos(theta), dr.sin(theta)
    cos_phi, sin_phi = dr.cos(phi), dr.sin(phi)
    v = [cos_phi * sin_theta, sin_phi * sin_theta, cos_theta]
    wi = np.tile([0, 0, 1], (steps, 1)).transpose()

    assert dr.allclose(mdf.smith_g1(v, wi),
                       [9.4031686e-01, 9.3310797e-01, 9.2485082e-01, 9.1534841e-01, 9.0435863e-01,
                        8.9158219e-01, 8.7664890e-01, 8.5909742e-01, 8.3835226e-01, 8.1369340e-01,
                        7.8421932e-01, 7.4880326e-01, 7.0604056e-01, 6.5419233e-01, 5.9112519e-01,
                        5.1425743e-01, 4.2051861e-01, 3.0633566e-01, 1.6765384e-01, 1.0861372e-06], atol=1e-5)

    assert dr.allclose(mdf_i.smith_g1(v, wi),
                       [9.9261039e-01, 9.9160647e-01, 9.9042398e-01, 9.8901933e-01, 9.8733366e-01,
                        9.8528832e-01, 9.8277503e-01, 9.7964239e-01, 9.7567332e-01, 9.7054905e-01,
                        9.6378750e-01, 9.5463598e-01, 9.4187391e-01, 9.2344058e-01, 8.9569420e-01,
                        8.5189372e-01, 7.7902949e-01, 6.5144652e-01, 4.1989169e-01, 3.2584082e-06], atol=1e-5)

    theta = dr.full(mi.Float, dr.pi / 2 * 0.98, steps)
    phi = dr.linspace(mi.Float, 0, 2 * dr.pi, steps)
    cos_theta, sin_theta = dr.cos(theta), dr.sin(theta)
    cos_phi, sin_phi = dr.cos(phi), dr.sin(phi)
    v = [cos_phi * sin_theta, sin_phi * sin_theta, cos_theta]

    assert dr.allclose(mdf.smith_g1(v, wi),
      [ 0.46130955,  0.36801264,  0.26822716,  0.21645154,  0.19341162,
                 0.18922243,  0.20219423,  0.23769052,  0.31108665,  0.43013984,
                 0.43013984,  0.31108665,  0.23769052,  0.20219423,  0.18922243,
                 0.19341162,  0.21645154,  0.26822716,  0.36801264,  0.46130955])

    assert dr.allclose(mdf_i.smith_g1(v, wi), dr.full(mi.Float, 0.46130955, steps))


def test05_sample_ggx(variants_vec_backends_once):
    mdf = mi.MicrofacetDistribution(mi.MicrofacetType.GGX, 0.1, 0.3, False)

    # Compare against data obtained from previous Mitsuba v0.6 implementation
    steps = 6
    u = dr.linspace(mi.Float, 0, 1, steps, endpoint=False)
    u1, u2 = dr.meshgrid(u, u)
    u = [u1, u2]
    wi = np.tile([0, 0, 1], (steps * steps, 1))
    result = mdf.sample(wi.transpose(), u)

    ref = ((np.array([[ 0.0000000e+00,  0.0000000e+00,  1.0000000e+00],
                      [ 4.4676583e-02,  0.0000000e+00,  9.9900150e-01],
                      [ 7.0534222e-02,  0.0000000e+00,  9.9750936e-01],
                      [ 9.9504232e-02,  0.0000000e+00,  9.9503714e-01],
                      [ 1.4002767e-01,  0.0000000e+00,  9.9014759e-01],
                      [ 2.1821797e-01,  0.0000000e+00,  9.7590005e-01],
                      [ 0.0000000e+00,  0.0000000e+00,  1.0000000e+00],
                      [ 2.2205815e-02,  1.1538482e-01,  9.9307263e-01],
                      [ 3.4752376e-02,  1.8057868e-01,  9.8294640e-01],
                      [ 4.8336815e-02,  2.5116551e-01,  9.6673650e-01],
                      [ 6.6226624e-02,  3.4412369e-01,  9.3658578e-01],
                      [ 9.6225053e-02,  5.0000012e-01,  8.6066288e-01],
                      [-0.0000000e+00,  0.0000000e+00,  1.0000000e+00],
                      [-2.2205820e-02,  1.1538481e-01,  9.9307263e-01],
                      [-3.4752384e-02,  1.8057866e-01,  9.8294640e-01],
                      [-4.8336826e-02,  2.5116548e-01,  9.6673650e-01],
                      [-6.6226639e-02,  3.4412366e-01,  9.3658578e-01],
                      [-9.6225075e-02,  5.0000012e-01,  8.6066288e-01],
                      [-0.0000000e+00, -0.0000000e+00,  1.0000000e+00],
                      [-4.4676583e-02, -1.1717252e-08,  9.9900150e-01],
                      [-7.0534222e-02, -1.8498891e-08,  9.9750936e-01],
                      [-9.9504232e-02, -2.6096808e-08,  9.9503714e-01],
                      [-1.4002767e-01, -3.6724821e-08,  9.9014759e-01],
                      [-2.1821797e-01, -5.7231659e-08,  9.7590005e-01],
                      [-0.0000000e+00, -0.0000000e+00,  1.0000000e+00],
                      [-2.2205811e-02, -1.1538481e-01,  9.9307263e-01],
                      [-3.4752373e-02, -1.8057866e-01,  9.8294640e-01],
                      [-4.8336808e-02, -2.5116548e-01,  9.6673650e-01],
                      [-6.6226617e-02, -3.4412366e-01,  9.3658578e-01],
                      [-9.6225038e-02, -5.0000012e-01,  8.6066288e-01],
                      [ 0.0000000e+00, -0.0000000e+00,  1.0000000e+00],
                      [ 2.2205831e-02, -1.1538480e-01,  9.9307263e-01],
                      [ 3.4752402e-02, -1.8057865e-01,  9.8294640e-01],
                      [ 4.8336852e-02, -2.5116548e-01,  9.6673650e-01],
                      [ 6.6226676e-02, -3.4412363e-01,  9.3658578e-01],
                      [ 9.6225120e-02, -5.0000000e-01,  8.6066294e-01]]),
     np.array([10.610329,  7.390399,  4.751113,  2.6924708, 1.214469,  0.3171101,
               10.610329,  7.5235577, 4.965429,  2.9359221, 1.4349725, 0.46230313,
               10.610329,  7.5235577, 4.96543,   2.9359221, 1.4349725, 0.4623032,
               10.610329,  7.390399,  4.751113,  2.6924708, 1.214469,  0.3171101,
               10.610329,  7.5235577, 4.96543,   2.9359221, 1.4349728, 0.4623032,
               10.610329,  7.5235577, 4.965429,  2.9359221, 1.4349725, 0.4623031])))

    assert dr.allclose(ref[0].transpose(), result[0], atol=5e-4)
    assert dr.allclose(ref[1], result[1], atol=1e-4)


@pytest.mark.parametrize("sample_visible", [True, False])
@pytest.mark.parametrize("alpha", [0.125, 0.5])
@pytest.mark.parametrize("md_type_name", ['GGX', 'Beckmann'])
@pytest.mark.parametrize("angle", [15, 80, 30])
def test06_chi2(variants_vec_backends_once, md_type_name, alpha, sample_visible, angle):
    if md_type_name == "GGX":
        md_type = mi.MicrofacetType.GGX
    else:
        md_type = mi.MicrofacetType.Beckmann

    sample_func, pdf_func = mi.chi2.MicrofacetAdapter(md_type, alpha, sample_visible)

    chi2 = mi.chi2.ChiSquareTest(
        domain=mi.chi2.SphericalDomain(),
        sample_func=lambda *args: sample_func(*(list(args) + [angle])),
        pdf_func=lambda *args: pdf_func(*(list(args) + [angle])),
        sample_dim=2,
        res=128,
        ires=32
    )

    assert chi2.run()
