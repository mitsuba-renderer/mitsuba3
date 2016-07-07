#include <mitsuba/core/stream.h>
#include <mitsuba/core/astream.h>
#include <mitsuba/core/dstream.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/core/zstream.h>

#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/logger.h>
#include "python.h"

namespace {
struct declare_stream_accessors {
    using PyClass = pybind11::class_<mitsuba::Stream,
                                     mitsuba::ref<mitsuba::Stream>>;

    template <typename T>
    static void apply(PyClass &c) {
        c.def("write", [](Stream& s, const T &value) {
            s.write(value);
        }, DM(Stream, write, 2));
    }
};
struct declare_astream_accessors {
    using PyClass = pybind11::class_<mitsuba::AnnotatedStream,
                                     mitsuba::ref<mitsuba::AnnotatedStream>>;

    template <typename T>
    static void apply(PyClass &c) {
        c.def("set", [](AnnotatedStream& s,
                        const std::string &name, const T &value) {
            s.set(name, value);
        }, DM(AnnotatedStream, set));
    }
};

/// Use this type alias to list the supported types. Be wary of automatic type conversions.
// TODO: support all supported types that can occur in Python
// TODO: Python `long` type probably depends on the architecture, test on 32bits
using methods_declarator = for_each_type<bool, int64_t, Float, std::string>;

}  // end anonymous namespace

MTS_PY_EXPORT(Stream) {
#define DECLARE_READ(Type, ReadableName) \
    def("read" ReadableName, [](Stream& s) {     \
        Type v;                                  \
        s.read(v);                               \
        return py::cast(v);                      \
    }, DM(Stream, read, 2))

    auto c = MTS_PY_CLASS(Stream, Object)
        .mdef(Stream, close)
        .mdef(Stream, setByteOrder)
        .mdef(Stream, byteOrder)
        .mdef(Stream, seek)
        .mdef(Stream, truncate)
        .mdef(Stream, tell)
        .mdef(Stream, size)
        .mdef(Stream, flush)
        .mdef(Stream, canRead)
        .mdef(Stream, canWrite)
        .def_static("hostByteOrder", Stream::hostByteOrder, DM(Stream, hostByteOrder))
        .DECLARE_READ(int64_t, "Long")
        .DECLARE_READ(Float, "Float")
        .DECLARE_READ(bool, "Boolean")
        .DECLARE_READ(std::string, "String")
        .def("__repr__", &Stream::toString);
#undef DECLARE_READ

    methods_declarator::recurse<declare_stream_accessors>(c);

    py::enum_<Stream::EByteOrder>(c, "EByteOrder", DM(Stream, EByteOrder))
        .value("EBigEndian", Stream::EBigEndian)
        .value("ELittleEndian", Stream::ELittleEndian)
        .value("ENetworkByteOrder", Stream::ENetworkByteOrder)
        .export_values();
}

MTS_PY_EXPORT(DummyStream) {
    MTS_PY_CLASS(DummyStream, Stream)
        .def(py::init<>(), DM(DummyStream, DummyStream));
}

MTS_PY_EXPORT(FileStream) {
    MTS_PY_CLASS(FileStream, Stream)
        .def(py::init<const mitsuba::filesystem::path &, bool>(), DM(FileStream, FileStream))
        .mdef(FileStream, path);
}

MTS_PY_EXPORT(MemoryStream) {
    MTS_PY_CLASS(MemoryStream, Stream)
        .def(py::init<size_t>(), DM(MemoryStream, MemoryStream),
             py::arg("initialSize") = 512);
}

MTS_PY_EXPORT(ZStream) {
    auto c = MTS_PY_CLASS(ZStream, Stream);

    py::enum_<ZStream::EStreamType>(c, "EStreamType", DM(ZStream, EStreamType))
        .value("EDeflateStream", ZStream::EDeflateStream)
        .value("EGZipStream", ZStream::EGZipStream)
        .export_values();


    c.def(py::init<Stream*, ZStream::EStreamType, int>(), DM(ZStream, ZStream),
          py::arg("childStream"),
          py::arg("streamType") = ZStream::EDeflateStream,
          py::arg("level") = Z_DEFAULT_COMPRESSION)
        .def("childStream", [](ZStream &stream) {
            return py::cast(stream.childStream());
        }, DM(ZStream, childStream));


}

MTS_PY_EXPORT(AnnotatedStream) {
    auto c = MTS_PY_CLASS(AnnotatedStream, Object)
        .def(py::init<ref<Stream>, bool, bool>(),
             DM(AnnotatedStream, AnnotatedStream),
             py::arg("stream"), py::arg("writeMode"), py::arg("throwOnMissing") = true)
        .mdef(AnnotatedStream, close)
        .mdef(AnnotatedStream, push)
        .mdef(AnnotatedStream, pop)
        .mdef(AnnotatedStream, keys)
        .mdef(AnnotatedStream, size)
        .mdef(AnnotatedStream, canRead)
        .mdef(AnnotatedStream, canWrite)
        .def("__repr__", &AnnotatedStream::toString);

    // Get: we can "infer" the type from type information stored in the ToC.
    // We perform a series of try & catch until we find the right type. This is
    // very inefficient, but better than leaking the type info abstraction, which
    // is private to the AnnotatedStream.
    c.def("get", [](AnnotatedStream& s, const std::string &name) {
        const auto keys = s.keys();
        bool keyExists = find(keys.begin(), keys.end(), name) != keys.end();
        if (!keyExists) {
            const auto logLevel = s.compatibilityMode() ? EWarn : EError;
            Log(logLevel, "Key \"%s\" does not exist in AnnotatedStream. Available keys: %s",
                        name, s.keys());
        }

#define TRY_GET_TYPE(Type)                     \
        try {                                  \
            Type v;                            \
            s.get(name, v);                    \
            return py::cast(v);                \
        } catch(std::runtime_error&) {}      \

        TRY_GET_TYPE(bool);
        TRY_GET_TYPE(int64_t);
        TRY_GET_TYPE(Float);
        TRY_GET_TYPE(std::string);

#undef TRY_GET_TYPE

        Log(EError, "Key \"%s\" exists but does not have a supported type.",
                    name);
        return py::cast(py::none());
    }, DM(AnnotatedStream, get));

    // get & set declarations for many types
    methods_declarator::recurse<declare_astream_accessors>(c);
}
