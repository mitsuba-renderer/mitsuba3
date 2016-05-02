import unittest
from mitsuba import Properties as Prop

class PropertiesTest(unittest.TestCase):

    def test01_name_and_id(self):
        p = Prop()
        p.setID("magic")
        p.setPluginName("unicorn")
        self.assertEqual(p.getID(), "magic")
        self.assertEqual(p.getPluginName(), "unicorn")

        p2 = Prop(p.getPluginName())
        self.assertEqual(p.getPluginName(), p2.getPluginName())

    def test01_management_of_properties(self):
        pass

    def test02_queried_properties(self):
        pass

    def test03_copy_and_merge(self):
        pass

    def test04_printing(self):
        # p = Prop()
        # p['key'] = "value"
        # self.assertEqual(p.__str__(), "Properties[ { \"key\" = \"value\" ]")
        pass

if __name__ == '__main__':
    unittest.main()
