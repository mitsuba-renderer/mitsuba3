import pytest
import drjit as dr
import mitsuba as mi


def _make_tracking_bsdf():
    """Return a fresh BSDF base class that records parameters_changed keys on the subclass."""
    class _TrackingBSDF(mi.BSDF):
        update_keys = []

        def sample(self, *args, **kwargs):
            return (dr.zeros(mi.BSDFSample3f), dr.zeros(mi.Color3f))
        def eval(self, *args, **kwargs):
            return dr.zeros(mi.Color3f)
        def pdf(self, *args, **kwargs):
            return 0
        def eval_pdf(self, *args, **kwargs):
            return (dr.zeros(mi.Color3f), 0)
        def to_string(self, *args, **kwargs):
            return f"{type(self).__name__}[]"
        def parameters_changed(self, keys):
            type(self).update_keys = list(keys)
    
    return _TrackingBSDF


def _make_tracking_texture():
    """Return a fresh Texture base class that records parameters_changed keys on the subclass."""
    class _TrackingTexture(mi.Texture):
        update_keys = []

        def eval(self, *args, **kwargs):
            return dr.zeros(mi.Color3f)
        def eval_1(self, *args, **kwargs):
            return 0
        def eval_1_grad(self, *args, **kwargs):
            return dr.zeros(mi.Vector2f)
        def eval_3(self, *args, **kwargs):
            return dr.zeros(mi.Color3f)
        def mean(self, *args, **kwargs):
            return 0
        def parameters_changed(self, keys):
            type(self).update_keys = list(keys)
    
    return _TrackingTexture


def test01_traverse_flags(variants_vec_backends_once_rgb):
    _TrackingBSDF = _make_tracking_bsdf()
    _TrackingTexture = _make_tracking_texture()

    class MyBSDF(_TrackingBSDF):
        def __init__(self, props):
            mi.BSDF.__init__(self, props)
            self.tex1 = props["texture1"]
            self.tex2 = props["texture2"]

        def traverse(self, cb):
            cb.put("bsdf_tex_diff", self.tex1, mi.ParamFlags.Differentiable)
            cb.put("bsdf_tex_nondiff", self.tex2, mi.ParamFlags.NonDifferentiable)

    class MyTexture(_TrackingTexture):
        def traverse(self, cb):
            cb.put("tex_param_diff", 0.1, mi.ParamFlags.Differentiable)
            cb.put(
                "tex_param_disc",
                0.1,
                mi.ParamFlags.Differentiable | mi.ParamFlags.Discontinuous
            )

    mi.register_bsdf("mybsdf", MyBSDF)
    mi.register_texture("mytexture", MyTexture)

    bsdf = mi.load_dict({
        "type": "mybsdf",
        "texture1": {"type": "mytexture"},
        "texture2": {"type": "mytexture"},
    }, optimize=False)

    params = mi.traverse(bsdf)

    assert len(params) == 4

    assert (
        params.flags("bsdf_tex_diff.tex_param_diff")
        == mi.ParamFlags.Differentiable.value
    )
    assert (
        params.flags("bsdf_tex_diff.tex_param_disc")
        == mi.ParamFlags.Differentiable.value | mi.ParamFlags.Discontinuous
    )
    assert (
        params.flags("bsdf_tex_nondiff.tex_param_diff")
        == mi.ParamFlags.NonDifferentiable.value
    ), "Non-differentiabilty should be propagated!"
    assert (
        params.flags("bsdf_tex_nondiff.tex_param_disc")
        == mi.ParamFlags.NonDifferentiable.value
    ), "Discontinuities don't matter if the parameter is non-differentiable!"


def test02_traverse_update(variants_all_ad_rgb):
    _TrackingBSDF = _make_tracking_bsdf()
    _TrackingTexture = _make_tracking_texture()

    class MyBSDF(_TrackingBSDF):
        def __init__(self, props):
            mi.BSDF.__init__(self, props)
            self.tex1 = props["texture1"]
            self.tex2 = props["texture2"]

        def traverse(self, cb):
            cb.put("bsdf_tex_diff", self.tex1, mi.ParamFlags.Differentiable)

    class MyTexture(_TrackingTexture):
        def __init__(self, props):
            mi.Texture.__init__(self, props)
            self.tex_param_nondiff = mi.Float(3)

        def traverse(self, cb):
            cb.put("tex_param_diff", mi.Point3f(1, 1, 1), mi.ParamFlags.Differentiable)
            cb.put("tex_param_scalar", mi.ScalarFloat(2), mi.ParamFlags.NonDifferentiable)
            cb.put("tex_param_nondiff", self.tex_param_nondiff, mi.ParamFlags.NonDifferentiable)

    def reset_update_keys():
        MyBSDF.update_keys = []
        MyTexture.update_keys = []


    mi.register_bsdf("mybsdf", MyBSDF)
    mi.register_texture("mytexture", MyTexture)

    bsdf = mi.load_dict({
        "type": "mybsdf",
        "texture1": {"type": "mytexture"},
        "texture2": {"type": "mytexture"},
    })

    params = mi.traverse(bsdf)
    assert len(params) == 3

    # Normal usage
    params["bsdf_tex_diff.tex_param_diff"][0] = 0
    params.update()
    assert (
        MyTexture.update_keys == ['tex_param_diff']
    ), "Only the updated parameter should figure in the keys!"
    assert (
        MyBSDF.update_keys == ['bsdf_tex_diff']
    ), "Updated parameters should propagate to their parents!"
    reset_update_keys()

    # Propagate changes to plugin variable
    params["bsdf_tex_diff.tex_param_nondiff"] = 0.2
    params.update()
    assert dr.allclose(bsdf.tex1.tex_param_nondiff, 0.2)
    reset_update_keys()

    params["bsdf_tex_diff.tex_param_diff"] = mi.Point3f(0, 0, 0)
    params.update()
    assert (
        MyTexture.update_keys == ['tex_param_diff'] and
        MyBSDF.update_keys == ['bsdf_tex_diff']
    ), "Updating an entire container should be supported!"
    reset_update_keys()


    # No-ops
    params["bsdf_tex_diff.tex_param_diff"] = params["bsdf_tex_diff.tex_param_diff"]
    params.update()
    assert (
        MyTexture.update_keys == [] and MyBSDF.update_keys == []
    ), "A no-op should not trigger an update (self-assignment)!"
    reset_update_keys()

    params["bsdf_tex_diff.tex_param_diff"] = (params["bsdf_tex_diff.tex_param_diff"] + mi.Point3f(0, 0, 0))
    params.update()
    assert (
        MyTexture.update_keys == [] and MyBSDF.update_keys == []
    ), "A no-op should not trigger an update (adding zero)!"
    reset_update_keys()

    params["bsdf_tex_diff.tex_param_scalar"] = mi.ScalarFloat(2)
    params.update()
    assert (
        MyTexture.update_keys == [] and MyBSDF.update_keys == []
    ), "A no-op should not trigger an update (scalar with same value)!"


    # Side-effects
    dr.enable_grad(params["bsdf_tex_diff.tex_param_diff"])
    params.update()
    assert (
        MyTexture.update_keys == ['tex_param_diff'] and
        MyBSDF.update_keys == ['bsdf_tex_diff']
    ), "Updates should be triggered when enabling gradients!"
    reset_update_keys()

    def update_inplace(value: mi.Point3f):
        value += 0.2
    update_inplace(params["bsdf_tex_diff.tex_param_diff"])
    params.update()
    assert (
        MyTexture.update_keys == ['tex_param_diff'] and
        MyBSDF.update_keys == ['bsdf_tex_diff']
    ), "Updates should be triggered when modifying parameters through side-effects!"
    reset_update_keys()


    # Gradients on non-diff params warning
    logger = mi.logger()
    l = logger.error_level()
    try:
        logger.set_error_level(mi.LogLevel.Warn)
        with pytest.raises(Exception) as e:
            dr.enable_grad(params["bsdf_tex_diff.tex_param_nondiff"])
            params.update()
    finally:
        logger.set_error_level(l)
    e.match(f"Parameter 'bsdf_tex_diff.tex_param_nondiff' is marked as "
            "non-differentiable but has gradients enabled, unexpected results "
            "may occur!")
    reset_update_keys()


def test03_scene_parameters_keep(variants_all_ad_rgb):
    bsdf = mi.load_dict({'type': 'principled'})
    params = mi.traverse(bsdf)

    params.keep(r'.*\.value')
    assert len(params) == 11

    params.keep(['clearcoat.value', 'flatness.value'])
    assert len(params) == 2


def test04_scene_parameters_update(variants_all_ad_rgb):
    bsdf = mi.load_dict({'type': 'principled'})
    params = mi.traverse(bsdf)

    ret = params.update({
        'clearcoat.value': 0.5,
        'flatness.value': 0.4,
    })

    assert dr.allclose(params['clearcoat.value'], 0.5)
    assert dr.allclose(params['flatness.value'], 0.4)

    # Check parameter changes have been applied to plugin
    si = dr.zeros(mi.SurfaceInteraction3f)
    si.uv = [0.5, 0.5]
    assert dr.allclose(bsdf.eval_attribute_1('clearcoat', si), 0.5)
    assert dr.allclose(bsdf.eval_attribute_1('flatness', si), 0.4)

    assert ret[-1][-1] == {'clearcoat', 'flatness'}

    # keep() — update(dict) leaves all keys intact so assertions still hold
    params.keep(r'.*\.value')
    assert len(params) == 11
    params.keep(['clearcoat.value', 'flatness.value'])
    assert len(params) == 2


def test05_merge(variants_all_ad_rgb):
    _TrackingBSDF = _make_tracking_bsdf()
    _TrackingTexture = _make_tracking_texture()

    class MyBSDF(_TrackingBSDF):
        def __init__(self, props):
            mi.BSDF.__init__(self, props)
            self.tex = props["texture"]

        def traverse(self, cb):
            cb.put("bsdf_tex", self.tex, mi.ParamFlags.Differentiable)

    class MyTexture(_TrackingTexture):
        def __init__(self, props):
            mi.Texture.__init__(self, props)
            self.tex_param = mi.Point3f(1, 1, 1)

        def traverse(self, cb):
            cb.put("tex_param", self.tex_param, mi.ParamFlags.Differentiable)

    mi.register_bsdf("mybsdf2", MyBSDF)
    mi.register_texture("mytexture2", MyTexture)

    bsdf1 = mi.load_dict({"type": "mybsdf2", "texture": {"type": "mytexture2"}})
    bsdf2 = mi.load_dict({"type": "mybsdf2", "texture": {"type": "mytexture2"}})

    params1 = mi.traverse(bsdf1)
    params2 = mi.traverse(bsdf2)
    n1, n2 = len(params1), len(params2)

    params1.merge(params2, other_prefix='other')

    # Structure: key count, prefixed keys present, flags preserved
    assert len(params1) == n1 + n2
    for k in params2.keys():
        assert f'other.{k}' in params1
        assert params1.flags(f'other.{k}') == params2.flags(k)

    # keep() works on prefixed keys (separate instance to leave params1 intact)
    params_tmp = mi.traverse(bsdf1)
    params_tmp.merge(mi.traverse(bsdf2), other_prefix='other')
    params_tmp.keep(r'other\..*')
    assert len(params_tmp) == n2

    # Update propagates to the correct instance; local names (not prefixed) reach callbacks
    params1['other.bsdf_tex.tex_param'] = mi.Point3f(0, 0, 0)
    params1.update()
    assert dr.allclose(bsdf1.tex.tex_param, mi.Point3f(1, 1, 1))
    assert dr.allclose(bsdf2.tex.tex_param, mi.Point3f(0, 0, 0))
    assert MyTexture.update_keys == ['tex_param']
    assert MyBSDF.update_keys == ['bsdf_tex']

    # Conflict: merge without prefix raises ValueError on overlapping keys
    with pytest.raises(ValueError, match="Key conflict"):
        mi.traverse(bsdf1).merge(mi.traverse(bsdf1))


def test06_nested_same_instance(variants_all_ad_rgb):
    _TrackingBSDF = _make_tracking_bsdf()
    _TrackingTexture = _make_tracking_texture()

    class MyBSDF(_TrackingBSDF):
        def __init__(self, props):
            mi.BSDF.__init__(self, props)
            self.tex1 = props["bsdf_tex1"]
            self.tex2 = props["bsdf_tex2"]
            self.tex3 = props["bsdf_tex3"]

        def traverse(self, cb):
            cb.put("bsdf_tex1", self.tex1, mi.ParamFlags.Differentiable)
            cb.put("bsdf_tex2", self.tex2, mi.ParamFlags.Differentiable)
            cb.put("bsdf_tex3", self.tex3, mi.ParamFlags.Differentiable)

        def parameters_changed(self, keys):
            type(self).update_keys = set(keys)

    class MyTexture(_TrackingTexture):
        def __init__(self, props):
            mi.Texture.__init__(self, props)
            self.value = mi.Float(1)

        def traverse(self, cb):
            cb.put("value", self.value, mi.ParamFlags.Differentiable)

        def parameters_changed(self, keys):
            type(self).update_keys = set(keys)

    
    mi.register_bsdf("mybsdf3", MyBSDF)
    mi.register_texture("mytexture3", MyTexture)

    texture = mi.load_dict({ "type": "mytexture3" })
    bsdf = mi.load_dict({
        "type": "mybsdf3",
        "bsdf_tex1": texture,
        "bsdf_tex2": texture,
        "bsdf_tex3": texture,
    })

    texture_params = mi.traverse(texture)
    bsdf_params = mi.traverse(bsdf)

    bsdf_params.merge(texture_params, other_prefix='texture')

    bsdf_params['texture.value'] = 0.5
    bsdf_params.update()

    assert dr.allclose(texture.value, 0.5)
    assert MyBSDF.update_keys == set(['bsdf_tex1', 'bsdf_tex2', 'bsdf_tex3'])
    assert MyTexture.update_keys == set(['value'])


def test07_traverse_pytree(variants_all_ad_rgb):
    import dataclasses

    bsdf1 = mi.load_dict({'type': 'diffuse'})
    bsdf2 = mi.load_dict({'type': 'diffuse'})
    bsdf1_params = mi.traverse(bsdf1)
    n = len(bsdf1_params)

    # List: keys prefixed with elem_i
    params = mi.traverse([bsdf1, bsdf2])
    assert len(params) == 2 * n
    for key in bsdf1_params.keys():
        assert f"elem_0.{key}" in params

    # Nested list: elem_0.elem_0, elem_1.elem_0
    params = mi.traverse([[bsdf1], [bsdf2]])
    assert len(params) == 2 * n
    assert any(k.startswith('elem_0.elem_0.') for k in params.keys())
    assert any(k.startswith('elem_1.elem_0.') for k in params.keys())

    # Dict: keys prefixed with dict key
    params = mi.traverse({'a': bsdf1, 'b': bsdf2})
    assert len(params) == 2 * n
    assert all(k.startswith('a.') or k.startswith('b.') for k in params.keys())

    # Dataclass
    @dataclasses.dataclass
    class Pair:
        x: object
        y: object

    params = mi.traverse(Pair(x=bsdf1, y=bsdf2))
    assert len(params) == 2 * n
    assert all(k.startswith('x.') or k.startswith('y.') for k in params.keys())

    # Single mi.Object: unchanged behaviour (no prefix)
    params_single = mi.traverse(bsdf1)
    params_pytree = mi.traverse([bsdf1])
    assert list(params_single.keys()) == [k[len('elem_0.'):] for k in params_pytree.keys()]

    # Non-object leaves are silently ignored
    params = mi.traverse({'bsdf': bsdf1, 'ignored': 42})
    assert len(params) == n

    # Deduplication: same object aliased multiple times → one parameters_changed, all aliases updated
    _TrackingBSDF = _make_tracking_bsdf()

    class AliasedBSDF(_TrackingBSDF):
        def __init__(self, props):
            mi.BSDF.__init__(self, props)
            self.value = mi.Float(1)

        def traverse(self, cb):
            cb.put("value", self.value, mi.ParamFlags.Differentiable)

    mi.register_bsdf("mybsdf4", AliasedBSDF)
    bsdf_shared = mi.load_dict({'type': 'mybsdf4'})
    params = mi.traverse({"first": bsdf_shared, "second": (bsdf_shared, bsdf_shared)})
    params["second.elem_1.value"] = 0.5
    params.update()
    assert AliasedBSDF.update_keys == ['value']
    assert dr.allclose(bsdf_shared.value, 0.5)
    assert dr.allclose(params["first.value"], 0.5)
    assert dr.allclose(params["second.elem_0.value"], 0.5)
    assert dr.allclose(params["second.elem_1.value"], 0.5)

def test08_shared_node_multiple_parents(variants_all_ad_rgb):
    # --- Custom plugins: shared texture, 5 BSDFs each get parameters_changed ---
    _TrackingBSDF = _make_tracking_bsdf()
    _TrackingTexture = _make_tracking_texture()

    class MyTexture(_TrackingTexture):
        def __init__(self, props):
            mi.Texture.__init__(self, props)
            self.value = mi.Float(3)

        def traverse(self, cb):
            cb.put("value", self.value, mi.ParamFlags.Differentiable)

        def parameters_changed(self, keys):
            self.update_keys = keys

    class MyBSDF(_TrackingBSDF):
        def __init__(self, props):
            mi.BSDF.__init__(self, props)
            self.tex = props["tex"]
            self.update_keys = []

        def traverse(self, cb):
            cb.put("tex_prop", self.tex, mi.ParamFlags.Differentiable)

        def parameters_changed(self, keys):
            self.update_keys = keys

    mi.register_texture("mytexture5", MyTexture)
    mi.register_bsdf("mybsdf5", MyBSDF)

    tex = mi.load_dict({'type': 'mytexture5'})
    all_bsdfs = [mi.load_dict({'type': 'mybsdf5', 'tex': tex}) for _ in range(5)]
    params = mi.traverse([all_bsdfs, tex])
    params["elem_1.value"] = 0.5
    params.update()
    for bsdf in all_bsdfs:
        assert dr.allclose(bsdf.tex.value, 0.5)
        assert bsdf.update_keys == ['tex_prop']

    # --- Built-in BSDF shared across 5 mi.Scene: all 5 scenes notified ---
    bsdf = mi.load_dict({'type': 'diffuse'})
    all_scenes = [
        mi.load_dict({'type': 'scene', 'sphere': {'type': 'sphere', 'bsdf': bsdf}})
        for _ in range(5)
    ]
    params = mi.traverse([all_scenes, bsdf])
    params["elem_1.reflectance.value"] = mi.Float(0.5)
    updated_nodes = {node for node, _ in params.update()}
    for scene in all_scenes:
        assert scene in updated_nodes


def test09_render_fwd_assert(variants_all_ad_rgb):
    scene = mi.load_dict({
        'type': 'scene',
        'shape': {
            'type': 'sphere'
        }
    })

    with pytest.raises(Exception) as e:
        mi.render(scene, params=[])

    params = mi.traverse(scene)
    key = 'shape.bsdf.reflectance.value'
    dr.enable_grad(params[key])
    params.update()

    with pytest.raises(Exception) as e:
        img = mi.render(scene)
        dr.forward_to(img)


def test10_variant_context():
    # Select the first variant which is not 'scalar_rgb'
    for variant in mi.variants():
        if variant != "scalar_rgb":
            override_variant = variant
            break
    else:
        pytest.skip("Only the 'scalar_rgb' variant was compiled.")

    # Now, the test
    mi.set_variant("scalar_rgb")

    # The active variant is temporarily overridden
    with mi.variant_context(override_variant):
        assert mi.variant() == override_variant
    assert mi.variant() == "scalar_rgb"

    # The initial variant is restored if an exception is raised
    try:
        with mi.variant_context(override_variant):
            assert mi.variant() == override_variant
            raise RuntimeError
    except RuntimeError:
        pass
    assert mi.variant() == "scalar_rgb"
