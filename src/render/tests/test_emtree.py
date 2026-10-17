import pytest
import drjit as dr
import mitsuba as mi
import numpy as np


def many_lights(rng, n_point=20, n_area=8, n_spot=4, infinite=True, extra=None):
    """A scene with a mix of finite and infinite emitters"""
    d = {'type': 'scene'}
    for i in range(n_point):
        d[f'point_{i}'] = {
            'type': 'point',
            'position': rng.uniform(-5, 5, 3).tolist(),
            'intensity': float(rng.uniform(0.1, 5))
        }
    for i in range(n_area):
        d[f'area_{i}'] = {
            'type': 'rectangle',
            'to_world': mi.ScalarTransform4f()
                .translate(rng.uniform(-5, 5, 3).tolist())
                .rotate([1, 0, 0], float(rng.uniform(0, 180)))
                .rotate([0, 1, 0], float(rng.uniform(0, 180))),
            'emitter': {'type': 'area', 'radiance': float(rng.uniform(0.1, 5))}
        }
    for i in range(n_spot):
        d[f'spot_{i}'] = {
            'type': 'spot',
            'to_world': mi.ScalarTransform4f().look_at(
                rng.uniform(-5, 5, 3).tolist(), [0, 0, 0], [0, 0, 1]),
            'intensity': float(rng.uniform(1, 5)),
            'cutoff_angle': float(rng.uniform(10, 60))
        }
    if infinite:
        d['env'] = {'type': 'constant', 'radiance': 0.3}
        d['sun'] = {'type': 'directional', 'direction': [0.3, 0.2, -1],
                    'irradiance': 2.0}
    if extra:
        d.update(extra)
    return d


def receivers(rng, n):
    """Random reference points, a quarter of them without a normal"""
    p = rng.uniform(-6, 6, (n, 3)).astype(np.float32)
    nrm = rng.normal(size=(n, 3)).astype(np.float32)
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True)
    nrm[: n // 4] = 0
    ref = dr.zeros(mi.Interaction3f, n if dr.is_jit_v(mi.Float) else 1)
    ref.p = mi.Point3f(p.T if dr.is_jit_v(mi.Float) else p[0])
    ref.n = mi.Normal3f(nrm.T if dr.is_jit_v(mi.Float) else nrm[0])
    return ref


def picked(ds):
    """The emitter of a single sample"""
    return ds.emitter[0] if dr.is_jit_v(mi.Float) else ds.emitter


def check_pdf_consistency(scene, ref, sample, two_sided):
    """The replayed selection probability matches the sampled one, and
    inactive lanes yield a zero density (scalar variants ignore masks)"""
    ds, spec = scene.sample_emitter_direction(ref, sample, False, two_sided)
    spec = mi.unpolarized_spectrum(spec)
    dr.assert_true(dr.all(dr.isfinite(ds.pdf) & (ds.pdf >= 0) & dr.isfinite(spec.x)))
    # Area emitters keep a nonzero ds.pdf for back-facing samples whose
    # weight is zero, so only samples with a weight take part
    if not dr.is_jit_v(mi.Float):
        if ds.emitter is None:
            return ds, spec
        pdf = scene.pdf_emitter_direction(ref, ds, two_sided)
        if not ds.delta and dr.any(spec != 0):
            assert abs(pdf - ds.pdf) <= 1e-4 * ds.pdf
        return ds, spec
    pdf = scene.pdf_emitter_direction(ref, ds, two_sided)
    valid = ~ds.delta & dr.any(spec != 0)
    dr.assert_true(dr.all(~valid | (dr.abs(pdf - ds.pdf) <= 1e-4 * ds.pdf)))
    dr.assert_true(dr.all(scene.pdf_emitter_direction(ref, ds, two_sided, False) == 0))
    return ds, spec


@pytest.mark.parametrize('kind', ['mixed', 'partial_groups', 'deep'])
@pytest.mark.parametrize('two_sided', [False, True])
def test01_pdf_consistency(variants_all_backends_once, kind, two_sided):
    """Sampling and PDF replay agree on a tree with infinite and hidden
    emitters, on partially filled groups, and on leaves deeper than a
    balanced tree"""
    rng = np.random.default_rng(0)
    if kind == 'mixed':
        hidden = {'hidden': {'type': 'point', 'sampling_weight': 0.0}}
        scene = mi.load_dict(many_lights(rng, extra=hidden))
        n_tree = 32
    elif kind == 'partial_groups':
        scene = mi.load_dict(many_lights(rng, n_point=0, n_area=67, n_spot=0,
                                         infinite=False))
        n_tree = 67
    else:
        positions = np.geomspace(1, 1e6, 513)
        scene = mi.load_dict({
            'type': 'scene',
            **{f'light{i}': {'type': 'rectangle',
                             'to_world': mi.ScalarTransform4f().translate([float(x), 0, 0]),
                             'emitter': {'type': 'area'}}
               for i, x in enumerate(positions)},
        })
        n_tree = 513

    emitters = scene.emitters()
    codes = [e.emitter_tree_code() for e in emitters]
    in_tree = [c for c in codes if c != 0]
    assert len(in_tree) == n_tree and len(set(in_tree)) == n_tree
    assert all(c < 0x80000000 for c in in_tree)
    if kind == 'mixed':
        hidden = [i for i, e in enumerate(emitters) if e.id() == 'hidden']
        assert len(hidden) == 1 and codes[hidden[0]] == 0
    elif kind == 'deep':
        # Some leaves lie deeper than the four levels of a balanced tree
        assert max(codes) >= 1 << 15

    if not dr.is_jit_v(mi.Float):
        for i in range(64):
            ref = receivers(rng, 1)
            if i % 4 == 0:
                ref.n = mi.Normal3f(0)
            sample = mi.Point2f(rng.uniform(0, 1, 2))
            check_pdf_consistency(scene, ref, sample, two_sided)
        return

    n = 8192
    ref = receivers(rng, n)
    sampler = mi.load_dict({'type': 'independent'})
    sampler.seed(0, n)
    # The first lanes draw the endpoints of the sample range
    index = dr.arange(mi.UInt32, n)
    sample = sampler.next_2d()
    sample.x = dr.select(index < 64, dr.select(index % 2 == 0, 0.0,
                                               dr.one_minus_epsilon(mi.Float)),
                         sample.x)
    with dr.scoped_set_flag(dr.JitFlag.Debug, True):
        ds, spec = check_pdf_consistency(scene, ref, sample, two_sided)
    if kind == 'mixed':
        # Every emitter but the hidden one is picked at some point
        picks = set(emitters.index(e) for e in ds.emitter if e is not None)
        assert picks == set(range(len(emitters))) - set(hidden)


@pytest.mark.parametrize('animated', [False, True])
def test02_unbiased(variants_vec_backends_once_rgb, animated):
    """The tree and the selection by weight estimate the same incident
    radiance, also for animated emitters whose bounds cover the motion"""
    rng = np.random.default_rng(2)
    n = 1 << 18
    ref = dr.zeros(mi.Interaction3f, n)
    sampler = mi.load_dict({'type': 'independent'})
    if animated:
        def light(t0, t1, emitter):
            return {'type': emitter, 'intensity': 10.0,
                    'to_world': mi.AnimatedTransform4f({0.0: t0, 1.0: t1})}
        d = {
            'type': 'scene',
            'spot': {
                **light(mi.ScalarTransform4f().look_at([3, 4, 2], [0, 1, 0], [0, 1, 0]),
                        mi.ScalarTransform4f().look_at([-3, 3, 2], [0, -1, 0], [0, 1, 0]),
                        'spot'),
                'cutoff_angle': 30.0,
            },
            'point': light(mi.ScalarTransform4f().translate([-3, 4, -2]),
                           mi.ScalarTransform4f().translate([3, 4, -2]), 'point'),
        }
        ref.n = mi.Normal3f(0, 1, 0)
        sampler.seed(0, n)
        ref.time = sampler.next_1d()
    else:
        d = many_lights(rng, n_point=10, n_area=6, n_spot=3)
        ref.p = mi.Point3f(0.3, -0.2, 0.1)
        ref.n = mi.Normal3f(0, 0, 1)

    means = []
    for emitter_tree in [True, False]:
        d['emitter_tree'] = emitter_tree
        scene = mi.load_dict(d)
        if animated and emitter_tree:
            assert scene.emitters()[0].bounds().theta_o > 0.5
        sampler.seed(1, n)
        ds, spec = scene.sample_emitter_direction(ref, sampler.next_2d(),
                                                  False, False)
        # Only the hemisphere above the receiver counts
        cos = dr.maximum(dr.dot(ds.d, ref.n), 0)
        means.append(dr.mean(mi.unpolarized_spectrum(spec).x * cos))
    dr.assert_allclose(means[0], means[1], rtol=0.03)


@pytest.mark.parametrize('infinite', [False, True])
def test03_one_sided(variants_all_backends_once, infinite):
    """A one-sided receiver facing away from every finite emitter never
    samples them, and draws from the infinite emitters alone when there
    are any"""
    rng = np.random.default_rng(3)
    scene = mi.load_dict(many_lights(rng, n_point=10, n_area=4, n_spot=2,
                                     infinite=infinite))
    finite = [e for e in scene.emitters()
              if not mi.has_flag(e.flags(), mi.EmitterFlags.Infinite)]
    for e in finite:
        assert e.bounds().bbox.max.z <= 7.0

    ref = dr.zeros(mi.Interaction3f)
    ref.p = mi.Point3f(0, 0, 8)
    ref.n = mi.Normal3f(0, 0, 1)
    for u in np.linspace(0, 0.99, 8):
        sample = mi.Point2f(float(u), 0.7)
        ds, _ = scene.sample_emitter_direction(ref, sample, False, False)
        if infinite:
            assert picked(ds) not in finite
        else:
            dr.assert_true(dr.all(ds.pdf == 0))
        ds, _ = scene.sample_emitter_direction(ref, sample, False, True)
        dr.assert_true(dr.all(ds.pdf != 0))


@pytest.mark.parametrize('case', ['disabled', 'single', 'no_bounds'])
def test04_selection_by_weight(variants_all_backends_once, case):
    """Without a tree, the scene selects emitters by sampling weight: when
    the tree is disabled, when only one emitter can be selected, and when
    an emitter cannot provide bounds"""
    if case == 'disabled':
        d = {'emitter_tree': False,
             'a': {'type': 'point', 'position': [1, 0, 0], 'sampling_weight': 3.0},
             'b': {'type': 'point', 'position': [-1, 0, 0], 'sampling_weight': 1.0}}
        picks = [(0.1, 'a', 4.0 / 3.0), (0.9, 'b', 4.0)]
    elif case == 'single':
        d = {'a': {'type': 'point', 'position': [1, 0, 0]},
             'hidden': {'type': 'point', 'position': [-1, 0, 0],
                        'sampling_weight': 0.0},
             'env': {'type': 'constant', 'visibility': 'primary'}}
        picks = [(0.1, 'a', 1.0), (0.9, 'a', 1.0)]
    else:
        class NoBounds(mi.Emitter):
            """An emitter without bounds() disables the tree with a warning"""
            def __init__(self, props):
                mi.Emitter.__init__(self, props)
                self.m_flags = mi.EmitterFlags.DeltaPosition

            def sample_direction(self, it, sample, active=True):
                ds = dr.zeros(mi.DirectionSample3f, dr.width(it)) \
                    if dr.is_jit_v(mi.Float) else mi.DirectionSample3f()
                return ds, mi.Spectrum(0.0)

            def bbox(self):
                return mi.ScalarBoundingBox3f([0, 0, 1])

            def to_string(self):
                return 'NoBounds[]'

        mi.register_emitter('nobounds', NoBounds)
        d = {'a': {'type': 'nobounds'},
             'b': {'type': 'point', 'position': [-1, 0, 0]}}
        picks = [(0.9, 'b', 2.0)]

    scene = mi.load_dict({'type': 'scene', **d})
    emitters = {e.id(): e for e in scene.emitters()}
    assert all(e.emitter_tree_code() == 0 for e in scene.emitters())
    ref = dr.zeros(mi.Interaction3f)
    for u, name, weight in picks:
        ds, spec = scene.sample_emitter_direction(ref, mi.Point2f(u, 0.5), False)
        assert picked(ds) == emitters[name]
        dr.assert_allclose(mi.unpolarized_spectrum(spec).x, weight)


def test05_parameter_update(variants_all_backends_once):
    """Moving an emitter rebuilds the tree"""
    scene = mi.load_dict({
        'type': 'scene',
        'a': {'type': 'point', 'position': [1, 0, 0]},
        'b': {'type': 'point', 'position': [-1, 0, 0]},
        'c': {'type': 'point', 'position': [0, 1, 0]},
    })
    ref = dr.zeros(mi.Interaction3f)
    ref.p = mi.Point3f(0.9, 0, 0)

    def pick(u):
        ds, _ = scene.sample_emitter_direction(ref, mi.Point2f(u, 0.5), False)
        return scene.emitters().index(picked(ds))

    # The emitter at [1, 0, 0] dominates
    assert pick(0.5) == 0

    params = mi.traverse(scene)
    params['a.to_world'] = mi.Transform4f().translate([-10, 0, 0])
    params['b.to_world'] = mi.Transform4f().translate([0.95, 0, 0])
    params.update()
    assert pick(0.5) == 1


@pytest.mark.parametrize('radiance, n_z, share', [
    (1e4, 1, 0.5), (1e-4, 1, 0.0), (1e4, -1, 1.0)])
def test06_infinite_cap(variants_vec_backends_once_rgb, radiance, n_z, share):
    """Without portals, a bright sky never outweighs the tree, unless the
    receiver faces away from every finite emitter"""
    n = 1 << 14
    ref = dr.zeros(mi.Interaction3f, n)
    ref.n = mi.Normal3f(0, 0, n_z)
    sampler = mi.load_dict({'type': 'independent'})
    sampler.seed(0, n)
    scene = mi.load_dict({
        'type': 'scene',
        'light1': {'type': 'point', 'position': [0, 0, 1], 'intensity': 1.0},
        'light2': {'type': 'point', 'position': [0, 0.1, 1], 'intensity': 1.0},
        'sky': {'type': 'constant', 'radiance': radiance},
    })
    ds, spec = scene.sample_emitter_direction(ref, sampler.next_2d(), False, False)
    sky = dr.mean(dr.select(ds.emitter == scene.environment(), 1.0, 0.0))[0]
    assert abs(sky - share) < 0.02
    check_pdf_consistency(scene, ref, sampler.next_2d(), False)


def test07_portals(variants_vec_backends_once_rgb):
    """With portals, the sky is weighed like an area emitter in the window"""
    n = 1 << 14
    sampler = mi.load_dict({'type': 'independent'})
    sampler.seed(0, n)
    scene = mi.load_dict({
        'type': 'scene',
        'light1': {'type': 'point', 'position': [0, 0, 1], 'intensity': 1.0},
        'light2': {'type': 'point', 'position': [0, 0.1, 1], 'intensity': 1.0},
        'sky': {'type': 'constant', 'radiance': 1e4},
        # A window in the plane x = 2 facing the -x half-space
        'window': {'type': 'portal',
                   'to_world': mi.ScalarTransform4f().look_at([2, 0, 0], [1, 0, 0], [0, 0, 1])},
    })
    assert len(scene.portals()) == 1

    ref = dr.zeros(mi.Interaction3f, n)
    ref.n = mi.Normal3f(1, 0, 0)

    # In front of the window the sky dominates, behind it the point emitters
    # are all that remains
    ref.p = mi.Point3f(0, 0, 0)
    ds, _ = scene.sample_emitter_direction(ref, sampler.next_2d(), False, False)
    assert dr.mean(dr.select(ds.emitter == scene.environment(), 1.0, 0.0))[0] > 0.9
    check_pdf_consistency(scene, ref, sampler.next_2d(), False)

    ref.p = mi.Point3f(4, 0, 0)
    ref.n = mi.Normal3f(-1, 0, 0)
    ds, _ = scene.sample_emitter_direction(ref, sampler.next_2d(), False, False)
    assert dr.all(ds.emitter != scene.environment())


@pytest.mark.parametrize('radiance', [1e-20, 1.0, 1e20])
def test09_intensity_scale(variants_all_backends_once, radiance):
    """Probabilities must not under/overflow through products of node weights."""
    count = 64
    scene = mi.load_dict({
        'type': 'scene',
        **{f'light{i}': {'type': 'rectangle',
                         'emitter': {'type': 'area', 'radiance': radiance}}
           for i in range(count)},
    })
    ref = dr.zeros(mi.Interaction3f)
    ref.p = mi.Point3f(0, 0, 2)
    ref.n = mi.Normal3f(0, 0, -1)
    ds, spec = scene.sample_emitter_direction(ref, mi.Point2f(0.37, 0.6), False)
    assert dr.all(dr.isfinite(ds.pdf) & (ds.pdf > 0))
    assert dr.all(dr.isfinite(spec) & (spec > 0))
    pdf = scene.pdf_emitter_direction(ref, ds)
    dr.assert_allclose(pdf, ds.pdf, rtol=1e-5)
    # All identical rectangles have probability 1/count. Their conditional
    # solid-angle density is distance squared / (area * emitter cosine).
    expected = dr.square(ds.dist) / (4 * dr.abs(ds.d.z) * count)
    dr.assert_allclose(ds.pdf, expected, rtol=1e-5)


@pytest.mark.parametrize('cutoff', [0.25, 10.0, 30.0, 89.9, 120.0])
def test10_quantized_cone(variants_vec_backends_once_rgb, cutoff):
    """Packing a tilted cone must preserve support near its emission
    boundary, also behind the axis for a cutoff above 90 degrees"""
    t = mi.ScalarTransform4f().look_at([0, 0, 0], [1, 1, 1], [0, 1, 0])
    light = {'type': 'spot', 'to_world': t, 'cutoff_angle': cutoff,
             'beam_width': 0.9 * cutoff}
    scene = mi.load_dict({'type': 'scene', 'a': light, 'b': light})
    phi = dr.arange(mi.Float, 1024) * (2 * dr.pi / 1024)
    theta = (0.99 if cutoff < 1 else 0.999) * cutoff * (dr.pi / 180)
    ref = dr.zeros(mi.Interaction3f, 1024)
    ref.p = mi.Transform4f(t) @ mi.Point3f(dr.sin(theta) * dr.cos(phi),
                                         dr.sin(theta) * dr.sin(phi), dr.cos(theta))
    ds, spec = scene.sample_emitter_direction(ref, mi.Point2f(0.37, 0.5), False)
    dr.assert_allclose(ds.pdf, 0.5)
    dr.assert_true(dr.all(spec > 0))


@pytest.mark.parametrize('alpha', [0.05, 0.5, 5.0, 45.0, 80.0])
def test11_fp16_orientation_cone(variants_all_backends_once, alpha):
    """The float16 orientation cone of a cluster keeps support at its boundary."""
    cutoff = 20.0
    def spot(pos, tilt):
        t = mi.ScalarTransform4f().translate(pos).rotate([1, 0, 0], tilt)
        return {'type': 'spot', 'to_world': t, 'cutoff_angle': cutoff,
                'beam_width': 0.9 * cutoff}
    # Eight pairs of spots tilted about x by -alpha and +alpha, one pair at
    # the origin and the others above it with their lit sides facing away.
    # Each pair forms one cluster in the group of the root.
    d = {'type': 'scene'}
    for i in range(8):
        d[f'a{i}'] = spot([0, 0, 100 * i], -alpha)
        d[f'b{i}'] = spot([0, 0, 100 * i], alpha)
    scene = mi.load_dict(d)
    codes = {e.id(): e.emitter_tree_code() for e in scene.emitters()}
    assert all(c >> 6 == 1 for c in codes.values())
    assert all(codes[f'a{i}'] & 7 == codes[f'b{i}'] & 7 for i in range(8))

    # Receivers just inside the union of orientation and emission cones,
    # lit by a0 alone (rotating about x by -alpha moves +z toward +y)
    theta = np.deg2rad(0.999 * (cutoff + alpha))
    pts = np.array([[0, np.sin(theta), np.cos(theta)]] * 8) * np.linspace(0.5, 50, 8)[:, None]
    ref = dr.zeros(mi.Interaction3f, len(pts))
    if dr.is_jit_v(mi.Float):
        ref.p = mi.Point3f(pts.T.astype(np.float32))
        ds, spec = scene.sample_emitter_direction(ref, mi.Point2f(0.37, 0.5), False)
        dr.assert_true(dr.all(spec > 0))
    else:
        for q in pts:
            ref.p = mi.Point3f(q)
            ds, spec = scene.sample_emitter_direction(ref, mi.Point2f(0.37, 0.5), False)
            assert dr.all(spec > 0)


@pytest.mark.parametrize('infinite', [False, True])
@pytest.mark.parametrize('auto_opaque', [False, True])
def test12_frozen_tree_rebuild(variants_vec_backends_once_rgb, infinite, auto_opaque):
    """Frozen sampling and PDF replay see the buffers owned by the rebuilt tree."""
    def transform(x):
        return mi.ScalarTransform4f().translate([x, 0, 3]).rotate([1, 0, 0], 180)

    d = {'type': 'scene'}
    for name, x in [('a', -3), ('b', 0), ('c', 3)]:
        d[name] = {'type': 'rectangle', 'to_world': transform(x),
                   'emitter': {'type': 'area', 'radiance': 1.0}}
    if infinite:
        d['env'] = {'type': 'constant', 'radiance': 0.3}
    scene = mi.load_dict(d)
    params = mi.traverse(scene)
    ref = dr.zeros(mi.Interaction3f)
    sample = mi.Point2f(dr.linspace(mi.Float, 0.001, 0.999, 128), 0.5)

    def sample_and_pdf(scene, ref, sample):
        ds, spec = scene.sample_emitter_direction(ref, sample, False)
        pdf = scene.pdf_emitter_direction(ref, ds)
        return ds.p, ds.pdf, spec, pdf

    frozen = dr.freeze(sample_and_pdf, auto_opaque=auto_opaque)
    first = None
    for i in range(3):
        params['a.to_world'] = mi.Transform4f(transform(-3 + 0.5 * i))
        params['b.emitter.radiance.value'] = mi.Color3f(1 + i)
        if infinite:
            params['env.radiance.value'] = mi.Color3f(0.3 + i)
        params.update()

        result = frozen(scene, ref, sample)
        expected = sample_and_pdf(scene, ref, sample)
        for actual, reference in zip(result, expected):
            dr.assert_allclose(actual, reference, rtol=1e-5, atol=1e-6)
        dr.assert_allclose(result[1], result[3], rtol=1e-4)
        if first is None:
            first = result[2]
        else:
            assert not dr.allclose(first, result[2])
    # auto_opaque promotes the changing radiance literal on the second call.
    assert frozen.n_recordings == (2 if auto_opaque else 1)
