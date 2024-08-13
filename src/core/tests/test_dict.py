import pytest
import drjit as dr
import mitsuba as mi

from mitsuba.test.util import fresolver_append_path


def test01_dict_plugins(variants_all):
    plugins = [('emitter', 'point'), ('film', 'hdrfilm'),
               ('sensor', 'perspective'), ('integrator', 'path'),
               ('bsdf', 'diffuse'), ('texture', 'checkerboard'),
               ('rfilter', 'box'), ('spectrum', 'd65')]

    for p_name, p_type in plugins:
        o1 = mi.load_dict({ "type": p_type })
        o2 = mi.load_string("""<%s type="%s" version="3.0.0"/>""" % (p_name, p_type))
        assert str(o1) == str(o2)


def test02_dict_missing_type(variants_all):
    with pytest.raises(Exception) as e:
        mi.load_dict({
            "center" : [0, 0, -10],
            "radius" : 10.0,
        })
    e.match("""Missing key 'type'""")


def test03_dict_simple_field(variants_all):
    import numpy as np

    s1 = mi.load_dict({
        "type" : "sphere",
        "center" : mi.ScalarPoint3f(0, 0, -10),
        "radius" : 10.0,
        "flip_normals" : False,
    })

    s2 = mi.load_dict({
        "type" : "sphere",
        "center" : [0, 0, -10], # list -> mi.ScalarPoint3f
        "radius" : 10, # int -> float
        "flip_normals" : False,
    })

    s3 = mi.load_dict({
        "type" : "sphere",
        "center" : np.array([0, 0, -10]), # numpy array -> mi.ScalarPoint3f
        "radius" : 10.0,
        "flip_normals" : False,
    })

    s4 = mi.load_dict({
        "type" : "sphere",
        "center" : (0, 0, -10), # tuple -> mi.ScalarPoint3f
        "radius" : 10.0,
        "flip_normals" : False,
    })

    s5 = mi.load_string("""
        <shape type="sphere" version="3.0.0">
            <point name="center" value="0.0 0.0 -10.0"/>
            <float name="radius" value="10.0"/>
            <boolean name="flip_normals" value="false"/>
        </shape>
    """)

    assert str(s1) == str(s2)
    assert str(s1) == str(s3)
    assert str(s1) == str(s4)
    assert str(s1) == str(s5)


def test04_dict_nested(variants_all):
    s1 = mi.load_dict({
        "type" : "sphere",
        "emitter" : {
            "type" : "area",
        }
    })

    s2 = mi.load_string("""
        <shape type="sphere" version="3.0.0">
            <emitter type="area"/>
        </shape>
    """)

    assert str(s1) == str(s2)


    s1 = mi.load_dict({
        "type" : "sphere",
        "bsdf" : {
            "type" : "roughdielectric",
            "specular_reflectance" : {
                "type" : "checkerboard",
                "color0" : {
                    "type" : "rgb",
                    "value" : [0.0, 0.8, 0.0]
                },
                "color1" : {
                    "type" : "rgb",
                    "value" : [0.8, 0.8, 0.0]
                }
            },
            "specular_transmittance" : {
                "type" : "checkerboard",
                "color0" : {
                    "type" : "rgb",
                    "value" : 0.5
                },
                "color1" : {
                    "type" : "rgb",
                    "value" : 0.9
                }
            },
        }
    })

    s2 = mi.load_string("""
        <shape type="sphere" version="3.0.0">
            <bsdf type="roughdielectric">
                <texture type="checkerboard" name="specular_reflectance">
                    <rgb name="color0" value="0.0 0.8 0.0"/>
                    <rgb name="color1" value="0.8 0.8 0.0"/>
                </texture>
                <texture type="checkerboard" name="specular_transmittance">
                    <rgb name="color0" value="0.5"/>
                    <rgb name="color1" value="0.9"/>
                </texture>
            </bsdf>
        </shape>
    """)

    assert str(s1.bsdf()) == str(s2.bsdf())


def test05_dict_nested_object(variants_all):
    bsdf = mi.load_dict({"type" : "diffuse"})

    s1 = mi.load_dict({
        "type" : "sphere",
        "bsdf" : bsdf
    })

    s2 = mi.load_string("""
        <shape type="sphere" version="3.0.0">
            <bsdf type="diffuse"/>
        </shape>
    """)

    assert str(s1.bsdf()) == str(s2.bsdf())


def test06_dict_rgb(variants_all_scalar):
    e1 = mi.load_dict({
        "type" : "point",
        "intensity" : {
            "type": "rgb",
            "value" : mi.ScalarColor3f(0.5, 0.2, 0.5)
        }
    })

    e2 = mi.load_dict({
        "type" : "point",
        "intensity" : {
            "type": "rgb",
            "value" : [0.5, 0.2, 0.5] # list -> mi.ScalarColor3f
        }
    })

    e3 = mi.load_string("""
        <emitter type="point" version="3.0.0">
            <rgb name="intensity" value="0.5, 0.2, 0.5"/>
        </emitter>
    """)

    assert str(e1) == str(e2)
    assert str(e1) == str(e3)

    e1 = mi.load_dict({
        "type" : "point",
        "intensity" : {
            "type": "rgb",
            "value" : 0.5 # float -> mi.ScalarColor3f
        }
    })

    e2 = mi.load_string("""
        <emitter type="point" version="3.0.0">
            <rgb name="intensity" value="0.5"/>
        </emitter>
    """)

    assert str(e1) == str(e2)

    import numpy as np
    e1 = mi.load_dict({
        "type" : "point",
        "intensity" : {
            "type": "rgb",
            "value" : np.array([0.5, 0.2, 0.5])
        }
    })
    assert str(e1) == str(e3)

    with pytest.raises(Exception) as e:
        mi.load_dict({'type': 'rgb', 'value': np.array([[0.5,0.5,0.3]])})
    assert "Could not convert [[0.5 0.5 0.3]] into Color3f" in str(e.value)


def test07_dict_spectrum(variants_all_scalar):
    e1 = mi.load_dict({
        "type" : "point",
        "intensity" : {
            "type" : "spectrum",
            "value" : [(400, 0.1), (500, 0.2), (600, 0.4), (700, 0.1)]
        }
    })

    e2 = mi.load_string("""
        <emitter type="point" version="3.0.0">
            <spectrum name="intensity" value="400:0.1 500:0.2 600:0.4 700:0.1"/>
        </emitter>
    """)

    assert str(e1) == str(e2)

    e1 = mi.load_dict({
        "type" : "point",
        "intensity" : {
            "type" : "rgb",
            "value" : 0.44
        }
    })

    e2 = mi.load_string("""
        <emitter type="point" version="3.0.0">
            <rgb name="intensity" value="0.44"/>
        </emitter>
    """)

    assert str(e1) == str(e2)

    with pytest.raises(Exception) as e:
        mi.load_dict({
            "type" : "point",
            "intensity" : {
                "type" : "spectrum",
                "value" : [(400, 0.1), (500, 0.2), (300, 0.4)]
            }
        })
    e.match("Wavelengths must be specified in increasing order")


def test07_dict_scene(variant_scalar_rgb):
    s1 = mi.load_dict({
        "type" : "scene",
        "myintegrator" : {
            "type" : "path",
        },
        "mysensor" : {
            "type" : "perspective",
            "near_clip": 1.0,
            "far_clip": 1000.0,
            "to_world" : mi.ScalarTransform4f().look_at(origin=[1, 1, 1],
                                                      target=[0, 0, 0],
                                                      up=[0, 0, 1]),
            "myfilm" : {
                "type" : "hdrfilm",
                "rfilter" : { "type" : "box"},
                "width" : 1024,
                "height" : 768,
            },
            "mysampler" : {
                "type" : "independent",
                "sample_count" : 4,
            },
        },
        "myemitter" : {"type" : "constant"},
        "myshape" : {"type" : "sphere"}
    })

    s2 = mi.load_string("""
        <scene version='2.0.0'>
            <emitter type="constant"/>

            <integrator type='path'/>

            <sensor type="perspective">
                <float name="near_clip" value="1"/>
                <float name="far_clip" value="1000"/>

                <film type="hdrfilm">
                    <rfilter type="box"/>
                    <integer name="width" value="1024"/>
                    <integer name="height" value="768"/>
                </film>

                <sampler type="independent">
                    <integer name="sample_count" value="4"/>
                </sampler>

                <transform name="to_world">
                    <lookat origin="1, 1, 1"
                            target="0, 0, 0"
                            up    ="0, 0, 1"/>
                </transform>

            </sensor>

            <shape type="sphere"/>
    </scene>
    """)

    assert str(s1) == str(s2)

    scene = mi.load_dict({
        "type" : "scene",
        "mysensor0" : { "type" : "perspective" },
        "mysensor1" : { "type" : "perspective" },
        "emitter0" : { "type" : "point" },
        "emitter1" : { "type" : "directional" },
        "emitter3" : { "type" : "constant" },
        "shape0" : { "type" : "sphere" },
        "shape1" : { "type" : "rectangle" },
        "shape2" : { "type" : "disk" },
        "shape3" : { "type" : "cylinder" },
    })

    assert len(scene.sensors())  == 2
    assert len(scene.emitters()) == 3
    assert len(scene.shapes())   == 4


def test08_dict_unreferenced_attribute_error(variants_all):
    with pytest.raises(Exception) as e:
        mi.load_dict({
            "type" : "point",
            "foo": 0.44
        })
    e.match("Unreferenced property")



def test09_dict_scene_reference(variant_scalar_rgb):
    scene = mi.load_dict({
        "type" : "scene",
        # reference using its key
        "bsdf1_key" : { "type" : "conductor" },
        # reference using its key or its id
        "bsdf2_key" : {
            "type" : "roughdielectric",
            "id" : "bsdf2_id"
        },
        "texture1_id" : { "type" : "checkerboard"},

        "shape0" : {
            "type" : "sphere",
            "foo" : {
                "type" : "ref",
                "id" : "bsdf1_key"
            }
        },

        "shape1" : {
            "type" : "sphere",
            "foo" : {
                "type" : "ref",
                "id" : "bsdf2_id"
            }
        },

        "shape2" : {
            "type" : "sphere",
            "foo" : {
                "type" : "ref",
                "id" : "bsdf2_key"
            }
        },

        "shape3" : {
            "type" : "sphere",
            "bsdf" : {
                "type" : "diffuse",
                "reflectance" : {
                    "type" : "ref",
                    "id" : "texture1_id"
                }
            }
        },
    })

    bsdf1 = mi.load_dict({ "type" : "conductor" })
    bsdf2 = mi.load_dict({
        "type" : "roughdielectric",
        "id" : "bsdf2_id"
    })
    texture = mi.load_dict({ "type" : "checkerboard"})
    bsdf3 = mi.load_dict({
        "type" : "diffuse",
        "reflectance" : texture
    })

    assert str(scene.shapes()[0].bsdf()) == str(bsdf1)
    assert str(scene.shapes()[1].bsdf()) == str(bsdf2)
    assert str(scene.shapes()[2].bsdf()) == str(bsdf2)
    assert str(scene.shapes()[3].bsdf()) == str(bsdf3)

    with pytest.raises(Exception) as e:
        scene = mi.load_dict({
            "type" : "scene",
            "shape0" : {
                "type" : "sphere",
                "id" : "shape_id",
            },
            "shape1" : {
                "type" : "sphere",
                "id" : "shape_id",
            },
        })
    e.match("has duplicate id")

    with pytest.raises(Exception) as e:
        scene = mi.load_dict({
            "type" : "scene",
            "bsdf1_id" : { "type" : "conductor" },
            "shape1" : {
                "type" : "sphere",
                "foo" : {
                    "type" : "ref",
                    "id" : "bsdf2_id"
                }
            },
        })
    e.match("""Referenced id "bsdf2_id" not found""")


@fresolver_append_path
def test10_dict_expand_nested_object(variant_scalar_spectral):
    # Nested dictionary objects should be expanded
    b0 = mi.load_dict({
        "type" : "diffuse",
        "reflectance" : {
            "type" : "d65",
        }
    })

    b1 = mi.load_string("""
        <bsdf type="diffuse" version="3.0.0">
            <spectrum version='2.0.0' type='d65' name="reflectance"/>
        </bsdf>
    """)

    assert str(b0) == str(b1)

    # Check that root object is expanded
    spectrum = mi.load_dict({
            "type" : "d65",
    })
    assert len(spectrum.expand()) == 0
    assert spectrum.class_().name() == "RegularSpectrum"

    # Object should be expanded when used through a reference
    scene = mi.load_dict({
        "type" : "scene",
        "mytexture" : {
            "type" : "d65",
        },
        "shape1" : {
            "type" : "sphere",
            "bsdf" : {
                "type" : "diffuse",
                "reflectance" : {
                    "type" : "ref",
                    "id" : "mytexture"
                }
            }
        },
    })
    assert str(b0) == str(scene.shapes()[0].bsdf())


def test11_dict_spectrum_srgb(variant_scalar_rgb):
    spectrum = [(400.0, 0.1), (500.0, 0.2), (600.0, 0.4), (700.0, 0.1)]

    w = [s[0] for s in spectrum]
    v = [s[1] * mi.MI_CIE_Y_NORMALIZATION for s in spectrum]
    rgb = mi.spectrum_list_to_srgb(w, v, d65=True)
    assert dr.allclose(rgb, [0.380475, 0.289928, 0.134294])

    s1 = mi.load_dict({
        "type" : "spectrum",
        "value" : spectrum
    })

    s2 = mi.load_string(f"""
        <spectrum  type='srgb' version='2.0.0'>
            <rgb name="color" value="0.380475, 0.289928, 0.134294"/>
        </spectrum>
    """)

    assert str(s1) == str(s2)


def test12_dict_spectrum(variant_scalar_spectral):
    s1 = mi.load_dict({
        "type" : "spectrum",
        "value" : [(400.0, 0.1), (500.0, 0.2), (600.0, 0.4), (700.0, 0.1)]
    })

    s2 = mi.load_string(f"""
        <spectrum  type='regular' version='2.0.0'>
           <float name="wavelength_min" value="400"/>
           <float name="wavelength_max" value="700"/>
           <string name="values" value="0.1, 0.2, 0.4, 0.1"/>
       </spectrum>
    """)

    assert str(s1) == str(s2)
