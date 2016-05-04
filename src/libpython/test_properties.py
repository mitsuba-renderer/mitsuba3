import unittest
from mitsuba import Properties as Prop

def fillProperties(p):
    """Sets up some properties with various types"""
    p['prop_1'] = 1
    p['prop_2'] = 'two'
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
        # TODO: make sure, this is important but could go unnoticed in Python
        # TODO: test updating a property but using a different type
        pass

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
        pass

    def test06_equality(self):
        pass

    def test07_printing(self):
        # p = Prop()
        # p['key'] = "value"
        # self.assertEqual(p.__str__(), "Properties[ { \"key\" = \"value\" ]")
        pass

if __name__ == '__main__':
    unittest.main()
