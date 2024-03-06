#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/core/mmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/python/python.h>

#include <nanobind/ndarray.h>

MI_PY_EXPORT(MemoryMappedFile) {
    MI_PY_CLASS(MemoryMappedFile, Object)
        .def(nb::init<const mitsuba::filesystem::path &, size_t>(),
            D(MemoryMappedFile, MemoryMappedFile), "filename"_a, "size"_a)
        .def(nb::init<const mitsuba::filesystem::path &, bool>(),
            D(MemoryMappedFile, MemoryMappedFile, 2), "filename"_a, "write"_a = false)
        .def("__init__", [](MemoryMappedFile* t, 
            const mitsuba::filesystem::path &p, 
            nb::ndarray<nb::device::cpu> array) {
            size_t size = array.size() * array.itemsize();
            auto m = new (t) MemoryMappedFile(p, size);
            memcpy(m->data(), array.data(), size);
        }, "filename"_a, "array"_a)
        .def("size", &MemoryMappedFile::size, D(MemoryMappedFile, size))
        .def("data", nb::overload_cast<>(&MemoryMappedFile::data), D(MemoryMappedFile, data))
        .def("resize", &MemoryMappedFile::resize, D(MemoryMappedFile, resize))
        .def("filename", &MemoryMappedFile::filename, D(MemoryMappedFile, filename))
        .def("can_write", &MemoryMappedFile::can_write, D(MemoryMappedFile, can_write))
        .def_static("create_temporary", &MemoryMappedFile::create_temporary, D(MemoryMappedFile, create_temporary))
        .def("__array__", [](MemoryMappedFile &m) {
            return nb::ndarray<nb::numpy, uint8_t>((uint8_t*) m.data(), { m.size() }, nb::handle());
        }, nb::rv_policy::reference_internal);
}
