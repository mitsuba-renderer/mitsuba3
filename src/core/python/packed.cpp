#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/core/packed.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>

MI_PY_EXPORT(PackedFile) {
    auto cls = MI_PY_CLASS(PackedFile, Object)
        .def(nb::init<const fs::path &>(), "filename"_a,
             D(PackedFile, PackedFile))
        .def_static_method(PackedFile, open, "filename"_a)
        .def_static_method(PackedFile, clear_cache)
        .def_method(PackedFile, filename)
        .def_method(PackedFile, can_write)
        .def_method(PackedFile, entry_count)
        .def_method(PackedFile, entry_name, "index"_a)
        .def_method(PackedFile, find, "name"_a)
        .def("entry", &PackedFile::entry, "index"_a, D(PackedFile, entry),
             nb::keep_alive<0, 1>())
        .def_method(PackedFile, begin, "name"_a)
        .def("stream", &PackedFile::stream, D(PackedFile, stream),
             nb::rv_policy::reference_internal)
        .def("write_array", [](PackedFile &f, nb::bytes data) {
                const char *ptr = data.c_str();
                size_t size = data.size();
                nb::gil_scoped_release release;
                f.write_array(ptr, size);
             }, "data"_a, D(PackedFile, write_array))
        .def_static("compress_array", [](nb::bytes data) {
                const char *ptr = data.c_str();
                size_t size = data.size();
                ref<MemoryStream> stream;
                {
                    nb::gil_scoped_release release;
                    stream = new MemoryStream();
                    PackedFile::write_array(stream.get(), ptr, size);
                }
                return nb::bytes((const char *) stream->raw_buffer(),
                                 stream->size());
             }, "data"_a,
             "Return the encoding that ``write_array()`` would append to "
             "the current entry, for writing through ``stream()`` later")
        .def_method(PackedFile, close);

    using Entry = PackedFile::Entry;
    nb::class_<Entry>(cls, "Entry", D(PackedFile, Entry))
        .def("name", &Entry::name, D(PackedFile, Entry, name))
        .def("index", &Entry::index, D(PackedFile, Entry, index))
        .def("size", &Entry::size, D(PackedFile, Entry, size))
        .def("remaining", &Entry::remaining, D(PackedFile, Entry, remaining))
        .def("read", [](Entry &e, size_t size) {
                const char *ptr = (const char *) e.data();
                e.skip(size);
                return nb::bytes(ptr, size);
             }, "size"_a, "Return the next ``size`` bytes of the entry")
        .def("read_uint8", &Entry::read<uint8_t>, D(PackedFile, Entry, read))
        .def("read_uint16", &Entry::read<uint16_t>, D(PackedFile, Entry, read))
        .def("read_uint32", &Entry::read<uint32_t>, D(PackedFile, Entry, read))
        .def("read_uint64", &Entry::read<uint64_t>, D(PackedFile, Entry, read))
        .def("read_float", &Entry::read<float>, D(PackedFile, Entry, read))
        .def("read_string", &Entry::read_string, D(PackedFile, Entry, read_string))
        .def("skip", &Entry::skip, "size"_a, D(PackedFile, Entry, skip))
        .def("read_array", [](Entry &e, size_t size) {
                std::unique_ptr<char[]> buf(new char[size]);
                {
                    nb::gil_scoped_release release;
                    e.read_array(buf.get(), size);
                }
                return nb::bytes(buf.get(), size);
             }, "size"_a, D(PackedFile, Entry, read_array))
        .def("skip_array", &Entry::skip_array, D(PackedFile, Entry, skip_array));
}
