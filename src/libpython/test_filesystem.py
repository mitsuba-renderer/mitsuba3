import unittest
from mitsuba import filesystem as fs
from mitsuba.filesystem import PREFERRED_SEPARATOR as sep

# TODO: double-check that all of this is cross-platform
# TODO: write platform-specific tests (use python's own fs lib?), e.g. "C:\" is an absolute path
class FilesystemTest(unittest.TestCase):
    path_here_relative = fs.path("." + sep)
    path_here = fs.current_path()

    path1 = path_here / fs.path("dir 1" + sep + "dir 2" + sep)
    path2 = fs.path("dir 3")
    path_empty = fs.path()

    def __init__(self, *args, **kwargs):
        super(FilesystemTest, self).__init__(*args, **kwargs)
        FilesystemTest.path_empty.clear()

    def test00_empty_path(self):
        self.assertTrue(FilesystemTest.path_empty.empty())
        self.assertTrue(FilesystemTest.path_empty.is_relative())
        self.assertEqual(FilesystemTest.path_empty.parent_path(), fs.path(".."))
        self.assertFalse(FilesystemTest.path_here_relative.empty())

    def test01_path_status(self):
        self.assertFalse(fs.exists(fs.path("some random" + sep + "path")))

        self.assertTrue(fs.is_directory(FilesystemTest.path_here_relative))
        self.assertTrue(fs.is_directory(FilesystemTest.path_here_relative))
        self.assertFalse(fs.is_regular_file(FilesystemTest.path_here_relative))
        self.assertTrue(fs.is_directory(FilesystemTest.path_here))
        self.assertTrue(fs.is_directory(FilesystemTest.path_here))
        self.assertFalse(fs.is_regular_file(FilesystemTest.path_here))

    def test02_path_to_string(self):
        # TODO: this is not cross-platform
        self.assertEqual(FilesystemTest.path1.__str__(),
                         FilesystemTest.path_here.__str__() + sep + "dir 1" + sep + "dir 2")

    def test03_create_and_remove_directory(self):
        # TODO: this is not cross-platform
        new_dir = FilesystemTest.path_here / fs.path("my_shiny_new_directory")
        self.assertFalse(fs.exists(new_dir))
        self.assertTrue(fs.create_directory(new_dir))
        self.assertTrue(fs.exists(new_dir))
        self.assertTrue(fs.is_directory(new_dir))
        self.assertTrue(fs.remove(new_dir))

    def test04_navigation(self):
        # TODO: this is not cross-platform
        self.assertEqual(fs.path("dir 1" + sep + "dir 2") / FilesystemTest.path2,
                         fs.path("dir 1" + sep + "dir 2" + sep + "dir 3"))
        self.assertEqual((FilesystemTest.path1 / FilesystemTest.path2).parent_path(),
                         FilesystemTest.path1)
        self.assertEqual(FilesystemTest.path1.parent_path().parent_path(),
                         FilesystemTest.path_here)

        # Parent of the single-element (e.g. ./) paths should be the empty path
        self.assertTrue(FilesystemTest.path_here_relative.parent_path().empty())
        self.assertTrue(FilesystemTest.path2.parent_path().empty())

    def test05_comparison(self):
        self.assertTrue(fs.path("file1.ext") == fs.path("file1.ext"))
        self.assertTrue(not (fs.path("file1.ext") == fs.path("another.ext")))

    def test06_filename(self):
        self.assertEqual(fs.path("weird 'file'.ext").filename(), u"weird 'file'.ext")
        self.assertEqual(fs.path("dir" + sep + "file.ext").filename(), u"file.ext")
        self.assertEqual(fs.path("dir" + sep).filename(), u"dir")
        self.assertEqual((FilesystemTest.path_here / fs.path("file")).filename(), u"file")

        self.assertEqual(FilesystemTest.path_empty.filename(), u"")
        self.assertEqual(fs.path(".").filename(), u".")
        self.assertEqual(fs.path("..").filename(), u"..")
        self.assertEqual(fs.path("foo" + sep + ".").filename(), u".")
        self.assertEqual(fs.path("foo" + sep + "..").filename(), u"..")

    def test07_extension(self):
        self.assertEqual(fs.path("dir" + sep + "file.ext").extension(), u".ext")
        self.assertEqual(fs.path("dir" + sep).extension(), u"")
        self.assertEqual((FilesystemTest.path_here / fs.path(".hidden")).extension(), u".hidden")

        self.assertEqual(FilesystemTest.path_empty.extension(), u"")
        self.assertEqual(fs.path(".").extension(), u"")
        self.assertEqual(fs.path("..").extension(), u"")
        self.assertEqual(fs.path("foo" + sep + ".").extension(), u"")
        self.assertEqual(fs.path("foo" + sep + "..").extension(), u"")

if __name__ == '__main__':
    unittest.main()
