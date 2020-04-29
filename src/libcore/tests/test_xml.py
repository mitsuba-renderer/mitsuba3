import enoki as ek
import pytest
import mitsuba


def test01_invalid_xml(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception):
        xml.load_string('<?xml version="1.0"?>')


def test02_invalid_root_node(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception):
        xml.load_string('<?xml version="1.0"?><invalid></invalid>')


def test03_invalid_root_node(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception) as e:
        xml.load_string('<?xml version="1.0"?><integer name="a" '
                   'value="10"></integer>')
    e.match('root element "integer" must be an object')


def test04_valid_root_node(variant_scalar_rgb):
    from mitsuba.core import xml
    from mitsuba.render import Scene

    obj1 = xml.load_string('<?xml version="1.0"?>\n<scene version="2.0.0">'
                      '</scene>')
    obj2 = xml.load_string('<scene version="2.0.0"></scene>')
    assert type(obj1) is Scene
    assert type(obj2) is Scene


def test05_duplicate_id(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception) as e:
        xml.load_string("""
        <scene version="2.0.0">
            <shape type="ply" id="my_id"/>
            <shape type="ply" id="my_id"/>
        </scene>
        """)
    e.match('Error while loading "<string>" \\(at line 4, col 14\\):'
        ' "shape" has duplicate id "my_id" \\(previous was at line 3,'
        ' col 14\\)')


def test06_reserved_id(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception) as e:
        xml.load_string('<scene version="2.0.0">' +
                   '<shape type="ply" id="_test"/></scene>')
    e.match('invalid id "_test" in element "shape": leading underscores '
        'are reserved for internal identifiers.')


def test06_reserved_name(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception) as e:
        xml.load_string('<scene version="2.0.0">' +
                   '<shape type="ply">' +
                   '<integer name="_test" value="1"/></shape></scene>')
    e.match('invalid parameter name "_test" in element "integer": '
        'leading underscores are reserved for internal identifiers.')


def test06_incorrect_nesting(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                   <shape type="ply">
                   <integer name="value" value="1">
                   <shape type="ply"/>
                   </integer></shape></scene>""")
    e.match('node "shape" cannot occur as child of a property')


def test07_incorrect_nesting(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                   <shape type="ply">
                   <integer name="value" value="1">
                   <float name="value" value="1"/>
                   </integer></shape></scene>""")
    e.match('node "float" cannot occur as child of a property')


def test08_incorrect_nesting(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                   <shape type="ply">
                   <translate name="value" x="0" y="1" z="2"/>
                   </shape></scene>""")
    e.match('transform operations can only occur in a transform node')


def test09_incorrect_nesting(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                   <shape type="ply">
                   <transform name="toWorld">
                   <integer name="value" value="10"/>
                   </transform>
                   </shape></scene>""")
    e.match('transform nodes can only contain transform operations')


def test10_unknown_id(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                   <ref id="unknown"/>
                   </scene>""")
    e.match('reference to unknown object "unknown"')


def test11_unknown_attribute(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                   <shape type="ply" param2="abc">
                   </shape></scene>""")
    e.match('unexpected attribute "param2" in element "shape".')


def test12_missing_attribute(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                   <integer name="a"/></scene>""")
    e.match('missing attribute "value" in element "integer".')


def test13_duplicate_parameter(variant_scalar_rgb):
    from mitsuba.core import xml
    from mitsuba.core import Thread, LogLevel

    logger = Thread.thread().logger()
    l = logger.error_level()
    try:
        logger.set_error_level(LogLevel.Warn)
        with pytest.raises(Exception) as e:
            xml.load_string("""<scene version="2.0.0">
                       <integer name="a" value="1"/>
                       <integer name="a" value="1"/>
                       </scene>""")
    finally:
        logger.set_error_level(l)
    e.match('Property "a" was specified multiple times')


def test14_missing_parameter(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                   <shape type="ply"/>
                   </scene>""")
    e.match('Property "filename" has not been specified')


def test15_incorrect_parameter_type(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                   <shape type="ply">
                      <float name="filename" value="1.0"/>
                   </shape></scene>""")
    e.match('The property "filename" has the wrong type'
            ' \\(expected <string>\\).')


def test16_invalid_integer(variant_scalar_rgb):
    from mitsuba.core import xml

    def test_input(value, valid):
        expected = (r'.*unreferenced property ."test_number".*' if valid
                    else r'could not parse integer value "{}".'.format(value))
        with pytest.raises(Exception, match=expected):
            xml.load_string("""<scene version="2.0.0">
                       <integer name="test_number" value="{}"/>
                       </scene>""".format(value))
    test_input("a", False)
    test_input("50.5", False)
    test_input("50f", False)
    test_input("50 a", False)
    test_input("50 10", False)
    test_input("50, 10", False)
    test_input("1e10", False)
    test_input("1e-5", False)
    test_input("42", True)
    test_input("1000   ", True)
    test_input("  50    ", True)


def test17_invalid_float(variant_scalar_rgb):
    from mitsuba.core import xml

    def test_input(value, valid):
        expected = (r'.*unreferenced property ."test_number".*' if valid
                    else r'could not parse floating point value "{}".'.format(value))
        with pytest.raises(Exception, match=expected):
            xml.load_string("""<scene version="2.0.0">
                       <float name="test_number" value="{}"/>
                       </scene>""".format(value))
    test_input("a", False)
    test_input("50.0 43", False)
    test_input("50.0.5", False)
    test_input("50.0, 0.5", False)
    test_input("50.0 a", False)
    test_input("35.f", False)
    test_input("42", True)
    test_input("50.0", True)
    test_input("  50.0    ", True)
    test_input("1e-5", True)
    test_input("1e10", True)
    test_input("1e+12", True)


def test18_invalid_boolean(variant_scalar_rgb):
    from mitsuba.core import xml

    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                   <boolean name="10" value="a"/>
                   </scene>""")
    e.match('could not parse boolean value "a"'
            ' -- must be "true" or "false".')


def test19_invalid_vector(variant_scalar_rgb):
    from mitsuba.core import xml

    err_str = 'could not parse floating point value "a"'
    err_str2 = '"value" attribute must have exactly 1 or 3 elements'
    err_str3 = 'can\'t mix and match "value" and "x"/"y"/"z" attributes'

    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                   <vector name="10" x="a" y="b" z="c"/>
                   </scene>""")
    e.match(err_str)

    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                   <vector name="10" value="a, b, c"/>
                   </scene>""")
    e.match(err_str)

    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                   <vector name="10" value="1, 2"/>
                   </scene>""")
    e.match(err_str2)

    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                   <vector name="10" value="1, 2, 3" x="4"/>
                   </scene>""")
    e.match(err_str3)


def test20_upgrade_tree(variant_scalar_rgb):
    from mitsuba.core import xml

    err_str = 'unreferenced property'
    with pytest.raises(Exception) as e:
        xml.load_string("""<scene version="2.0.0">
                            <bsdf type="dielectric">
                                <float name="intIOR" value="1.33"/>
                            </bsdf>
                        </scene>""")
    e.match(err_str)

    xml.load_string("""<scene version="0.1.0">
                           <bsdf type="dielectric">
                               <float name="intIOR" value="1.33"/>
                           </bsdf>
                       </scene>""")


def test21_dict_plugins(variants_all_rgb):
    from mitsuba.core import xml

    plugins = [('emitter', 'point'), ('film', 'hdrfilm'),
               ('sensor', 'perspective'), ('integrator', 'path'),
               ('bsdf', 'diffuse'), ('texture', 'checkerboard'),
               ('rfilter', 'box'), ('spectrum', 'd65')]

    for p_name, p_type in plugins:
        o1 = xml.load_dict({ "type": p_type })
        o2 = xml.load_string("""<%s type="%s" version="2.0.0"/>""" % (p_name, p_type))
        assert str(o1) == str(o2)


def test22_dict_missing_type(variants_all_rgb):
    from mitsuba.core import xml
    with pytest.raises(Exception) as e:
        xml.load_dict({
            "center" : [0, 0, -10],
            "radius" : 10.0,
        })
    e.match("""Missing key 'type'""")


def test23_dict_simple_field(variants_all_rgb):
    from mitsuba.core import xml, Point3f
    import numpy as np

    s1 = xml.load_dict({
        "type" : "sphere",
        "center" : Point3f(0, 0, -10),
        "radius" : 10.0,
        "flip_normals" : False,
    })

    s2 = xml.load_dict({
        "type" : "sphere",
        "center" : [0, 0, -10], # list -> Point3f
        "radius" : 10, # int -> float
        "flip_normals" : False,
    })

    s3 = xml.load_dict({
        "type" : "sphere",
        "center" : np.array([0, 0, -10]), # numpy array -> Point3f
        "radius" : 10.0,
        "flip_normals" : False,
    })

    s4 = xml.load_string("""
        <shape type="sphere" version="2.0.0">
            <point name="center" value="0.0 0.0 -10.0"/>
            <float name="radius" value="10.0"/>
            <boolean name="flip_normals" value="false"/>
        </shape>
    """)

    assert str(s1) == str(s2)
    assert str(s1) == str(s3)
    assert str(s1) == str(s4)


def test24_dict_nested(variants_all_rgb):
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


def test25_dict_nested_object(variants_all_rgb):
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


def test26_dict_rgb(variants_scalar_all):
    from mitsuba.core import xml, Color3f

    e1 = xml.load_dict({
        "type" : "point",
        "intensity" : {
            "type": "rgb",
            "value" : Color3f(0.5, 0.2, 0.5)
        }
    })

    e2 = xml.load_dict({
        "type" : "point",
        "intensity" : {
            "type": "rgb",
            "value" : [0.5, 0.2, 0.5] # list -> Color3f
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
            "value" : 0.5 # float -> Color3f
        }
    })

    e2 = xml.load_string("""
        <emitter type="point" version="2.0.0">
            <rgb name="intensity" value="0.5"/>
        </emitter>
    """)

    assert str(e1) == str(e2)


def test27_dict_spectrum(variants_scalar_all):
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


def test27_dict_scene(variants_all_rgb):
    from mitsuba.core import xml, Transform4f

    s1 = xml.load_dict({
        "type" : "scene",
        "myintegrator" : {
            "type" : "path",
        },
        "mysensor" : {
            "type" : "perspective",
            "near_clip": 1.0,
            "far_clip": 1000.0,
            "to_world" : Transform4f.look_at(origin=[1, 1, 1],
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


def test28_dict_unreferenced_attribute_error(variants_all_rgb):
    from mitsuba.core import xml
    with pytest.raises(Exception) as e:
        xml.load_dict({
            "type" : "point",
            "foo": 0.44
        })
    e.match("Unreferenced attribute")
