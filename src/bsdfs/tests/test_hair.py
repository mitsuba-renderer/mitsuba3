import pytest
import drjit as dr
import mitsuba as mi
from mitsuba.python.util import traverse
import gc

def test01_create():
    mi.set_variant('llvm_ad_rgb')
    b = mi.load_dict({'type': 'hair'})
    assert b is not None
    assert b.component_count() == 1
    assert b.flags(0) == (mi.BSDFFlags.Glossy | mi.BSDFFlags.FrontSide | mi.BSDFFlags.BackSide | mi.BSDFFlags.NonSymmetric)
    assert b.flags() == b.flags(0)
    params = traverse(b)
    print (params)


def test02_white_furnace(variants_vec_backends_once_rgb):
    total = 300000
    sampler = mi.load_dict({'type': 'independent', 'sample_count': total})
    sampler.seed(seed = 0, wavefront_size = total)

    si    = mi.SurfaceInteraction3f()
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.wi = mi.warp.square_to_uniform_sphere(sampler.next_2d())
    si.sh_frame = mi.Frame3f(si.n)
    
    ctx = mi.BSDFContext()

    beta_m = 0.1
    beta_n = 0.1
    while(beta_m < 1):
        beta_n = 0.1
        while(beta_n < 1):
            # estimate reflected uniform incident radiance from hair
            sum = 0.

            si.uv = [0, sampler.next_1d()]
            sigma_a = 0.
            bsdf = mi.load_dict({        
                'type': 'hair',
                'beta_m': beta_m,
                'beta_n': beta_n,
                'alpha': 0.,
                'int_ior': 1.55,
            })

            wo = mi.warp.square_to_uniform_sphere(sampler.next_2d())
            sum = bsdf.eval(ctx, si, wo)

            assert dr.allclose(dr.sum(sum.y)/(total * mi.warp.square_to_uniform_sphere_pdf(1.)), 1, rtol = 0.05)
            beta_n += 0.2
        beta_m += 0.2


def test03_white_furnace_importance_sample(variants_vec_backends_once_rgb):
    total = 3000000
    sampler = mi.load_dict({'type': 'independent', 'sample_count': total})
    sampler.seed(seed = 0, wavefront_size = total)

    si    = mi.SurfaceInteraction3f()
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.wi = mi.warp.square_to_uniform_sphere(sampler.next_2d())
    ctx = mi.BSDFContext()

    beta_m = 0.1
    beta_n = 0.1

    while(beta_m < 1):
        beta_n = 0.1
        while(beta_n < 1):
            # estimate reflected uniform incident radiance from hair
            count = total
            sum = 0.

            si.uv = [0, sampler.next_1d()] 
            sigma_a = 0.
            bsdf = mi.load_dict({        
                'type': 'hair',
                'beta_m': beta_m,
                'beta_n': beta_n,
                'alpha': 0.,
                'int_ior': 1.55,
            })
            bs, spec = bsdf.sample(ctx, si, sampler.next_1d(), sampler.next_2d())
            pdf = bsdf.pdf(ctx, si, bs.wo)
            sum += spec
            count -= 1
            assert dr.allclose(dr.sum(sum.y) / total, 1, rtol = 0.01)
            beta_n += 0.2
        beta_m += 0.2


def test04_sample_numeric(variants_vec_backends_once_rgb):
    total = 50000
    sampler = mi.load_dict({'type': 'independent', 'sample_count': total})
    sampler.seed(seed = 0, wavefront_size = total)

    si    = mi.SurfaceInteraction3f()
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)

    ctx = mi.BSDFContext()

    beta_m = 0.1
    beta_n = 0.1
    
    while(beta_m < 1):
        beta_n = 0.1
        while(beta_n < 1):
            count = total
            # estimate reflected uniform incident radiance from hair
            sigma_a = 0.
            si.uv = [0, sampler.next_1d()] 
            bsdf = mi.load_dict({        
                'type': 'hair',
                'beta_m': beta_m,
                'beta_n': beta_n,
                'alpha': 2.,
                'int_ior': 1.55,
            })
            si.wi = mi.warp.square_to_uniform_sphere(sampler.next_2d())

            d1 = sampler.next_1d()
            d2 = sampler.next_2d()

            bs, spec = bsdf.sample(ctx, si, d1, d2)
            
            # Sometimes when pdf is zero spec will return 0
            spec = dr.select(abs(spec.y) < 1e-6, 1, spec)

            flag = dr.allclose(spec.y, 1, atol = 0.001)
            assert (dr.all(flag))

            beta_n += 0.2
        beta_m += 0.2
    


def test04_sample_pdf(variants_vec_backends_once_rgb):
    total = 300000
    sampler = mi.load_dict({'type': 'independent', 'sample_count': total})
    sampler.seed(seed = 2, wavefront_size = total)

    si    = mi.SurfaceInteraction3f()
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)

    ctx = mi.BSDFContext()

    beta_m = 0.1
    beta_n = 0.1
    
    while(beta_m < 1):
        beta_n = 0.1
        while(beta_n < 1):
            # estimate reflected uniform incident radiance from hair
            si.uv = [0, sampler.next_1d()]
            sigma_a = 0.25
            bsdf = mi.load_dict({        
                'type': 'hair',
                'beta_m': beta_m,
                'beta_n': beta_n,
                'alpha': 0.,
                'int_ior': 1.55,
                })
            si.wi = mi.warp.square_to_uniform_sphere(sampler.next_2d())
            bs, spec = bsdf.sample(ctx, si, sampler.next_1d(), sampler.next_2d())

            pdf = bsdf.pdf(ctx, si, bs.wo)
            eval = bsdf.eval(ctx, si, bs.wo)
            eval_pdf = bsdf.eval_pdf(ctx, si, bs.wo)

            assert dr.allclose(pdf, bs.pdf, rtol = 0.005)
            assert dr.allclose(eval, eval_pdf[0], rtol = 0.001)
            assert dr.allclose(pdf, eval_pdf[1], rtol = 0.001)
            assert dr.allclose(spec * pdf , eval, rtol = 0.001)

            beta_n += 0.2
        beta_m += 0.2

def test05_sampleConsistency(variants_vec_backends_once_rgb):
    def helper_Li(spec):
        return spec.z * spec.z

    total = 128 * 1024
    sampler = mi.load_dict({'type': 'independent', 'sample_count': total})
    sampler.seed(seed = 4, wavefront_size = total)

    si    = mi.SurfaceInteraction3f()
    si.p  = [0, 0, 0]
    si.n  = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.uv = [0, sampler.next_1d()] # h = 1

    ctx = mi.BSDFContext()

    beta_m = 0.2
    beta_n = 0.4
    
    while(beta_m < 1):
        beta_n = 0.4
        while(beta_n < 1):

            si.wi = mi.warp.square_to_uniform_sphere(sampler.next_2d())
            sigma_a = [0.25, 0.25, 0.25]
            fImportance = 0.
            fUniform = 0.
            
            bsdf = mi.load_dict({        
                'type': 'hair',
                'sigma_a': sigma_a,
                'beta_m': beta_m,
                'beta_n': beta_n,
                'alpha': 2.,
                'int_ior': 1.55,
                })

            bs, spec = bsdf.sample(ctx, si, sampler.next_1d(), sampler.next_2d())
            
            fImportance = spec * helper_Li(bs.wo)

            wo = mi.warp.square_to_uniform_sphere(sampler.next_2d())

            fUniform = bsdf.eval(ctx, si, wo) * helper_Li(wo) * (dr.pi * 4)

            print (dr.sum(fImportance.y), dr.sum(fUniform.y))

            assert dr.allclose(dr.sum(fImportance.y), dr.sum(fUniform.y), rtol=0.05)

            beta_n += 0.2
        beta_m += 0.2    


def test06_chi2():
    import sys
    sys.path.insert(1, '.')
    from mitsuba.chi2 import BSDFAdapter, ChiSquareTest

    mi.set_variant('cuda_ad_rgb')

    sigma_a = [10.0, 10.0, 10.0]
    alpha = 0
    eta = 1.55
    total = 10

    beta_m = 0.2
    beta_n = 0.4
    sample_count = 1000000
    res = 101
    ires = 256
    # TODO Adaptive pdf integration

    phi = 0
    theta = 0

    while(beta_m <=1.0):
        beta_n = 0.4
        while(beta_n <= 0.6):
            for count_theta in range(-total // 2, total // 2 + 1): # -total // 2 + 1, total // 2
                for count_phi in range(0, total+1):

                        theta = count_theta / total * dr.pi
                        phi =  count_phi / total * 2 * dr.pi
                        u = 0.5

                        # beta_m = 1
                        # beta_n = 0.6
                        # theta = 0 / total * dr.pi
                        # phi = total / (total * 2) * dr.pi

                        xml = f"""<float name="alpha" value="{alpha}" />
                                <vector name="sigma_a" value="10., 10., 10." />
                                <float name="beta_m" value="{beta_m}" />
                                <float name="beta_n" value="{beta_n}" />
                                <float name="int_ior" value="{eta}" />
                            """
                        
                        x = dr.sin(theta)
                        y = dr.cos(theta) * dr.cos(phi)
                        z = dr.cos(theta) * dr.sin(phi)

                        wi = dr.normalize(mi.ScalarVector3f(x, y, z))
                    
                        sample_func, pdf_func = BSDFAdapterUV("hair", xml, u=u, wi=wi)

                        chi2 = mi.chi2.ChiSquareTest(
                        # chi2 = ChiSquareTest(
                            domain=SphericalDomain(),
                            sample_func=sample_func,
                            pdf_func=pdf_func,
                            sample_dim = 3,
                            res = res,
                            ires = ires,
                            seed = 1,
                            sample_count = sample_count
                        )

                        print ("beta_m: ", beta_m)
                        print ("beta_n: ", beta_n)
                        print ("count_theta: ", count_theta)
                        print ("count_phi: ", count_phi)
                        print ("wi: ", wi)

                        flag = chi2.run()
                        if (flag == False):
                            with open("failed.txt", "a") as f:
                                f.write("beta_m: {}, beta_n: {}, count_theta: {}, count_phi: {}\n".format(beta_m, beta_n, count_theta, count_phi))

                        # input()
            beta_n += 0.2
            gc.collect()
        beta_m += 0.2


class SphericalDomain:
    'Maps between the unit sphere and a [cos(theta), phi] parameterization.'

    def bounds(self):
        return mi.ScalarBoundingBox2f([-dr.pi, -1], [dr.pi, 1])

    def aspect(self):
        return 2

    def map_forward(self, p):
        sin_theta = p.y
        cos_theta = dr.safe_sqrt(dr.fma(-sin_theta, sin_theta, 1))
        sin_phi, cos_phi = dr.sincos(p.x)

        return mi.Vector3f(
            sin_theta,
            cos_theta * cos_phi,
            cos_theta * sin_phi
        )

    def map_backward(self, p):
        return mi.Vector2f(dr.atan2(p.z, p.y), p.x)


def BSDFAdapterUV(bsdf_type, extra, u = 0.5, wi=[0, 0, 1], ctx=None):
    if ctx is None:
        ctx = mi.BSDFContext()

    def make_context(n):
        si = dr.zeros(mi.SurfaceInteraction3f, n)
        si.p  = [0, 0, 0]
        si.n  = [0, 0, 1]
        si.sh_frame = mi.Frame3f(si.n)
        si.uv = [0, u]
        si.wi = wi
        return (si, ctx)

    def instantiate(args):
        xml = """<bsdf version="3.0.0" type="%s">
            %s
        </bsdf>""" % (bsdf_type, extra)
        return mi.load_string(xml % args)

    def sample_functor(sample, *args):
        n = dr.width(sample)
        plugin = instantiate(args)
        (si, ctx) = make_context(n)
        bs, weight = plugin.sample(ctx, si, sample[0], [sample[1], sample[2]])

        w = dr.full(mi.Float, 1.0, dr.width(weight))
        w[dr.all(dr.eq(weight, 0))] = 0
        
        return bs.wo, w

    def pdf_functor(wo, *args):
        n = dr.width(wo)
        plugin = instantiate(args)
        (si, ctx) = make_context(n)
        return plugin.pdf(ctx, si, wo)

    return sample_functor, pdf_functor


