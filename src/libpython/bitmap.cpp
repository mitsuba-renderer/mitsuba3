#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/stream.h>
#include "python.h"

MTS_PY_EXPORT(Bitmap) {
    auto bitmap = MTS_PY_CLASS(Bitmap, Object)
        .def(py::init<Bitmap::EPixelFormat, Struct::EType, const Vector2s &, size_t>(),
             "pixel_format"_a, "component_format"_a, "size"_a, "channel_count"_a = 0, D(Bitmap, Bitmap))

        .def("__init__", [](Bitmap &bitmap, py::array obj) {
            auto struct_ = py::module::import("mitsuba.core").attr("Struct");
            Struct::EType component_format = struct_.attr("EType")(obj.dtype()).cast<Struct::EType>();
            size_t channel_count = 0;
            if (obj.ndim() != 2 && obj.ndim() != 3)
                throw py::type_error("Expected an array of size 2 or 3");

            Bitmap::EPixelFormat pixel_format = Bitmap::ELuminance;
            if (obj.ndim() == 3) {
                channel_count = obj.shape()[2];
                switch (channel_count) {
                    case 1: pixel_format = Bitmap::ELuminance; break;
                    case 2: pixel_format = Bitmap::ELuminanceAlpha; break;
                    case 3: pixel_format = Bitmap::ERGB; break;
                    case 4: pixel_format = Bitmap::ERGBA; break;
                    default: pixel_format = Bitmap::EMultiChannel; break;
                }
            }

            obj = py::array::ensure(obj, py::array::c_style);
            Vector2s size(obj.shape()[1], obj.shape()[0]);
            new (&bitmap) Bitmap(pixel_format, component_format, size, channel_count);
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
        .def("resample", py::overload_cast<Bitmap *, const ReconstructionFilter *,
            const std::pair<Bitmap::EBoundaryCondition, Bitmap::EBoundaryCondition> &,
            const std::pair<Float, Float> &, Bitmap *>(&Bitmap::resample, py::const_),
            "target"_a, "rfilter"_a = py::none(),
            "bc"_a = std::make_pair(Bitmap::EBoundaryCondition::EClamp,
                                    Bitmap::EBoundaryCondition::EClamp),
            "clamp"_a = std::make_pair(-std::numeric_limits<Float>::infinity(),
                                        std::numeric_limits<Float>::infinity()),
            "temp"_a = py::none(),
            D(Bitmap, resample)
        )
        .def("resample", py::overload_cast<const Vector2s &, const ReconstructionFilter *,
            const std::pair<Bitmap::EBoundaryCondition, Bitmap::EBoundaryCondition> &,
            const std::pair<Float, Float> &>(&Bitmap::resample, py::const_),
            "res"_a, "rfilter"_a = py::none(),
            "bc"_a = std::make_pair(Bitmap::EBoundaryCondition::EClamp,
                                    Bitmap::EBoundaryCondition::EClamp),
            "clamp"_a = std::make_pair(-std::numeric_limits<Float>::infinity(),
                                        std::numeric_limits<Float>::infinity()),
            D(Bitmap, resample, 2)
        )
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

    bitmap
        .def(py::init<const fs::path &, Bitmap::EFileFormat>(), "path"_a,
             "format"_a = Bitmap::EAuto)
        .def(py::init<Stream *, Bitmap::EFileFormat>(), "stream"_a,
             "format"_a = Bitmap::EAuto)
        .def("write",
             py::overload_cast<Stream *, Bitmap::EFileFormat, int>(
                 &Bitmap::write, py::const_),
             "stream"_a, "format"_a = Bitmap::EAuto, "quality"_a = -1,
             D(Bitmap, write))
        .def("write",
             py::overload_cast<const fs::path &, Bitmap::EFileFormat, int>(
                 &Bitmap::write, py::const_),
             "path"_a, "format"_a = Bitmap::EAuto, "quality"_a = -1,
             D(Bitmap, write, 2))
        .def_property_readonly("__array_interface__", [](Bitmap &bitmap) -> py::object {
            if (bitmap.struct_()->size() == 0)
                return py::none();
            auto field = bitmap.struct_()->operator[](0);
            py::dict result;
            result["shape"] = py::make_tuple(bitmap.height(), bitmap.width(),
                                             bitmap.channel_count());

            std::string code(3, '\0');
            #if defined(LITTLE_ENDIAN)
                code[0] = '<';
            #else
                code[0] = '>';
            #endif

            if (field.is_integer()) {
                if (field.is_signed())
                    code[1] = 'i';
                else
                    code[1] = 'u';
            } else if (field.is_float()) {
                code[1] = 'f';
            } else {
                Throw("Internal error: unknown component type!");
            }

            code[2] = '0' + field.size;
            result["typestr"] = code;
            result["data"] = py::make_tuple(size_t(bitmap.uint8_data()), false);
            result["version"] = 3;
            return result;
        });
}
