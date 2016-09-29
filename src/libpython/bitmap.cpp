#include <mitsuba/core/bitmap.h>
#include "python.h"

MTS_PY_EXPORT(Bitmap) {
    MTS_PY_CLASS(Bitmap, Object)
        .def(py::init<Bitmap::EPixelFormat, Struct::EType, const Vector2s &, uint32_t>(),
             py::arg("pFmt"), py::arg("cFmt"), py::arg("size"), py::arg("channelCount") = 0,
             DM(Bitmap, Bitmap))
        .mdef(Bitmap, size)
        .mdef(Bitmap, channelCount)
        .mdef(Bitmap, bytesPerPixel)
        .mdef(Bitmap, bufferSize);
}

