#include <mitsuba/core/stream.h>
#include <mitsuba/core/annotated_stream.h>
#include <mitsuba/core/dummy_stream.h>
#include <mitsuba/core/file_stream.h>
#include <mitsuba/core/memory_stream.h>

#include <mitsuba/core/filesystem.h>
#include "python.h"

// TODO: do we really need to do that for templated functions?
#define READ_WRITE_TYPE(Type)                               \
    def("readValue", [](Stream& s, Type &value) {           \
        s.readValue(value);                                 \
    }, DM(Stream, readValue))                               \
    .def("writeValue", [](Stream& s, const Type &value) {   \
        s.writeValue(value);                                \
    }, DM(Stream, writeValue))                              \

#define ANNOATED_GET_SET_TYPE(Type)                         \
    def("get", [](AnnotatedStream& s,                       \
                  const std::string &name, Type &value) {   \
        return s.get(name, value);                          \
    }, DM(AnnotatedStream, get))                            \
    .def("set", [](AnnotatedStream& s,                      \
                   const std::string &name,                 \
                   const Type &value) {                     \
        s.set(name, value);                                 \
    }, DM(AnnotatedStream, set))                            \

MTS_PY_EXPORT(Stream) {
    MTS_PY_CLASS(Stream, Object)
        .mdef(Stream, canWrite)
        .mdef(Stream, canRead)
        // TODO: handle py <=> c++ casts explicitly?
        // TODO: readValue method should be pythonic
        .READ_WRITE_TYPE(int8_t)
        .READ_WRITE_TYPE(std::string)
        .READ_WRITE_TYPE(Float)
        // TODO: lots and lots of other types
        .def("__repr__", &Stream::toString);
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
        .def(py::init<size_t>(), DM(MemoryStream, MemoryStream))
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
    MTS_PY_CLASS(AnnotatedStream, Object)
        .def(py::init<ref<Stream>, bool>(), DM(AnnotatedStream, AnnotatedStream))
        .mdef(AnnotatedStream, push)
        .mdef(AnnotatedStream, pop)
        .mdef(AnnotatedStream, keys)
        // TODO: auto-declare the same getters & setters as in Stream
        .ANNOATED_GET_SET_TYPE(int8_t)
        .ANNOATED_GET_SET_TYPE(std::string)
        .ANNOATED_GET_SET_TYPE(Float)
        .mdef(AnnotatedStream, getSize)
        .mdef(AnnotatedStream, canRead)
        .mdef(AnnotatedStream, canWrite);
}
