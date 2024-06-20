#include <mitsuba/core/filesystem.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/string.h>
#include <nanobind/stl/wstring.h>

using namespace mitsuba::filesystem;

MI_PY_EXPORT(filesystem) {
    // Create dedicated submodule
    auto fs = m.def_submodule("filesystem", "Lightweight cross-platform filesystem utilities");

    nb::class_<path>(fs, "path", D(filesystem, path))
        .def(nb::init<>(), D(filesystem, path, path))
        .def(nb::init<const path &>(), D(filesystem, path, path, 2))
        .def(nb::init<const string_type &>(), D(filesystem, path, path, 4))
        .def("clear", &path::clear, D(filesystem, path, clear))
        .def("empty", &path::empty, D(filesystem, path, empty))
        .def("is_absolute", &path::is_absolute, D(filesystem, path, is_absolute))
        .def("is_relative", &path::is_relative, D(filesystem, path, is_relative))
        .def("parent_path", &path::parent_path, D(filesystem, path, parent_path))
        .def("extension", &path::extension, D(filesystem, path, extension))
        .def("replace_extension", &path::replace_extension, D(filesystem, path, replace_extension))
        .def("filename", &path::filename, D(filesystem, path, filename))
        .def("native", &path::native, D(filesystem, path, native))
        .def(nb::self / nb::self, D(filesystem, path, operator_div))
        .def(nb::self == nb::self, D(filesystem, path, operator_eq))
        .def(nb::self != nb::self, D(filesystem, path, operator_ne))
        .def("__repr__", &path::native, D(filesystem, path, native));

    fs.attr("preferred_separator") = nb::cast(string_type(1, preferred_separator));

    fs.def("current_path", &current_path, D(filesystem, current_path));
    fs.def("absolute", &absolute, D(filesystem, absolute));
    fs.def("is_regular_file", &is_regular_file, D(filesystem, is_regular_file));
    fs.def("is_directory", &is_directory, D(filesystem, is_directory));
    fs.def("exists", &exists, D(filesystem, exists));
    fs.def("file_size", &file_size, D(filesystem, file_size));
    fs.def("equivalent", &equivalent, D(filesystem, equivalent));
    fs.def("create_directory", &create_directory, D(filesystem, create_directory));
    fs.def("resize_file", &resize_file, D(filesystem, resize_file));
    fs.def("remove", &filesystem::remove, D(filesystem, remove));

    nb::implicitly_convertible<nb::str, path>();
}
