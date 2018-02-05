"""Tests common to all integrator implementations."""

import numpy as np
import pytest
import warnings

from mitsuba.test.scenes import empty_scene, teapot_scene, make_integrator

integrators = [
    'int_name', [
        "depth",
        "direct",
        "path"
    ]
]

def check_scene(integrator, scene):
    # Scalar render
    assert integrator.render(scene, vectorize=False) is True
    res_scalar = np.array(scene.film().bitmap(), copy=True)

    # Vectorized render
    scene.film().clear()
    assert integrator.render(scene, vectorize=True) is True
    res_vector = np.array(scene.film().bitmap(), copy=True)

    # Check vector and scalar match
    # TODO: much stronger verification
    # n = float(np.prod(scene.film().size()))
    # scalar_sum = np.sum(res_scalar[:, :, :3]) / n
    # vector_sum = np.sum(res_vector[:, :, :3]) / n
    # assert np.allclose(scalar_sum, vector_sum, rtol=1e-2), \
    #        "Scalar and vector renders do not match."

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
            """)
        # Cannot specify both shading_samples and (emitter_samples | bsdf_samples)
        with pytest.raises(RuntimeError):
            integrator = make_integrator(int_name, """
                    <integer name="shading_samples" value="3"/>
                    <integer name="emitter_samples" value="5"/>
                """)
    elif int_name == "path":
        # These properties should be queried
        integrator = make_integrator(int_name, """
                <integer name="rr_depth" value="5"/>
                <integer name="max_depth" value="-1"/>
                <boolean name="strict_normals" value="true"/>
            """)
        # Cannot use a negative `max_depth`, except -1 (unlimited depth)
        with pytest.raises(RuntimeError):
            integrator = make_integrator(int_name, """
                    <integer name="max_depth" value="-2"/>
                """)

@pytest.mark.parametrize(*integrators)
def test02_render_empty_scene(int_name, empty_scene):
    integrator = make_integrator(int_name)

    (res, _) = check_scene(integrator, empty_scene)
    assert np.all(res[:, :, :3] == 0)   # Pixel values
    assert np.all(res[:, :, 3]  >= 0)   # Reconstruction weights

@pytest.mark.parametrize(*integrators)
def test03_render_teapot(int_name, teapot_scene):
    integrator = make_integrator(int_name)

    (res, _) = check_scene(integrator, teapot_scene)
    n = res.shape[0] * res.shape[1]

    # TODO: replace by a proper reference image once the implementation has stabilized.
    nnz = np.sum(np.sum(res[:, :, :3], axis=2) > 1e-7)
    assert (nnz >= 0.20 * n) and (nnz < 0.4 * n)

