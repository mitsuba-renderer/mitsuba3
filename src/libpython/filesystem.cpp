#include "python.h"
#include <mitsuba/core/filesystem.h>

using namespace mitsuba::filesystem;

MTS_PY_EXPORT(filesystem) {
    auto m2 = m.def_submodule("filesystem", "Lightweight cross-platform filesystem utilities");

    py::class_<path>(m2, "path", DM(filesystem, path))
        .def(py::init<>(), DM(filesystem, path, path))
        .def(py::init<const path &>(), DM(filesystem, path, path, 2))
        .def(py::init<const string_type &>(), DM(filesystem, path, path, 4))
        .def("clear", &path::clear, DM(filesystem, path, clear))
        .def("empty", &path::empty, DM(filesystem, path, empty))
        .def("is_absolute", &path::is_absolute, DM(filesystem, path, is_absolute))
        .def("is_relative", &path::is_relative, DM(filesystem, path, is_relative))
        .def("parent_path", &path::parent_path, DM(filesystem, path, parent_path))
        .def("extension", &path::extension, DM(filesystem, path, extension))
        .def("filename", &path::filename, DM(filesystem, path, filename))
        .def("native", &path::native, DM(filesystem, path, native))
        .def(py::self / py::self, DM(filesystem, path, operator_div))
        .def(py::self == py::self, DM(filesystem, path, operator_eq))
        .def(py::self != py::self, DM(filesystem, path, operator_ne))
        .def("__repr__", &path::native, DM(filesystem, path, native));

    m2.attr("preferred_separator") = py::cast(preferred_separator);

    m2.def("current_path", &current_path, DM(filesystem, current_path));
    m2.def("absolute", &absolute, DM(filesystem, absolute));
    m2.def("is_regular_file", &is_regular_file, DM(filesystem, is_regular_file));
    m2.def("is_directory", &is_directory, DM(filesystem, is_directory));
    m2.def("exists", &exists, DM(filesystem, exists));
    m2.def("file_size", &file_size, DM(filesystem, file_size));
    m2.def("equivalent", &equivalent, DM(filesystem, equivalent));
    m2.def("create_directory", &create_directory, DM(filesystem, create_directory));
    m2.def("resize_file", &resize_file, DM(filesystem, resize_file));
    m2.def("remove", &filesystem::remove, DM(filesystem, remove));

#if 0
    py::class_<resolver>(fs_module, "resolver")
        .def(py::init<>())
        .def("__len__", &resolver::size)
        .def("append", &resolver::append)
        .def("prepend", &resolver::prepend)
        .def("resolve", &resolver::resolve)
        .def("__getitem__", [](const resolver &r, size_t i) {
             if (i >= r.size())
                 throw py::index_error();
             return r[i];
         })
        .def("__setitem__", [](resolver &r, size_t i, path &v) {
             if (i >= r.size())
                 throw py::index_error();
             r[i] = v;
         })
        .def("__delitem__", [](resolver &r, size_t i) {
             if (i >= r.size())
                 throw py::index_error();
             r.erase(r.begin() + i);
         })
        .def("__repr__", [](const resolver &r) {
            std::ostringstream oss;
            oss << r;
            return oss.str();
        });

    py::class_<MemoryMappedFile>(fs_module, "MemoryMappedFile")
        .def(py::init<fs::path, size_t>())
        .def(py::init<fs::path, bool>())
        .def(py::init<fs::path>())
        .def("resize", &MemoryMappedFile::resize)
        .def("__repr__", &MemoryMappedFile::toString)
        .def("size", &MemoryMappedFile::size)
        .def("filename", &MemoryMappedFile::filename)
        .def("readOnly", &MemoryMappedFile::readOnly)
        .def_static("createTemporary", &MemoryMappedFile::createTemporary)
        .def_buffer([](MemoryMappedFile &m) -> py::buffer_info {
            return py::buffer_info(
                m.data(),
                sizeof(uint8_t),
                py::format_descriptor<uint8_t>::value(),
                1,
                { (size_t) m.size() },
                { sizeof(uint8_t) }
            );
        });

#endif
    py::implicitly_convertible<py::str, path>();
}
