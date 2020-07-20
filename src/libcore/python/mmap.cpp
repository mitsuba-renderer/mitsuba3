#include <mitsuba/core/mmap.h>
#include <mitsuba/core/filesystem.h>
#include <pybind11/numpy.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(MemoryMappedFile) {
    MTS_PY_CLASS(MemoryMappedFile, Object, py::buffer_protocol())
        .def(py::init<const mitsuba::filesystem::path &, size_t>(),
            D(MemoryMappedFile, MemoryMappedFile), "filename"_a, "size"_a)
        .def(py::init<const mitsuba::filesystem::path &, bool>(),
            D(MemoryMappedFile, MemoryMappedFile, 2), "filename"_a, "write"_a = false)
        .def(py::init([](const mitsuba::filesystem::path &p, py::array array) {
            size_t size = array.size() * array.itemsize();
            auto m = new MemoryMappedFile(p, size);
            memcpy(m->data(), array.data(), size);
            return m;
        }), "filename"_a, "array"_a)
        .def("size", &MemoryMappedFile::size, D(MemoryMappedFile, size))
        .def("data", py::overload_cast<>(&MemoryMappedFile::data, py::const_), D(MemoryMappedFile, data))
        .def("resize", &MemoryMappedFile::resize, D(MemoryMappedFile, resize))
        .def("filename", &MemoryMappedFile::filename, D(MemoryMappedFile, filename))
        .def("can_write", &MemoryMappedFile::can_write, D(MemoryMappedFile, can_write))
        .def_static("create_temporary", &MemoryMappedFile::create_temporary, D(MemoryMappedFile, create_temporary))
        .def_buffer([](MemoryMappedFile &m) -> py::buffer_info {
            return py::buffer_info(
                m.data(),
                sizeof(uint8_t),
                py::format_descriptor<uint8_t>::format(),
                1,
                { (size_t) m.size() },
                { sizeof(uint8_t) }
            );
        });
}
