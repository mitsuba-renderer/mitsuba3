#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Bitmap) {
    using ScalarFloat = typename Bitmap::Float;
    using Vector2s    = typename Bitmap::Vector2s;
    using ReconstructionFilter = typename Bitmap::ReconstructionFilter;
    MTS_IMPORT_CORE_TYPES()

    MTS_PY_CHECK_ALIAS(Bitmap, m) {
        auto bitmap = MTS_PY_CLASS(Bitmap, Object)
            .def(py::init<PixelFormat, FieldType, const Vector2s &, size_t>(),
                "pixel_format"_a, "component_format"_a, "size"_a, "channel_count"_a = 0,
                D(Bitmap, Bitmap))

            .def(py::init([](py::array obj, py::object pixel_format_) {
                FieldType component_format = obj.dtype().cast<FieldType>();
                if (obj.ndim() != 2 && obj.ndim() != 3)
                    throw py::type_error("Expected an array of size 2 or 3");

                PixelFormat pixel_format = PixelFormat::Y;
                size_t channel_count = 1;
                if (obj.ndim() == 3) {
                    channel_count = obj.shape()[2];
                    switch (channel_count) {
                        case 1: pixel_format = PixelFormat::Y; break;
                        case 2: pixel_format = PixelFormat::YA; break;
                        case 3: pixel_format = PixelFormat::RGB; break;
                        case 4: pixel_format = PixelFormat::RGBA; break;
                        case 5: pixel_format = PixelFormat::RGBAW; break;
                        default: pixel_format = PixelFormat::MultiChannel; break;
                    }
                }

                if (!pixel_format_.is_none())
                    pixel_format = pixel_format_.cast<PixelFormat>();

                obj = py::array::ensure(obj, py::array::c_style);
                Vector2s size(obj.shape()[1], obj.shape()[0]);
                auto bitmap = new Bitmap(pixel_format, component_format, size, channel_count);
                memcpy(bitmap->data(), obj.data(), bitmap->buffer_size());
                return bitmap;
            }), "array"_a, "pixel_format"_a = py::none(), "Initialize a Bitmap from a NumPy array")
            .def(py::init<const Bitmap &>())
            .def_method(Bitmap, pixel_format)
            .def_method(Bitmap, component_format)
            .def_method(Bitmap, size)
            .def_method(Bitmap, width)
            .def_method(Bitmap, height)
            .def_method(Bitmap, pixel_count)
            .def_method(Bitmap, channel_count)
            .def_method(Bitmap, has_alpha)
            .def_method(Bitmap, bytes_per_pixel)
            .def_method(Bitmap, buffer_size)
            .def_method(Bitmap, srgb_gamma)
            .def_method(Bitmap, set_srgb_gamma)
            .def_method(Bitmap, clear)
            .def("metadata", py::overload_cast<>(&Bitmap::metadata), D(Bitmap, metadata),
                py::return_value_policy::reference_internal)
            .def("resample", py::overload_cast<Bitmap *, const ReconstructionFilter *,
                const std::pair<FilterBoundaryCondition, FilterBoundaryCondition> &,
                const std::pair<ScalarFloat, ScalarFloat> &, Bitmap *>(&Bitmap::resample, py::const_),
                "target"_a, "rfilter"_a = py::none(),
                "bc"_a = std::make_pair(FilterBoundaryCondition::Clamp,
                                        FilterBoundaryCondition::Clamp),
                "clamp"_a = std::make_pair(-math::Infinity<ScalarFloat>, math::Infinity<ScalarFloat>),
                "temp"_a = py::none(),
                D(Bitmap, resample)
            )
            .def("resample", py::overload_cast<const Vector2s &, const ReconstructionFilter *,
                const std::pair<FilterBoundaryCondition, FilterBoundaryCondition> &,
                const std::pair<ScalarFloat, ScalarFloat> &>(&Bitmap::resample, py::const_),
                "res"_a, "rfilter"_a = py::none(),
                "bc"_a = std::make_pair(FilterBoundaryCondition::Clamp,
                                        FilterBoundaryCondition::Clamp),
                "clamp"_a = std::make_pair(-math::Infinity<ScalarFloat>, math::Infinity<ScalarFloat>),
                D(Bitmap, resample, 2)
            )
            .def("convert", py::overload_cast<PixelFormat, FieldType, bool>(
                &Bitmap::convert, py::const_), D(Bitmap, convert),
                "pixel_format"_a, "component_format"_a, "srgb_gamma"_a,
                py::call_guard<py::gil_scoped_release>())
            .def("convert", py::overload_cast<Bitmap *>(&Bitmap::convert, py::const_),
                D(Bitmap, convert, 2), "target"_a,
                py::call_guard<py::gil_scoped_release>())
            .def("accumulate", py::overload_cast<const Bitmap *, ScalarPoint2i,
                                                 ScalarPoint2i, ScalarVector2i>(
                    &Bitmap::accumulate), D(Bitmap, accumulate),
                "bitmap"_a, "source_offset"_a, "target_offset"_a, "size"_a)
            .def("accumulate", py::overload_cast<const Bitmap *, const ScalarPoint2i &>(
                    &Bitmap::accumulate), D(Bitmap, accumulate, 2),
                "bitmap"_a, "target_offset"_a)
            .def("accumulate", py::overload_cast<const Bitmap *>(
                    &Bitmap::accumulate), D(Bitmap, accumulate, 3),
                "bitmap"_a)
            .def_method(Bitmap, vflip)
            .def("struct_", &Bitmap::struct_, D(Bitmap, struct))
            .def(py::self == py::self)
            .def(py::self != py::self);

        py::enum_<PixelFormat>(bitmap, "PixelFormat", D(Bitmap, PixelFormat))
            .value("Y", PixelFormat::Y, D(Bitmap, PixelFormat, Y))
            .value("YA", PixelFormat::YA, D(Bitmap, PixelFormat, YA))
            .value("RGB", PixelFormat::RGB, D(Bitmap, PixelFormat, RGB))
            .value("RGBA", PixelFormat::RGBA, D(Bitmap, PixelFormat, RGBA))
            .value("RGBAW", PixelFormat::RGBAW, D(Bitmap, PixelFormat, RGBAW))
            .value("XYZ", PixelFormat::XYZ, D(Bitmap, PixelFormat, XYZ))
            .value("XYZA", PixelFormat::XYZA, D(Bitmap, PixelFormat, XYZA))
            .value("XYZAW", PixelFormat::XYZAW, D(Bitmap, PixelFormat, XYZAW))
            .value("MultiChannel", PixelFormat::MultiChannel, D(Bitmap, PixelFormat, MultiChannel))
            .export_values();

        py::enum_<ImageFileFormat>(bitmap, "ImageFileFormat", D(Bitmap, ImageFileFormat))
            .value("PNG", ImageFileFormat::PNG, D(Bitmap, ImageFileFormat, PNG))
            .value("OpenEXR", ImageFileFormat::OpenEXR, D(Bitmap, ImageFileFormat, OpenEXR))
            .value("RGBE", ImageFileFormat::RGBE, D(Bitmap, ImageFileFormat, RGBE))
            .value("PFM", ImageFileFormat::PFM, D(Bitmap, ImageFileFormat, PFM))
            .value("PPM", ImageFileFormat::PPM, D(Bitmap, ImageFileFormat, PPM))
            .value("JPEG", ImageFileFormat::JPEG, D(Bitmap, ImageFileFormat, JPEG))
            .value("TGA", ImageFileFormat::TGA, D(Bitmap, ImageFileFormat, TGA))
            .value("BMP", ImageFileFormat::BMP, D(Bitmap, ImageFileFormat, BMP))
            .value("Unknown", ImageFileFormat::Unknown, D(Bitmap, ImageFileFormat, Unknown))
            .value("Auto", ImageFileFormat::Auto, D(Bitmap, ImageFileFormat, Auto))
            .export_values();

        auto fieldtype_ = m.attr("FieldType");
        bitmap.attr("UInt8")   = fieldtype_.attr("UInt8");
        bitmap.attr("Int8")    = fieldtype_.attr("Int8");
        bitmap.attr("UInt16")  = fieldtype_.attr("UInt16");
        bitmap.attr("Int16")   = fieldtype_.attr("Int16");
        bitmap.attr("UInt32")  = fieldtype_.attr("UInt32");
        bitmap.attr("Int32")   = fieldtype_.attr("Int32");
        bitmap.attr("UInt64")  = fieldtype_.attr("UInt64");
        bitmap.attr("Int64")   = fieldtype_.attr("Int64");
        bitmap.attr("Float16") = fieldtype_.attr("Float16");
        bitmap.attr("Float32") = fieldtype_.attr("Float32");
        bitmap.attr("Float64") = fieldtype_.attr("Float64");
        bitmap.attr("Invalid") = fieldtype_.attr("Invalid");

        bitmap.def(py::init<const fs::path &, ImageFileFormat>(), "path"_a,
                "format"_a = ImageFileFormat::Auto,
                py::call_guard<py::gil_scoped_release>())
            .def(py::init<Stream *, ImageFileFormat>(), "stream"_a,
                "format"_a = ImageFileFormat::Auto,
                py::call_guard<py::gil_scoped_release>())
            .def("write",
                py::overload_cast<Stream *, ImageFileFormat, int>(
                    &Bitmap::write, py::const_),
                "stream"_a, "format"_a = ImageFileFormat::Auto, "quality"_a = -1,
                D(Bitmap, write), py::call_guard<py::gil_scoped_release>())
            .def("write",
                py::overload_cast<const fs::path &, ImageFileFormat, int>(
                    &Bitmap::write, py::const_),
                "path"_a, "format"_a = ImageFileFormat::Auto, "quality"_a = -1,
                D(Bitmap, write, 2), py::call_guard<py::gil_scoped_release>())
            .def("split", &Bitmap::split, D(Bitmap, split))
            .def_static("detect_file_format", &Bitmap::detect_file_format, D(Bitmap, detect_file_format))
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
                return py::object(result);
            });
    }
}
