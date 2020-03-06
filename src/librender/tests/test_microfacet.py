import mitsuba
import pytest
import enoki as ek
from enoki.dynamic import Float32 as Float
import numpy as np


def test01_construct(variant_scalar_rgb):
    from mitsuba.render import MicrofacetDistribution, MicrofacetType

    md = MicrofacetDistribution(MicrofacetType.Beckmann, 0.1, True)
    assert md.type() == MicrofacetType.Beckmann
    assert ek.allclose(md.alpha_u(), 0.1)
    assert ek.allclose(md.alpha_v(), 0.1)
    assert md.sample_visible()


def test02_eval_pdf_beckmann(variant_packet_rgb):
    from mitsuba.core import Vector3f
    from mitsuba.render import MicrofacetDistribution, MicrofacetType

    # Compare against data obtained from previous Mitsuba v0.6 implementation
    mdf   = MicrofacetDistribution(MicrofacetType.Beckmann, 0.1, 0.3, False)
    mdf_i = MicrofacetDistribution(MicrofacetType.Beckmann, 0.1, False)
    assert not mdf.is_isotropic()
    assert mdf.is_anisotropic()
    assert mdf_i.is_isotropic()
    assert not mdf_i.is_anisotropic()

    steps = 20
    theta = ek.linspace(Float, 0, ek.pi, steps)
    phi = Float.full(ek.pi / 2, steps)
    cos_theta, sin_theta = ek.cos(theta), ek.sin(theta)
    cos_phi, sin_phi = ek.cos(phi), ek.sin(phi)
    v = [cos_phi * sin_theta, sin_phi * sin_theta, cos_theta]

    wi = Vector3f(0, 0, 1)
    ek.set_slices(wi, steps)

    assert ek.allclose(mdf.eval(v),
              [  1.06103287e+01,   8.22650051e+00,   3.57923722e+00,
                 6.84863329e-01,   3.26460004e-02,   1.01964230e-04,
                 5.87322635e-10,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00])

    assert ek.allclose(mdf.pdf(wi, v),
               [  1.06103287e+01,   8.11430168e+00,   3.38530421e+00,
                 6.02319300e-01,   2.57622823e-02,   6.90584930e-05,
                 3.21235011e-10,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00])

    assert ek.allclose(mdf_i.eval(v),
               [  3.18309879e+01,   2.07673073e+00,   3.02855828e-04,
                 1.01591990e-11,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00])

    assert ek.allclose(mdf_i.pdf(wi, v),
               [  3.18309879e+01,   2.04840684e+00,   2.86446273e-04,
                 8.93474877e-12,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00])

    theta = Float.full(0.1, steps)
    phi = ek.linspace(Float, 0, 2*ek.pi, steps)
    cos_theta, sin_theta = ek.cos(theta), ek.sin(theta)
    cos_phi, sin_phi = ek.cos(phi), ek.sin(phi)
    v = [cos_phi * sin_theta, sin_phi * sin_theta, cos_theta]

    assert ek.allclose(mdf.eval(v),
               [ 3.95569706,  4.34706259,  5.54415846,  7.4061389 ,  9.17129803,
                9.62056446,  8.37803268,  6.42071199,  4.84459257,  4.05276537,
                4.05276537,  4.84459257,  6.42071199,  8.37803268,  9.62056446,
                9.17129803,  7.4061389 ,  5.54415846,  4.34706259,  3.95569706])

    assert ek.allclose(mdf.pdf(wi, v),
               Float([ 3.95569706,  4.34706259,  5.54415846,  7.4061389 ,  9.17129803,
                9.62056446,  8.37803268,  6.42071199,  4.84459257,  4.05276537,
                4.05276537,  4.84459257,  6.42071199,  8.37803268,  9.62056446,
                9.17129803,  7.4061389 ,  5.54415846,  4.34706259,  3.95569706]) * ek.cos(0.1))

    assert ek.allclose(mdf_i.eval(v), Float.full(11.86709118, steps))
    assert ek.allclose(mdf_i.pdf(wi, v), Float.full(11.86709118 * ek.cos(0.1), steps))


def test03_smith_g1_beckmann(variant_packet_rgb):
    from mitsuba.core import Vector3f
    from mitsuba.render import MicrofacetDistribution, MicrofacetType

    # Compare against data obtained from previous Mitsuba v0.6 implementation
    mdf   = MicrofacetDistribution(MicrofacetType.Beckmann, 0.1, 0.3, False)
    mdf_i = MicrofacetDistribution(MicrofacetType.Beckmann, 0.1, False)
    steps = 20
    theta = ek.linspace(Float, ek.pi/3, ek.pi/2, steps)

    phi = Float.full(ek.pi/2, steps)
    cos_theta, sin_theta = ek.cos(theta), ek.sin(theta)
    cos_phi, sin_phi = ek.cos(phi), ek.sin(phi)
    v = [cos_phi * sin_theta, sin_phi * sin_theta, cos_theta]
    wi = Vector3f(0, 0, 1)
    ek.set_slices(wi, steps)

    assert np.allclose(mdf.smith_g1(v, wi),
                       [1.0000000e+00, 1.0000000e+00, 1.0000000e+00, 1.0000523e+00, 9.9941480e-01,
                        9.9757767e-01, 9.9420297e-01, 9.8884594e-01, 9.8091525e-01, 9.6961778e-01,
                        9.5387781e-01, 9.3222123e-01, 9.0260512e-01, 8.6216795e-01, 8.0686140e-01,
                        7.3091686e-01, 6.2609726e-01, 4.8074335e-01, 2.7883825e-01, 1.9197471e-06])

    assert ek.allclose(mdf_i.smith_g1(v, wi),
               [1.0000000e+00, 1.0000000e+00, 1.0000000e+00, 1.0000000e+00, 1.0000000e+00,
                1.0000000e+00, 1.0000000e+00, 1.0000000e+00, 1.0000000e+00, 1.0000000e+00,
                1.0000000e+00, 1.0000000e+00, 1.0000000e+00, 1.0000000e+00, 9.9828446e-01,
                9.8627287e-01, 9.5088160e-01, 8.5989666e-01, 6.2535185e-01, 5.7592310e-06])

    steps = 20
    theta = Float.full(ek.pi / 2 * 0.98, steps)
    phi = ek.linspace(Float, 0, 2 * ek.pi, steps)
    cos_theta, sin_theta = ek.cos(theta), ek.sin(theta)
    cos_phi, sin_phi = ek.cos(phi), ek.sin(phi)
    v = [cos_phi * sin_theta, sin_phi * sin_theta, cos_theta]

    assert ek.allclose(mdf.smith_g1(v, wi),
               [ 0.67333597,  0.56164336,  0.42798978,  0.35298213,  0.31838724,
                 0.31201753,  0.33166203,  0.38421196,  0.48717275,  0.63746351,
                 0.63746351,  0.48717275,  0.38421196,  0.33166203,  0.31201753,
                 0.31838724,  0.35298213,  0.42798978,  0.56164336,  0.67333597])

    assert ek.allclose(mdf_i.smith_g1(v, wi), Float.full(0.67333597, steps))


def test04_sample_beckmann(variant_packet_rgb):
    from mitsuba.core import Vector3f
    from mitsuba.render import MicrofacetDistribution, MicrofacetType

    mdf = MicrofacetDistribution(MicrofacetType.Beckmann, 0.1, 0.3, False)

    # Compare against data obtained from previous Mitsuba v0.6 implementation
    steps = 6
    u = ek.linspace(Float, 0, 1, steps)
    u1, u2 = ek.meshgrid(u, u)
    u = [u1, u2]
    wi = Vector3f(0, 0, 1)
    ek.set_slices(wi, steps * steps)

    result = mdf.sample(wi, u)

    ref = (np.array([[  0.00000000e+00,   0.00000000e+00,   1.00000000e+00],
           [  4.71862517e-02,   1.23754589e-08,   9.98886108e-01],
           [  7.12896436e-02,   1.86970155e-08,   9.97455657e-01],
           [  9.52876359e-02,   2.49909284e-08,   9.95449781e-01],
           [  1.25854731e-01,   3.30077086e-08,   9.92048681e-01],
           [  1.00000000e+00,   2.62268316e-07,   0.00000000e+00],
           [  0.00000000e+00,   0.00000000e+00,   1.00000000e+00],
           [  1.44650340e-02,   1.33556545e-01,   9.90935624e-01],
           [  2.16356069e-02,   1.99762881e-01,   9.79605377e-01],
           [  2.85233315e-02,   2.63357669e-01,   9.64276493e-01],
           [  3.68374363e-02,   3.40122312e-01,   9.39659417e-01],
           [  1.07676744e-01,   9.94185984e-01,   0.00000000e+00],
           [ -0.00000000e+00,   0.00000000e+00,   1.00000000e+00],
           [ -3.80569659e-02,   8.29499215e-02,   9.95826781e-01],
           [ -5.72742373e-02,   1.24836378e-01,   9.90522861e-01],
           [ -7.61397704e-02,   1.65956154e-01,   9.83189344e-01],
           [ -9.96606201e-02,   2.17222810e-01,   9.71021116e-01],
           [ -4.17001039e-01,   9.08905983e-01,   0.00000000e+00],
           [ -0.00000000e+00,  -0.00000000e+00,   1.00000000e+00],
           [ -3.80568914e-02,  -8.29499587e-02,   9.95826781e-01],
           [ -5.72741292e-02,  -1.24836430e-01,   9.90522861e-01],
           [ -7.61396214e-02,  -1.65956244e-01,   9.83189344e-01],
           [ -9.96605307e-02,  -2.17223123e-01,   9.71021056e-01],
           [ -4.17000234e-01,  -9.08906400e-01,   0.00000000e+00],
           [  0.00000000e+00,  -0.00000000e+00,   1.00000000e+00],
           [  1.44650899e-02,  -1.33556545e-01,   9.90935624e-01],
           [  2.16356907e-02,  -1.99762881e-01,   9.79605377e-01],
           [  2.85234209e-02,  -2.63357460e-01,   9.64276552e-01],
           [  3.68375629e-02,  -3.40122133e-01,   9.39659476e-01],
           [  1.07677162e-01,  -9.94185925e-01,   0.00000000e+00],
           [  0.00000000e+00,   0.00000000e+00,   1.00000000e+00],
           [  4.71862517e-02,   8.25030622e-09,   9.98886108e-01],
           [  7.12896436e-02,   1.24646773e-08,   9.97455657e-01],
           [  9.52876359e-02,   1.66606196e-08,   9.95449781e-01],
           [  1.25854731e-01,   2.20051408e-08,   9.92048681e-01],
           [  1.00000000e+00,   1.74845553e-07,   0.00000000e+00]]),
    np.array([ 10.61032867,   8.51669121,   6.41503906,   4.302598  ,
             2.17350101,   0.        ,  10.61032867,   8.72333431,
             6.77215099,   4.7335186 ,   2.55768704,   0.        ,
            10.61032867,   8.59542656,   6.55068302,   4.46557426,
             2.31778312,   0.        ,  10.61032867,   8.59542656,
             6.55068302,   4.46557426,   2.31778359,   0.        ,
            10.61032867,   8.72333431,   6.77215099,   4.73351765,
             2.55768657,   0.        ,  10.61032867,   8.51669121,
             6.41503906,   4.302598  ,   2.17350101,   0.        ]))

    assert ek.allclose(ref[0], result[0], atol=5e-4)
    assert ek.allclose(ref[1], result[1], atol=1e-4)


def test03_smith_g1_ggx(variant_packet_rgb):
    from mitsuba.render import MicrofacetDistribution, MicrofacetType

    # Compare against data obtained from previous Mitsuba v0.6 implementation
    mdf   = MicrofacetDistribution(MicrofacetType.GGX, 0.1, 0.3, False)
    mdf_i = MicrofacetDistribution(MicrofacetType.GGX, 0.1, False)
    steps = 20
    theta = ek.linspace(Float, ek.pi/3, ek.pi/2, steps)
    phi = Float.full(ek.pi/2, steps)
    cos_theta, sin_theta = ek.cos(theta), ek.sin(theta)
    cos_phi, sin_phi = ek.cos(phi), ek.sin(phi)
    v = [cos_phi * sin_theta, sin_phi * sin_theta, cos_theta]
    wi = np.tile([0, 0, 1], (steps, 1))

    assert ek.allclose(mdf.smith_g1(v, wi),
                       [9.4031686e-01, 9.3310797e-01, 9.2485082e-01, 9.1534841e-01, 9.0435863e-01,
                        8.9158219e-01, 8.7664890e-01, 8.5909742e-01, 8.3835226e-01, 8.1369340e-01,
                        7.8421932e-01, 7.4880326e-01, 7.0604056e-01, 6.5419233e-01, 5.9112519e-01,
                        5.1425743e-01, 4.2051861e-01, 3.0633566e-01, 1.6765384e-01, 1.0861372e-06])

    assert ek.allclose(mdf_i.smith_g1(v, wi),
                       [9.9261039e-01, 9.9160647e-01, 9.9042398e-01, 9.8901933e-01, 9.8733366e-01,
                        9.8528832e-01, 9.8277503e-01, 9.7964239e-01, 9.7567332e-01, 9.7054905e-01,
                        9.6378750e-01, 9.5463598e-01, 9.4187391e-01, 9.2344058e-01, 8.9569420e-01,
                        8.5189372e-01, 7.7902949e-01, 6.5144652e-01, 4.1989169e-01, 3.2584082e-06])

    theta = Float.full(ek.pi / 2 * 0.98, steps)
    phi = ek.linspace(Float, 0, 2 * ek.pi, steps)
    cos_theta, sin_theta = ek.cos(theta), ek.sin(theta)
    cos_phi, sin_phi = ek.cos(phi), ek.sin(phi)
    v = [cos_phi * sin_theta, sin_phi * sin_theta, cos_theta]

    assert ek.allclose(mdf.smith_g1(v, wi),
      [ 0.46130955,  0.36801264,  0.26822716,  0.21645154,  0.19341162,
                 0.18922243,  0.20219423,  0.23769052,  0.31108665,  0.43013984,
                 0.43013984,  0.31108665,  0.23769052,  0.20219423,  0.18922243,
                 0.19341162,  0.21645154,  0.26822716,  0.36801264,  0.46130955])

    assert ek.allclose(mdf_i.smith_g1(v, wi), Float.full(0.46130955, steps))


def test05_sample_ggx(variant_packet_rgb):
    from mitsuba.render import MicrofacetDistribution, MicrofacetType

    mdf = MicrofacetDistribution(MicrofacetType.GGX, 0.1, 0.3, False)

    # Compare against data obtained from previous Mitsuba v0.6 implementation
    steps = 6
    u = ek.linspace(Float, 0, 1, steps)
    u1, u2 = ek.meshgrid(u, u)
    u = [u1, u2]
    wi = np.tile([0, 0, 1], (steps * steps, 1))
    result = mdf.sample(wi, u)
    ref = ((np.array([[  0.00000000e+00,   0.00000000e+00,   1.00000000e+00],
           [  4.99384739e-02,   1.30972797e-08,   9.98752296e-01],
           [  8.13788623e-02,   2.13430980e-08,   9.96683240e-01],
           [  1.21566132e-01,   3.18829443e-08,   9.92583334e-01],
           [  1.96116075e-01,   5.14350340e-08,   9.80580688e-01],
           [  1.00000000e+00,   2.62268316e-07,   0.00000000e+00],
           [  0.00000000e+00,   0.00000000e+00,   1.00000000e+00],
           [  1.52942007e-02,   1.41212299e-01,   9.89861190e-01],
           [  2.45656986e-02,   2.26816610e-01,   9.73627627e-01],
           [  3.57053429e-02,   3.29669625e-01,   9.43420947e-01],
           [  5.36015145e-02,   4.94906068e-01,   8.67291689e-01],
           [  1.07676744e-01,   9.94185984e-01,   0.00000000e+00],
           [ -0.00000000e+00,   0.00000000e+00,   1.00000000e+00],
           [ -4.02617380e-02,   8.77555013e-02,   9.95328069e-01],
           [ -6.52425364e-02,   1.42204270e-01,   9.87684846e-01],
           [ -9.64000970e-02,   2.10116088e-01,   9.72912252e-01],
           [ -1.50845990e-01,   3.28787714e-01,   9.32278991e-01],
           [ -4.17001039e-01,   9.08905983e-01,   0.00000000e+00],
           [ -0.00000000e+00,  -0.00000000e+00,   1.00000000e+00],
           [ -4.02616598e-02,  -8.77555385e-02,   9.95328069e-01],
           [ -6.52425662e-02,  -1.42204687e-01,   9.87684786e-01],
           [ -9.64000225e-02,  -2.10116416e-01,   9.72912192e-01],
           [ -1.50845826e-01,  -3.28788161e-01,   9.32278872e-01],
           [ -4.17000234e-01,  -9.08906400e-01,   0.00000000e+00],
           [  0.00000000e+00,  -0.00000000e+00,   1.00000000e+00],
           [  1.52942603e-02,  -1.41212285e-01,   9.89861190e-01],
           [  2.45657954e-02,  -2.26816595e-01,   9.73627627e-01],
           [  3.57054621e-02,  -3.29669416e-01,   9.43421006e-01],
           [  5.36017120e-02,  -4.94905949e-01,   8.67291749e-01],
           [  1.07677162e-01,  -9.94185925e-01,   0.00000000e+00],
           [  0.00000000e+00,   0.00000000e+00,   1.00000000e+00],
           [  4.99384739e-02,   8.73152040e-09,   9.98752296e-01],
           [  8.13788623e-02,   1.42287320e-08,   9.96683240e-01],
           [  1.21566132e-01,   2.12552980e-08,   9.92583334e-01],
           [  1.96116075e-01,   3.42900250e-08,   9.80580688e-01],
           [  1.00000000e+00,   1.74845553e-07,   0.00000000e+00]]),
     np.array([ 10.61032867,   6.81609201,   3.85797882,   1.73599267,
             0.45013079,   0.        ,  10.61032867,   7.00141668,
             4.13859272,   2.02177191,   0.65056872,   0.        ,
            10.61032867,   6.88668203,   3.96438813,   1.84343493,
             0.52378261,   0.        ,  10.61032867,   6.88668203,
             3.96438861,   1.84343517,   0.52378279,   0.        ,
            10.61032867,   7.00141668,   4.13859272,   2.02177191,
             0.65056866,   0.        ,  10.61032867,   6.81609201,
             3.85797882,   1.73599267,   0.45013079,   0.        ])))

    assert ek.allclose(ref[0], result[0], atol=5e-4)
    assert ek.allclose(ref[1], result[1], atol=1e-4)


@pytest.mark.parametrize("sample_visible", [True, False])
@pytest.mark.parametrize("alpha", [0.1, 0.5])
@pytest.mark.parametrize("md_type_name", ['GGX', 'Beckmann'])
@pytest.mark.parametrize("angle", [0, 80, 30])
def test06_chi2(variant_packet_rgb, md_type_name, alpha, sample_visible, angle):
    from mitsuba.python.chi2 import MicrofacetAdapter, ChiSquareTest, SphericalDomain
    from mitsuba.render import MicrofacetType

    if md_type_name == "GGX":
        md_type = MicrofacetType.GGX
    else:
        md_type = MicrofacetType.Beckmann

    sample_func, pdf_func = MicrofacetAdapter(md_type, alpha, sample_visible)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=lambda *args: sample_func(*(list(args) + [angle])),
        pdf_func=lambda *args: pdf_func(*(list(args) + [angle])),
        sample_dim=2,
        res=203,
        ires=10
    )

    assert chi2.run()