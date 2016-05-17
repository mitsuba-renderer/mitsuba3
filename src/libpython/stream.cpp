#include <mitsuba/core/stream.h>
#include <mitsuba/core/astream.h>
#include <mitsuba/core/dstream.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/mstream.h>

#include <mitsuba/core/filesystem.h>
#include "python.h"

NAMESPACE_BEGIN()

/** \brief The following is used to ensure that the getters and setters
 * for all the same types are available for both \ref Stream implementations
 * and \AnnotatedStream. */

template<typename... Args> struct for_each_type;

template <typename T, typename ...Args>
struct for_each_type<T, Args...> {
    // TODO: improve syntax for the caller (e.g. operator() or something)
    template <typename UserFunctionType, typename ...Params>
    static void recurse(Params&& ...params) {
        UserFunctionType::template perform<T>(std::forward<Params>(params)...);
        for_each_type<Args...>::template recurse<UserFunctionType>(std::forward<Params>(params)...);
    }
};

/// Base case
template<>
struct for_each_type<> {
    template <typename UserFunctionType, typename... Params>
    static void recurse(Params&&... params) {}
};

// User provides a templated [T] function that takes arguments (which might depend on T)
struct declare_stream_accessors {
    using PyClass = pybind11::class_<mitsuba::Stream,
                                     mitsuba::ref<mitsuba::Stream>>;

    template <typename T>
    static void perform(PyClass &c) {
        c.def("readValue", [](Stream& s, T &value) {
            s.readValue(value);
        }, DM(Stream, readValue));
        c.def("writeValue", [](Stream& s, const T &value) {
            s.writeValue(value);
        }, DM(Stream, writeValue));
    }
};
struct declare_astream_accessors {
    using PyClass = pybind11::class_<mitsuba::AnnotatedStream,
                                     mitsuba::ref<mitsuba::AnnotatedStream>>;

    template <typename T>
    static void perform(PyClass &c) {
        c.def("get", [](AnnotatedStream& s,
                        const std::string &name, T &value) {
            return s.get(name, value);
        }, DM(AnnotatedStream, get));
        c.def("set", [](AnnotatedStream& s,
                        const std::string &name, const T &value) {
            s.set(name, value);
        }, DM(AnnotatedStream, set));
    }
};

/// Use this type alias to list the supported types.
// TODO: support all supported types that can occur in Python
using methods_declarator = for_each_type<int32_t, int64_t, Float,
                                         bool, std::string, char>;

NAMESPACE_END()

MTS_PY_EXPORT(Stream) {
    auto c = MTS_PY_CLASS(Stream, Object)
        .mdef(Stream, canWrite)
        .mdef(Stream, canRead)
        .mdef(Stream, setByteOrder)
        .mdef(Stream, getByteOrder)
        .mdef(Stream, getHostByteOrder)
        .def("__repr__", &Stream::toString);

    // TODO: readValue method should be pythonic
    methods_declarator::recurse<declare_stream_accessors>(c);

    py::enum_<Stream::EByteOrder>(c, "EByteOrder", DM(Stream, EByteOrder))
        .value("EBigEndian", Stream::EBigEndian)
        .value("ELittleEndian", Stream::ELittleEndian)
        .value("ENetworkByteOrder", Stream::ENetworkByteOrder)
        .export_values();
}

MTS_PY_EXPORT(DummyStream) {
    MTS_PY_CLASS(DummyStream, Stream)
        .def(py::init<>(), DM(DummyStream, DummyStream))
        .mdef(DummyStream, seek)
        .mdef(DummyStream, truncate)
        .mdef(DummyStream, getPos)
        .mdef(DummyStream, getSize)
        .mdef(DummyStream, flush)
        .mdef(DummyStream, canWrite)
        .mdef(DummyStream, canRead)
        .mdef(DummyStream, canRead)
        .def("__repr__", &DummyStream::toString);
}

MTS_PY_EXPORT(FileStream) {
    MTS_PY_CLASS(FileStream, Stream)
        .def(py::init<const mitsuba::filesystem::path &, bool>(), DM(FileStream, FileStream))
        .mdef(FileStream, seek)
        .mdef(FileStream, truncate)
        .mdef(FileStream, getPos)
        .mdef(FileStream, getSize)
        .mdef(FileStream, flush)
        .def("__repr__", &FileStream::toString);
}

MTS_PY_EXPORT(MemoryStream) {
    MTS_PY_CLASS(MemoryStream, Stream)
        .def(py::init<size_t>(), DM(MemoryStream, MemoryStream),
             py::arg("initialSize") = 512)
        .mdef(MemoryStream, seek)
        .mdef(MemoryStream, truncate)
        .mdef(MemoryStream, getPos)
        .mdef(MemoryStream, getSize)
        .mdef(MemoryStream, flush)
        .mdef(MemoryStream, canRead)
        .mdef(MemoryStream, canWrite)
        .def("__repr__", &MemoryStream::toString);
}

MTS_PY_EXPORT(AnnotatedStream) {
    auto c = MTS_PY_CLASS(AnnotatedStream, Object)
        .def(py::init<ref<Stream>, bool>(), DM(AnnotatedStream, AnnotatedStream),
             py::arg("stream"), py::arg("throwOnMissing") = true)
        .mdef(AnnotatedStream, push)
        .mdef(AnnotatedStream, pop)
        .mdef(AnnotatedStream, keys)
        .mdef(AnnotatedStream, getSize)
        .mdef(AnnotatedStream, canRead)
        .mdef(AnnotatedStream, canWrite)
        .def("__repr__", &AnnotatedStream::toString);

    // get & set declarations for many types
    // TODO: read & set methods à la dict (see Properties bindings)
    methods_declarator::recurse<declare_astream_accessors>(c);
}
