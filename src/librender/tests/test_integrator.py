"""Tests common to all integrator implementations."""

import numpy as np
import pytest
import warnings

from mitsuba.test.scenes import empty_scene, teapot_scene, make_integrator

integrators = [
    'int_name', [
        "depth",
        # "direct"
    ]
]

def check_scene(integrator, scene):
    # Scalar render
    assert integrator.render_scalar(scene) is True
    res_scalar = np.array(scene.film().bitmap(), copy=True)
    scene.film().clear()
    assert np.all(np.array(scene.film().bitmap(), copy=False) == 0)

    try:
        # Vectorized render
        assert integrator.render_vector(scene) is True
        res_vector = np.array(scene.film().bitmap(), copy=True)

        # TODO: check they match
        # assert np.allclose(res_scalar[:, :, :3], res_vector[:, :, :3])
    except RuntimeError as e:
        warnings.warn("Vectorized render failed with exception: %s" % str(e))
        res_vector = None

    return (res_scalar, res_vector)

@pytest.mark.parametrize(*integrators)
def test01_create(int_name):
    integrator = make_integrator(int_name)
    assert integrator is not None

    if int_name == "direct":
        # These properties should be queried
        integrator = make_integrator(int_name, """
                <integer name="emitter_samples" value="3"/>
                <integer name="bsdf_samples" value="12"/>
                <boolean name="strict_normals" value="true"/>
                <boolean name="hide_emitters" value="true"/>
            """)
        # Cannot specify both shading_samples and (emitter_samples | bsdf_samples)
        with pytest.raises(RuntimeError):
            integrator = make_integrator(int_name, """
                    <integer name="shading_samples" value="3"/>
                    <integer name="emitter_samples" value="5"/>
                """)

@pytest.mark.parametrize(*integrators)
def test02_render_empty_scene(int_name, empty_scene):
    integrator = make_integrator(int_name)
    integrator.configure_sampler(empty_scene, empty_scene.sampler())

    (res, _) = check_scene(integrator, empty_scene)
    assert np.all(res[:, :, :3] == 0)   # Pixel values
    assert np.all(res[:, :, 3]  >= 0)   # Reconstruction weights

@pytest.mark.parametrize(*integrators)
def test03_render_teapot(int_name, teapot_scene):
    integrator = make_integrator(int_name)
    integrator.configure_sampler(teapot_scene, teapot_scene.sampler())

    (res, _) = check_scene(integrator, teapot_scene)
    n = res.shape[0] * res.shape[1]

    # TODO: replace by a proper reference image once the implementation has stabilized.
    nnz = np.sum(np.sum(res[:, :, :3], axis=2) > 1e-7)
    assert (nnz >= 0.20 * n) and (nnz < 0.4 * n)

from mitsuba.core.xml import load_string
from mitsuba.test.util import fresolver_append_path

@fresolver_append_path
def test04_render_direct():
    scene = load_string("""
        <scene version='2.0.0'>
            <sensor type="perspective">
                <float name="near_clip" value="1"/>
                <float name="far_clip" value="1000"/>

                <transform name="to_world">
                    <lookat target="0.0, 0.0, 1.5"
                            origin="0.0, -14.0, 1.5"
                            up    ="0.0, 0.0, 1.0"/>
                </transform>

                <film type="hdrfilm">
                    <integer name="width" value="396"/>
                    <integer name="height" value="216"/>
                </film>

                <sampler type="independent">
                    <integer name="sample_count" value="16"/>
                </sampler>
            </sensor>

            <shape type="ply">
                <string name="filename"
                        value="resources/data/ply/teapot.ply"/>

                <bsdf type="diffuse">
                    <spectrum name="radiance" value="0.1f, 0.3f, 1.0f, 0.5f"/>
                </bsdf>

            </shape>

            <emitter type="point">
                <point name="position" x="2" y="-6.0" z="4.5"/>
                <spectrum name="intensity" value="3.0f, 3.0f, 3.0f, 0.5f"/>
            </emitter>
            <emitter type="point">
                <point name="position" x="-3" y="-3" z="-0.5"/>
                <spectrum name="intensity" value="1.0f, 0.05f, 0.0f, 0.5f"/>
            </emitter>
        </scene>
    """)

    integrator = make_integrator("direct", xml="""
            <integer name="emitter_samples" value="4"/>
            <integer name="bsdf_samples" value="0"/>
        """)
    integrator.configure_sampler(scene, scene.sampler())
    check_scene(integrator, scene)
