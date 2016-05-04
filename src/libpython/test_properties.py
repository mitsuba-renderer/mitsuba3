import unittest
from mitsuba import Properties as Prop

def fillProperties(p):
    """Sets up some properties with various types"""
    p['prop_1'] = 1
    p['prop_2'] = '1'
    p['prop_3'] = False
    p['prop_4'] = 3.14

class PropertiesTest(unittest.TestCase):

    def setUp(self):
        # Provide a fresh Properties instance
        self.p = Prop()

    def test01_name_and_id(self):
        self.p.setID("magic")
        self.p.setPluginName("unicorn")
        self.assertEqual(self.p.getID(), "magic")
        self.assertEqual(self.p.getPluginName(), "unicorn")

        p2 = Prop(self.p.getPluginName())
        self.assertEqual(self.p.getPluginName(), p2.getPluginName())

    def test02_type_is_preserved(self):
        fillProperties(self.p)
        self.assertEqual(self.p['prop_1'], 1)
        self.assertEqual(self.p['prop_2'], '1')
        self.assertEqual(self.p['prop_3'], False)
        # TODO: why such an extreme loss of precision?
        self.assertAlmostEqual(self.p['prop_4'], 3.14, places=6)

        # Updating an existing property but using a different type
        self.p['prop_2'] = 2
        self.assertEqual(self.p['prop_2'], 2)

    def test03_management_of_properties(self):
        fillProperties(self.p)
        # Existance
        self.assertTrue(self.p.hasProperty('prop_1'))
        self.assertFalse(self.p.hasProperty('random_unset_property'))
        # Removal
        self.assertTrue(self.p.removeProperty('prop_2'))
        self.assertFalse(self.p.hasProperty('prop_2'))
        # Update
        self.p['prop_1'] = 42
        self.assertTrue(self.p.hasProperty('prop_1'))

    def test04_queried_properties(self):
        fillProperties(self.p)
        # Make some queries
        _ = self.p['prop_1']
        _ = self.p['prop_2']
        # Check queried status
        self.assertTrue(self.p.wasQueried('prop_1'))
        self.assertTrue(self.p.wasQueried('prop_2'))
        self.assertFalse(self.p.wasQueried('prop_3'))
        self.assertEqual(self.p.getUnqueried(), ['prop_3', 'prop_4'])

        # Mark field as queried explicitly
        self.p.markQueried('prop_3')
        self.assertTrue(self.p.wasQueried('prop_3'))
        self.assertEqual(self.p.getUnqueried(), ['prop_4'])

    def test05_copy_and_merge(self):
        fillProperties(self.p)

        # Merge with dinstinct sets
        p2 = Prop()
        p2['hello'] = 42
        self.p.merge(p2)
        self.assertEqual(self.p['hello'], 42)
        self.assertEqual(self.p['prop_3'], False)
        self.assertEqual(p2['hello'], 42) # p2 unchanged

        # Merge with override (and different type)
        p3 = Prop()
        p3['hello'] = 'world'
        p2.merge(p3)
        self.assertEqual(p2['hello'], 'world')
        self.assertEqual(p2['hello'], 'world') # p3 unchanged
        pass

    def test06_equality(self):
        fillProperties(self.p)

        # Equality should encompass properties, their type,
        # the instance's pluginName and id properties
        p2 = Prop()
        p2['prop_1'] = 1
        p2['prop_2'] = '1'
        p2['prop_3'] = False
        self.assertFalse(self.p == p2)

        p2['prop_4'] = 3.14
        self.assertTrue(self.p == p2)

        p2.setPluginName("some_name")
        self.assertFalse(self.p == p2)
        p2.setPluginName("")
        p2.setID("some_id")
        self.p.setID("some_id")
        self.assertTrue(self.p == p2)

        pass

    def test07_printing(self):
        self.p.setID('some_id')
        self.p['prop_1'] = 1
        self.p['prop_2'] = 'hello'

        string = str(self.p)
        self.assertEqual(string,
"""Properties[
  pluginName = "",
  id = "some_id",
  elements = {
    "prop_1" -> 1,
    "prop_2" -> "hello"
  }
]
""")

if __name__ == '__main__':
    unittest.main()
