#include <mitsuba/core/bitmap.h>
#include "python.h"

MTS_PY_EXPORT(Bitmap) {
    auto bitmap = MTS_PY_CLASS(Bitmap, Object)
        .def(py::init<Bitmap::EPixelFormat, Struct::EType, const Vector2s &, size_t>(),
             "pfmt"_a, "cfmt"_a, "size"_a, "channel_count"_a = 0,
             D(Bitmap, Bitmap))
        .def("__init__", [](Bitmap &bitmap, py::array obj) {
            Bitmap::EPixelFormat pFmt = Bitmap::ELuminance;
            Struct::EType cFmt = py::module::import("mitsuba.core").attr("Struct").attr("EType")(obj.dtype()).cast<Struct::EType>();
            size_t channel_count = 0;
            if (obj.ndim() != 2 && obj.ndim() != 3)
                throw py::type_error("Expected an array of size 2 or 3");
            if (obj.ndim() == 3) {
                channel_count = obj.shape()[2];
                switch (channel_count) {
                    case 1: pFmt = Bitmap::ELuminance; break;
                    case 2: pFmt = Bitmap::ELuminanceAlpha; break;
                    case 3: pFmt = Bitmap::ERGB; break;
                    case 4: pFmt = Bitmap::ERGBA; break;
                    default: pFmt = Bitmap::EMultiChannel; break;
                }
            }
            obj = py::array::ensure(obj, py::array::c_style);
            new (&bitmap) Bitmap(pFmt, cFmt, Vector2s(obj.shape()[1], obj.shape()[0]), channel_count);
            memcpy(bitmap.data(), obj.data(), bitmap.buffer_size());
        })
        .def(py::init<const Bitmap &>())
        .mdef(Bitmap, pixel_format)
        .mdef(Bitmap, component_format)
        .mdef(Bitmap, size)
        .mdef(Bitmap, width)
        .mdef(Bitmap, height)
        .mdef(Bitmap, pixel_count)
        .mdef(Bitmap, channel_count)
        .mdef(Bitmap, has_alpha)
        .mdef(Bitmap, bytes_per_pixel)
        .mdef(Bitmap, buffer_size)
        .mdef(Bitmap, gamma)
        .mdef(Bitmap, set_gamma)
        .mdef(Bitmap, clear)
        .def("struct_", &Bitmap::struct_, D(Bitmap, struct));

    py::enum_<Bitmap::EPixelFormat>(bitmap, "EPixelFormat", D(Bitmap, EPixelFormat))
        .value("ELuminance", Bitmap::ELuminance)
        .value("ELuminanceAlpha", Bitmap::ELuminanceAlpha)
        .value("ERGB", Bitmap::ERGB)
        .value("ERGBA", Bitmap::ERGBA)
        .value("EXYZ", Bitmap::EXYZ)
        .value("EXYZA", Bitmap::EXYZA)
        .value("EMultiChannel", Bitmap::EMultiChannel)
        .export_values();

    py::enum_<Bitmap::EFileFormat>(bitmap, "EFileFormat", D(Bitmap, EFileFormat))
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
