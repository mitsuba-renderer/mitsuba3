#include <mitsuba/core/filesystem.h>
#include <mitsuba/python/python.h>

using namespace mitsuba::filesystem;

MTS_PY_EXPORT(filesystem) {
    // Create dedicated submodule
    auto fs = m.def_submodule("filesystem", "Lightweight cross-platform filesystem utilities");

    py::class_<path>(fs, "path", D(filesystem, path))
        .def(py::init<>(), D(filesystem, path, path))
        .def(py::init<const path &>(), D(filesystem, path, path, 2))
        .def(py::init<const string_type &>(), D(filesystem, path, path, 4))
        .def("clear", &path::clear, D(filesystem, path, clear))
        .def("empty", &path::empty, D(filesystem, path, empty))
        .def("is_absolute", &path::is_absolute, D(filesystem, path, is_absolute))
        .def("is_relative", &path::is_relative, D(filesystem, path, is_relative))
        .def("parent_path", &path::parent_path, D(filesystem, path, parent_path))
        .def("extension", &path::extension, D(filesystem, path, extension))
        .def("replace_extension", &path::replace_extension, D(filesystem, path, replace_extension))
        .def("filename", &path::filename, D(filesystem, path, filename))
        .def("native", &path::native, D(filesystem, path, native))
        .def(py::self / py::self, D(filesystem, path, operator_div))
        .def(py::self == py::self, D(filesystem, path, operator_eq))
        .def(py::self != py::self, D(filesystem, path, operator_ne))
        .def("__repr__", &path::native, D(filesystem, path, native));

    fs.attr("preferred_separator") = py::cast(preferred_separator);

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

    py::implicitly_convertible<py::str, path>();
}
