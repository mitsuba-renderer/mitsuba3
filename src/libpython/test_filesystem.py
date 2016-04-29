import unittest
from mitsuba import filesystem as fs

class FilesystemTest(unittest.TestCase):
    # TODO: test for windows-style paths on windows
    path1 = fs.path("/dir 1/dir 2/")
    path2 = fs.path("dir 3")
    path_empty = fs.path()
    path_here = fs.path("./")

    def __init__(self, *args, **kwargs):
        super(FilesystemTest, self).__init__(*args, **kwargs)
        FilesystemTest.path_empty.clear()

    def test00_empty_path(self):
        self.assertTrue(FilesystemTest.path_empty.empty())
        self.assertTrue(FilesystemTest.path_empty.is_relative())
        self.assertEqual(FilesystemTest.path_empty.parent_path(), fs.path(".."))

    def test01_path_status(self):
        self.assertFalse(fs.exists(FilesystemTest.path1))
        # TODO: this is not cross-platform
        self.assertTrue(fs.is_directory(FilesystemTest.path_here))
        self.assertFalse(fs.is_regular_file(FilesystemTest.path_here))
        self.assertFalse(FilesystemTest.path_here.empty())

    def test02_path_to_string(self):
        self.assertEqual(FilesystemTest.path1.__str__(), u"/dir 1/dir 2")

    def test03_create_and_remove_directory(self):
        # TODO: this is not cross-platform
        new_dir = fs.path("./my_shiny_new_directory")
        self.assertFalse(fs.exists(new_dir))
        self.assertTrue(fs.create_directory(new_dir))
        self.assertTrue(fs.exists(new_dir))
        self.assertTrue(fs.is_directory(new_dir))
        self.assertTrue(fs.remove(new_dir))

    def test04_navigation(self):
        # TODO: this is not cross-platform
        self.assertEqual(FilesystemTest.path1 / FilesystemTest.path2,
                         fs.path("/dir 1/dir 2/dir 3"))
        self.assertEqual((FilesystemTest.path1 / FilesystemTest.path2).parent_path(),
                         FilesystemTest.path1)

        # Parent of the single-element (e.g. ./) paths should be the empty path
        self.assertTrue(FilesystemTest.path_here.parent_path().empty())
        self.assertTrue(FilesystemTest.path2.parent_path().empty())

if __name__ == '__main__':
    unittest.main()
