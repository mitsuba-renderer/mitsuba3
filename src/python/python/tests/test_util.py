import pytest
import drjit as dr
import mitsuba as mi

def test01_traverse_flags(variants_vec_backends_once_rgb):
    class MyBSDF(mi.BSDF):
        def __init__(self, props):
            mi.BSDF.__init__(self, props)
            self.tex1 = props["texture1"]
            self.tex2 = props["texture2"]

        def traverse(self, callback):
            callback.put_object("bsdf_tex_diff", self.tex1, mi.ParamFlags.Differentiable)
            callback.put_object("bsdf_tex_nondiff", self.tex2, mi.ParamFlags.NonDifferentiable)

        def sample(self, *args, **kwargs):
            return (dr.zeros(mi.BSDFSample3f), dr.zeros(mi.Color3f))

        def eval(self, *args, **kwargs):
            return dr.zeros(mi.Color3f)

        def pdf(self, *args, **kwargs):
            return 0

        def eval_pdf(self, *args, **kwargs):
            return (dr.zeros(mi.Color3f), 0)

        def to_string(self, *args, **kwargs):
            return "MyBSDF[]"

    class MyTexture(mi.Texture):
        def __init__(self, props):
            mi.Texture.__init__(self, props)

        def traverse(self, callback):
            callback.put_parameter("tex_param_diff", 0.1, mi.ParamFlags.Differentiable)
            callback.put_parameter(
                "tex_param_disc",
                0.1,
                mi.ParamFlags.Differentiable | mi.ParamFlags.Discontinuous
            )

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

    mi.register_bsdf("mybsdf", MyBSDF)
    mi.register_texture("mytexture", MyTexture)

    bsdf = mi.load_dict({
        "type": "mybsdf",
        "texture1": {"type": "mytexture"},
        "texture2": {"type": "mytexture"},
    })

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
    class MyBSDF(mi.BSDF):
        update_keys = []

        def __init__(self, props):
            mi.BSDF.__init__(self, props)
            self.tex1 = props["texture1"]
            self.tex2 = props["texture2"]

        def traverse(self, callback):
            callback.put_object("bsdf_tex_diff", self.tex1, mi.ParamFlags.Differentiable)

        def parameters_changed(self, keys):
            MyBSDF.update_keys = keys

        def sample(self, *args, **kwargs):
            return (dr.zeros(mi.BSDFSample3f), dr.zeros(mi.Color3f))

        def eval(self, *args, **kwargs):
            return dr.zeros(mi.Color3f)

        def pdf(self, *args, **kwargs):
            return 0

        def eval_pdf(self, *args, **kwargs):
            return (dr.zeros(mi.Color3f), 0)

        def to_string(self, *args, **kwargs):
            return "MyBSDF[]"

    class MyTexture(mi.Texture):
        update_keys = []

        def __init__(self, props):
            mi.Texture.__init__(self, props)

        def traverse(self, callback):
            callback.put_parameter("tex_param_diff", mi.Point3f(1, 1, 1), mi.ParamFlags.Differentiable)
            callback.put_parameter("tex_param_scalar", mi.ScalarFloat(2), mi.ParamFlags.NonDifferentiable)
            callback.put_parameter("tex_param_nondiff", mi.Float(3), mi.ParamFlags.NonDifferentiable)

        def parameters_changed(self, keys):
            MyTexture.update_keys = keys

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

    def inplace_update(value: mi.Point3f):
        value += 0.2
    inplace_update(params["bsdf_tex_diff.tex_param_diff"])
    params.update()
    assert (
        MyTexture.update_keys == ['tex_param_diff'] and
        MyBSDF.update_keys == ['bsdf_tex_diff']
    ), "Updates should be triggered when modifying parameters through side-effects!"
    reset_update_keys()


    # Gradients on non-diff params warning
    logger = mi.Thread.thread().logger()
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

    assert ret[-1][-1] == {'clearcoat', 'flatness'}


def test05_render_fwd_assert(variants_all_ad_rgb):
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
