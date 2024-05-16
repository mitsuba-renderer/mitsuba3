import pytest
import drjit as dr
import mitsuba as mi


def create_rectangle():
    mesh = mi.Mesh("rectangle", 4, 2, has_vertex_texcoords = True)

    params = mi.traverse(mesh)

    params['vertex_positions'] = [0.0, 0.0, 0.0,
                                  1.0, 0.0, 0.0,
                                  0.0, 1.0, 0.0,
                                  1.0, 1.0, 0.0]
    params['vertex_texcoords'] = [0.0, 0.0,
                                  1.0, 0.0,
                                  0.0, 1.0,
                                  1.0, 1.0]
    params['faces'] = [0, 1, 2, 1, 3, 2]
    params.update()

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
    mesh = create_rectangle()

    # Check vertex color attribute

    texture = mi.load_dict({
        "type" : "mesh_attribute",
        "name" : "vertex_color",
    })

    for u, v in [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0), (0.3, 0.4), (0.5, 0.5)]:
        value = texture.eval(mesh.eval_parameterization([u, v]))
        assert dr.allclose(value, [u, v, 0])

        value = texture.eval_3(mesh.eval_parameterization([u, v]))
        assert dr.allclose(value, [u, v, 0])

        with pytest.raises(Exception) as e:
            texture.eval_1(mesh.eval_parameterization([u, v]))
        e.match("requested but had size 3")

    # Check vertex mono attribute

    texture = mi.load_dict({
        "type" : "mesh_attribute",
        "name" : "vertex_mono",
    })

    for u, v in [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0), (0.3, 0.4), (0.5, 0.5)]:
        a, b, c, d = [1.0, 2.0, 3.0, 4.0]
        value = (1.0 - v) * (a * (1.0 - u) + b * u) + v * (c * (1.0 - u) + d * u)
        assert dr.allclose(value, texture.eval_1(mesh.eval_parameterization([u, v])))


    # Check face color attribute

    texture = mi.load_dict({
        "type" : "mesh_attribute",
        "name" : "face_color",
    })

    for u, v in [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0), (0.3, 0.4), (0.5, 0.5)]:
        si = mesh.eval_parameterization([u, v])
        assert dr.allclose(texture.eval(si), [0, 0, si.prim_index])

    # Check face mono attribute

    texture = mi.load_dict({
        "type" : "mesh_attribute",
        "name" : "face_mono",
    })

    for u, v in [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0), (0.3, 0.4), (0.5, 0.5)]:
        si = mesh.eval_parameterization([u, v])
        assert dr.allclose(texture.eval_1(si), si.prim_index)


def test02_eval_spectrum(variants_vec_spectral):
    import numpy as np
    mesh = create_rectangle()

    texture = mi.load_dict({
        "type" : "mesh_attribute",
        "name" : "vertex_color",
    })

    wavelengths = np.linspace(mi.MI_CIE_MIN, mi.MI_CIE_MAX, mi.MI_WAVELENGTH_SAMPLES)

    for u, v in [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0), (0.3, 0.4), (0.5, 0.5)]:
        si = mesh.eval_parameterization([u, v])
        si.wavelengths = wavelengths
        dr.allclose(texture.eval(si), mi.srgb_model_eval(mi.srgb_model_fetch([u, v, 0]), wavelengths))


def test03_invalid_attribute(variant_scalar_rgb):
    mesh = create_rectangle()

    texture = mi.load_dict({
        "type" : "mesh_attribute",
        "name" : "vertex_colorr",
    })

    si =  mesh.eval_parameterization([0, 0])

    with pytest.raises(Exception) as e:
        texture.eval(si)
    e.match("Invalid attribute requested")
