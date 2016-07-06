try:
    import unittest2 as unittest
except:
    import unittest

from mitsuba.core.xml import loadString
from mitsuba.render import Scene


class XMLTest(unittest.TestCase):
    def test01_invalidXML(self):
        with self.assertRaises(Exception):
            loadString('<?xml version="1.0"?>')

    def test02_invalidRootNode(self):
        with self.assertRaises(Exception):
            loadString('<?xml version="1.0"?><invalid></invalid>')

    def test03_invalidRootNode(self):
        err_str = 'root element "integer" must be an object'
        with self.assertRaisesRegexp(Exception, err_str):
            loadString('<?xml version="1.0"?><integer name="a" '
                       'value="10"></integer>')

    def test04_validRootNode(self):
        obj1 = loadString('<?xml version="1.0"?>\n<scene version="0.4.0">'
                          '</scene>')
        obj2 = loadString('<scene version="0.4.0"></scene>')
        self.assertTrue(type(obj1) is Scene)
        self.assertTrue(type(obj2) is Scene)

    def test05_duplicateID(self):
        err_str = r'Error while parsing "<string>" \(at line 4, col 18\):' + \
            ' "shape" has duplicate id "myID" \(previous was at line 3,' + \
            ' col 18\)'
        with self.assertRaisesRegexp(Exception, err_str):
            loadString("""
            <scene version="0.4.0">
                <shape type="ply" id="myID"/>
                <shape type="ply" id="myID"/>
            </scene>
            """)

    def test06_reservedID(self):
        err_str = 'invalid id "_test" in "shape": ' + \
            'leading underscores are reserved for internal identifiers'
        with self.assertRaisesRegexp(Exception, err_str):
            loadString('<scene version="0.4.0">' +
                       '<shape type="ply" id="_test"/></scene>')

    def test06_reservedName(self):
        err_str = 'invalid parameter name "_test" in "integer": ' + \
            'leading underscores are reserved for internal identifiers'
        with self.assertRaisesRegexp(Exception, err_str):
            loadString('<scene version="0.4.0">' +
                       '<shape type="ply">' +
                       '<integer name="_test" value="1"/></shape></scene>')

    def test06_incorrectNesting(self):
        err_str = 'node "shape" cannot occur as child of a property'
        with self.assertRaisesRegexp(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <shape type="ply">
                       <integer name="value" value="1">
                       <shape type="ply"/>
                       </integer></shape></scene>""")

    def test07_incorrectNesting(self):
        err_str = 'node "float" cannot occur as child of a property'
        with self.assertRaisesRegexp(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <shape type="ply">
                       <integer name="value" value="1">
                       <float name="value" value="1"/>
                       </integer></shape></scene>""")

    def test08_incorrectNesting(self):
        err_str = 'transform operations can only occur in a transform node'
        with self.assertRaisesRegexp(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <shape type="ply">
                       <translate name="value" x="0" y="1" z="2"/>
                       </shape></scene>""")

    def test09_incorrectNesting(self):
        err_str = 'transform nodes can only contain transform operations'
        with self.assertRaisesRegexp(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <shape type="ply">
                       <transform name="toWorld">
                       <integer name="value" value="10"/>
                       </transform>
                       </shape></scene>""")

    def test10_unknownID(self):
        err_str = 'reference to unknown object "unknown"'
        with self.assertRaisesRegexp(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <ref id="unknown"/>
                       </scene>""")

    def test11_unknownAttribute(self):
        err_str = 'unexpected attribute "param2" in "shape"'
        with self.assertRaisesRegexp(Exception, err_str):
            loadString("""<scene version="0.4.0">
                       <shape type="ply" param2="abc">
                       </shape></scene>""")

if __name__ == '__main__':
    unittest.main()
