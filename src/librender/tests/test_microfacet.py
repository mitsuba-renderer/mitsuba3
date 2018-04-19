import numpy as np

from mitsuba.core.xml import load_string
from mitsuba.render import MicrofacetDistribution


def test01_construct():
    md = MicrofacetDistribution(MicrofacetDistribution.EBeckmann, 0.1, True)
    assert md.type() == MicrofacetDistribution.EBeckmann
    assert np.allclose(md.alpha_u(), 0.1)
    assert np.allclose(md.alpha_v(), 0.1)
    assert md.sample_visible()


def test02_eval_pdf_beckmann():
    # Compare against data obtained from previous Mitsuba v0.6 implementation
    mdf = MicrofacetDistribution(MicrofacetDistribution.EBeckmann, 0.1, 0.3, False)
    mdf_i = MicrofacetDistribution(MicrofacetDistribution.EBeckmann, 0.1, False)
    assert not mdf.is_isotropic()
    assert mdf.is_anisotropic()
    assert mdf_i.is_isotropic()
    assert not mdf_i.is_anisotropic()

    steps = 20
    theta = np.linspace(0, np.pi, steps)
    phi = np.full(steps, np.pi / 2)
    cos_theta, sin_theta = np.cos(theta), np.sin(theta)
    cos_phi, sin_phi = np.cos(phi), np.sin(phi)
    v = np.column_stack([cos_phi * sin_theta, sin_phi * sin_theta, cos_theta])
    wi = np.tile([0, 0, 1], (steps, 1))

    assert np.allclose(mdf.eval(v),
     np.array([  1.06103287e+01,   8.22650051e+00,   3.57923722e+00,
                 6.84863329e-01,   3.26460004e-02,   1.01964230e-04,
                 5.87322635e-10,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00]))

    assert np.allclose(mdf.pdf(wi, v),
     np.array([  1.06103287e+01,   8.11430168e+00,   3.38530421e+00,
                 6.02319300e-01,   2.57622823e-02,   6.90584930e-05,
                 3.21235011e-10,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00]))

    assert np.allclose(mdf_i.eval(v),
     np.array([  3.18309879e+01,   2.07673073e+00,   3.02855828e-04,
                 1.01591990e-11,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00]))

    assert np.allclose(mdf_i.pdf(wi, v),
     np.array([  3.18309879e+01,   2.04840684e+00,   2.86446273e-04,
                 8.93474877e-12,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,   0.00000000e+00,   0.00000000e+00,
                 0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00,  -0.00000000e+00,
                -0.00000000e+00,  -0.00000000e+00]))

    theta = np.full(steps, 0.1)
    phi = np.linspace(0, 2*np.pi, steps)
    cos_theta, sin_theta = np.cos(theta), np.sin(theta)
    cos_phi, sin_phi = np.cos(phi), np.sin(phi)
    v = np.column_stack([cos_phi * sin_theta, sin_phi * sin_theta, cos_theta])

    assert np.allclose(mdf.eval(v),
     np.array([ 3.95569706,  4.34706259,  5.54415846,  7.4061389 ,  9.17129803,
                9.62056446,  8.37803268,  6.42071199,  4.84459257,  4.05276537,
                4.05276537,  4.84459257,  6.42071199,  8.37803268,  9.62056446,
                9.17129803,  7.4061389 ,  5.54415846,  4.34706259,  3.95569706]))

    assert np.allclose(mdf.pdf(wi, v),
     np.array([ 3.95569706,  4.34706259,  5.54415846,  7.4061389 ,  9.17129803,
                9.62056446,  8.37803268,  6.42071199,  4.84459257,  4.05276537,
                4.05276537,  4.84459257,  6.42071199,  8.37803268,  9.62056446,
                9.17129803,  7.4061389 ,  5.54415846,  4.34706259,  3.95569706]) * np.cos(0.1))

    assert np.allclose(mdf_i.eval(v), np.full(steps, 11.86709118))
    assert np.allclose(mdf_i.pdf(wi, v), np.full(steps, 11.86709118 * np.cos(0.1)))

def test03_smith_g1_beckmann():
    # Compare against data obtained from previous Mitsuba v0.6 implementation
    mdf = MicrofacetDistribution(MicrofacetDistribution.EBeckmann, 0.1, 0.3, False)
    mdf_i = MicrofacetDistribution(MicrofacetDistribution.EBeckmann, 0.1, False)
    steps = 20
    theta = np.linspace(np.pi/3, np.pi/2, steps)
    phi = np.full(steps, np.pi/2)
    cos_theta, sin_theta = np.cos(theta), np.sin(theta)
    cos_phi, sin_phi = np.cos(phi), np.sin(phi)
    v = np.column_stack([cos_phi * sin_theta, sin_phi * sin_theta, cos_theta])
    wi = np.tile([0, 0, 1], (steps, 1))

    assert np.allclose(mdf.smith_g1(v, wi),
      np.array([  1.00000000e+00,   1.00000000e+00,   1.00000000e+00,
                  1.00005233e+00,   9.99414802e-01,   9.97577667e-01,
                  9.94203031e-01,   9.88845885e-01,   9.80915368e-01,
                  9.69617903e-01,   9.53877807e-01,   9.32221234e-01,
                  9.02605236e-01,   8.62168074e-01,   8.06861639e-01,
                  7.30917037e-01,   6.26097679e-01,   4.80744094e-01,
                  2.78839469e-01,   7.21521140e-16]))

    assert np.allclose(mdf_i.smith_g1(v, wi),
      np.array([  1.00000000e+00,   1.00000000e+00,   1.00000000e+00,
                  1.00000000e+00,   1.00000000e+00,   1.00000000e+00,
                  1.00000000e+00,   1.00000000e+00,   1.00000000e+00,
                  1.00000000e+00,   1.00000000e+00,   1.00000000e+00,
                  1.00000000e+00,   1.00000000e+00,   9.98284459e-01,
                  9.86272812e-01,   9.50881779e-01,   8.59897316e-01,
                  6.25353634e-01,   2.16456326e-15]))

    steps = 20
    theta = np.full(steps, np.pi / 2 * 0.98)
    phi = np.linspace(0, 2 * np.pi, steps)
    cos_theta, sin_theta = np.cos(theta), np.sin(theta)
    cos_phi, sin_phi = np.cos(phi), np.sin(phi)
    v = np.column_stack([cos_phi * sin_theta, sin_phi * sin_theta, cos_theta])

    assert np.allclose(mdf.smith_g1(v, wi),
      np.array([ 0.67333597,  0.56164336,  0.42798978,  0.35298213,  0.31838724,
                 0.31201753,  0.33166203,  0.38421196,  0.48717275,  0.63746351,
                 0.63746351,  0.48717275,  0.38421196,  0.33166203,  0.31201753,
                 0.31838724,  0.35298213,  0.42798978,  0.56164336,  0.67333597]))

    assert np.allclose(mdf_i.smith_g1(v, wi), np.full(steps, 0.67333597))


def test04_sample_beckmann():
    mdf = MicrofacetDistribution(MicrofacetDistribution.EBeckmann, 0.1, 0.3, False)

    # Compare against data obtained from previous Mitsuba v0.6 implementation
    steps = 6
    u = np.linspace(0, 1, steps)
    u1, u2 = np.meshgrid(u, u, indexing='xy')
    u = np.column_stack((u1.ravel(), u2.ravel()))
    wi = np.tile([0, 0, 1], (steps * steps, 1))
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

    assert np.allclose(ref[0], result[0], atol=5e-4)
    assert np.allclose(ref[1], result[1], atol=1e-4)


def test03_smith_g1_ggx():
    # Compare against data obtained from previous Mitsuba v0.6 implementation
    mdf = MicrofacetDistribution(MicrofacetDistribution.EGGX, 0.1, 0.3, False)
    mdf_i = MicrofacetDistribution(MicrofacetDistribution.EGGX, 0.1, False)
    steps = 20
    theta = np.linspace(np.pi/3, np.pi/2, steps)
    phi = np.full(steps, np.pi/2)
    cos_theta, sin_theta = np.cos(theta), np.sin(theta)
    cos_phi, sin_phi = np.cos(phi), np.sin(phi)
    v = np.column_stack([cos_phi * sin_theta, sin_phi * sin_theta, cos_theta])
    wi = np.tile([0, 0, 1], (steps, 1))

    assert np.allclose(mdf.smith_g1(v, wi),
      np.array([  9.40316856e-01,   9.33107972e-01,   9.24850941e-01,
                 9.15348530e-01,   9.04358625e-01,   8.91582191e-01,
                 8.76648903e-01,   8.59097540e-01,   8.38352323e-01,
                 8.13693404e-01,   7.84219384e-01,   7.48803258e-01,
                 7.06040680e-01,   6.54192448e-01,   5.91125309e-01,
                 5.14257669e-01,   4.20519054e-01,   3.06336224e-01,
                 1.67654604e-01,   4.08215618e-16]))

    assert np.allclose(mdf_i.smith_g1(v, wi),
      np.array([  9.92610395e-01,   9.91606474e-01,   9.90424097e-01,
                  9.89019334e-01,   9.87333655e-01,   9.85288322e-01,
                  9.82775033e-01,   9.79642451e-01,   9.75673318e-01,
                  9.70549047e-01,   9.63787496e-01,   9.54635978e-01,
                  9.41873908e-01,   9.23440576e-01,   8.95694196e-01,
                  8.51893902e-01,   7.79029906e-01,   6.51447296e-01,
                  4.19893295e-01,   1.22464680e-15]))

    theta = np.full(steps, np.pi / 2 * 0.98)
    phi = np.linspace(0, 2 * np.pi, steps)
    cos_theta, sin_theta = np.cos(theta), np.sin(theta)
    cos_phi, sin_phi = np.cos(phi), np.sin(phi)
    v = np.column_stack([cos_phi * sin_theta, sin_phi * sin_theta, cos_theta])

    assert np.allclose(mdf.smith_g1(v, wi),
      np.array([ 0.46130955,  0.36801264,  0.26822716,  0.21645154,  0.19341162,
                 0.18922243,  0.20219423,  0.23769052,  0.31108665,  0.43013984,
                 0.43013984,  0.31108665,  0.23769052,  0.20219423,  0.18922243,
                 0.19341162,  0.21645154,  0.26822716,  0.36801264,  0.46130955]))

    assert np.allclose(mdf_i.smith_g1(v, wi), np.full(steps, 0.46130955))


def test05_sample_ggx():
    mdf = MicrofacetDistribution(MicrofacetDistribution.EGGX, 0.1, 0.3, False)

    # Compare against data obtained from previous Mitsuba v0.6 implementation
    steps = 6
    u = np.linspace(0, 1, steps)
    u1, u2 = np.meshgrid(u, u, indexing='xy')
    u = np.column_stack((u1.ravel(), u2.ravel()))
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

    assert np.allclose(ref[0], result[0], atol=5e-4)
    assert np.allclose(ref[1], result[1], atol=1e-4)


