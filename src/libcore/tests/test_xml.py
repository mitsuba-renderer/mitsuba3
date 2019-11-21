from mitsuba.scalar_rgb.core.xml import load_string
from mitsuba.scalar_rgb.render import Scene
from mitsuba.scalar_rgb.core import Thread, LogLevel
import pytest


def test01_invalid_xml():
    with pytest.raises(Exception):
        load_string('<?xml version="1.0"?>')


def test02_invalid_root_node():
    with pytest.raises(Exception):
        load_string('<?xml version="1.0"?><invalid></invalid>')


def test03_invalid_root_node():
    with pytest.raises(Exception) as e:
        load_string('<?xml version="1.0"?><integer name="a" '
                   'value="10"></integer>')
    e.match('root element "integer" must be an object')


def test04_valid_root_node():
    obj1 = load_string('<?xml version="1.0"?>\n<scene version="2.0.0">'
                      '</scene>')
    obj2 = load_string('<scene version="2.0.0"></scene>')
    assert type(obj1) is Scene
    assert type(obj2) is Scene


def test05_duplicate_id():
    with pytest.raises(Exception) as e:
        load_string("""
        <scene version="2.0.0">
            <shape type="ply" id="my_id"/>
            <shape type="ply" id="my_id"/>
        </scene>
        """)
    e.match('Error while loading "<string>" \\(at line 4, col 14\\):'
        ' "shape" has duplicate id "my_id" \\(previous was at line 3,'
        ' col 14\\)')


def test06_reserved_id():
    with pytest.raises(Exception) as e:
        load_string('<scene version="2.0.0">' +
                   '<shape type="ply" id="_test"/></scene>')
    e.match('invalid id "_test" in element "shape": leading underscores '
        'are reserved for internal identifiers.')


def test06_reserved_name():
    with pytest.raises(Exception) as e:
        load_string('<scene version="2.0.0">' +
                   '<shape type="ply">' +
                   '<integer name="_test" value="1"/></shape></scene>')
    e.match('invalid parameter name "_test" in element "integer": '
        'leading underscores are reserved for internal identifiers.')


def test06_incorrect_nesting():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="2.0.0">
                   <shape type="ply">
                   <integer name="value" value="1">
                   <shape type="ply"/>
                   </integer></shape></scene>""")
    e.match('node "shape" cannot occur as child of a property')


def test07_incorrect_nesting():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="2.0.0">
                   <shape type="ply">
                   <integer name="value" value="1">
                   <float name="value" value="1"/>
                   </integer></shape></scene>""")
    e.match('node "float" cannot occur as child of a property')


def test08_incorrect_nesting():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="2.0.0">
                   <shape type="ply">
                   <translate name="value" x="0" y="1" z="2"/>
                   </shape></scene>""")
    e.match('transform operations can only occur in a transform node')


def test09_incorrect_nesting():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="2.0.0">
                   <shape type="ply">
                   <transform name="toWorld">
                   <integer name="value" value="10"/>
                   </transform>
                   </shape></scene>""")
    e.match('transform nodes can only contain transform operations')


def test10_unknown_id():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="2.0.0">
                   <ref id="unknown"/>
                   </scene>""")
    e.match('reference to unknown object "unknown"')


def test11_unknown_attribute():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="2.0.0">
                   <shape type="ply" param2="abc">
                   </shape></scene>""")
    e.match('unexpected attribute "param2" in element "shape".')


def test12_missing_attribute():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="2.0.0">
                   <integer name="a"/></scene>""")
    e.match('missing attribute "value" in element "integer".')


def test13_duplicate_parameter():
    logger = Thread.thread().logger()
    l = logger.error_level()
    try:
        logger.set_error_level(LogLevel.Warn)
        with pytest.raises(Exception) as e:
            load_string("""<scene version="2.0.0">
                       <integer name="a" value="1"/>
                       <integer name="a" value="1"/>
                       </scene>""")
    finally:
        logger.set_error_level(l)
    e.match('Property "a" was specified multiple times')


def test14_missing_parameter():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="2.0.0">
                   <shape type="ply"/>
                   </scene>""")
    e.match('Property "filename" has not been specified')


def test15_incorrect_parameter_type():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="2.0.0">
                   <shape type="ply">
                      <float name="filename" value="1.0"/>
                   </shape></scene>""")
    e.match('The property "filename" has the wrong type'
            ' \\(expected <string>\\).')


def test16_invalid_integer():
    def test_input(value, valid):
        expected = (r'.*unreferenced property ."test_number".*' if valid
                    else r'could not parse integer value "{}".'.format(value))
        with pytest.raises(Exception, match=expected):
            load_string("""<scene version="2.0.0">
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


def test17_invalid_float():
    def test_input(value, valid):
        expected = (r'.*unreferenced property ."test_number".*' if valid
                    else r'could not parse floating point value "{}".'.format(value))
        with pytest.raises(Exception, match=expected):
            load_string("""<scene version="2.0.0">
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


def test18_invalid_boolean():
    with pytest.raises(Exception) as e:
        load_string("""<scene version="2.0.0">
                   <boolean name="10" value="a"/>
                   </scene>""")
    e.match('could not parse boolean value "a"'
            ' -- must be "true" or "false".')


def test19_invalid_vector():
    err_str = 'could not parse floating point value "a"'
    err_str2 = '"value" attribute must have exactly 1 or 3 elements'
    err_str3 = 'can\'t mix and match "value" and "x"/"y"/"z" attributes'

    with pytest.raises(Exception) as e:
        load_string("""<scene version="2.0.0">
                   <vector name="10" x="a" y="b" z="c"/>
                   </scene>""")
    e.match(err_str)

    with pytest.raises(Exception) as e:
        load_string("""<scene version="2.0.0">
                   <vector name="10" value="a, b, c"/>
                   </scene>""")
    e.match(err_str)

    with pytest.raises(Exception) as e:
        load_string("""<scene version="2.0.0">
                   <vector name="10" value="1, 2"/>
                   </scene>""")
    e.match(err_str2)

    with pytest.raises(Exception) as e:
        load_string("""<scene version="2.0.0">
                   <vector name="10" value="1, 2, 3" x="4"/>
                   </scene>""")
    e.match(err_str3)
