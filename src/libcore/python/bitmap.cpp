#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Bitmap) {
    auto bitmap = MTS_PY_CLASS(Bitmap, Object)
        .def(py::init<Bitmap::EPixelFormat, Struct::EType, const Vector2s &, size_t>(),
             "pixel_format"_a, "component_format"_a, "size"_a, "channel_count"_a = 0, D(Bitmap, Bitmap))

        .def(py::init([](py::array obj, py::object pixel_format_) {
            auto struct_ = py::module::import("mitsuba.core").attr("Struct");
            Struct::EType component_format = struct_.attr("EType")(obj.dtype()).cast<Struct::EType>();
            if (obj.ndim() != 2 && obj.ndim() != 3)
                throw py::type_error("Expected an array of size 2 or 3");

            Bitmap::EPixelFormat pixel_format = Bitmap::EY;
            size_t channel_count = 1;
            if (obj.ndim() == 3) {
                channel_count = obj.shape()[2];
                switch (channel_count) {
                    case 1: pixel_format = Bitmap::EY; break;
                    case 2: pixel_format = Bitmap::EYA; break;
                    case 3: pixel_format = Bitmap::ERGB; break;
                    case 4: pixel_format = Bitmap::ERGBA; break;
                    case 5: pixel_format = Bitmap::ERGBAW; break;
                    default: pixel_format = Bitmap::EMultiChannel; break;
                }
            }

            if (!pixel_format_.is_none())
                pixel_format = pixel_format_.cast<Bitmap::EPixelFormat>();

            obj = py::array::ensure(obj, py::array::c_style);
            Vector2s size(obj.shape()[1], obj.shape()[0]);
            auto bitmap = new Bitmap(pixel_format, component_format, size, channel_count);
            memcpy(bitmap->data(), obj.data(), bitmap->buffer_size());
            return bitmap;
        }), "array"_a, "pixel_format"_a = py::none(), "Initialize a Bitmap from a NumPy array")
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
        .mdef(Bitmap, srgb_gamma)
        .mdef(Bitmap, set_srgb_gamma)
        .mdef(Bitmap, clear)
        .def("metadata", py::overload_cast<>(&Bitmap::metadata), D(Bitmap, metadata),
             py::return_value_policy::reference_internal)
        .def("resample", py::overload_cast<Bitmap *, const ReconstructionFilter *,
            const std::pair<Bitmap::EBoundaryCondition, Bitmap::EBoundaryCondition> &,
            const std::pair<Float, Float> &, Bitmap *>(&Bitmap::resample, py::const_),
            "target"_a, "rfilter"_a = py::none(),
            "bc"_a = std::make_pair(Bitmap::EBoundaryCondition::EClamp,
                                    Bitmap::EBoundaryCondition::EClamp),
            "clamp"_a = std::make_pair(-math::Infinity, math::Infinity),
            "temp"_a = py::none(),
            D(Bitmap, resample)
        )
        .def("resample", py::overload_cast<const Vector2s &, const ReconstructionFilter *,
            const std::pair<Bitmap::EBoundaryCondition, Bitmap::EBoundaryCondition> &,
            const std::pair<Float, Float> &>(&Bitmap::resample, py::const_),
            "res"_a, "rfilter"_a = py::none(),
            "bc"_a = std::make_pair(Bitmap::EBoundaryCondition::EClamp,
                                    Bitmap::EBoundaryCondition::EClamp),
            "clamp"_a = std::make_pair(-math::Infinity, math::Infinity),
            D(Bitmap, resample, 2)
        )
        .def("convert", py::overload_cast<Bitmap::EPixelFormat, Struct::EType, bool>(
             &Bitmap::convert, py::const_), D(Bitmap, convert),
             "pixel_format"_a, "component_format"_a, "srgb_gamma"_a)
        .def("convert", py::overload_cast<Bitmap *>(&Bitmap::convert, py::const_),
             D(Bitmap, convert, 2), "target"_a)
        .def("accumulate", py::overload_cast<const Bitmap *, Point2i,
                                             Point2i, Vector2i>(
                &Bitmap::accumulate), D(Bitmap, accumulate),
             "bitmap"_a, "source_offset"_a, "target_offset"_a, "size"_a)
        .def("accumulate", py::overload_cast<const Bitmap *, const Point2i &>(
                &Bitmap::accumulate), D(Bitmap, accumulate, 2),
             "bitmap"_a, "target_offset"_a)
        .def("accumulate", py::overload_cast<const Bitmap *>(
                &Bitmap::accumulate), D(Bitmap, accumulate, 3),
             "bitmap"_a)
        .mdef(Bitmap, vflip)
        .def("struct_", &Bitmap::struct_, D(Bitmap, struct))
        .def(py::self == py::self)
        .def(py::self != py::self);

    py::enum_<Bitmap::EPixelFormat>(bitmap, "EPixelFormat", D(Bitmap, EPixelFormat))
        .value("EY", Bitmap::EY, D(Bitmap, EPixelFormat, EY))
        .value("EYA", Bitmap::EYA, D(Bitmap, EPixelFormat, EYA))
        .value("ERGB", Bitmap::ERGB, D(Bitmap, EPixelFormat, ERGB))
        .value("ERGBA", Bitmap::ERGBA, D(Bitmap, EPixelFormat, ERGBA))
        .value("ERGBAW", Bitmap::ERGBAW, D(Bitmap, EPixelFormat, ERGBAW))
        .value("EXYZ", Bitmap::EXYZ, D(Bitmap, EPixelFormat, EXYZ))
        .value("EXYZA", Bitmap::EXYZA, D(Bitmap, EPixelFormat, EXYZA))
        .value("EXYZAW", Bitmap::EXYZAW, D(Bitmap, EPixelFormat, EXYZAW))
        .value("EMultiChannel", Bitmap::EMultiChannel, D(Bitmap, EPixelFormat, EMultiChannel))
        .export_values();

    py::enum_<Bitmap::EFileFormat>(bitmap, "EFileFormat", D(Bitmap, EFileFormat))
        .value("EPNG", Bitmap::EPNG, D(Bitmap, EFileFormat, EPNG))
        .value("EOpenEXR", Bitmap::EOpenEXR, D(Bitmap, EFileFormat, EOpenEXR))
        .value("ERGBE", Bitmap::ERGBE, D(Bitmap, EFileFormat, ERGBE))
        .value("EPFM", Bitmap::EPFM, D(Bitmap, EFileFormat, EPFM))
        .value("EPPM", Bitmap::EPPM, D(Bitmap, EFileFormat, EPPM))
        .value("EJPEG", Bitmap::EJPEG, D(Bitmap, EFileFormat, EJPEG))
        .value("ETGA", Bitmap::ETGA, D(Bitmap, EFileFormat, ETGA))
        .value("EBMP", Bitmap::EBMP, D(Bitmap, EFileFormat, EBMP))
        .value("EAuto", Bitmap::EAuto, D(Bitmap, EFileFormat, EAuto))
        .export_values();


    auto struct_ = py::module::import("mitsuba.core").attr("Struct");
    bitmap.attr("EUInt8") = struct_.attr("EUInt8");
    bitmap.attr("EInt8") = struct_.attr("EInt8");
    bitmap.attr("EUInt16") = struct_.attr("EUInt16");
    bitmap.attr("EInt16") = struct_.attr("EInt16");
    bitmap.attr("EUInt32") = struct_.attr("EUInt32");
    bitmap.attr("EInt32") = struct_.attr("EInt32");
    bitmap.attr("EUInt64") = struct_.attr("EUInt64");
    bitmap.attr("EInt64") = struct_.attr("EInt64");
    bitmap.attr("EFloat16") = struct_.attr("EFloat16");
    bitmap.attr("EFloat32") = struct_.attr("EFloat32");
    bitmap.attr("EFloat64") = struct_.attr("EFloat64");
    bitmap.attr("EInvalid") = struct_.attr("EInvalid");

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

            code[2] = (char) ('0' + field.size);
            #if PY_MAJOR_VERSION > 3
                result["typestr"] = code;
            #else
                result["typestr"] = py::bytes(code);
            #endif
            result["data"] = py::make_tuple(size_t(bitmap.uint8_data()), false);
            result["version"] = 3;
            return result;
        });
}
