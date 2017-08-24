#include <mitsuba/core/mmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(MemoryMappedFile) {
    py::class_<MemoryMappedFile>(m, "MemoryMappedFile", D(MemoryMappedFile), py::buffer_protocol())
        .def(py::init<mitsuba::filesystem::path &, size_t>(), D(MemoryMappedFile, MemoryMappedFile))
        .def(py::init<mitsuba::filesystem::path &, bool>(), D(MemoryMappedFile, MemoryMappedFile))
        .def(py::init<mitsuba::filesystem::path &>(), D(MemoryMappedFile, MemoryMappedFile))
        .def("__init__", [](MemoryMappedFile &m, mitsuba::filesystem::path &p, py::buffer b) {
            py::buffer_info info = b.request();
            if (info.format != py::format_descriptor<uint8_t>::format() || info.ndim != 1)
                throw std::runtime_error("Incompatible buffer format!");
            new (&m) MemoryMappedFile(p, (size_t)info.shape[0]);
            memcpy(m.data(), info.ptr, sizeof(uint8_t) * m.size());
        })
        .def("size", &MemoryMappedFile::size, D(MemoryMappedFile, size))
        .def("resize", &MemoryMappedFile::resize, D(MemoryMappedFile, resize))
        .def("filename", &MemoryMappedFile::filename, D(MemoryMappedFile, filename))
        .def("is_read_only", &MemoryMappedFile::is_read_only, D(MemoryMappedFile, is_read_only))
        .def("__repr__", &MemoryMappedFile::to_string, D(MemoryMappedFile, to_string))
        .def("create_temporary", &MemoryMappedFile::create_temporary, D(MemoryMappedFile, create_temporary))
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
