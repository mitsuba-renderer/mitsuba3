from typing import Any, Callable, List, Optional, Tuple
import pytest
import drjit as dr
import mitsuba as mi
import numpy as np
from os.path import join, realpath, dirname, basename, splitext, exists

from mitsuba.scalar_rgb.test.util import find_resource

TUTORIALS_DIR = realpath(join(dirname(__file__), "../../../tutorials"))
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

def run_assert(input, n, func, update: Optional[Callable] = None, n_recordings=2):
    frozen = dr.freeze(func)

    for i in range(n):
        if update:
            params = mi.traverse(input)
            update(i, params)
            params.update()

        res = frozen(input)
        ref = func(input)

        assert dr.allclose(ref, res)
    assert frozen.n_recordings == n_recordings

def test01_cornell_box(variants_vec_rgb):
    w, h = (16, 16)
    n = 5
    k = "light.emitter.radiance.value"

    def func(scene: mi.Scene) -> mi.TensorXf:
        with dr.profile_range("render"):
            result = mi.render(scene, spp=1)
        return result

    scene = mi.cornell_box()
    scene["sensor"]["film"]["width"] = w
    scene["sensor"]["film"]["height"] = h
    scene = mi.load_dict(scene, parallel=True)

    params = mi.traverse(scene)
    value = mi.Float(params[k].x)

    def update(i, params):
        params[k].x = value + 10.0 * i

    run_assert(scene, n, func, update)


def test02_cornell_box_native(variants_vec_rgb):
    if mi.MI_ENABLE_EMBREE:
        pytest.skip("Embree enabled, cannot test the native KD-Tree")

    w, h = (16, 16)
    n = 5
    k = "light.emitter.radiance.value"

    def func(scene: mi.Scene) -> mi.TensorXf:
        with dr.profile_range("render"):
            result = mi.render(scene, spp=1)
        return result

    scene = mi.cornell_box()
    scene["sensor"]["film"]["width"] = w
    scene["sensor"]["film"]["height"] = h
    scene = mi.load_dict(scene, parallel=True)

    params = mi.traverse(scene)
    value = mi.Float(params[k].x)

    def update(i, params):
        params[k].x = value + 10.0 * i

    run_assert(scene, n, func, update)


@pytest.mark.parametrize(
    "integrator",
    [
        "direct",
        "prb",
        "prb_basic",
        "direct_projective",
        "prb_projective",
    ],
)
def test02_pose_estimation(variants_vec_rgb, integrator):
    w, h = (16, 16)
    n = 10

    def apply_transformation(initial_vertex_positions, opt, params):
        opt["trans"] = dr.clip(opt["trans"], -0.5, 0.5)
        opt["angle"] = dr.clip(opt["angle"], -0.5, 0.5)

        trafo = (
            mi.Transform4f()
            .translate([opt["trans"].x, opt["trans"].y, 0.0])
            .rotate([0, 1, 0], opt["angle"] * 100.0)
        )

        params["bunny.vertex_positions"] = dr.ravel(trafo @ initial_vertex_positions)

    def mse(image, image_ref):
        return dr.sum(dr.square(image - image_ref), axis=None)

    def optimize(scene, ref, initial_vertex_positions, other):
        params = mi.traverse(scene)

        image = mi.render(scene, params, spp=1, seed=1, seed_grad=2)

        # Evaluate the objective function from the current rendered image
        loss = mse(image, ref)

        # Backpropagate through the rendering process
        dr.backward(loss)

        return image, loss

    frozen = dr.freeze(optimize)

    def load_scene():
        from mitsuba.scalar_rgb import Transform4f as T

        scene = mi.cornell_box()
        del scene["large-box"]
        del scene["small-box"]
        del scene["green-wall"]
        del scene["red-wall"]
        del scene["floor"]
        del scene["ceiling"]
        scene["bunny"] = {
            "type": "ply",
            "filename": f"{TUTORIALS_DIR}/scenes/meshes/bunny.ply",
            "to_world": T().scale(6.5),
            "bsdf": {
                "type": "diffuse",
                "reflectance": {"type": "rgb", "value": (0.3, 0.3, 0.75)},
            },
        }
        scene["integrator"] = {
            "type": "prb",
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

    def run(scene: mi.Scene, optimize, n) -> Tuple[mi.TensorXf, mi.Point3f, mi.Float]:
        params = mi.traverse(scene)

        params.keep("bunny.vertex_positions")
        initial_vertex_positions = dr.unravel(
            mi.Point3f, params["bunny.vertex_positions"]
        )

        image_ref = mi.render(scene, spp=4)

        opt = mi.ad.Adam(lr=0.025)
        opt["angle"] = mi.Float(0.25)
        opt["trans"] = mi.Point3f(0.1, -0.25, 0.0)

        for i in range(n):
            params = mi.traverse(scene)
            params.keep("bunny.vertex_positions")

            apply_transformation(initial_vertex_positions, opt, params)

            with dr.profile_range("optimize"):
                image, loss = optimize(
                    scene,
                    image_ref,
                    initial_vertex_positions,
                    [
                        params["bunny.vertex_positions"],
                    ],
                )

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
    initial_vertex_positions = mi.Float(params["bunny.vertex_positions"])

    img_ref, trans_ref, angle_ref = run(scene, optimize, n)

    # Reset parameters:
    params["bunny.vertex_positions"] = initial_vertex_positions
    params.update()

    img_frozen, trans_frozen, angle_frozen = run(scene, frozen, n)

    # NOTE: cannot compare results as errors accumulate and the result will never be the same.

    assert dr.allclose(trans_ref, trans_frozen)
    assert dr.allclose(angle_ref, angle_frozen)
    assert frozen.n_recordings == 2
    if integrator != "prb_projective":
        assert dr.allclose(img_ref, img_frozen, atol=1e-4)


def test03_optimize_color(variants_vec_rgb):
    k = "red.reflectance.value"
    w, h = (16, 16)
    n = 10

    def mse(image, image_ref):
        return dr.sum(dr.square(image - image_ref), axis=None)

    def optimize(scene, image_ref):
        params = mi.traverse(scene)
        params.keep(k)

        image = mi.render(scene, params, spp=1)

        loss = mse(image, image_ref)

        dr.backward(loss)

        return image, loss

    frozen = dr.freeze(optimize)

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

    assert frozen.n_recordings == 2
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
                if props.has_property("eta"):
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
                callback.put_parameter("tint", self.tint, mi.ParamFlags.Differentiable)

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
def test04_bsdf(variants_vec_rgb, bsdf):
    w, h = (16, 16)
    n = 5

    def load_scene(bsdf: str):
        scene = mi.cornell_box()
        scene["sensor"]["film"]["width"] = w
        scene["sensor"]["film"]["height"] = h
        scene["white"] = bsdf_dict(bsdf)
        scene = mi.load_dict(scene, parallel=True)
        return scene

    def func(scene: mi.Scene) -> mi.TensorXf:
        with dr.profile_range("render"):
            result = mi.render(scene, spp=1)
        return result

    scene2 = load_scene(bsdf)
    scene = load_scene(bsdf)
    run_assert(scene, n, func)

@pytest.mark.parametrize("bsdf", BSDFS)
def test05_bsdf_eval(variants_vec_rgb, bsdf):
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

    frozen = dr.freeze(func)

    bsdf = mi.load_dict(bsdf_dict(bsdf))

    for i in range(n):
        uv = dr.full(mi.Point2f, 0.5, n + 2)

        res = frozen(bsdf, uv)
        ref = func(bsdf, uv)

        assert dr.allclose(res, ref)

    assert frozen.n_recordings > 0 and frozen.n_recordings <= 2

@pytest.mark.parametrize(
    "emitter",
    [
        "area",
        "point",
        "constant",
        "envmap",
        "spot",
        "projector",
        "directional",
        "directionalarea",
    ],
)
def test06_emitter(variants_vec_rgb, emitter):
    w, h = (16, 16)
    n = 5

    def func(scene: mi.Scene) -> mi.TensorXf:
        with dr.profile_range("render"):
            result = mi.render(scene, spp=1)
        return result

    def load_scene(emitter):
        scene = mi.cornell_box()
        scene["sensor"]["film"]["width"] = w
        scene["sensor"]["film"]["height"] = h

        if emitter == "point":
            scene["emitter"] = {
                "type": "point",
                "position": [0.0, 0.0, 0.0],
                "intensity": {
                    "type": "rgb",
                    "value": mi.ScalarColor3d(50, 50, 50),
                },
            }
        elif emitter == "constant":
            scene["emitter"] = {
                "type": "constant",
                "radiance": {
                    "type": "rgb",
                    "value": 1.0,
                },
            }
        elif emitter == "envmap":
            scene["emitter"] = {
                "type": "envmap",
                "filename": find_resource(
                    "resources/data/scenes/matpreview/envmap.exr"
                ),
            }
        elif emitter == "spot":
            scene["emitter"] = {
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
            scene["emitter"] = {
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
            scene["emitter"] = {
                "type": "directional",
                "direction": [0, 0, -1],
                "irradiance": {
                    "type": "rgb",
                    "value": 1.0,
                },
            }
        elif emitter == "directionalarea":
            scene["light"]["emitter"] = {
                "type": "directionalarea",
                "radiance": {
                    "type": "rgb",
                    "value": 1.0,
                },
            }

        scene = mi.load_dict(scene, parallel=True)
        return scene

    scene = load_scene(emitter)
    run_assert(scene, n, func)


@pytest.mark.parametrize(
    "integrator",
    [
        "direct",
        "path",
        "prb",
        "prb_basic",
        "direct_projective",
        "prb_projective",
        "moment",
        "ptracer",
        "depth",
    ],
)
def test07_integrators(variants_vec_rgb, integrator):
    w, h = (16, 16)
    n = 5

    def func(scene: mi.Scene) -> mi.TensorXf:
        with dr.profile_range("render"):
            result = mi.render(scene, spp=1)
        return result

    def load_scene():
        scene = mi.cornell_box()
        scene["sensor"]["film"]["width"] = w
        scene["sensor"]["film"]["height"] = h

        if integrator == "path":
            scene["integrator"] = {
                "type": "path",
                "max_depth": 4,
            }
        elif integrator == "moment":
            scene["integrator"] = {
                "type": "moment",
                "nested": {
                    "type": "path",
                },
            }
        elif integrator == "ptracer":
            scene["integrator"] = {
                "type": "ptracer",
                "max_depth": 8,
            }
        else:
            scene["integrator"] = {"type": integrator}

        scene = mi.load_dict(scene, parallel=True)
        return scene

    scene = load_scene()
    run_assert(scene, n, func)


@pytest.mark.parametrize(
    "shape",
    [
        "mesh",
        "disk",
        "cylinder",
        "bsplinecurve",
        "linearcurve",
        "sdfgrid",
        # "instance",
        "sphere",
    ],
)
def test08_shape(variants_vec_rgb, shape):
    w, h = (16, 16)
    n = 5

    def func(scene: mi.Scene) -> mi.TensorXf:
        with dr.profile_range("render"):
            result = mi.render(scene, spp=1)
        return result

    def load_scene():
        from mitsuba.scalar_rgb import Transform4f as T

        scene = mi.cornell_box()
        scene["sensor"]["film"]["width"] = w
        scene["sensor"]["film"]["height"] = h
        del scene["small-box"]
        del scene["large-box"]

        if shape == "mesh":
            scene["shape"] = {
                "type": "ply",
                "filename": find_resource("resources/data/common/meshes/teapot.ply"),
                "to_world": T().scale(0.1),
            }
        elif shape == "disk":
            scene["shape"] = {
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
            scene["shape"] = {
                "type": "cylinder",
                "radius": 0.3,
                "material": {"type": "diffuse"},
                "to_world": mi.ScalarTransform4f().rotate(axis=[1, 0, 0], angle=10),
            }
        elif shape == "bsplinecurve":
            scene["shape"] = {
                "type": "bsplinecurve",
                "to_world": mi.ScalarTransform4f()
                .scale(1.0)
                .rotate(axis=[0, 1, 0], angle=45),
                "filename": find_resource("resources/data/common/meshes/curve.txt"),
                "silhouette_sampling_weight": 1.0,
            }
        elif shape == "linearcurve":
            scene["shape"] = {
                "type": "linearcurve",
                "to_world": mi.ScalarTransform4f()
                .translate([0, -1, 0])
                .scale(1)
                .rotate(axis=[0, 1, 0], angle=45),
                "filename": find_resource("resources/data/common/meshes/curve.txt"),
            }
        elif shape == "sdfgrid":
            scene["shape"] = {
                "type": "sdfgrid",
                "bsdf": {"type": "diffuse"},
                "filename": find_resource(
                    "resources/data/docs/scenes/sdfgrid/torus_sdfgrid.vol"
                ),
            }
        elif shape == "instance":
            scene["sg"] = {
                "type": "shapegroup",
                "first_object": {
                    "type": "ply",
                    "filename": find_resource(
                        "resources/data/common/meshes/teapot.ply"
                    ),
                    "bsdf": {
                        "type": "roughconductor",
                    },
                    "to_world": mi.ScalarTransform4f()
                    .translate([0.5, 0, 0])
                    .scale([0.2, 0.2, 0.2]),
                },
                "second_object": {
                    "type": "sphere",
                    "to_world": mi.ScalarTransform4f()
                    .translate([-0.5, 0, 0])
                    .scale([0.2, 0.2, 0.2]),
                    "bsdf": {
                        "type": "diffuse",
                    },
                },
            }
            scene["first_instance"] = {
                "type": "instance",
                "shapegroup": {"type": "ref", "id": "sg"},
            }
        elif shape == "sphere":
            scene["shape"] = {
                "type": "sphere",
                "center": [0, 0, 0],
                "radius": 0.5,
                "bsdf": {"type": "diffuse"},
            }

        scene = mi.load_dict(scene, parallel=True)
        return scene

    update = None
    if shape == "bsplinecurve" or shape == "linearcurve":
        def update(i, params):
            params["shape.control_points"][0] = i * 0.1

    scene = load_scene()
    run_assert(scene, n, func, update)


@pytest.mark.parametrize("optimizer", ["sgd", "adam"])
def test08_optimizer(variants_vec_rgb, optimizer):
    w, h = (16, 16)
    n = 10
    k = "red.reflectance.value"

    def mse(image, image_ref):
        return dr.sum(dr.square(image - image_ref), axis=None)

    def optimize(scene, opt, image_ref):
        params = mi.traverse(scene)
        params.update(opt)

        image = mi.render(scene, params, spp=1)

        loss = mse(image, image_ref)

        dr.backward(loss)

        opt.step()

        return image, loss

    frozen = dr.freeze(optimize)

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
        elif optimizer == "sgd":
            opt = mi.ad.SGD(lr=0.005, momentum=0.1)
        opt[k] = mi.Color3f(0.01, 0.2, 0.9)

        for i in range(n):
            image, loss = optimize(scene, opt, image_ref)

        return image, opt[k]

    image_ref, param_ref = run(n, optimize)

    image_frozen, param_frozen = run(n, frozen)
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
def test09_medium(variants_vec_rgb, medium):
    w, h = (16, 16)
    n = 5

    def func(scene: mi.Scene) -> mi.TensorXf:
        with dr.profile_range("render"):
            result = mi.render(scene, spp=1)
        return result

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
                        "resources/data/docs/scenes/textures/albedo.vol"
                    ),
                },
            }

        scene = mi.load_dict(scene, parallel=True)
        return scene

    scene = load_scene()
    run_assert(scene, n, func)


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
def test10_sampler(variants_vec_rgb, sampler):
    w, h = (16, 16)
    n = 5

    def func(scene: mi.Scene) -> mi.TensorXf:
        with dr.profile_range("render"):
            # spp of 4 to suppress warning
            result = mi.render(scene, spp=4)
        return result

    def load_scene():
        scene = mi.cornell_box()
        scene["sensor"]["film"]["width"] = w
        scene["sensor"]["film"]["height"] = h

        scene["sensor"]["sampler"] = {"type": sampler}

        scene = mi.load_dict(scene, parallel=True)
        return scene

    scene = load_scene()
    run_assert(scene, n, func)

