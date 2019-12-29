import platform
import re
import enoki as ek
import pytest
import mitsuba

mitsuba.set_variant('scalar_rgb')

from mitsuba.core import filesystem as fs
from mitsuba.core.filesystem import preferred_separator as sep

path_here_relative = fs.path("." + sep)
path_here = fs.current_path()

path1 = path_here / fs.path("dir 1" + sep + "dir 2" + sep)
path2 = fs.path("dir 3")
platform_system = platform.system()
empty_path = fs.path()
empty_path.clear()


def test00_empty_path():
    assert empty_path.empty()
    assert empty_path.is_relative()
    assert empty_path.parent_path() == fs.path("..")
    assert not path_here_relative.empty()


def test01_path_status():
    assert not fs.exists(fs.path("some random" + sep + "path"))

    assert fs.is_directory(path_here_relative)
    assert fs.is_directory(path_here_relative)
    assert not fs.is_regular_file(path_here_relative)
    assert fs.is_directory(path_here)
    assert fs.is_directory(path_here)
    assert not fs.is_regular_file(path_here)


def test02_path_to_string():
    assert path1.__str__() == \
        path_here.__str__() + sep + "dir 1" + sep + "dir 2"


def test03_create_and_remove_directory():
    new_dir = path_here / fs.path("my_shiny_new_directory")
    assert fs.create_directory(new_dir)
    assert fs.exists(new_dir)
    assert fs.is_directory(new_dir)

    # Asking to create a directory that already exists should not be treated
    # as an error, and return true.
    assert fs.create_directory(new_dir)

    assert fs.remove(new_dir)
    assert not fs.exists(new_dir)


def test04_navigation():
    assert fs.path("dir 1" + sep + "dir 2") / path2 == \
        fs.path("dir 1" + sep + "dir 2" + sep + "dir 3")
    assert (path1 / path2).parent_path() == path1
    assert path1.parent_path().parent_path() == path_here

    # Parent of the single-element (e.g. ./) paths should be the empty path
    assert path_here_relative.parent_path().empty()
    assert path2.parent_path().empty()


def test05_comparison():
    assert fs.path("file1.ext") == fs.path("file1.ext")
    assert not (fs.path("file1.ext") == fs.path("another.ext"))


def test06_filename():
    assert fs.path("weird 'file'.ext").filename() == "weird 'file'.ext"
    assert fs.path("dir" + sep + "file.ext").filename() == "file.ext"
    assert fs.path("dir" + sep).filename() == "dir"
    assert (path_here / fs.path("file")).filename() == "file"

    assert empty_path.filename() == ""
    assert fs.path(".").filename() == "."
    assert fs.path("..").filename() == ".."
    assert fs.path("foo" + sep + ".").filename() == "."
    assert fs.path("foo" + sep + "..").filename() == ".."


def test07_extension():
    assert fs.path("dir" + sep + "file.ext").extension() == ".ext"
    assert fs.path("dir" + sep).extension() == ""
    assert (path_here / fs.path(".hidden")).extension() == ".hidden"

    assert empty_path.extension() == ""
    assert fs.path(".").extension() == ""
    assert fs.path("..").extension() == ""
    assert fs.path("foo" + sep + ".").extension() == ""
    assert fs.path("foo" + sep + "..").extension() == ""

    # Replace extension
    assert empty_path.replace_extension(".ext") == empty_path
    assert fs.path(".").replace_extension(".ext") == fs.path(".")
    assert fs.path("..").replace_extension(".ext") == fs.path("..")
    assert fs.path("foo.bar").replace_extension(".ext") == fs.path("foo.ext")
    assert fs.path("foo.bar").replace_extension(fs.path(".ext")) == \
        fs.path("foo.ext")
    assert fs.path("foo").replace_extension(".ext") == fs.path("foo.ext")
    assert fs.path("foo.bar").replace_extension("none") == fs.path("foo.none")
    assert fs.path(sep + "foo" + sep + "bar.foo").replace_extension(".jpg") == \
        fs.path(sep + "foo" + sep + "bar.jpg")


def test08_make_absolute():
    assert fs.absolute(path_here_relative) == path_here


# Assumes either Windows or a POSIX system
def test09_system_specific_tests():
    if platform_system == 'Windows':
        drive_letter_regexp = re.compile('^[A-Z]:')
        assert drive_letter_regexp.match(str(path_here))
        assert drive_letter_regexp.match(str(fs.absolute(path_here_relative)))
        assert fs.path('C:\\hello').is_absolute()
        assert fs.path('..\\hello').is_relative()

        # Both kinds of separators should be accepted
        assert fs.path('../hello/world').native() == '..\\hello\\world'
    else:
        assert str(path_here)[0] == '/'
        assert str(fs.absolute(path_here_relative))[0] == '/'

        assert fs.path('/hello').is_absolute()
        assert fs.path('./hello').is_relative()


def test10_equivalence():
    p1 = fs.path('../my_directory')
    assert fs.create_directory(p1)

    p2 = fs.path('././../././my_directory')
    assert not p1 == p2
    assert fs.equivalent(p1, p2)

    assert fs.remove(p1)


def test11_implicit_string_cast():
    assert not fs.exists("some random" + sep + "path")


def test12_truncate_and_file_size():
    p = path_here / 'test_file_for_resize.txt'
    open(str(p), 'a').close()
    assert fs.resize_file(p, 42)
    assert fs.file_size(p) == 42
    assert fs.remove(p)
    assert not fs.exists(p)
