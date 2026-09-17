import pytest
import drjit as dr
import mitsuba as mi
import numpy as np

def test01_trampoline_id(variants_vec_backends_once_rgb):
    class DummyIntegrator(mi.SamplingIntegrator):
        def __init__(self, props):
            mi.SamplingIntegrator.__init__(self, props)
            self.depth = props['depth']

        def traverse(self, cb):
            cb.put('depth', self.depth, mi.ParamFlags.NonDifferentiable)

    mi.register_integrator('dummy_integrator', DummyIntegrator)

    scene_description = mi.cornell_box()
    del scene_description['integrator']
    scene_description['my_integrator'] = {
        'type': 'dummy_integrator',
        'depth': 3.1
    }
    scene = mi.load_dict(scene_description)

    params = mi.traverse(scene)
    assert 'my_integrator.depth' in params


def test02_path_directly_visible(variants_all_rgb):
    def render(hide_emitters):
        scene_description = mi.cornell_box()
        # Look only at light
        scene_description['sensor']['film']['crop_offset_x'] = 124
        scene_description['sensor']['film']['crop_offset_y'] = 36
        scene_description['sensor']['film']['crop_width'] = 1
        scene_description['sensor']['film']['crop_height'] = 1
        # The deprecated hide_emitters flag only takes effect when the
        # integrator is part of the scene
        scene_description['integrator'] = {
            'type': 'path',
            'max_depth': 1,
            'hide_emitters': hide_emitters,
        }
        return mi.render(mi.load_dict(scene_description))

    # Should only see the emitter contribution
    img = render(hide_emitters=False)
    assert dr.allclose(img.array, [18.387, 13.9873, 6.75357])

    # Should be completely black (we ignore the emitter)
    img = render(hide_emitters=True)
    assert dr.allclose(img.array, 0)


# ---------------------------------------------------------------------------
# Visibility flags and surfaces with null transmission
# ---------------------------------------------------------------------------

def floor_light_scene(window=None, integrator='path', spp=256,
                      camera=([0, 0, -4], [0, 0, 0], [0, 1, 0]), res=32,
                      max_depth=6):
    """A diffuse floor at y=-1 lit by an area light at y=1.5. ``window`` adds
    a rectangle with the given properties at y=0.5 between them. The direct
    integrators ignore ``max_depth``."""
    T = mi.ScalarTransform4f
    integrator = {'type': integrator}
    if not integrator['type'].startswith('direct'):
        integrator['max_depth'] = max_depth
    d = {
        'type': 'scene',
        'integrator': integrator,
        'sensor': {
            'type': 'perspective',
            'fov': 40,
            'to_world': T().look_at(*camera),
            'film': {'type': 'hdrfilm', 'width': res, 'height': res,
                     'rfilter': {'type': 'box'}},
            'sampler': {'type': 'independent', 'sample_count': spp},
        },
        'floor': {
            'type': 'rectangle',
            'to_world': T().translate([0, -1, 0]) @ T().rotate([1, 0, 0], -90) @ T().scale(3),
            'bsdf': {'type': 'diffuse', 'reflectance': 0.5},
        },
        'light': {
            'type': 'rectangle',
            'to_world': T().translate([0, 1.5, 0]) @ T().rotate([1, 0, 0], 90) @ T().scale(0.5),
            'emitter': {'type': 'area', 'radiance': 10.0},
        },
    }
    if window is not None:
        d['window'] = {
            'type': 'rectangle',
            'to_world': T().translate([0, 0.5, 0]) @ T().rotate([1, 0, 0], 90) @ T().scale(2),
            **window,
        }
    return mi.load_dict(d)


_mean_render_cache = {}

def mean_render(**kwargs):
    """Mean pixel value of a rendering of ``floor_light_scene(**kwargs)``,
    cached per variant so that reference renders are shared"""
    key = (mi.variant(), repr(sorted(kwargs.items())))
    if key not in _mean_render_cache:
        img = mi.render(floor_light_scene(**kwargs))
        _mean_render_cache[key] = np.array(img).mean()
    return _mean_render_cache[key]


def rect_at(z, bsdf, flip=False, scale=1.0, **extra):
    """A rectangle in the plane z=``z`` facing +z, or -z with ``flip``"""
    T = mi.ScalarTransform4f
    to_world = T().translate([0, 0, z])
    if flip:
        to_world = to_world @ T().rotate([1, 0, 0], 180)
    to_world = to_world @ T().scale(scale)
    return dict(type='rectangle', to_world=to_world, bsdf=bsdf, **extra)


def estimate(shapes, max_depth=6, spp=1, integrator='path', radiance=1.0, **env):
    """Mean radiance of the +z ray from the origin under a constant
    environment with the given radiance and extra properties ``env``. The
    integrator is named by a string or described by a dictionary, and the
    direct integrator ignores ``max_depth``."""
    integrator = dict(type=integrator) if isinstance(integrator, str) \
                 else dict(integrator)
    if integrator['type'] != 'direct':
        integrator['max_depth'] = max_depth
    scene = mi.load_dict(dict(
        type='scene', integrator=integrator,
        env=dict(type='constant', radiance=radiance, **env), **shapes))
    sampler = mi.load_dict(dict(type='independent'))
    ray = mi.Ray3f([0, 0, 0], [0, 0, 1])

    def run(seed, count):
        sampler.seed(seed, count)
        spec = scene.integrator().sample(scene, sampler, ray, None, True)[0]
        return np.array(mi.unpolarized_spectrum(spec)).mean()

    if mi.variant().startswith('scalar'):
        return np.mean([run(seed, 1) for seed in range(spp)])
    return run(0, spp)


NEUTRAL = dict(type='mask', opacity=0.0, material=dict(type='diffuse'))
WINDOW_BSDFS = [
    {'type': 'mask', 'opacity': 0.3, 'material': {'type': 'diffuse', 'reflectance': 0.2}},
    {'type': 'thindielectric'},
    {'type': 'null'},
]
WINDOW_IDS = ['mask', 'thindielectric', 'null']
BELOW = ([0, -0.5, 0], [0, -1, 0], [0, 0, 1])
PYTHON_INTEGRATORS = ('prb', 'prb_basic', 'prb_projective', 'prbvolpath',
                      'direct_projective')
UNPOLARIZED_INTEGRATORS = ('volpathmis', 'prbvolpath')


@pytest.mark.parametrize('integrator, reference, camera, ref_depth', [
    ('path', 'volpath', None, 6),
    ('direct', 'path', BELOW, 2),
    ('volpath', 'path', None, 6),
    ('volpathmis', 'path', None, 6),
    ('prb', 'path', None, 6),
    ('prb_basic', 'path', None, 6),
    ('prb_projective', 'path', None, 6),
    ('prbvolpath', 'path', None, 6),
    ('direct_projective', 'path', BELOW, 2),
], ids=['path', 'direct', 'volpath', 'volpathmis', 'prb', 'prb_basic',
        'prb_projective', 'prbvolpath', 'direct_projective'])
@pytest.mark.parametrize('bsdf', WINDOW_BSDFS, ids=WINDOW_IDS)
def test03_null_consistency(variants_all_rgb, integrator, reference, camera,
                            ref_depth, bsdf):
    """Every integrator agrees with a reference integrator on a scene with a
    window of null transmission between the light and the floor. Emitter
    sampling sees through the window, BSDF-sampled rays cross it with the
    same transmittance, and crossings do not count as path vertices. The
    direct integrators are compared against a path tracer of depth two,
    with the camera below the window."""
    if integrator in PYTHON_INTEGRATORS and '_ad_' not in mi.variant():
        pytest.skip('Python integrators require an AD variant')
    if integrator in UNPOLARIZED_INTEGRATORS and mi.is_polarized:
        pytest.skip('Integrator does not support polarized variants')
    kwargs = dict(window=dict(bsdf=bsdf))
    if camera is not None:
        kwargs['camera'] = camera
    ref = mean_render(integrator=reference, max_depth=ref_depth, **kwargs)
    img = mean_render(integrator=integrator, **kwargs)
    assert abs(img - ref) < 0.03 * ref


@pytest.mark.parametrize('bsdf', [
    {'type': 'null'},
    {'type': 'normalmap', 'normalmap': {'type': 'rgb', 'value': [0.5, 0.5, 1.0]},
     'bsdf': {'type': 'null'}},
], ids=['null', 'normalmap'])
def test04_path_null_neutral(variants_all_rgb, bsdf):
    """A shape with the 'null' BSDF does not change the image, also when the
    BSDF is wrapped in a flat normal map."""
    ref = mean_render()
    assert abs(mean_render(window=dict(bsdf=bsdf)) - ref) < 0.03 * ref


def test05_path_null_depth_boundary(variants_all_rgb):
    """Paths that exhausted their depth budget still continue through
    surfaces with null transmission, matching emitter sampling. The
    BSDF-sampled continuation of a diffuse floor crosses a neutral sheet
    even at max_depth=2."""
    assert np.allclose(estimate({}, 1), 1.0)
    assert np.allclose(estimate(dict(w=rect_at(1, NEUTRAL)), 1), 1.0)

    floor = rect_at(1, dict(type='diffuse', reflectance=0.5), flip=True)
    assert abs(estimate(dict(floor=floor), 2, spp=4096) - 0.5) < 0.025
    assert abs(estimate(dict(floor=floor, sheet=rect_at(0.5, NEUTRAL)), 2,
                        spp=4096) - 0.5) < 0.025


def test06_path_null_hidden_emitter(variants_all_rgb):
    """Segments before the first vertex remain camera rays: an emitter
    hidden from primary rays behind a null surface neither emits nor
    occludes."""
    hidden = rect_at(2, dict(type='diffuse', reflectance=0.0),
                     visibility='secondary',
                     emitter=dict(type='area', radiance=1.0))
    assert np.allclose(estimate(dict(light=hidden), 2), 1.0)
    assert np.allclose(estimate(dict(light=hidden, w=rect_at(1, NEUTRAL)), 2), 1.0)


VISIBLE = {'all': (True, True), 'primary': (True, False),
           'secondary': (False, True), 'hidden': (False, False)}

@pytest.mark.parametrize('kind', ['blocker', 'area', 'constant'])
@pytest.mark.parametrize('visibility', list(VISIBLE))
def test07_visibility(variants_all_rgb, visibility, kind):
    """The visibility property hides a shape or emitter from primary rays,
    from secondary rays, or from both. A hidden emitter is not seen or does
    not illuminate, and a hidden blocker does not appear or does not cast a
    shadow, each as prescribed by the four values."""
    seen, secondary = VISIBLE[visibility]
    black = dict(type='diffuse', reflectance=0.0)
    floor = dict(floor=rect_at(1, dict(type='diffuse', reflectance=0.5),
                               flip=True))

    if kind == 'constant':
        assert estimate({}, 1, visibility=visibility) == (1.0 if seen else 0.0)
        lit = estimate(floor, 4, spp=4096, visibility=visibility)
        if secondary:
            assert abs(lit - 0.5) < 0.05
        else:
            assert lit == 0.0

    elif kind == 'area':
        # A light in front of the camera, then a light behind it
        front = dict(light=rect_at(1, black, flip=True, visibility=visibility,
                                   emitter=dict(type='area', radiance=2.0)))
        assert estimate(front, 1) == (2.0 if seen else 1.0)

        def lit(visibility):
            back = rect_at(-1, black, scale=2, visibility=visibility,
                           emitter=dict(type='area', radiance=2.0))
            return estimate(dict(light=back, **floor), 2, spp=4096,
                            radiance=0.0)
        ref = lit('all')
        assert ref > 0.1
        if secondary:
            assert abs(lit(visibility) - ref) < 0.05 * ref
        else:
            assert lit(visibility) == 0.0

    else:
        blocker = rect_at(-1, black, scale=3, visibility=visibility)
        assert estimate(dict(b=rect_at(1, black, flip=True,
                                       visibility=visibility)), 1) \
            == (0.0 if seen else 1.0)
        shadowed = estimate(dict(b=blocker, **floor), 4, spp=4096)
        if secondary:
            ref = estimate(dict(b=rect_at(-1, black, scale=3), **floor), 4,
                           spp=4096)
            assert 0.5 - ref > 0.1
            assert abs(shadowed - ref) < 0.03
        else:
            assert abs(shadowed - 0.5) < 0.03


def test08_direct_null_traversal(variants_all_rgb):
    """A camera ray of the direct integrator that hits a surface with null
    transmission still finds emitters behind it, and BSDF-sampled rays cross
    such surfaces with the same transmittance as shadow rays. With a diffuse
    floor and a half-transparent sheet behind the camera, an inconsistency
    between the two strategies would show up in the MIS weights."""
    kwargs = dict(integrator='direct')

    assert np.allclose(estimate({}, **kwargs), 1.0)
    assert np.allclose(estimate(dict(w=rect_at(1, NEUTRAL)), **kwargs), 1.0)
    glass = rect_at(1, dict(type='thindielectric'))
    assert abs(estimate(dict(w=glass), spp=4096, **kwargs) - 1.0) < 0.025

    floor = rect_at(1, dict(type='diffuse', reflectance=0.5), flip=True)
    half = dict(type='mask', opacity=0.5,
                material=dict(type='diffuse', reflectance=0.0))
    sheet = rect_at(-1, half, scale=100)
    assert abs(estimate(dict(floor=floor), spp=4096, **kwargs) - 0.5) < 0.025
    assert abs(estimate(dict(floor=floor, sheet=sheet), spp=4096, **kwargs)
               - 0.25) < 0.0125


# ---------------------------------------------------------------------------
# Visibility classes across specular events
# ---------------------------------------------------------------------------

def pane_scene(bsdf, integrator, slab=False):
    """Red and blue radiance seen by a camera at z=-1 that looks along +z at
    a pane of primary visibility at z=1, or at a slab with an exit face at
    z=1.2. Primary-only red backdrops face the camera from z=3 and z=-2, and
    secondary-only blue lights sit in front of them at z=2 and z=-1.5."""
    T = mi.ScalarTransform4f
    integ = dict(type=integrator)
    if not integrator.startswith('direct'):
        integ['max_depth'] = 3
    d = dict(type='scene', integrator=integ, sensor=dict(
        type='perspective', fov=5,
        to_world=T().look_at([0, 0, -1], [0, 0, 0], [0, 1, 0]),
        film=dict(type='hdrfilm', width=1, height=1, rfilter=dict(type='box')),
        sampler=dict(type='independent', sample_count=32)))
    d['entry'] = rect_at(1, bsdf, flip=True, visibility='primary')
    if slab:
        d['exit'] = rect_at(1.2, bsdf, visibility='primary')
    black = dict(type='diffuse', reflectance=0.0)
    for i, (z, visibility, color) in enumerate([
            (3, 'primary', [1, 0, 0]), (2, 'secondary', [0, 0, 1]),
            (-1.5, 'secondary', [0, 0, 1]), (-2, 'primary', [1, 0, 0])]):
        d['emitter_%i' % i] = rect_at(
            z, black, flip=z > 0, scale=4, visibility=visibility,
            emitter=dict(type='area', radiance=dict(type='rgb', value=color)))
    return np.array(mi.render(mi.load_dict(d)))[0, 0, [0, 2]]


@pytest.mark.parametrize('integrator', [
    'path', 'direct', 'volpath', 'volpathmis', 'prb', 'prb_basic',
    'prb_projective', 'prbvolpath', 'direct_projective'])
def test10_specular_visibility_class(variants_all_rgb, integrator):
    """Refraction through glass and reflection off a mirror keep the camera
    ray primary, so it sees only the red backdrops. This also holds across
    both faces of a slab, whose exit face only primary rays can hit. A rough
    or diffuse pane turns the path into a secondary ray that sees only the
    blue lights. The direct integrators cannot reach past the exit face."""
    if integrator in PYTHON_INTEGRATORS and '_ad_' not in mi.variant():
        pytest.skip('Python integrators require an AD variant')
    if integrator in UNPOLARIZED_INTEGRATORS and mi.is_polarized:
        pytest.skip('Integrator does not support polarized variants')

    glass = dict(type='dielectric')
    cases = [
        (glass, False, True),
        (dict(type='conductor', material='none'), False, True),
        (dict(type='roughdielectric', alpha=0.01), False, False),
        (dict(type='diffuse', reflectance=0.5), False, False),
    ]
    if not integrator.startswith('direct'):
        cases.append((glass, True, True))

    for bsdf, slab, primary in cases:
        red, blue = pane_scene(bsdf, integrator, slab)
        assert (red > 0, blue > 0) == (primary, not primary), (bsdf, slab)


# ---------------------------------------------------------------------------
# Clamping of individual path contributions
# ---------------------------------------------------------------------------

@pytest.mark.parametrize('integrator', ['path', 'volpath', 'volpathmis'])
def test11_clamp(variants_vec_backends_once_rgb, integrator):
    def run(shapes, max_depth, spp=1, radiance=1.0, **clamp):
        return estimate(shapes, max_depth=max_depth, spp=spp, radiance=radiance,
                        integrator=dict(type=integrator, **clamp))

    # Emission seen by the camera ray is a direct contribution. The limit
    # bounds the mean channel value, so a purely red environment of mean
    # radiance 4 is scaled as a whole instead of being clipped per channel.
    red = {'type': 'rgb', 'value': [12, 0, 0]}
    assert run({}, 1, radiance=red) == 4
    assert run({}, 1, radiance=red, clamp_indirect=2) == 4
    assert run({}, 1, radiance=red, clamp_direct=2) == 2

    # Two diffuse panes facing each other. The near one receives direct light
    # only, the far one is reached after an extra scattering event.
    panes = dict(near=rect_at(1, dict(type='diffuse'), flip=True),
                 far=rect_at(-1, dict(type='diffuse')))
    ref_2, ref_4 = (run(panes, d, spp=1024) for d in (2, 4))
    assert run(panes, 2, spp=1024, clamp_indirect=1e-3) == ref_2
    assert run(panes, 4, spp=1024, clamp_indirect=1e6) == ref_4
    assert run(panes, 4, spp=1024, clamp_indirect=1e-3) < 0.95 * ref_4
