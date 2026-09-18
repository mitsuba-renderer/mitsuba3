
import pytest
import drjit as dr
from drjit.scalar import ArrayXu as UInt32
import mitsuba as mi


def test01_create(variant_scalar_rgb):
    bsdf = mi.load_string("""<bsdf version="3.0.0" type="twosided">
        <bsdf type="diffuse"/>
    </bsdf>""")
    assert bsdf is not None
    assert bsdf.component_count() == 2
    assert bsdf.flags(0) == mi.BSDFFlags.DiffuseReflection | mi.BSDFFlags.FrontSide
    assert bsdf.flags(1) == mi.BSDFFlags.DiffuseReflection | mi.BSDFFlags.BackSide
    assert bsdf.flags() == bsdf.flags(0) | bsdf.flags(1)

    bsdf = mi.load_string("""<bsdf version="3.0.0" type="twosided">
        <bsdf type="roughconductor"/>
        <bsdf type="diffuse"/>
    </bsdf>""")
    assert bsdf is not None
    assert bsdf.component_count() == 2
    assert bsdf.flags(0) == mi.BSDFFlags.GlossyReflection  | mi.BSDFFlags.FrontSide
    assert bsdf.flags(1) == mi.BSDFFlags.DiffuseReflection | mi.BSDFFlags.BackSide
    assert bsdf.flags() == bsdf.flags(0) | bsdf.flags(1)


def test02_pdf(variant_scalar_rgb):
    bsdf = mi.load_string("""<bsdf version="3.0.0" type="twosided">
        <bsdf type="diffuse"/>
    </bsdf>""")

    si = mi.SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.wi = [0, 0, 1]
    ctx = mi.BSDFContext()
    p_pdf = bsdf.pdf(ctx, si, [0, 0, 1])
    assert dr.allclose(p_pdf, dr.inv_pi)

    p_pdf = bsdf.pdf(ctx, si, [0, 0, -1])
    assert dr.allclose(p_pdf, 0.0)


def test03_sample_eval_pdf(variant_scalar_rgb):
    bsdf = mi.load_string("""<bsdf version="3.0.0" type="twosided">
        <bsdf type="diffuse">
            <rgb name="reflectance" value="0.1, 0.1, 0.1"/>
        </bsdf>
        <bsdf type="diffuse">
            <rgb name="reflectance" value="0.9, 0.9, 0.9"/>
        </bsdf>
    </bsdf>""")

    si = mi.SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)

    n = 5
    ctx = mi.BSDFContext()
    for u in dr.arange(UInt32, n):
        for v in dr.arange(UInt32, n):
            si.wi = mi.warp.square_to_uniform_sphere([u / float(n-1),
                                                      v / float(n-1)])
            up = dr.dot(si.wi, [0, 0, 1]) > 0

            for x in dr.arange(UInt32, n):
                for y in dr.arange(UInt32, n):
                    sample = [x / float(n-1), y / float(n-1)]
                    (bs, s_value) = bsdf.sample(ctx, si, 0.5, sample)

                    if dr.any(s_value > 0):
                        # Multiply by square_to_cosine_hemisphere_theta
                        s_value *= bs.wo[2] * dr.inv_pi
                        if not up:
                            s_value *= -1

                        e_value = bsdf.eval(ctx, si, bs.wo)
                        p_pdf   = bsdf.pdf(ctx, si, bs.wo)

                        assert dr.allclose(s_value, e_value, atol=1e-2)
                        assert dr.allclose(bs.pdf, p_pdf)
                        assert not dr.any(dr.isnan(e_value) | dr.isnan(s_value))

                        # Otherwise, sampling failed and we can't rely on bs.wo.
                        v_eval_pdf = bsdf.eval_pdf(ctx, si, bs.wo)
                        assert dr.allclose(e_value, v_eval_pdf[0])
                        assert dr.allclose(p_pdf, v_eval_pdf[1])

def test04_features(variants_vec_rgb):
    bsdf_front = mi.load_dict({
        'type': 'diffuse',
        'reflectance': {
            'type': 'rgb',
            'value': [0.1, 0.1, 0.1]
        }
    })
    bsdf_back = mi.load_dict({
        'type': 'diffuse',
        'reflectance': {
            'type': 'rgb',
            'value': [0.9, 0.9, 0.9]
        }
    })
    bsdf = mi.load_dict({
        'type': 'twosided',
        'a': bsdf_front,
        'b': bsdf_back,
    })

    si = mi.SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)

    n = 5
    epsilon = 0.0001
    for u in dr.linspace(mi.Float, epsilon, 1 - epsilon, n):
        for v in dr.linspace(mi.Float, epsilon, 1 - epsilon, n):
            si.wi = mi.warp.square_to_uniform_sphere([u / float(n-1),
                                                      v / float(n-1)])
            up = mi.Frame3f.cos_theta(si.wi) > 0.0

            value = bsdf.eval_features(si).albedo
            value_front = bsdf_front.eval_features(si).albedo
            si.wi.z *= -1
            value_back = bsdf_back.eval_features(si).albedo

            assert dr.allclose(dr.select(up, value - value_front, 0), 0.0)
            assert dr.allclose(dr.select(up, 0, value - value_back), 0.0)


def test05_eval_attribute(variants_vec_rgb):
    bsdf_front = mi.load_dict({
        'type': 'roughconductor',
        'material': 'Al',
        'distribution': 'ggx',
        'alpha': 0.2
    })
    bsdf_back = mi.load_dict({
        'type': 'roughconductor',
        'material': 'Al',
        'distribution': 'ggx',
        'alpha': 0.1
    })
    bsdf = mi.load_dict({
        'type': 'twosided',
        'a': bsdf_front,
        'b': bsdf_back,
    })

    si = mi.SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)

    value = bsdf.has_attribute("alpha")
    value_front = bsdf_front.has_attribute("alpha")
    value_back = bsdf_back.has_attribute("alpha")
    assert (value == value_front) and (value == value_back)

    n = 5
    epsilon = 0.0001
    for u in dr.linspace(mi.Float, epsilon, 1 - epsilon, n):
        for v in dr.linspace(mi.Float, epsilon, 1 - epsilon, n):
            si.wi = mi.warp.square_to_uniform_sphere([u / float(n-1),
                                                      v / float(n-1)])
            up = mi.Frame3f.cos_theta(si.wi) > 0.0

            value = bsdf.eval_attribute("alpha", si)
            value_front = bsdf_front.eval_attribute("alpha", si)
            si.wi.z *= -1
            value_back = bsdf_back.eval_attribute("alpha", si)

            assert dr.allclose(dr.select(up, value - value_front, 0), 0.0)
            assert dr.allclose(dr.select(up, 0, value - value_back), 0.0)


def test06_tilted_shading_normal(variant_scalar_rgb):
    # The viewer is above the geometric surface but behind a shading normal
    # tilted by 60 degrees. Light must only arrive from above the geometry.
    bsdf = mi.load_dict({'type': 'twosided', 'bsdf': {'type': 'diffuse'}})
    si = mi.SurfaceInteraction3f()
    si.t = 0.1
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    tilt = dr.deg2rad(60.0)
    si.sh_frame = mi.Frame3f(mi.Normal3f(dr.sin(tilt), 0, dr.cos(tilt)))
    wi_world = dr.normalize(mi.Vector3f(-0.9, 0, 0.44))
    assert dr.dot(wi_world, si.n) > 0 and dr.dot(wi_world, si.sh_frame.n) < 0
    si.wi = si.to_local(wi_world)
    ctx = mi.BSDFContext()

    below = si.to_local(mi.Vector3f(0, 0, -1))
    above = si.to_local(mi.Vector3f(0, 0, 1))
    leak = si.to_local(dr.normalize(mi.Vector3f(0.9, 0, -0.3)))
    assert leak.z > 0
    for wo in (below, leak):
        assert dr.allclose(bsdf.eval(ctx, si, wo), 0.0)
        assert dr.allclose(bsdf.pdf(ctx, si, wo), 0.0)
    assert dr.allclose(bsdf.eval(ctx, si, above),
                       0.5 * dr.inv_pi * dr.cos(tilt))
    for i in range(200):
        bs, weight = bsdf.sample(ctx, si, 0.5, [(i % 20) / 20.0, (i // 20) / 10.0])
        if bs.pdf > 0:
            assert dr.dot(si.to_world(bs.wo), si.n) >= -1e-6

    # Mirrored configuration behind the surface
    si.wi = si.to_local(-wi_world)
    assert dr.allclose(bsdf.eval(ctx, si, above), 0.0)
    assert dr.all(bsdf.eval(ctx, si, below) > 0.0)
