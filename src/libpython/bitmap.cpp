#include <mitsuba/core/bitmap.h>
#include "python.h"

MTS_PY_EXPORT(Bitmap) {
    auto bitmap = MTS_PY_CLASS(Bitmap, Object)
        .def(py::init<Bitmap::EPixelFormat, Struct::EType, const Vector2s &, size_t>(),
             py::arg("pFmt"), py::arg("cFmt"), py::arg("size"), py::arg("channelCount") = 0,
             DM(Bitmap, Bitmap))
        .def(py::init<const Bitmap &>())
        .mdef(Bitmap, pixelFormat)
        .mdef(Bitmap, componentFormat)
        .mdef(Bitmap, size)
        .mdef(Bitmap, width)
        .mdef(Bitmap, height)
        .mdef(Bitmap, pixelCount)
        .mdef(Bitmap, channelCount)
        .mdef(Bitmap, hasAlpha)
        .mdef(Bitmap, bytesPerPixel)
        .mdef(Bitmap, bufferSize)
        .mdef(Bitmap, gamma)
        .mdef(Bitmap, setGamma)
        .mdef(Bitmap, clear);

    py::enum_<Bitmap::EPixelFormat>(bitmap, "EPixelFormat", DM(Bitmap, EPixelFormat))
        .value("ELuminance", Bitmap::ELuminance)
        .value("ELuminanceAlpha", Bitmap::ELuminanceAlpha)
        .value("ERGB", Bitmap::ERGB)
        .value("ERGBA", Bitmap::ERGBA)
        .value("EXYZ", Bitmap::EXYZ)
        .value("EXYZA", Bitmap::EXYZA)
        .value("EMultiChannel", Bitmap::EMultiChannel)
        .export_values();

    py::enum_<Bitmap::EFileFormat>(bitmap, "EFileFormat", DM(Bitmap, EFileFormat))
        .value("EPNG", Bitmap::EPNG)
        .value("EOpenEXR", Bitmap::EOpenEXR)
        .value("ERGBE", Bitmap::ERGBE)
        .value("EPFM", Bitmap::EPFM)
        .value("EPPM", Bitmap::EPPM)
        .value("EJPEG", Bitmap::EJPEG)
        .value("ETGA", Bitmap::ETGA)
        .value("EBMP", Bitmap::EBMP)
        .value("EAuto", Bitmap::EAuto);
}
