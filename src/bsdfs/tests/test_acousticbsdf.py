# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
import pytest
import drjit as dr
import misuka as mi
mi.set_log_level(mi.LogLevel.Info)

def test01_create(variants_all_acoustic):
    bsdf = mi.load_dict({
        'type': 'acousticbsdf',
        'scattering' : 0.2,
        'absorption' : 0.5,
        'specular_lobe_width'  : 0.13,
    })

    assert bsdf is not None
    assert bsdf.component_count() == 2
    assert bsdf.flags(0) == mi.BSDFFlags.GlossyReflection  | mi.BSDFFlags.FrontSide
    assert bsdf.flags(1) == mi.BSDFFlags.DiffuseReflection | mi.BSDFFlags.FrontSide
    assert bsdf.flags( ) == mi.BSDFFlags.GlossyReflection  | mi.BSDFFlags.DiffuseReflection | mi.BSDFFlags.FrontSide

def test02_traverse(variants_all_acoustic):
    bsdf = mi.load_dict({
        'type': 'acousticbsdf',
        'scattering' : 0.2,
        'absorption' : 0.5,
        'specular_lobe_width'  : 0.3,
    })

    params = mi.traverse(bsdf)

    assert 'scattering.value' in params.keys()
    assert 'absorption.value' in params.keys()
    assert 'specular_lobe_width'        in params.keys()

@pytest.mark.parametrize("idx", [0, 1, 2, 3])
def test03_eval_pdf_all(variants_all_acoustic, idx):
    ''' Compare AcousticBSDF evaluation against a BlendBSDF composed of
    a RoughConductor and a Diffuse BSDF. Test eval(), pdf() and eval_pdf().'''

    frequencies       = [500, 1000, 2000, 4000]
    absorption_values = [0.1,  0.9,  0.1,  0.9]
    scattering_values = [0.1,  0.1,  0.9,  0.9]
    specular_lobe_width = 0.3

    bsdf = mi.load_dict({
        'type': 'acousticbsdf',
        'absorption' : {
            'type': 'spectrum',
            'value': [(frequencies[i], absorption_values[i]) for i in range(len(frequencies))],
        },
        'scattering' : {
            'type': 'spectrum',
            'value': [(frequencies[i], scattering_values[i]) for i in range(len(frequencies))],
        },
        'specular_lobe_width' : specular_lobe_width,
    })

    blendbsdf = mi.load_dict({
        'type': 'blendbsdf',
        'weight': scattering_values[idx], # spectrally varying weight not supported
        'bsdf_0': {
            'type': 'roughconductor',
            'alpha': specular_lobe_width,
            'specular_reflectance': {
                'type': 'spectrum',
                'value': [(frequencies[i], 1-absorption_values[i]) for i in range(len(frequencies))],
            },
        },
        'bsdf_1': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'spectrum',
                'value': [(frequencies[i], 1-absorption_values[i]) for i in range(len(frequencies))],
            }
        },
    })


    si = mi.SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.wi = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.wavelengths = [frequencies[idx]]
    ctx = mi.BSDFContext()

    for i in range(20):
        theta = i / 19.0 * (dr.pi / 2)
        wo = [dr.sin(theta), 0, dr.cos(theta)]
        assert dr.allclose(bsdf.eval(ctx, si, wo), blendbsdf.eval(ctx, si, wo))
        assert dr.allclose(bsdf.pdf(ctx, si, wo), blendbsdf.pdf(ctx, si, wo))

        v_pdf  = bsdf.pdf(ctx, si, wo=wo)
        v_eval = bsdf.eval(ctx, si, wo=wo)[0]
        v_eval_pdf = bsdf.eval_pdf(ctx, si, wo=wo)
        assert dr.allclose(v_eval, v_eval_pdf[0])
        assert dr.allclose(v_pdf, v_eval_pdf[1])


@pytest.mark.parametrize("idx", [0, 1, 2, 3])
def test03_eval_pdf_components(variants_all_acoustic, idx):
    '''Same as above but testing using individual components.'''

    frequencies       = [500, 1000, 2000, 4000]
    absorption_values = [0.1,  0.9,  0.1,  0.9]
    scattering_values = [0.1,  0.1,  0.9,  0.9]
    specular_lobe_width = 0.1

    bsdf = mi.load_dict({
        'type': 'acousticbsdf',
        'absorption' : {
            'type': 'spectrum',
            'value': [(frequencies[i], absorption_values[i]) for i in range(len(frequencies))],
        },
        'scattering' : {
            'type': 'spectrum',
            'value': [(frequencies[i], scattering_values[i]) for i in range(len(frequencies))],
        },
        'specular_lobe_width' : specular_lobe_width,
    })

    blendbsdf = mi.load_dict({
        'type': 'blendbsdf',
        'weight': scattering_values[idx], # spectrally varying weight not supported
        'bsdf_0': {
            'type': 'roughconductor',
            'alpha': specular_lobe_width,
            'specular_reflectance': {
                'type': 'spectrum',
                'value': [(frequencies[i], 1-absorption_values[i]) for i in range(len(frequencies))],
            },
        },
        'bsdf_1': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'spectrum',
                'value': [(frequencies[i], 1-absorption_values[i]) for i in range(len(frequencies))],
            }
        },
    })


    si = mi.SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.wi = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.wavelengths = [frequencies[idx]]
    ctx = mi.BSDFContext()

    for i in range(20):
        theta = i / 19.0 * (dr.pi / 2)

        ctx.component = 0

        wo = [dr.sin(theta), 0, dr.cos(theta)]
        assert dr.allclose(bsdf.eval(ctx, si, wo), blendbsdf.eval(ctx, si, wo))
        assert dr.allclose(bsdf.pdf(ctx, si, wo), blendbsdf.pdf(ctx, si, wo))

        v_pdf  = bsdf.pdf(ctx, si, wo=wo)
        v_eval = bsdf.eval(ctx, si, wo=wo)[0]
        v_eval_pdf = bsdf.eval_pdf(ctx, si, wo=wo)
        assert dr.allclose(v_eval, v_eval_pdf[0])
        assert dr.allclose(v_pdf, v_eval_pdf[1])

        ctx.component = 1

        wo = [dr.sin(theta), 0, dr.cos(theta)]
        assert dr.allclose(bsdf.eval(ctx, si, wo), blendbsdf.eval(ctx, si, wo))
        assert dr.allclose(bsdf.pdf(ctx, si, wo), blendbsdf.pdf(ctx, si, wo))

        v_pdf  = bsdf.pdf(ctx, si, wo=wo)
        v_eval = bsdf.eval(ctx, si, wo=wo)[0]
        v_eval_pdf = bsdf.eval_pdf(ctx, si, wo=wo)
        assert dr.allclose(v_eval, v_eval_pdf[0])
        assert dr.allclose(v_pdf, v_eval_pdf[1])




@pytest.mark.parametrize("idx", [0])
def test_sample_all(variants_all_acoustic, idx):
    ''' Test sample() against a BlendBSDF using both components.'''

    frequencies       = [500, 1000, 2000, 4000]
    absorption_values = [0.1,  0.9,  0.1,  0.9]
    scattering_values = [0.1,  0.1,  0.9,  0.9]
    specular_lobe_width = 0.1

    bsdf = mi.load_dict({
        'type': 'acousticbsdf',
        'absorption' : {
            'type': 'spectrum',
            'value': [(frequencies[i], absorption_values[i]) for i in range(len(frequencies))],
        },
        'scattering' : {
            'type': 'spectrum',
            'value': [(frequencies[i], scattering_values[i]) for i in range(len(frequencies))],
        },
        'specular_lobe_width' : specular_lobe_width,
    })

    blendbsdf = mi.load_dict({
        'type': 'blendbsdf',
        'weight': scattering_values[idx], # spectrally varying weight not supported
        'bsdf_0': {
            'type': 'roughconductor',
            'alpha': specular_lobe_width,
            'specular_reflectance': {
                'type': 'spectrum',
                'value': [(frequencies[i], 1-absorption_values[i]) for i in range(len(frequencies))],
            },
        },
        'bsdf_1': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'spectrum',
                'value': [(frequencies[i], 1-absorption_values[i]) for i in range(len(frequencies))],
            }
        },
    })


    si = mi.SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.wi = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.wavelengths = [frequencies[idx]]
    ctx = mi.BSDFContext()

    s1 = 0.5
    s2 = 0.6521
    bsdfsample1, result1 =      bsdf.sample(ctx, si, s1, s2)
    bsdfsample2, result2 = blendbsdf.sample(ctx, si, s1, s2)
    assert dr.allclose(bsdfsample1.wo, bsdfsample2.wo)
    assert dr.allclose(result1, result2)


@pytest.mark.parametrize("idx", [0])
def test_sample_components(variants_all_acoustic, idx):
    ''' Test sample() against a BlendBSDF using individual components.'''

    frequencies       = [500, 1000, 2000, 4000]
    absorption_values = [0.1,  0.9,  0.1,  0.9]
    scattering_values = [0.1,  0.1,  0.9,  0.9]
    specular_lobe_width = 0.1

    bsdf = mi.load_dict({
        'type': 'acousticbsdf',
        'absorption' : {
            'type': 'spectrum',
            'value': [(frequencies[i], absorption_values[i]) for i in range(len(frequencies))],
        },
        'scattering' : {
            'type': 'spectrum',
            'value': [(frequencies[i], scattering_values[i]) for i in range(len(frequencies))],
        },
        'specular_lobe_width' : specular_lobe_width,
    })

    blendbsdf = mi.load_dict({
        'type': 'blendbsdf',
        'weight': scattering_values[idx], # spectrally varying weight not supported
        'bsdf_0': {
            'type': 'roughconductor',
            'alpha': specular_lobe_width,
            'specular_reflectance': {
                'type': 'spectrum',
                'value': [(frequencies[i], 1-absorption_values[i]) for i in range(len(frequencies))],
            },
        },
        'bsdf_1': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'spectrum',
                'value': [(frequencies[i], 1-absorption_values[i]) for i in range(len(frequencies))],
            }
        },
    })


    si = mi.SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.wi = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.wavelengths = [frequencies[idx]]
    ctx = mi.BSDFContext()

    s1 = 0.5
    s2 = 0.6521

    ctx.component = 0
    bsdfsample1, result1 =      bsdf.sample(ctx, si, s1, s2)
    bsdfsample2, result2 = blendbsdf.sample(ctx, si, s1, s2)
    assert dr.allclose(bsdfsample1.wo, bsdfsample2.wo)
    assert dr.allclose(result1, result2)

    ctx.component = 1
    bsdfsample1, result1 =      bsdf.sample(ctx, si, s1, s2)
    bsdfsample2, result2 = blendbsdf.sample(ctx, si, s1, s2)
    assert dr.allclose(bsdfsample1.wo, bsdfsample2.wo)
    assert dr.allclose(result1, result2)

@pytest.mark.parametrize("idx", [0, 1, 2])
def test_05_validate_spectra(variants_all_acoustic, idx):
    '''validate that results with irregular, regular and uniform spectra are equal.'''

    frequencies       = [100,    1000,  20100]
    absorption_values = [0.123,  0.123, 0.123] #uniform absorption
    scattering_values = [0.456,  0.456, 0.456] #uniform scattering
    specular_lobe_width = 0.1

    bsdf_irregular = mi.load_dict({
        'type': 'acousticbsdf',
        'absorption' : {
            'type': 'spectrum',
            'value': [(frequencies[i], absorption_values[i]) for i in range(len(frequencies))],
        },
        'scattering' : {
            'type': 'spectrum',
            'value': [(frequencies[i], scattering_values[i]) for i in range(len(frequencies))],
        },
        'specular_lobe_width' : specular_lobe_width,
    })

    bsdf_regular = mi.load_dict({
        'type': 'acousticbsdf',
        'absorption' : {
            'type': 'spectrum',
            'value': [(frequencies[i], absorption_values[i]) for i in [0, len(frequencies)-1]],
        },
        'scattering' : {
            'type': 'spectrum',
            'value': [(frequencies[i], scattering_values[i]) for i in [0, len(frequencies)-1]],
        },
        'specular_lobe_width' : specular_lobe_width,
    })

    bsdf_uniform = mi.load_dict({
        'type': 'acousticbsdf',
        'absorption' : {
            'type': 'spectrum',
            'value': absorption_values[0],
        },
        'scattering' : {
            'type': 'spectrum',
            'value': scattering_values[0],
        },
        'specular_lobe_width' : specular_lobe_width,
    })

    # print()
    # print(f'bsdf_irregular = \n{bsdf_irregular}')
    # print(f'bsdf_regular = \n{bsdf_regular}')
    # print(f'bsdf_uniform = \n{bsdf_uniform}')

    si = mi.SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.n = [0, 0, 1]
    si.wi = [0, 0, 1]
    si.sh_frame = mi.Frame3f(si.n)
    si.wavelengths = [frequencies[idx]]
    ctx = mi.BSDFContext()

    # test sample
    s1 = 0.5
    s2 = 0.6521
    bsdfsample1, result1 = bsdf_irregular.sample(ctx, si, s1, s2)
    bsdfsample2, result2 = bsdf_regular.sample(ctx, si, s1, s2)
    bsdfsample3, result3 = bsdf_uniform.sample(ctx, si, s1, s2)
    assert dr.allclose(bsdfsample1.wo, bsdfsample2.wo)
    assert dr.allclose(result1, result2)
    assert dr.allclose(bsdfsample1.wo, bsdfsample3.wo)
    assert dr.allclose(result1, result3)

    # test eval and pdf consistency
    N_angles = 3
    for i in range(N_angles):
        theta = i / (N_angles - 1) * (dr.pi / 2)
        print(f'\ntheta = {theta/dr.pi*180:.1f} deg')

        wo = [dr.sin(theta), 0, dr.cos(theta)]

        eval_irregular = bsdf_irregular.eval(ctx, si, wo)
        eval_regular = bsdf_regular.eval(ctx, si, wo)
        eval_uniform = bsdf_uniform.eval(ctx, si, wo)

        print(f'eval_irregular = {eval_irregular}')
        print(f'eval_regular   = {eval_regular}')
        print(f'eval_uniform   = {eval_uniform}\n')

        pdf_irregular = bsdf_irregular.pdf(ctx, si, wo)
        pdf_regular = bsdf_regular.pdf(ctx, si, wo)
        pdf_uniform = bsdf_uniform.pdf(ctx, si, wo)
        print(f'pdf_irregular  = {pdf_irregular}')
        print(f'pdf_regular    = {pdf_regular}')
        print(f'pdf_uniform    = {pdf_uniform}')

        assert dr.allclose(eval_irregular, eval_regular)
        assert dr.allclose(eval_irregular, eval_uniform)
        assert dr.allclose(pdf_irregular, pdf_regular)
        assert dr.allclose(pdf_irregular, pdf_uniform)