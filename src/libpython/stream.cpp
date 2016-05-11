#include <mitsuba/core/stream.h>
#include "python.h"

// TODO: export concrete Stream implementations
MTS_PY_EXPORT(Stream) {
    py::class_<Stream>(m, "Stream", DM(Stream));
}
