from typing import Callable, Optional, Tuple
import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.scalar_rgb.test.util import find_resource

EMITTERS = [
    "area",
    "point",
    "constant",
    "envmap",
    "spot",
    "projector",
    "directional",
    "directionalarea",
]
BSDFS = [
    "diffuse",
    "dielectric",
    "thindielectric",
    "roughdielectric",
    "conductor",
    "roughconductor",
    "hair",
    "plastic",
    "roughplastic",
    "bumpmap",
    "normalmap",
    "blendbsdf",
    "mask",
    "twosided",
    "principled",
    "principledthin",
    "custom",
]
INTEGRATORS = [
    "direct",
    "path",
    "prb",
    "prb_basic",
    "direct_projective",
    "prb_projective",
    "moment",
    "ptracer",
    "depth",
    "aov",
]
SHAPES = [
    "mesh",
    "disk",
    "cylinder",
    "bsplinecurve",
    "linearcurve",
    "sdfgrid",
    # "instance",
    "sphere",
]


def mse(image, image_ref):
    return dr.sum(dr.square(image - image_ref), axis=None)

def assert_render(
    input,
    n,
    update: Optional[Callable] = None,
    n_recordings: Optional[int] = None,
    tmp_path=None,
    spp=1,
    auto_opaque: bool = True,
):
    if n_recordings is None:
        # A rendering step is captured by a single recording.
        n_recordings = 1

    def func(scene: mi.Scene) -> mi.TensorXf:
        with dr.profile_range("render"):
            result = mi.render(scene, spp=spp)
        return result

    frozen = dr.freeze(func, auto_opaque=auto_opaque)

    for i in range(n):
        if update:
            params = mi.traverse(input)
            update(i, params)
            params.update()

        res = frozen(input)
        ref = func(input)

        # NOTE: uncomment to save rendered images
        # if tmp_path:
        #     mi.util.write_bitmap(f"{tmp_path}/res{i}.exr", res)
        #     mi.util.write_bitmap(f"{tmp_path}/ref{i}.exr", ref)

        assert dr.allclose(ref, res)
    assert frozen.n_recordings == n_recordings

@pytest.mark.parametrize("auto_opaque", [False, True])
def test01_cornell_box(variants_vec_rgb, auto_opaque):
    """
    Test that it is possible to render the cornell_box in a frozen function.
    """
    w, h = (16, 16)
    n = 5
    k = "light.emitter.radiance.value"

    scene = mi.cornell_box()
    scene["sensor"]["film"]["width"] = w
    scene["sensor"]["film"]["height"] = h
    scene = mi.load_dict(scene, parallel=True)

    params = mi.traverse(scene)
    value = mi.Float(params[k].x)

    def update(i, params):
        params[k].x = value + 10.0 * i

    assert_render(scene, n, update, auto_opaque=auto_opaque)


@pytest.mark.parametrize("aovs", [False, True])
def test01b_single_recording(variants_vec_rgb, aovs):
    """
    Rendering a scene repeatedly should only ever require a single recording.
    This is a regression test for state that a rendering step used to leave
    behind: the film only allocated its storage block while rendering, the
    sensor's sampler kept the RNG state that was allocated when seeding it,
    and ``mi.render()`` bound the sensor to a Python object. All three showed
    up as changed inputs in the second call and forced a re-recording.
    """
    scene = mi.cornell_box()
    scene["sensor"]["film"]["width"] = 16
    scene["sensor"]["film"]["height"] = 16
    if aovs:
        # The film cannot know the AOV count ahead of the first ``prepare()``
        # call, and allocates a storage block of the wrong width. Only the
        # presence of that block matters, so a single recording still suffices.
        scene["integrator"] = {
            "type": "aov",
            "aovs": "dd.y:depth,nn:sh_normal",
            "integrator": { "type": "path" }
        }
    scene = mi.load_dict(scene, parallel=True)

    def func(scene):
        return mi.render(scene, spp=4)

    frozen = dr.freeze(func)

    for i in range(3):
        assert dr.allclose(frozen(scene), func(scene))

    assert frozen.n_recordings == 1


@pytest.mark.parametrize("auto_opaque", [False, True])
def test02_cornell_box_native(variants_vec_rgb, auto_opaque):
    """
    Test that it is possible to render the cornell_box with the native acceleration
    structure.
    """
    if mi.MI_ENABLE_EMBREE:
        pytest.skip("Embree enabled, cannot test the native KD-Tree")

    w, h = (16, 16)
    n = 5
    k = "light.emitter.radiance.value"

    scene = mi.cornell_box()
    scene["sensor"]["film"]["width"] = w
    scene["sensor"]["film"]["height"] = h
    scene = mi.load_dict(scene, parallel=True)

    params = mi.traverse(scene)
    value = mi.Float(params[k].x)

    def update(i, params):
        params[k].x = value + 10.0 * i

    assert_render(scene, n, update, auto_opaque = auto_opaque)


@pytest.mark.parametrize(
    "integrator",
    [
        "direct",
        "prb",
        "prb_basic",
        # Projective integrators are not yet supported
        # "direct_projective",
        # "prb_projective",
    ],
)
@pytest.mark.parametrize("auto_opaque", [False, True])
def test02_pose_estimation(variants_vec_rgb, integrator, auto_opaque):
    """
    Tests that it is possible to optimize the pose of an object, when freezing
    the forward and backward pass. Gradients are propagated through the inputs
    of the frozen function.

    Updates of the scene geometry is not possible inside of frozen functions,
    as this would require us to re-build the acceleration structure, which
    currently cannot be recorded.
    """

    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    w, h = (16, 16)
    n = 10

    def apply_transformation(initial_positions, opt, params):
        opt["trans"] = dr.clip(opt["trans"], -0.5, 0.5)
        opt["angle"] = dr.clip(opt["angle"], -0.5, 0.5)

        trafo = (
            mi.Transform4f()
            .translate([opt["trans"].x, opt["trans"].y, 0.0])
            .rotate([0, 1, 0], opt["angle"] * 100.0)
        )

        positions_new = trafo @ initial_positions
        params["bunny.positions"] = mi.TensorXf(positions_new, flip_axes=True)

    def compute_grad(scene, ref):
        params = mi.traverse(scene)

        image = mi.render(scene, params, spp=1, seed=1, seed_grad=2)

        # Evaluate the objective function from the current rendered image
        loss = mse(image, ref)

        # Backpropagate through the rendering process
        dr.backward(loss)

        return image, loss

    frozen = dr.freeze(compute_grad, auto_opaque = auto_opaque)

    def load_scene():
        scene = mi.cornell_box()
        del scene["large-box"]
        del scene["small-box"]
        del scene["green-wall"]
        del scene["red-wall"]
        del scene["floor"]
        del scene["ceiling"]
        scene["bunny"] = {
            "type": "ply",
            "filename": find_resource("resources/data/common/meshes/bunny.ply"),
            "to_world": mi.ScalarTransform4f().scale(6.5),
            "bsdf": {
                "type": "diffuse",
                "reflectance": {"type": "rgb", "value": (0.3, 0.3, 0.75)},
            },
        }
        scene["integrator"] = {
            "type": integrator
        }
        scene["sensor"]["film"] = {
            "type": "hdrfilm",
            "width": w,
            "height": h,
            "rfilter": {"type": "gaussian"},
            "sample_border": True,
        }

        scene = mi.load_dict(scene, parallel=True)
        return scene

    def run(
        scene: mi.Scene, compute_grad, n
    ) -> Tuple[mi.TensorXf, mi.Point3f, mi.Float]:
        params = mi.traverse(scene)

        params.keep("bunny.positions")
        initial_positions = mi.Point3f(params["bunny.positions"],
                                       flip_axes=True)

        image_ref = mi.render(scene, spp=4)

        opt = mi.ad.Adam(lr=0.025)
        opt["angle"] = mi.Float(0.25)
        opt["trans"] = mi.Point3f(0.1, -0.25, 0.0)

        for i in range(n):
            params = mi.traverse(scene)
            params.keep("bunny.positions")

            apply_transformation(initial_positions, opt, params)

            params.update(opt)

            with dr.profile_range("optimize"):
                image, loss = compute_grad(scene, image_ref)

            opt.step()

        image_final = mi.render(scene, spp=4, seed=1, seed_grad=2)

        return image_final, opt["trans"], opt["angle"]

    # NOTE:
    # In this case, we have to use the same scene object
    # for the frozen and non-frozen case, as re-loading
    # the scene causes mitsuba to render different images,
    # leading to diverging descent trajectories.

    scene = load_scene()
    params = mi.traverse(scene)
    initial_positions = mi.TensorXf(params["bunny.positions"])

    img_ref, trans_ref, angle_ref = run(scene, compute_grad, n)

    # Reset parameters:
    params["bunny.positions"] = initial_positions
    params.update()

    img_frozen, trans_frozen, angle_frozen = run(scene, frozen, n)

    # NOTE: cannot compare results as errors accumulate and the result will never be the same.

    assert dr.allclose(trans_ref, trans_frozen, atol = 0.001)
    assert dr.allclose(angle_ref, angle_frozen, atol = 0.001)
    # ``auto_opaque`` promotes a literal on the second call, and the ``direct``
    # integrator differentiates through the scene, whose parameters carry
    # gradients from the first call on. Either costs one more recording.
    assert frozen.n_recordings == (2 if auto_opaque or integrator == "direct"
                                     else 1)
    # if integrator != "prb_projective":
    #     assert dr.allclose(img_ref, img_frozen, atol=0.001)


def test02b_cross_scene_rebind(variants_vec_rgb):
    """
    Freeze a render on one scene, then replay the frozen function on a
    *different* but structurally-equivalent scene instance. The frozen function
    must render the second scene's geometry/materials, not the captured first
    scene.

    Before resource owners became rebindable freeze inputs, the Metal backend
    captured the acceleration structure by identity and silently traced the
    original scene; the other backends always exposed it as a traversed input.
    This test pins the (now uniform) behaviour across backends.
    """
    def make_scene(green):
        d = mi.cornell_box()
        d["sensor"]["film"]["width"] = 32
        d["sensor"]["film"]["height"] = 32
        d["green"]["reflectance"]["value"] = green
        return mi.load_dict(d)

    def render(scene):
        return mi.render(scene, spp=16)

    frozen = dr.freeze(render)

    scene_a = make_scene([0.1, 0.4, 0.1])
    scene_b = make_scene([0.4, 0.1, 0.1])

    res_a = frozen(scene_a)
    res_b = frozen(scene_b)   # replay on a *different* scene instance
    ref_b = render(scene_b)   # non-frozen reference for scene B

    # The frozen replay must match a fresh render of scene B ...
    assert dr.allclose(res_b, ref_b, atol=1e-3)
    # ... and must differ from scene A (i.e. it did not trace the captured one).
    assert not dr.allclose(res_b, res_a, atol=1e-3)


@pytest.mark.parametrize("auto_opaque", [False, True])
def test03_optimize_color(variants_vec_rgb, auto_opaque):
    """
    Tests freezing of optimizing a color parameter through backpropagation, by
    passing the gradients through the frozen function inputs.
    """
    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')


    k = "red.reflectance.value"
    w, h = (16, 16)
    n = 10

    def optimize(scene, image_ref):
        params = mi.traverse(scene)
        params.keep(k)

        image = mi.render(scene, params, spp=1)

        loss = mse(image, image_ref)

        dr.backward(loss)

        return image, loss

    frozen = dr.freeze(optimize, auto_opaque = auto_opaque)

    def run(n: int, optimize):
        scene = mi.cornell_box()
        scene["integrator"] = {
            "type": "prb",
        }
        scene["sensor"]["film"]["width"] = w
        scene["sensor"]["film"]["height"] = h
        scene = mi.load_dict(scene, parallel=True)

        image_ref = mi.render(scene, spp=512)

        params = mi.traverse(scene)
        params.keep(k)

        opt = mi.ad.Adam(lr=0.05)
        opt[k] = mi.Color3f(0.01, 0.2, 0.9)

        for i in range(n):
            params = mi.traverse(scene)
            params.keep(k)
            params.update(opt)

            image, loss = optimize(scene, image_ref)

            opt.step()

        return image, opt[k]

    image_ref, param_ref = run(n, optimize)

    image_frozen, param_frozen = run(n, frozen)

    assert frozen.n_recordings == (2 if auto_opaque else 1)
    # Optimizing the reflectance is not as prone to divergence,
    # therefore we can test if the two methods produce the same results
    assert dr.allclose(param_ref, param_frozen)


def bsdf_dict(bsdf: str):
    """
    Generate dictionaries, defining default versions of BSDFs, that are used
    in the bsdf tests, based on the bsdf name.
    """
    if bsdf == "custom":
        class MyBSDF(mi.BSDF):
            def __init__(self, props):
                mi.BSDF.__init__(self, props)

                # Read 'eta' and 'tint' properties from `props`
                self.eta = 1.33
                if 'eta' in props:
                    self.eta = props["eta"]

                self.tint = props["tint"]

                # Set the BSDF flags
                reflection_flags = (
                    mi.BSDFFlags.DeltaReflection
                    | mi.BSDFFlags.FrontSide
                    | mi.BSDFFlags.BackSide
                )
                transmission_flags = (
                    mi.BSDFFlags.DeltaTransmission
                    | mi.BSDFFlags.FrontSide
                    | mi.BSDFFlags.BackSide
                )
                self.m_components = [reflection_flags, transmission_flags]
                self.m_flags = reflection_flags | transmission_flags

            def sample(self, ctx, si, sample1, sample2, active):
                # Compute Fresnel terms
                cos_theta_i = mi.Frame3f.cos_theta(si.wi)
                r_i, cos_theta_t, eta_it, eta_ti = mi.fresnel(cos_theta_i, self.eta)
                t_i = dr.maximum(1.0 - r_i, 0.0)

                # Pick between reflection and transmission
                selected_r = (sample1 <= r_i) & active

                # Fill up the BSDFSample struct
                bs = mi.BSDFSample3f()
                bs.pdf = dr.select(selected_r, r_i, t_i)
                bs.sampled_component = dr.select(selected_r, mi.UInt32(0), mi.UInt32(1))
                bs.sampled_type = dr.select(
                    selected_r,
                    mi.UInt32(+mi.BSDFFlags.DeltaReflection),
                    mi.UInt32(+mi.BSDFFlags.DeltaTransmission),
                )
                bs.wo = dr.select(
                    selected_r,
                    mi.reflect(si.wi),
                    mi.refract(si.wi, cos_theta_t, eta_ti),
                )
                bs.eta = dr.select(selected_r, 1.0, eta_it)

                # For reflection, tint based on the incident angle (more tint at grazing angle)
                value_r = dr.lerp(
                    mi.Color3f(self.tint),
                    mi.Color3f(1.0),
                    dr.clip(cos_theta_i, 0.0, 1.0),
                )

                # For transmission, radiance must be scaled to account for the solid angle compression
                value_t = mi.Color3f(1.0) * dr.square(eta_ti)

                value = dr.select(selected_r, value_r, value_t)

                return (bs, value)

            def eval(self, ctx, si, wo, active):
                return 0.0

            def pdf(self, ctx, si, wo, active):
                return 0.0

            def eval_pdf(self, ctx, si, wo, active):
                return 0.0, 0.0

            def traverse(self, callback):
                callback.put("tint", self.tint, mi.ParamFlags.Differentiable)

            def parameters_changed(self, keys):
                print("🏝️ there is nothing to do here 🏝️")

            def to_string(self):
                return (
                    "MyBSDF[\n"
                    "    eta=%s,\n"
                    "    tint=%s,\n"
                    "]"
                    % (
                        self.eta,
                        self.tint,
                    )
                )

        mi.register_bsdf("mybsdf", lambda props: MyBSDF(props))

    if bsdf == "twosided":
        return {
            "type": "twosided",
            "material": {
                "type": "diffuse",
                "reflectance": {"type": "rgb", "value": 0.4},
            },
        }
    elif bsdf == "mask":
        return {
            "type": "mask",
            # Base material: a two-sided textured diffuse BSDF
            "material": {
                "type": "twosided",
                "bsdf": {
                    "type": "diffuse",
                    "reflectance": {
                        "type": "bitmap",
                        "filename": find_resource(
                            "resources/data/common/textures/wood.jpg"
                        ),
                    },
                },
            },
            # Fetch the opacity mask from a monochromatic texture
            "opacity": {
                "type": "bitmap",
                "filename": find_resource(
                    "resources/data/common/textures/leaf_mask.png"
                ),
            },
        }
    elif bsdf == "bumpmap":
        return {
            "type": "bumpmap",
            "arbitrary": {
                "type": "bitmap",
                "raw": True,
                "filename": find_resource(
                    "resources/data/common/textures/floor_tiles_bumpmap.png"
                ),
            },
            "bsdf": {"type": "roughplastic"},
        }
    elif bsdf == "normalmap":
        return {
            "type": "normalmap",
            "normalmap": {
                "type": "bitmap",
                "raw": True,
                "filename": find_resource(
                    "resources/data/common/textures/floor_tiles_normalmap.jpg"
                ),
            },
            "bsdf": {"type": "roughplastic"},
        }
    elif bsdf == "blendbsdf":
        return {
            "type": "blendbsdf",
            "weight": {
                "type": "bitmap",
                "filename": find_resource(
                    "resources/data/common/textures/noise_01.jpg"
                ),
            },
            "bsdf_0": {"type": "conductor"},
            "bsdf_1": {"type": "roughplastic", "diffuse_reflectance": 0.1},
        }
    elif bsdf == "diffuse":
        return {
            "type": "diffuse",
            "reflectance": {
                "type": "bitmap",
                "filename": find_resource(
                    "resources/data/common/textures/wood.jpg"
                ),
            },
        }
    elif bsdf == "custom":
        return {
            "type": "mybsdf",
            "tint": [0.2, 0.9, 0.2],
            "eta": 1.33,
        }
    else:
        return {
            "type": bsdf,
        }

@pytest.mark.parametrize("bsdf", BSDFS)
@pytest.mark.parametrize("auto_opaque", [False, True])
def test04_bsdf(variants_vec_rgb, tmp_path, bsdf, auto_opaque):
    """
    Tests that it is possible to freeze rendeirng a scene with each BSDF type.
    """
    w, h = (16, 16)
    n = 5

    def load_scene(bsdf: str):
        scene = mi.cornell_box()
        scene["sensor"]["film"]["width"] = w
        scene["sensor"]["film"]["height"] = h
        scene["white"] = bsdf_dict(bsdf)
        scene = mi.load_dict(scene, parallel=True)
        return scene

    scene2 = load_scene(bsdf)
    scene = load_scene(bsdf)
    assert_render(scene, n, tmp_path=tmp_path, auto_opaque=auto_opaque)


@pytest.mark.parametrize("bsdf", BSDFS)
@pytest.mark.parametrize("auto_opaque", [False, True])
def test05_bsdf_eval(variants_vec_rgb, bsdf, auto_opaque):
    """
    Tests that it is possible to evaluate a BSDF inside a frozen function.
    """
    n = 5

    def func(bsdf: mi.BSDF, uv: mi.Point2f) -> mi.Spectrum:
        with dr.profile_range("eval"):
            si = mi.SurfaceInteraction3f()
            si.t = 0.1
            si.dp_du = mi.Vector3f(0.5514372, 0.84608955, 0.41559092)
            si.dp_dv = mi.Vector3f(0.14551054, 0.54917541, 0.39286475)
            si.p = mi.Point3f(0, 0, 0)
            si.n = mi.Vector3f(0, 0, 1)
            si.sh_frame = mi.Frame3f(0, 0, 1)
            si.wi = mi.Vector3f(0, 0, 1)
            si.uv = uv

            wo = mi.Vector3f(0, 0, 1)
            ctx = mi.BSDFContext()

            return bsdf.eval(ctx, si, wo, active = True)

    frozen = dr.freeze(func, auto_opaque=auto_opaque)

    bsdf = mi.load_dict(bsdf_dict(bsdf))

    for i in range(n):
        uv = dr.full(mi.Point2f, 0.5, n + 2)

        res = frozen(bsdf, uv)
        ref = func(bsdf, uv)

        assert dr.allclose(res, ref)

    assert frozen.n_recordings > 0 and frozen.n_recordings <= 2


def emitter_dict(emitter: str):
    if emitter == "area":
        return {
            "type": "area",
            "radiance": {
                "type": "rgb",
                "value": [18.387, 13.9873, 6.75357],
            },
        }
    elif emitter == "point":
        return {
            "type": "point",
            "position": [0.0, 0.0, 0.0],
            "intensity": {
                "type": "rgb",
                "value": mi.ScalarColor3d(50, 50, 50),
            },
        }
    elif emitter == "constant":
        return {
            "type": "constant",
            "radiance": {
                "type": "rgb",
                "value": 1.0,
            },
        }
    elif emitter == "envmap":
        return {
            "type": "envmap",
            "filename": find_resource(
                "resources/data/scenes/matpreview/envmap.exr"
            ),
        }
    elif emitter == "spot":
        return {
            "type": "spot",
            "to_world": mi.ScalarTransform4f().look_at(
                origin=[0, 0, 0],
                target=[0, -1, 0],
                up=[0, 0, 1],
            ),
            "intensity": {
                "type": "rgb",
                "value": 1.0,
            },
        }
    elif emitter == "projector":
        return {
            "type": "projector",
            "to_world": mi.ScalarTransform4f().look_at(
                origin=[0, 0, 0],
                target=[0, -1, 0],
                up=[0, 0, 1],
            ),
            "fov": 45,
            "irradiance": {
                "type": "bitmap",
                "filename": find_resource(
                    "resources/data/common/textures/flower.bmp"
                ),
            },
        }
    elif emitter == "directional":
        return {
            "type": "directional",
            "direction": [0, 0, -1],
            "irradiance": {
                "type": "rgb",
                "value": 1.0,
            },
        }
    elif emitter == "directionalarea":
        return {
            "type": "directionalarea",
            "radiance": {
                "type": "rgb",
                "value": 1.0,
            },
        }

@pytest.mark.parametrize("emitter", EMITTERS)
@pytest.mark.parametrize("auto_opaque", [False, True])
def test06_emitter(variants_vec_rgb, tmp_path, emitter, auto_opaque):
    """
    Tests that it is possible to freeze rendeirng a scene with each emitter type.
    """
    w, h = (16, 16)
    n = 5

    def load_scene(emitter):
        scene = mi.cornell_box()
        scene["sensor"]["film"]["width"] = w
        scene["sensor"]["film"]["height"] = h
        scene["emitter"] = emitter_dict(emitter)
        scene = mi.load_dict(scene, parallel=True)
        return scene

    scene = load_scene(emitter)
    assert_render(scene, n, tmp_path = tmp_path, auto_opaque=auto_opaque)


@pytest.mark.parametrize("emitter", EMITTERS)
@pytest.mark.parametrize("auto_opaque", [False, True])
def test07_emitter_eval(variants_vec_rgb, emitter, auto_opaque):
    """
    Tests that it is possible to evaluate an emitter inside a frozen function.
    """
    n = 5

    def func(emitter: mi.Emitter, uv) -> mi.Spectrum:
        si = mi.SurfaceInteraction3f()
        si.t = 0.1
        si.dp_du = mi.Vector3f(0.5514372, 0.84608955, 0.41559092)
        si.dp_dv = mi.Vector3f(0.14551054, 0.54917541, 0.39286475)
        si.p = mi.Point3f(0, 0, 0)
        si.n = mi.Vector3f(0, 0, 1)
        si.sh_frame = mi.Frame3f(0, 0, 1)
        si.wi = mi.Vector3f(0, 0, 1)
        si.uv = uv

        return emitter.eval(si, True)

    frozen = dr.freeze(func, auto_opaque = auto_opaque)

    emitter = mi.load_dict(emitter_dict(emitter))

    for i in range(n):
        uv = dr.full(mi.Point2f, 0.5, n + 2)

        res = frozen(emitter, uv)
        ref = func(emitter, uv)

        assert dr.allclose(res, ref)

    assert frozen.n_recordings > 0 and frozen.n_recordings <= 2

def integrator_dict(integrator: str):
    if integrator == "path":
        return {
            "type": "path",
            "max_depth": 4,
        }
    elif integrator == "moment":
        return {
            "type": "moment",
            "nested": {
                "type": "path",
            },
        }
    elif integrator == "ptracer":
        return {
            "type": "ptracer",
            "max_depth": 8,
        }
    elif integrator == "aov":
        return {
            "type": "aov",
            "aovs": "dd.y:depth,nn:sh_normal",
            "my_image": {
                "type": "path",
            },
        }
    else:
        return {"type": integrator}

@pytest.mark.parametrize("integrator", INTEGRATORS)
@pytest.mark.parametrize("auto_opaque", [False, True])
def test08_integrators(variants_vec_rgb, tmp_path, integrator, auto_opaque):
    """
    Tests that it is possible to freeze rendering a scene with each integrator type.
    """
    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    w, h = (16, 16)
    n = 5

    def load_scene():
        scene = mi.cornell_box()
        scene["sensor"]["film"]["width"] = w
        scene["sensor"]["film"]["height"] = h
        scene["integrator"] = integrator_dict(integrator)
        scene = mi.load_dict(scene, parallel=True)
        return scene

    scene = load_scene()
    assert_render(scene, n, tmp_path = tmp_path, auto_opaque=auto_opaque)


def shape_dict(shape: str):
    if shape == "mesh":
        return {
            "type": "ply",
            "filename": find_resource("resources/data/common/meshes/teapot.ply"),
            "to_world": mi.ScalarTransform4f().scale(0.1),
        }
    elif shape == "disk":
        return {
            "type": "disk",
            "material": {
                "type": "diffuse",
                "reflectance": {
                    "type": "checkerboard",
                    "to_uv": mi.ScalarTransform4f().rotate(
                        axis=[1, 0, 0], angle=45
                    ),
                },
            },
        }
    elif shape == "cylinder":
        return {
            "type": "cylinder",
            "radius": 0.3,
            "material": {"type": "diffuse"},
            "to_world": mi.ScalarTransform4f().rotate(axis=[1, 0, 0], angle=10),
        }
    elif shape == "bsplinecurve":
        return {
            "type": "bsplinecurve",
            "to_world": mi.ScalarTransform4f()
            .scale(1.0)
            .rotate(axis=[0, 1, 0], angle=45),
            "filename": find_resource("resources/data/common/meshes/curve.txt"),
            "silhouette_sampling_weight": 1.0,
        }
    elif shape == "linearcurve":
        return {
            "type": "linearcurve",
            "to_world": mi.ScalarTransform4f()
            .translate([0, -1, 0])
            .scale(1)
            .rotate(axis=[0, 1, 0], angle=45),
            "filename": find_resource("resources/data/common/meshes/curve.txt"),
        }
    elif shape == "sdfgrid":
        return {
            "type": "sdfgrid",
            "bsdf": {"type": "diffuse"},
            "filename": find_resource(
                "resources/data/docs/scenes/sdfgrid/torus_sdfgrid.vol"
            ),
        }
    elif shape == "sphere":
        return {
            "type": "sphere",
            "center": [0, 0, 0],
            "radius": 0.5,
            "bsdf": {"type": "diffuse"},
        }

@pytest.mark.parametrize("shape", SHAPES)
@pytest.mark.parametrize("auto_opaque", [False, True])
def test09_shape(variants_vec_rgb, tmp_path, shape, auto_opaque):
    """
    Tests that it is possible to freeze rendeirng a scene with each shape type.
    """
    w, h = (16, 16)
    n = 5

    def load_scene():
        scene = mi.cornell_box()
        scene["sensor"]["film"]["width"] = w
        scene["sensor"]["film"]["height"] = h
        del scene["small-box"]
        del scene["large-box"]
        scene["shape"] = shape_dict(shape)
        scene = mi.load_dict(scene, parallel=True)
        return scene

    update = None
    if shape == "bsplinecurve" or shape == "linearcurve":
        def update(i, params):
            params["shape.control_points"][0] = i * 0.1
    elif shape == "sphere":
        # Moves the sphere, which re-records and rebinds its intersection
        # routine with a new data block
        def update(i, params):
            params["shape.to_world"] = mi.Transform4f().translate([0.1 * i, 0, 0])

    scene = load_scene()
    assert_render(scene, n, update, tmp_path=tmp_path, auto_opaque=auto_opaque)


@pytest.mark.parametrize("shape", SHAPES)
@pytest.mark.parametrize("auto_opaque", [False, True])
def test10_shape_sample_position(variants_vec_rgb, shape, auto_opaque):
    """
    Tests that it is possible to call ``sample_position`` on a shape inside a
    frozen function.
    """
    if shape == "bsplinecurve" or shape == "linearcurve":
        pytest.skip(
            "bsplinecurve and linearcurve do not implement ``sample_position`` for now."
        )

    n = 5

    def func(shape: mi.Shape, sample: mi.Point2f) -> mi.PositionSample3f:
        return shape.sample_position(0, sample)

    frozen = dr.freeze(func, auto_opaque = auto_opaque)

    shape = mi.load_dict(shape_dict(shape))

    for i in range(n):
        sampler: mi.Sampler = mi.load_dict({"type": "independent"})
        sampler.seed(i, i + 2)

        sample = sampler.next_2d()
        dr.make_opaque(sample)

        res = frozen(shape, sample)
        ref = func(shape, sample)

        assert dr.allclose(res.n, ref.n)
        assert dr.allclose(res.p, ref.p)
        assert dr.allclose(res.pdf, ref.pdf)
        assert dr.allclose(res.time, ref.time)
        assert dr.allclose(res.uv, ref.uv)
        assert dr.all(res.delta == ref.delta)


# TODO: add rmsprop
@pytest.mark.parametrize("optimizer", ["sgd", "rmsprop", "adam"])
@pytest.mark.parametrize("auto_opaque", [False, True])
def test11_optimizer(variants_vec_rgb, optimizer, auto_opaque):
    """
    Tests optimizing a non-geometric scene parameter using different optimizers
    in a frozen function. This also updates the parameters in the frozen function,
    which is not possible if the acceleration structure whould have to be updated.
    """
    w, h = (16, 16)
    n = 10
    k = "red.reflectance.value"

    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    def optimize(scene, opt, image_ref):
        params = mi.traverse(scene)
        params.update(opt)

        image = mi.render(scene, params, spp=1)

        loss = mse(image, image_ref)

        dr.backward(loss)

        opt.step()

        return image, loss

    frozen = dr.freeze(optimize, auto_opaque = auto_opaque)

    def run(n: int, optimize):
        scene = mi.cornell_box()
        scene["integrator"] = {
            "type": "prb",
        }
        scene["sensor"]["film"]["width"] = w
        scene["sensor"]["film"]["height"] = h
        scene = mi.load_dict(scene, parallel=True)

        image_ref = mi.render(scene, spp=512)

        params = mi.traverse(scene)
        params.keep(k)

        if optimizer == "adam":
            opt = mi.ad.Adam(lr=0.05)
        elif optimizer == "rmsprop":
            opt = dr.opt.RMSProp(lr = 0.001)
        elif optimizer == "sgd":
            opt = mi.ad.SGD(lr=0.005, momentum=0.1)
        opt[k] = mi.Color3f(0.01, 0.2, 0.9)

        for i in range(n):
            image, loss = optimize(scene, opt, image_ref)

        return image, opt[k]

    image_ref, param_ref = run(n, optimize)

    image_frozen, param_frozen = run(n, frozen)

    # The first call enables gradients on a scene parameter, which changes the
    # structure of the input and is captured by a second recording
    assert frozen.n_recordings == 2

    # Optimizing the reflectance is not as prone to divergence,
    # therefore we can test if the two methods produce the same results
    assert dr.allclose(param_ref, param_frozen)


@pytest.mark.parametrize(
    "medium",
    [
        "homogeneous",
        "heterogeneous",
    ],
)
@pytest.mark.parametrize("auto_opaque", [False, True])
def test12_medium(variants_vec_rgb, tmp_path, medium, auto_opaque):
    """
    Tests that it is possible to freeze rendeirng a scene with each medium type.
    """
    w, h = (16, 16)
    n = 5

    def load_scene():
        scene = mi.cornell_box()
        scene["sensor"]["film"]["width"] = w
        scene["sensor"]["film"]["height"] = h
        scene["integrator"] = {"type": "volpath"}
        scene["sensor"]["medium"] = {"type": "ref", "id": "fog"}

        if medium == "homogeneous":
            scene["sensor"]["medium"] = {
                "type": "homogeneous",
                "albedo": {"type": "rgb", "value": [0.99, 0.9, 0.96]},
                "sigma_t": {
                    "type": "rgb",
                    "value": [0.5, 0.25, 0.8],
                },
            }
        elif medium == "heterogeneous":
            scene["sensor"]["medium"] = {
                "type": "heterogeneous",
                "albedo": {"type": "rgb", "value": [0.99, 0.9, 0.96]},
                "sigma_t": {
                    "type": "gridvolume",
                    "filename": find_resource(
                        "resources/data/tests/scenes/participating_media/textures/albedo.vol"
                    ),
                },
            }

        scene = mi.load_dict(scene, parallel=True)
        return scene

    scene = load_scene()
    assert_render(scene, n, tmp_path=tmp_path, auto_opaque=auto_opaque)


@pytest.mark.parametrize(
    "sampler",
    [
        "independent",
        "stratified",
        "multijitter",
        "orthogonal",
        "ldsampler",
    ],
)
@pytest.mark.parametrize("auto_opaque", [False, True])
def test13_sampler(variants_vec_rgb, tmp_path, sampler, auto_opaque):
    """
    Tests that it is possible to freeze rendeirng a scene with each sampler type.
    """
    w, h = (16, 16)
    n = 5

    def load_scene():
        scene = mi.cornell_box()
        scene["sensor"]["film"]["width"] = w
        scene["sensor"]["film"]["height"] = h

        scene["sensor"]["sampler"] = {"type": sampler}

        scene = mi.load_dict(scene, parallel=True)
        return scene

    scene = load_scene()
    # spp of 4 to suppress warning
    assert_render(scene, n, tmp_path=tmp_path, spp = 4, auto_opaque = auto_opaque)

@pytest.mark.parametrize(
    "integrator",
    [
        # Octree guiding for indirect silhouettes is not yet supported
        "direct_projective",
        "prb_projective",
    ],
)
@pytest.mark.parametrize("auto_opaque", [False, True])
def test14_unsupported_integrators(variants_vec_rgb, integrator, auto_opaque):
    """
    Tests that using projective integrators with octree guiding in backward mode triggers an exception.
    """

    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    @dr.freeze(auto_opaque=auto_opaque)
    def frozen(scene, integrator):
        params = mi.traverse(scene)

        img = mi.render(scene, integrator=integrator, params=params)

        loss = dr.mean(dr.square(img))

        dr.backward(loss)

    scene = mi.cornell_box()
    scene = mi.load_dict(scene)

    integrator = mi.load_dict({
        "type": integrator
    })

    params = mi.traverse(scene)
    dr.enable_grad(params["red-wall.to_world"])
    params.update()

    with pytest.raises(RuntimeError):
        frozen(scene, integrator)


@pytest.mark.parametrize("integrator_name", ["direct_projective", "prb_projective"])
@pytest.mark.parametrize("guiding", ["none", "grid"])
@pytest.mark.parametrize("auto_opaque", [False, True])
def test15_projective_integrators(
    variants_all_ad_rgb, integrator_name, guiding, auto_opaque
):
    """
    Tests freezing and replaying projective integrators in backward mode with
    uniform and grid guiding across varying mesh silhouette counts.
    """
    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    scene_dict = mi.cornell_box()
    del scene_dict["large-box"]
    del scene_dict["small-box"]
    scene_dict["sensor"]["film"]["width"] = 8
    scene_dict["sensor"]["film"]["height"] = 8
    scene_dict["light"]["to_world"] = (
        scene_dict["light"]["to_world"] @ mi.ScalarTransform4f().scale(3.0)
    )
    scene_dict["bunny"] = {
        "type": "ply",
        "filename": find_resource("resources/data/common/meshes/bunny.ply"),
        "to_world": mi.ScalarTransform4f().translate([0.0, -0.2, 0.0]).scale(5.0),
        "bsdf": {
            "type": "diffuse",
            "reflectance": {"type": "rgb", "value": (0.3, 0.3, 0.75)},
        },
    }
    scene = mi.load_dict(scene_dict)

    def make_integrator():
        integ = mi.load_dict({
            "type": integrator_name,
            "guiding": guiding,
            "sppc": 2,
            "sppp": 4,
            "sppi": 4,
        })
        integ.guiding_grid_reso = [8, 8, 8]
        integ.proj_seed_spp = 0
        return integ

    integrator_frz = make_integrator()
    integrator_ref = make_integrator()

    params = mi.traverse(scene)
    initial_positions = mi.Point3f(
        dr.detach(params["bunny.positions"]), flip_axes=True
    )

    def eval_grad(scene, integrator):
        params = mi.traverse(scene)
        params.keep("bunny.positions")
        img = mi.render(
            scene, params=params, integrator=integrator, seed=1, seed_grad=2
        )
        loss = dr.mean(dr.square(img))
        dr.backward(loss)
        return img

    frozen = dr.freeze(eval_grad, auto_opaque=auto_opaque)

    angles = [0.0, 25.0, -20.0]
    for angle in angles:
        trafo = mi.Transform4f().rotate([0, 1, 0], angle)
        rotated = mi.TensorXf(trafo @ initial_positions, flip_axes=True)

        pos_frz = mi.TensorXf(rotated)
        dr.enable_grad(pos_frz)
        params["bunny.positions"] = pos_frz
        params.update()
        img_frz = frozen(scene, integrator_frz)
        grad_frz = mi.TensorXf(dr.grad(pos_frz))

        pos_ref = mi.TensorXf(rotated)
        dr.enable_grad(pos_ref)
        params["bunny.positions"] = pos_ref
        params.update()
        img_ref = eval_grad(scene, integrator_ref)
        grad_ref = mi.TensorXf(dr.grad(pos_ref))

        assert dr.allclose(img_ref, img_frz, atol=1e-4, rtol=1e-4)
        assert dr.allclose(grad_ref, grad_frz, atol=1e-3, rtol=1e-3)
        assert dr.any(grad_ref != 0)

    assert frozen.n_cached_recordings == 1
    if dr.backend_v(mi.Float) == dr.JitBackend.CUDA:
        # Lazy initialization of Mesh::m_dedge / m_sil_dedge_pmf on the first
        # call triggers one InputChanged re-recording; subsequent calls replay.
        assert frozen.n_recordings <= 2
    else:
        # On LLVM, scatter_reduce + dr.compress cannot infer output width
        # during dry run and falls back to re-tracing each call.
        assert frozen.n_recordings <= 1 + len(angles)


@pytest.mark.parametrize("integrator_name", ["direct_projective", "prb_projective"])
@pytest.mark.parametrize("auto_opaque", [False, True])
def test16_projective_moving_sensor(
    variants_all_ad_rgb, integrator_name, auto_opaque
):
    """
    Tests that updating sensor.to_world across calls to a frozen projective
    integrator updates primary silhouette precomputation rather than baking a
    constant viewpoint.
    """
    if mi.is_polarized:
        pytest.skip('Test must be adapted to polarized rendering.')

    scene_dict = mi.cornell_box()
    del scene_dict["large-box"]
    del scene_dict["small-box"]
    scene_dict["sensor"]["film"]["width"] = 8
    scene_dict["sensor"]["film"]["height"] = 8
    scene_dict["light"]["to_world"] = (
        scene_dict["light"]["to_world"] @ mi.ScalarTransform4f().scale(3.0)
    )
    scene_dict["bunny"] = {
        "type": "ply",
        "filename": find_resource("resources/data/common/meshes/bunny.ply"),
        "to_world": mi.ScalarTransform4f().translate([0.0, -0.2, 0.0]).scale(5.0),
        "bsdf": {
            "type": "diffuse",
            "reflectance": {"type": "rgb", "value": (0.3, 0.3, 0.75)},
        },
    }
    scene_dict["pillar"] = {
        "type": "cylinder",
        "p0": [-0.55, -1.0, -0.2],
        "p1": [-0.55, 0.2, -0.2],
        "radius": 0.15,
        "bsdf": {
            "type": "diffuse",
            "reflectance": {"type": "rgb", "value": (0.7, 0.4, 0.2)},
        },
    }
    scene = mi.load_dict(scene_dict)

    def make_integrator():
        integ = mi.load_dict({
            "type": integrator_name,
            "guiding": "grid",
            "sppc": 2,
            "sppp": 8,
            "sppi": 2,
        })
        integ.guiding_grid_reso = [8, 8, 8]
        integ.proj_seed_spp = 0
        return integ

    integrator_frz = make_integrator()
    integrator_ref = make_integrator()

    params = mi.traverse(scene)
    base_positions = mi.TensorXf(dr.detach(params["bunny.positions"]))
    base_pillar = mi.Transform4f(dr.detach(params["pillar.to_world"]))

    def eval_grad(scene, integrator):
        params = mi.traverse(scene)
        params.keep(["bunny.positions", "pillar.to_world"])
        img = mi.render(
            scene, params=params, integrator=integrator, seed=1, seed_grad=2
        )
        loss = dr.mean(dr.square(img))
        dr.backward(loss)
        return img

    frozen = dr.freeze(eval_grad, auto_opaque=auto_opaque)

    origins = [
        [0.0, 0.0, 3.8],
        [1.2, 0.4, 3.5],
        [-1.1, -0.3, 3.6],
    ]
    for origin in origins:
        sensor_trafo = mi.Transform4f().look_at(
            origin=origin, target=[0.0, -0.1, 0.0], up=[0.0, 1.0, 0.0]
        )

        pos_frz = mi.TensorXf(base_positions)
        pil_frz = mi.Transform4f(base_pillar)
        dr.enable_grad(pos_frz, pil_frz)
        params["sensor.to_world"] = mi.Transform4f(sensor_trafo)
        params["bunny.positions"] = pos_frz
        params["pillar.to_world"] = pil_frz
        params.update()
        img_frz = frozen(scene, integrator_frz)
        grad_pos_frz = mi.TensorXf(dr.grad(pos_frz))
        grad_pil_frz = mi.Matrix4f(dr.grad(pil_frz).matrix)

        pos_ref = mi.TensorXf(base_positions)
        pil_ref = mi.Transform4f(base_pillar)
        dr.enable_grad(pos_ref, pil_ref)
        params["sensor.to_world"] = mi.Transform4f(sensor_trafo)
        params["bunny.positions"] = pos_ref
        params["pillar.to_world"] = pil_ref
        params.update()
        img_ref = eval_grad(scene, integrator_ref)
        grad_pos_ref = mi.TensorXf(dr.grad(pos_ref))
        grad_pil_ref = mi.Matrix4f(dr.grad(pil_ref).matrix)

        assert dr.allclose(img_ref, img_frz, atol=1e-4, rtol=1e-4)
        assert dr.allclose(grad_pos_ref, grad_pos_frz, atol=1e-3, rtol=1e-3)
        assert dr.allclose(grad_pil_ref, grad_pil_frz, atol=1e-3, rtol=1e-3)
        assert dr.any(grad_pos_ref != 0, axis=None)
        assert dr.any(grad_pil_ref != 0, axis=None)

    assert frozen.n_cached_recordings == 1
    if dr.backend_v(mi.Float) == dr.JitBackend.CUDA:
        assert frozen.n_recordings <= 2
    else:
        assert frozen.n_recordings <= 1 + len(origins)


