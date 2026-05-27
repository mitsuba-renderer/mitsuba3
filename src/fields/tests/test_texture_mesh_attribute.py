"""
Tests of the mesh_attribute texture plugin: interpolation of vertex and
face attributes through the texture interface, and the spectral
conversion paths.
"""

import numpy as np
import pytest
import drjit as dr
import mitsuba as mi


def create_rectangle():
    """A unit quad with UVs equal to the position, carrying vertex/face
    attributes in mono and color flavors"""
    mesh = mi.Mesh("rectangle")
    mesh.from_fields(
        faces=[[0, 1, 2], [1, 3, 2]],
        positions=[[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]],
        texcoords=[[0, 0], [1, 0], [0, 1], [1, 1]])

    mesh.add_attribute("vertex_mono", [[1], [2], [3], [4]])
    mesh.add_attribute("vertex_color", [[0, 0, 0], [1, 0, 0],
                                        [0, 1, 0], [1, 1, 0]])
    mesh.add_attribute("face_mono", [[0], [1]])
    mesh.add_attribute("face_color", [[0, 0, 0], [0, 0, 1]])
    return mesh


# Probe near, but not directly on, the boundary to avoid numerical issues
UV_PROBES = [(0.001, 0.001), (0.998, 0.001), (0.001, 0.998), (0.999, 0.999),
             (0.3, 0.4), (0.5, 0.5)]


@pytest.mark.parametrize("name", ["vertex_mono", "vertex_color",
                                  "face_mono", "face_color"])
def test01_eval(variant_scalar_rgb, name):
    """Vertex attributes interpolate over the quad, while face attributes
    are constant per face."""
    mesh = create_rectangle()
    texture = mi.load_dict({"type": "mesh_attribute", "name": name})

    for u, v in UV_PROBES:
        si = mesh.eval_parameterization([u, v])
        if name == "vertex_color":
            # The color equals the interpolated (u, v, 0)
            dr.assert_allclose(texture.eval(si), [u, v, 0])
            dr.assert_allclose(texture.eval_3(si), [u, v, 0])
        elif name == "vertex_mono":
            a, b, c, d = [1.0, 2.0, 3.0, 4.0]
            ref = (1 - v) * (a * (1 - u) + b * u) + v * (c * (1 - u) + d * u)
            dr.assert_allclose(texture.eval_1(si), ref)
        elif name == "face_color":
            dr.assert_allclose(texture.eval(si), [0, 0, si.prim_index])
        else:
            dr.assert_allclose(texture.eval_1(si), si.prim_index)


def test02_eval_spectral_upsampling(variants_vec_spectral):
    """In spectral variants, *color* attributes store sRGB upsampling
    coefficients and expand into spectra on evaluation. The expansion is
    nonlinear, so it happens per vertex and the barycentric blend acts on
    the resulting spectra rather than on the colors."""
    mesh = create_rectangle()
    texture = mi.load_dict({"type": "mesh_attribute",
                            "name": "vertex_color"})

    wavelengths = np.linspace(mi.MI_CIE_MIN, mi.MI_CIE_MAX,
                              mi.MI_WAVELENGTH_SAMPLES)

    def spectrum(color):
        return mi.srgb_model_eval(mi.srgb_model_fetch(color),
                                  mi.UnpolarizedSpectrum(wavelengths))

    def reference(u, v):
        # The corners hold (u, v, 0), so a blend of the vertex colors is
        # again (u, v, 0). Those weights are the barycentric ones.
        if u + v <= 1:  # faces[0]: black, red, green
            corners = [(1 - u - v, [0, 0, 0]), (u, [1, 0, 0]),
                       (v, [0, 1, 0])]
        else:           # faces[1]: red, yellow, green
            corners = [(1 - v, [1, 0, 0]), (u + v - 1, [1, 1, 0]),
                       (1 - u, [0, 1, 0])]
        return sum(w * spectrum(c) for w, c in corners)

    for u, v in UV_PROBES:
        si = mesh.eval_parameterization([u, v])
        si.wavelengths = wavelengths
        dr.assert_allclose(texture.eval(si), reference(u, v), atol=1e-4)


def test03_eval_spectral_luminance_fold(variants_vec_spectral):
    """3-channel attributes without 'color' in their name are not
    upsampled. Spectral evaluation folds them to their luminance."""
    mesh = create_rectangle()
    val = np.float32([0.2, 0.5, 0.7])
    mesh.add_attribute("vertex_data",
                       np.tile(val, (4, 1)))
    texture = mi.load_dict({"type": "mesh_attribute", "name": "vertex_data"})

    si = mesh.eval_parameterization([0.5, 0.5])
    si.wavelengths = np.linspace(mi.MI_CIE_MIN, mi.MI_CIE_MAX,
                                 mi.MI_WAVELENGTH_SAMPLES)
    ref = mi.luminance(mi.Color3f(val))
    dr.assert_allclose(texture.eval(si), ref, atol=1e-5)

    # The raw values remain reachable through eval_attribute_3()
    dr.assert_allclose(mesh.eval_attribute_3("vertex_data", si), val)


def test04_missing_or_mismatched_reads_zero(variant_scalar_rgb):
    """Missing or channel-mismatched attributes read as zero rather than
    raising."""
    mesh = create_rectangle()
    si = mesh.eval_parameterization([0.3, 0.4])

    texture = mi.load_dict({"type": "mesh_attribute",
                            "name": "vertex_colorr"})
    dr.assert_allclose(texture.eval(si), 0)

    texture = mi.load_dict({"type": "mesh_attribute",
                            "name": "vertex_color"})
    dr.assert_allclose(texture.eval_1(si), 0)
    texture = mi.load_dict({"type": "mesh_attribute",
                            "name": "vertex_mono"})
    dr.assert_allclose(texture.eval_3(si), 0)

    with pytest.raises(RuntimeError, match="must start with either"):
        mi.load_dict({
            "type": "mesh_attribute",
            "name": "foo_vertex_color",
        })
