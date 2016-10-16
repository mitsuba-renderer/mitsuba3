#include "python.h"
#include <mitsuba/core/filesystem.h>

using namespace mitsuba::filesystem;

MTS_PY_EXPORT(filesystem) {
    MTS_PY_IMPORT_MODULE(fs, "mitsuba.core.filesystem");

    py::class_<path>(fs, "path", DM(filesystem, path))
        .def(py::init<>(), DM(filesystem, path, path))
        .def(py::init<const path &>(), DM(filesystem, path, path, 2))
        .def(py::init<const string_type &>(), DM(filesystem, path, path, 4))
        .def("clear", &path::clear, DM(filesystem, path, clear))
        .def("empty", &path::empty, DM(filesystem, path, empty))
        .def("is_absolute", &path::is_absolute, DM(filesystem, path, is_absolute))
        .def("is_relative", &path::is_relative, DM(filesystem, path, is_relative))
        .def("parent_path", &path::parent_path, DM(filesystem, path, parent_path))
        .def("extension", &path::extension, DM(filesystem, path, extension))
        .def("replace_extension", &path::replace_extension, DM(filesystem, path, replace_extension))
        .def("filename", &path::filename, DM(filesystem, path, filename))
        .def("native", &path::native, DM(filesystem, path, native))
        .def(py::self / py::self, DM(filesystem, path, operator_div))
        .def(py::self == py::self, DM(filesystem, path, operator_eq))
        .def(py::self != py::self, DM(filesystem, path, operator_ne))
        .def("__repr__", &path::native, DM(filesystem, path, native));

    fs.attr("preferred_separator") = py::cast(preferred_separator);

    fs.def("current_path", &current_path, DM(filesystem, current_path));
    fs.def("absolute", &absolute, DM(filesystem, absolute));
    fs.def("is_regular_file", &is_regular_file, DM(filesystem, is_regular_file));
    fs.def("is_directory", &is_directory, DM(filesystem, is_directory));
    fs.def("exists", &exists, DM(filesystem, exists));
    fs.def("file_size", &file_size, DM(filesystem, file_size));
    fs.def("equivalent", &equivalent, DM(filesystem, equivalent));
    fs.def("create_directory", &create_directory, DM(filesystem, create_directory));
    fs.def("resize_file", &resize_file, DM(filesystem, resize_file));
    fs.def("remove", &filesystem::remove, DM(filesystem, remove));

    py::implicitly_convertible<py::str, path>();
}
