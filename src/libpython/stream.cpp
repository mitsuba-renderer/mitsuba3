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
 * and \AnnotatedStream.
 *
 * TODO: yes, it is way overkill and overcomplicated, this was mostly written
 *       as an exercise to understand template metaprogramming better.
 */
template<typename... Args> struct template_methods_helper;

template <typename T, typename... Args>
struct template_methods_helper<T, Args...> {
    template <typename PyClass>
    static void declareGettersAndSetters(PyClass &c) {
        c.def("get", [](AnnotatedStream& s,
                        const std::string &name, T &value) {
            return s.get(name, value);
        }, DM(AnnotatedStream, get));
        c.def("set", [](AnnotatedStream& s,
                        const std::string &name, const T &value) {
            s.set(name, value);
        }, DM(AnnotatedStream, set));

        template_methods_helper<Args...>::declareGettersAndSetters(c);
    }

    template <typename PyClass>
    static void declareReadAndWriteMethods(PyClass &c) {
        c.def("readValue", [](Stream& s, T &value) {
            s.readValue(value);
        }, DM(Stream, readValue));
        c.def("writeValue", [](Stream& s, const T &value) {
            s.writeValue(value);
        }, DM(Stream, writeValue));

        template_methods_helper<Args...>::declareReadAndWriteMethods(c);
    }
};

template<>
struct template_methods_helper<> {
    template <typename PyClass>
    static void declareGettersAndSetters(PyClass &) { /* End of recursion*/ }

    template <typename PyClass>
    static void declareReadAndWriteMethods(PyClass &) { /* End of recursion*/ }
};

/// Use this type alias to list the supported types.
// TODO: support all types that can occur in Python
using methods_helper = template_methods_helper<int32_t, int64_t, Float,
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

    // TODO: handle py <=> c++ casts explicitly?
    // TODO: readValue method should be pythonic
    methods_helper::declareReadAndWriteMethods(c);

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
//        .def("canWrite", &FileStream::canWrite, DM(Stream, canWrite))
//        .def("canRead", &FileStream::canRead, DM(Stream, canRead));
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
        .mdef(AnnotatedStream, canWrite);
    // get & set declarations for many types
    methods_helper::declareGettersAndSetters(c);
}
