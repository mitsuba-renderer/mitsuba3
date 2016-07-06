try:
    import unittest2 as unittest
except:
    import unittest

from mitsuba.core.xml import loadString
from mitsuba.render import Scene
from mitsuba.core import Thread, EWarn


class XMLTest(unittest.TestCase):
    def test01_invalidXML(self):
        with self.assertRaises(Exception):
            loadString('<?xml version="1.0"?>')

    def test02_invalidRootNode(self):
        with self.assertRaises(Exception):
            loadString('<?xml version="1.0"?><invalid></invalid>')

    def test03_invalidRootNode(self):
        err_str = 'root element "integer" must be an object'
        with self.assertRaisesRegex(Exception, err_str):
            loadString('<?xml version="1.0"?><integer name="a" '
                       'value="10"></integer>')

    def test04_validRootNode(self):
        obj1 = loadString('<?xml version="1.0"?>\n<scene version="0.4.0">'
                          '</scene>')
        obj2 = loadString('<scene version="0.4.0"></scene>')
        self.assertTrue(type(obj1) is Scene)
        self.assertTrue(type(obj2) is Scene)

    def test05_duplicateID(self):
        err_str = r'Error while loading "<string>" \(at line 4, col 18\):' + \
            ' "shape" has duplicate id "myID" \(previous was at line 3,' + \
            ' col 18\)'
        with self.assertRaisesRegex(Exception, err_str):
            loadString("""
            <scene version="0.4.0">
                <shape type="ply" id="myID"/>
                <shape type="ply" id="myID"/>
            </scene>
            """)

    def test06_reservedID(self):
        err_str = 'invalid id "_test" in "shape": ' + \
            'leading underscores are reserved for internal identifiers'
        with self.assertRaisesRegex(Exception, err_str):
            loadString('<scene version="0.4.0">' +
                       '<shape type="ply" id="_test"/></scene>')

    def test06_reservedName(self):
        err_str = 'invalid parameter name "_test" in "integer": ' + \
            'leading underscores are reserved for internal identifiers'
        with self.assertRaisesRegex(Exception, err_str):
            loadString('<scene version="0.4.0">' +
                       '<shape type="ply">' +
                       '<integer name="_test" value="1"/></shape></scene>')

    def test06_incorrectNesting(self):
        err_str = 'node "shape" cannot occur as child of a property'
        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <shape type="ply">
                       <integer name="value" value="1">
                       <shape type="ply"/>
                       </integer></shape></scene>""")

    def test07_incorrectNesting(self):
        err_str = 'node "float" cannot occur as child of a property'
        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <shape type="ply">
                       <integer name="value" value="1">
                       <float name="value" value="1"/>
                       </integer></shape></scene>""")

    def test08_incorrectNesting(self):
        err_str = 'transform operations can only occur in a transform node'
        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <shape type="ply">
                       <translate name="value" x="0" y="1" z="2"/>
                       </shape></scene>""")

    def test09_incorrectNesting(self):
        err_str = 'transform nodes can only contain transform operations'
        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <shape type="ply">
                       <transform name="toWorld">
                       <integer name="value" value="10"/>
                       </transform>
                       </shape></scene>""")

    def test10_unknownID(self):
        err_str = 'reference to unknown object "unknown"'
        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <ref id="unknown"/>
                       </scene>""")

    def test11_unknownAttribute(self):
        err_str = 'unexpected attribute "param2" in "shape"'
        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <shape type="ply" param2="abc">
                       </shape></scene>""")

    def test12_missingAttribute(self):
        err_str = 'missing attribute "value" in "integer"'
        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <integer name="a"/></scene>""")

    def test13_duplicateParameter(self):
        err_str = 'Property "a" was specified multiple times'
        logger = Thread.thread().logger()
        l = logger.errorLevel()
        logger.setErrorLevel(EWarn)
        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <integer name="a" value="1"/>
                       <integer name="a" value="1"/>
                       </scene>""")
        logger.setErrorLevel(l)

    def test14_missingParameter(self):
        err_str = 'Property "filename" has not been specified'
        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <shape type="ply"/>
                       </scene>""")

    def test15_incorrectParameterType(self):
        err_str = r'The property "filename" has the wrong type' + \
            ' \(expected <string>\).'
        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <shape type="ply">
                          <float name="filename" value="1.0"/>
                       </shape></scene>""")

    def test16_invalidInteger(self):
        err_str = r'Could not parse integer value "a"'
        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <integer name="10" value="a"/>
                       </scene>""")

    def test17_invalidFloat(self):
        err_str = r'Could not parse floating point value "a"'
        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <float name="10" value="a"/>
                       </scene>""")

    def test18_invalidBoolean(self):
        err_str = r'Could not parse boolean value "a"' + \
                   ' -- must be "true" or "false"'
        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <boolean name="10" value="a"/>
                       </scene>""")

    def test19_invalidVector(self):
        err_str = r'Could not parse floating point value "a"'
        err_str2 = r'"value" attribute must have exactly 3 elements'
        err_str3 = r'Can\'t mix and match "value" and "x"/"y"/"z" attributes'

        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <vector name="10" x="a" y="b" z="c"/>
                       </scene>""")

        with self.assertRaisesRegex(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <vector name="10" value="a, b, c"/>
                       </scene>""")

        with self.assertRaisesRegex(Exception, err_str2):
            loadString("""<scene version="0.4.0">
                       <vector name="10" value="1, 2"/>
                       </scene>""")

        with self.assertRaisesRegex(Exception, err_str3):
            loadString("""<scene version="0.4.0">
                       <vector name="10" value="1, 2, 3" x="4"/>
                       </scene>""")


if __name__ == '__main__':
    unittest.main()
