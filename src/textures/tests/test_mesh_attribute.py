import mitsuba
import pytest
import enoki as ek
import numpy as np


def create_rectangle():
    from mitsuba.render import Mesh

    mesh = Mesh("rectangle", 4, 2, has_vertex_texcoords = True)
    mesh.vertex_positions_buffer()[:] = [0.0, 0.0, 0.0,
                                         1.0, 0.0, 0.0,
                                         0.0, 1.0, 0.0,
                                         1.0, 1.0, 0.0]
    mesh.vertex_texcoords_buffer()[:] = [0.0, 0.0,
                                         1.0, 0.0,
                                         0.0, 1.0,
                                         1.0, 1.0]
    mesh.faces_buffer()[:] = [0, 1, 2, 1, 3, 2]
    mesh.parameters_changed()

    mesh.add_attribute("vertex_mono", 1, [1.0, 2.0, 3.0, 4.0])
    mesh.add_attribute("vertex_color", 3, [0.0, 0.0, 0.0,
                                           1.0, 0.0, 0.0,
                                           0.0, 1.0, 0.0,
                                           1.0, 1.0, 0.0])

    mesh.add_attribute("face_mono", 1, [0.0, 1.0])
    mesh.add_attribute("face_color", 3, [0.0, 0.0, 0.0,
                                         0.0, 0.0, 1.0])

    return mesh


def test01_eval(variant_scalar_rgb):
    from mitsuba.core import xml, luminance

    mesh = create_rectangle()

    # Check vertex color attribute

    texture = xml.load_dict({
        "type" : "mesh_attribute",
        "name" : "vertex_color",
    })

    for u, v in [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0), (0.3, 0.4), (0.5, 0.5)]:
        value = texture.eval(mesh.eval_parameterization([u, v]))
        assert ek.allclose(value, [u, v, 0])

        value = texture.eval_3(mesh.eval_parameterization([u, v]))
        assert ek.allclose(value, [u, v, 0])

        with pytest.raises(Exception) as e:
            texture.eval_1(mesh.eval_parameterization([u, v]))
        e.match("requested but had size 3")

    # Check vertex mono attribute

    texture = xml.load_dict({
        "type" : "mesh_attribute",
        "name" : "vertex_mono",
    })

    for u, v in [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0), (0.3, 0.4), (0.5, 0.5)]:
        a, b, c, d = [1.0, 2.0, 3.0, 4.0]
        value = (1.0 - v) * (a * (1.0 - u) + b * u) + v * (c * (1.0 - u) + d * u)
        assert ek.allclose(value, texture.eval_1(mesh.eval_parameterization([u, v])))


    # Check face color attribute

    texture = xml.load_dict({
        "type" : "mesh_attribute",
        "name" : "face_color",
    })

    for u, v in [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0), (0.3, 0.4), (0.5, 0.5)]:
        si = mesh.eval_parameterization([u, v])
        assert ek.allclose(texture.eval(si), [0, 0, si.prim_index])

    # Check face mono attribute

    texture = xml.load_dict({
        "type" : "mesh_attribute",
        "name" : "face_mono",
    })

    for u, v in [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0), (0.3, 0.4), (0.5, 0.5)]:
        si = mesh.eval_parameterization([u, v])
        assert ek.allclose(texture.eval_1(si), si.prim_index)


def test02_eval_spectrum(variant_scalar_spectral):
    from mitsuba.core import xml
    from mitsuba.render import srgb_model_fetch, srgb_model_eval, srgb_model_mean
    from mitsuba.core import MTS_WAVELENGTH_MIN, MTS_WAVELENGTH_MAX, MTS_WAVELENGTH_SAMPLES

    mesh = create_rectangle()

    texture = xml.load_dict({
        "type" : "mesh_attribute",
        "name" : "vertex_color",
    })

    wavelengths = np.linspace(MTS_WAVELENGTH_MIN, MTS_WAVELENGTH_MAX, MTS_WAVELENGTH_SAMPLES)

    for u, v in [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0), (0.3, 0.4), (0.5, 0.5)]:
        si = mesh.eval_parameterization([u, v])
        si.wavelengths = wavelengths
        ek.allclose(texture.eval(si), srgb_model_eval(srgb_model_fetch([u, v, 0]), wavelengths))


def test03_invalid_attribute(variant_scalar_rgb):
    from mitsuba.core.xml import load_dict

    mesh = create_rectangle()

    texture = load_dict({
        "type" : "mesh_attribute",
        "name" : "vertex_colorr",
    })

    si =  mesh.eval_parameterization([0, 0])

    with pytest.raises(Exception) as e:
        texture.eval(si)
    e.match("Invalid attribute requested")