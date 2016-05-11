#include <mitsuba/core/stream.h>
#include <mitsuba/core/dstream.h>
#include "python.h"

// TODO: export concrete Stream implementations
MTS_PY_EXPORT(Stream) {
    MTS_PY_CLASS(Stream, Object);
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
