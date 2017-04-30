#include <mitsuba/core/stream.h>
#include <mitsuba/core/astream.h>
#include <mitsuba/core/dstream.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/core/zstream.h>

#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/python/python.h>

#define DECLARE_RW(Type, ReadableName)                                         \
    def("read_" ReadableName,                                                  \
        [](Stream &s) {                                                        \
            Type v;                                                            \
            s.read(v);                                                         \
            return py::cast(v);                                                \
        }, D(Stream, read, 2))                                                 \
    .def("write_" ReadableName,                                                \
         [](Stream &s, const Type &v) {                                        \
             s.write(v);                                                       \
             return py::cast(v);                                               \
         }, D(Stream, write, 2))

MTS_PY_EXPORT(Stream) {
    auto c = MTS_PY_CLASS(Stream, Object)
        .mdef(Stream, close)
        .mdef(Stream, set_byte_order)
        .mdef(Stream, byte_order)
        .mdef(Stream, seek)
        .mdef(Stream, truncate)
        .mdef(Stream, tell)
        .mdef(Stream, size)
        .mdef(Stream, flush)
        .mdef(Stream, can_read)
        .mdef(Stream, can_write)
        .def_static("host_byte_order", Stream::host_byte_order, D(Stream, host_byte_order))
        .def("write", [](Stream &s, py::bytes b) {
            std::string data(b);
            s.write(data.c_str(), data.size());
        }, D(Stream, write))
        .def("read", [](Stream &s, size_t size) {
            std::unique_ptr<char> tmp(new char[size]);
            s.read((void *) tmp.get(), size);
            return py::bytes(tmp.get(), size);
        }, D(Stream, write))
        .DECLARE_RW(int64_t, "long")
        .DECLARE_RW(float, "single")
        .DECLARE_RW(double, "double")
        .DECLARE_RW(Float, "float")
        .DECLARE_RW(bool, "bool")
        .DECLARE_RW(std::string, "string");

    py::enum_<Stream::EByteOrder>(c, "EByteOrder", D(Stream, EByteOrder))
        .value("EBigEndian", Stream::EBigEndian)
        .value("ELittleEndian", Stream::ELittleEndian)
        .value("ENetworkByteOrder", Stream::ENetworkByteOrder)
        .export_values();
}

#undef DECLARE_RW

MTS_PY_EXPORT(DummyStream) {
    MTS_PY_CLASS(DummyStream, Stream)
        .def(py::init<>(), D(DummyStream, DummyStream));
}

MTS_PY_EXPORT(FileStream) {
    auto fs = MTS_PY_CLASS(FileStream, Stream)
        .mdef(FileStream, path);

    py::enum_<FileStream::EMode>(fs, "EMode", D(FileStream, EMode))
        .value("ERead", FileStream::ERead)
        .value("EReadWrite", FileStream::EReadWrite)
        .value("ETruncReadWrite", FileStream::ETruncReadWrite)
        .export_values();

    fs.def(py::init<const mitsuba::filesystem::path &, FileStream::EMode>(),
           "p"_a, "mode"_a = FileStream::ERead, D(FileStream, FileStream));
}

MTS_PY_EXPORT(MemoryStream) {
    MTS_PY_CLASS(MemoryStream, Stream)
        .def(py::init<size_t>(), D(MemoryStream, MemoryStream),
             "capacity"_a = 512)
        .mdef(MemoryStream, capacity)
        .mdef(MemoryStream, owns_buffer);
}

MTS_PY_EXPORT(ZStream) {
    auto c = MTS_PY_CLASS(ZStream, Stream);

    py::enum_<ZStream::EStreamType>(c, "EStreamType", D(ZStream, EStreamType))
        .value("EDeflateStream", ZStream::EDeflateStream)
        .value("EGZipStream", ZStream::EGZipStream)
        .export_values();


    c.def(py::init<Stream*, ZStream::EStreamType, int>(), D(ZStream, ZStream),
          "child_stream"_a,
          "streamType"_a = ZStream::EDeflateStream,
          "level"_a = Z_DEFAULT_COMPRESSION)
        .def("child_stream", [](ZStream &stream) {
            return py::cast(stream.child_stream());
        }, D(ZStream, child_stream));


}

namespace {
struct declare_astream_accessors {
    using PyClass = pybind11::class_<mitsuba::AnnotatedStream,
                                     mitsuba::Object,
                                     mitsuba::ref<mitsuba::AnnotatedStream>>;

    template <typename T>
    static void apply(PyClass &c) {
        c.def("set", [](AnnotatedStream& s,
                        const std::string &name, const T &value) {
            s.set(name, value);
        }, D(AnnotatedStream, set));
    }
};

/// Use this type alias to list the supported types. Be wary of automatic type conversions.
// TODO: support all supported types that can occur in Python
using methods_declarator = for_each_type<bool, int64_t, Float, std::string>;

}  // end anonymous namespace

MTS_PY_EXPORT(AnnotatedStream) {
    auto c = MTS_PY_CLASS(AnnotatedStream, Object)
        .def(py::init<ref<Stream>, bool, bool>(),
             D(AnnotatedStream, AnnotatedStream),
             "stream"_a, "write_mode"_a, "throw_on_missing"_a = true)
        .mdef(AnnotatedStream, close)
        .mdef(AnnotatedStream, push)
        .mdef(AnnotatedStream, pop)
        .mdef(AnnotatedStream, keys)
        .mdef(AnnotatedStream, size)
        .mdef(AnnotatedStream, can_read)
        .mdef(AnnotatedStream, can_write);

    // Get: we can "infer" the type from type information stored in the ToC.
    // We perform a series of try & catch until we find the right type. This is
    // very inefficient, but better than leaking the type info abstraction, which
    // is private to the AnnotatedStream.
    c.def("get", [](AnnotatedStream& s, const std::string &name) {
        const auto keys = s.keys();
        bool keyExists = find(keys.begin(), keys.end(), name) != keys.end();
        if (!keyExists)
            Throw("Key \"%s\" does not exist in AnnotatedStream. Available "
                  "keys: %s", name, s.keys());

#define TRY_GET_TYPE(Type)                     \
        try {                                  \
            Type v;                            \
            s.get(name, v);                    \
            return py::cast(v);                \
        } catch (const std::runtime_error &) { }

        TRY_GET_TYPE(bool);
        TRY_GET_TYPE(int64_t);
        TRY_GET_TYPE(Float);
        TRY_GET_TYPE(std::string);
#undef TRY_GET_TYPE

        Throw("Key \"%s\" exists but does not have a supported type.", name);
    }, D(AnnotatedStream, get), "name"_a);

    // get & set declarations for many types
    methods_declarator::recurse<declare_astream_accessors>(c);
}
