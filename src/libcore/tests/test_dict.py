import enoki as ek
import pytest
import mitsuba

from mitsuba.python.test.util import fresolver_append_path


def test1_dict_plugins(variants_all_rgb):
    from mitsuba.core import xml

    plugins = [('emitter', 'point'), ('film', 'hdrfilm'),
               ('sensor', 'perspective'), ('integrator', 'path'),
               ('bsdf', 'diffuse'), ('texture', 'checkerboard'),
               ('rfilter', 'box'), ('spectrum', 'd65')]

    for p_name, p_type in plugins:
        o1 = xml.load_dict({ "type": p_type })
        o2 = xml.load_string("""<%s type="%s" version="2.0.0"/>""" % (p_name, p_type))
        assert str(o1) == str(o2)


def test2_dict_missing_type(variants_all_rgb):
    from mitsuba.core import xml
    with pytest.raises(Exception) as e:
        xml.load_dict({
            "center" : [0, 0, -10],
            "radius" : 10.0,
        })
    e.match("""Missing key 'type'""")


def test3_dict_simple_field(variants_all_rgb):
    from mitsuba.core import xml, ScalarPoint3f
    import numpy as np

    s1 = xml.load_dict({
        "type" : "sphere",
        "center" : ScalarPoint3f(0, 0, -10),
        "radius" : 10.0,
        "flip_normals" : False,
    })

    s2 = xml.load_dict({
        "type" : "sphere",
        "center" : [0, 0, -10], # list -> ScalarPoint3f
        "radius" : 10, # int -> float
        "flip_normals" : False,
    })

    s3 = xml.load_dict({
        "type" : "sphere",
        "center" : np.array([0, 0, -10]), # numpy array -> ScalarPoint3f
        "radius" : 10.0,
        "flip_normals" : False,
    })

    s4 = xml.load_dict({
        "type" : "sphere",
        "center" : (0, 0, -10), # tuple -> ScalarPoint3f
        "radius" : 10.0,
        "flip_normals" : False,
    })

    s5 = xml.load_string("""
        <shape type="sphere" version="2.0.0">
            <point name="center" value="0.0 0.0 -10.0"/>
            <float name="radius" value="10.0"/>
            <boolean name="flip_normals" value="false"/>
        </shape>
    """)

    assert str(s1) == str(s2)
    assert str(s1) == str(s3)
    assert str(s1) == str(s4)
    assert str(s1) == str(s5)


def test4_dict_nested(variants_all_rgb):
    from mitsuba.core import xml

    s1 = xml.load_dict({
        "type" : "sphere",
        "emitter" : {
            "type" : "area",
        }
    })

    s2 = xml.load_string("""
        <shape type="sphere" version="2.0.0">
            <emitter type="area"/>
        </shape>
    """)

    assert str(s1) == str(s2)


    s1 = xml.load_dict({
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

    s2 = xml.load_string("""
        <shape type="sphere" version="2.0.0">
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


def test5_dict_nested_object(variants_all_rgb):
    from mitsuba.core import xml

    bsdf = xml.load_dict({"type" : "diffuse"})

    s1 = xml.load_dict({
        "type" : "sphere",
        "bsdf" : bsdf
    })

    s2 = xml.load_string("""
        <shape type="sphere" version="2.0.0">
            <bsdf type="diffuse"/>
        </shape>
    """)

    assert str(s1.bsdf()) == str(s2.bsdf())


def test6_dict_rgb(variants_scalar_all):
    from mitsuba.core import xml, ScalarColor3f

    e1 = xml.load_dict({
        "type" : "point",
        "intensity" : {
            "type": "rgb",
            "value" : ScalarColor3f(0.5, 0.2, 0.5)
        }
    })

    e2 = xml.load_dict({
        "type" : "point",
        "intensity" : {
            "type": "rgb",
            "value" : [0.5, 0.2, 0.5] # list -> ScalarColor3f
        }
    })

    e3 = xml.load_string("""
        <emitter type="point" version="2.0.0">
            <rgb name="intensity" value="0.5, 0.2, 0.5"/>
        </emitter>
    """)

    assert str(e1) == str(e2)
    assert str(e1) == str(e3)

    e1 = xml.load_dict({
        "type" : "point",
        "intensity" : {
            "type": "rgb",
            "value" : 0.5 # float -> ScalarColor3f
        }
    })

    e2 = xml.load_string("""
        <emitter type="point" version="2.0.0">
            <rgb name="intensity" value="0.5"/>
        </emitter>
    """)

    assert str(e1) == str(e2)


def test7_dict_spectrum(variants_scalar_all):
    from mitsuba.core import xml

    e1 = xml.load_dict({
        "type" : "point",
        "intensity" : {
            "type" : "spectrum",
            "value" : [(400, 0.1), (500, 0.2), (600, 0.4), (700, 0.1)]
        }
    })

    e2 = xml.load_string("""
        <emitter type="point" version="2.0.0">
            <spectrum name="intensity" value="400:0.1 500:0.2 600:0.4 700:0.1"/>
        </emitter>
    """)

    assert str(e1) == str(e2)

    e1 = xml.load_dict({
        "type" : "point",
        "intensity" : {
            "type" : "spectrum",
            "value" : 0.44
        }
    })

    e2 = xml.load_string("""
        <emitter type="point" version="2.0.0">
            <spectrum name="intensity" value="0.44"/>
        </emitter>
    """)

    assert str(e1) == str(e2)

    with pytest.raises(Exception) as e:
        xml.load_dict({
            "type" : "point",
            "intensity" : {
                "type" : "spectrum",
                "value" : [(400, 0.1), (500, 0.2), (300, 0.4)]
            }
        })
    e.match("Wavelengths must be specified in increasing order")


def test7_dict_scene(variant_scalar_rgb):
    from mitsuba.core import xml, ScalarTransform4f

    s1 = xml.load_dict({
        "type" : "scene",
        "myintegrator" : {
            "type" : "path",
        },
        "mysensor" : {
            "type" : "perspective",
            "near_clip": 1.0,
            "far_clip": 1000.0,
            "to_world" : ScalarTransform4f.look_at(origin=[1, 1, 1],
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

    s2 = xml.load_string("""
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

    scene = xml.load_dict({
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


def test8_dict_unreferenced_attribute_error(variants_all_rgb):
    from mitsuba.core import xml
    with pytest.raises(Exception) as e:
        xml.load_dict({
            "type" : "point",
            "foo": 0.44
        })
    e.match("Unreferenced attribute")



def test9_dict_scene_reference(variant_scalar_rgb):
    from mitsuba.core import xml

    scene = xml.load_dict({
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

    bsdf1 = xml.load_dict({ "type" : "conductor" })
    bsdf2 = xml.load_dict({
        "type" : "roughdielectric",
        "id" : "bsdf2_id"
    })
    texture = xml.load_dict({ "type" : "checkerboard"})
    bsdf3 = xml.load_dict({
        "type" : "diffuse",
        "reflectance" : texture
    })

    assert str(scene.shapes()[0].bsdf()) == str(bsdf1)
    assert str(scene.shapes()[1].bsdf()) == str(bsdf2)
    assert str(scene.shapes()[2].bsdf()) == str(bsdf2)
    assert str(scene.shapes()[3].bsdf()) == str(bsdf3)

    with pytest.raises(Exception) as e:
        scene = xml.load_dict({
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
        scene = xml.load_dict({
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
def test10_dict_expand_nested_object(variant_scalar_rgb):
    from mitsuba.core import xml
    # Nested dictionary objects should be expanded
    b0 = xml.load_dict({
        "type" : "diffuse",
        "reflectance" : {
            "type" : "bitmap",
            "filename" : "resources/data/common/textures/museum.exr"
        }
    })

    b1 = xml.load_string("""
        <bsdf type="diffuse" version="2.0.0">
            <texture type="bitmap" name="reflectance">
                <string name="filename" value="resources/data/common/textures/museum.exr"/>
            </texture>
        </bsdf>
    """)

    assert str(b0) == str(b1)

    # Check that root object isn't expanded
    texture = xml.load_dict({
            "type" : "bitmap",
            "filename" : "resources/data/common/textures/museum.exr"
    })
    assert len(texture.expand()) == 1

    # But we should be able to use this object in another dict, and it will be expanded
    b3 = xml.load_dict({
        "type" : "diffuse",
        "reflectance" : texture
    })
    assert str(b0) == str(b3)

    # Object should be expanded when used through a reference
    scene = xml.load_dict({
        "type" : "scene",
        "mytexture" : {
            "type" : "bitmap",
            "filename" : "resources/data/common/textures/museum.exr"
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