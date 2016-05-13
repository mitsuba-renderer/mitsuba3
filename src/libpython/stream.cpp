#include <mitsuba/core/stream.h>
#include <mitsuba/core/astream.h>
#include <mitsuba/core/dstream.h>
#include "python.h"

MTS_PY_EXPORT(Stream) {
    MTS_PY_CLASS(Stream, Object);
    // TODO
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
        .mdef(DummyStream, canRead);
}

MTS_PY_EXPORT(AnnotatedStream) {
    MTS_PY_CLASS(AnnotatedStream, Object);
    // TODO
}
