#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/stream.h>
#include <pybind11/numpy.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Bitmap) {
    using Float = typename Bitmap::Float;
    MTS_IMPORT_CORE_TYPES()
    using ReconstructionFilter = typename Bitmap::ReconstructionFilter;

    auto bitmap = MTS_PY_CLASS(Bitmap, Object);

    py::enum_<Bitmap::PixelFormat>(bitmap, "PixelFormat", D(Bitmap, PixelFormat))
        .value("Y",     Bitmap::PixelFormat::Y,     D(Bitmap, PixelFormat, Y))
        .value("YA",    Bitmap::PixelFormat::YA,    D(Bitmap, PixelFormat, YA))
        .value("RGB",   Bitmap::PixelFormat::RGB,   D(Bitmap, PixelFormat, RGB))
        .value("RGBA",  Bitmap::PixelFormat::RGBA,  D(Bitmap, PixelFormat, RGBA))
        .value("RGBAW", Bitmap::PixelFormat::RGBAW, D(Bitmap, PixelFormat, RGBAW))
        .value("XYZ",   Bitmap::PixelFormat::XYZ,   D(Bitmap, PixelFormat, XYZ))
        .value("XYZA",  Bitmap::PixelFormat::XYZA,  D(Bitmap, PixelFormat, XYZA))
        .value("MultiChannel", Bitmap::PixelFormat::MultiChannel, D(Bitmap, PixelFormat, MultiChannel));

    py::enum_<Bitmap::FileFormat>(bitmap, "FileFormat", D(Bitmap, FileFormat))
        .value("PNG",     Bitmap::FileFormat::PNG,     D(Bitmap, FileFormat, PNG))
        .value("OpenEXR", Bitmap::FileFormat::OpenEXR, D(Bitmap, FileFormat, OpenEXR))
        .value("RGBE",    Bitmap::FileFormat::RGBE,    D(Bitmap, FileFormat, RGBE))
        .value("PFM",     Bitmap::FileFormat::PFM,     D(Bitmap, FileFormat, PFM))
        .value("PPM",     Bitmap::FileFormat::PPM,     D(Bitmap, FileFormat, PPM))
        .value("JPEG",    Bitmap::FileFormat::JPEG,    D(Bitmap, FileFormat, JPEG))
        .value("TGA",     Bitmap::FileFormat::TGA,     D(Bitmap, FileFormat, TGA))
        .value("BMP",     Bitmap::FileFormat::BMP,     D(Bitmap, FileFormat, BMP))
        .value("Unknown", Bitmap::FileFormat::Unknown, D(Bitmap, FileFormat, Unknown))
        .value("Auto",    Bitmap::FileFormat::Auto,    D(Bitmap, FileFormat, Auto));

    py::enum_<Bitmap::AlphaTransform>(bitmap, "AlphaTransform")
        .value("None",          Bitmap::AlphaTransform::None,
                D(Bitmap, AlphaTransform, None))
        .value("Premultiply",   Bitmap::AlphaTransform::Premultiply,
                D(Bitmap, AlphaTransform, Premultiply))
        .value("Unpremultiply", Bitmap::AlphaTransform::Unpremultiply,
                D(Bitmap, AlphaTransform, Unpremultiply));

    bitmap
        .def(py::init<Bitmap::PixelFormat, Struct::Type, const Vector2u &, size_t, std::vector<std::string>>(),
             "pixel_format"_a, "component_format"_a, "size"_a, "channel_count"_a = 0, "channel_names"_a = std::vector<std::string>(),
             D(Bitmap, Bitmap))
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
        .def_method(Bitmap, premultiplied_alpha)
        .def_method(Bitmap, set_premultiplied_alpha)
        .def_method(Bitmap, clear)
        .def("metadata", py::overload_cast<>(&Bitmap::metadata), D(Bitmap, metadata),
            py::return_value_policy::reference_internal)
        .def("resample", py::overload_cast<Bitmap *, const ReconstructionFilter *,
            const std::pair<FilterBoundaryCondition, FilterBoundaryCondition> &,
            const std::pair<ScalarFloat, ScalarFloat> &, Bitmap *>(&Bitmap::resample, py::const_),
            "target"_a, "rfilter"_a = py::none(),
            "bc"_a = std::make_pair(FilterBoundaryCondition::Clamp,
                                    FilterBoundaryCondition::Clamp),
            "clamp"_a = std::make_pair(-ek::Infinity<ScalarFloat>,
                                       ek::Infinity<ScalarFloat>),
            "temp"_a = py::none(),
            D(Bitmap, resample)
        )
        .def("resample", py::overload_cast<const ScalarVector2u &, const ReconstructionFilter *,
            const std::pair<FilterBoundaryCondition, FilterBoundaryCondition> &,
            const std::pair<ScalarFloat, ScalarFloat> &>(&Bitmap::resample, py::const_),
            "res"_a, "rfilter"_a = nullptr,
            "bc"_a = std::make_pair(FilterBoundaryCondition::Clamp,
                                    FilterBoundaryCondition::Clamp),
            "clamp"_a = std::make_pair(-ek::Infinity<ScalarFloat>, ek::Infinity<ScalarFloat>),
            D(Bitmap, resample, 2)
        )
        .def("convert",
             py::overload_cast<Bitmap::PixelFormat, Struct::Type, bool,
                               Bitmap::AlphaTransform>(&Bitmap::convert, py::const_),
             D(Bitmap, convert),
             "pixel_format"_a, "component_format"_a, "srgb_gamma"_a,
             "alpha_transform"_a = Bitmap::AlphaTransform::None,
             py::call_guard<py::gil_scoped_release>())
        .def("convert", py::overload_cast<Bitmap *>(&Bitmap::convert, py::const_),
             D(Bitmap, convert, 2), "target"_a,
             py::call_guard<py::gil_scoped_release>())
        .def("accumulate",
             py::overload_cast<const Bitmap *, ScalarPoint2i,
                               ScalarPoint2i, ScalarVector2i>(&Bitmap::accumulate),
             D(Bitmap, accumulate),
             "bitmap"_a, "source_offset"_a, "target_offset"_a, "size"_a)
        .def("accumulate",
             py::overload_cast<const Bitmap *, const ScalarPoint2i &>(&Bitmap::accumulate),
             D(Bitmap, accumulate, 2),
             "bitmap"_a, "target_offset"_a)
        .def("accumulate", py::overload_cast<const Bitmap *>(
                &Bitmap::accumulate), D(Bitmap, accumulate, 3),
             "bitmap"_a)
        .def_method(Bitmap, vflip)
        .def("struct_",
             py::overload_cast<>(&Bitmap::struct_, py::const_),
             D(Bitmap, struct))
        .def(py::self == py::self)
        .def(py::self != py::self);

    auto type_ = m.attr("Struct").attr("Type");
    bitmap.attr("UInt8")   = type_.attr("UInt8");
    bitmap.attr("Int8")    = type_.attr("Int8");
    bitmap.attr("UInt16")  = type_.attr("UInt16");
    bitmap.attr("Int16")   = type_.attr("Int16");
    bitmap.attr("UInt32")  = type_.attr("UInt32");
    bitmap.attr("Int32")   = type_.attr("Int32");
    bitmap.attr("UInt64")  = type_.attr("UInt64");
    bitmap.attr("Int64")   = type_.attr("Int64");
    bitmap.attr("Float16") = type_.attr("Float16");
    bitmap.attr("Float32") = type_.attr("Float32");
    bitmap.attr("Float64") = type_.attr("Float64");
    bitmap.attr("Invalid") = type_.attr("Invalid");

    bitmap.def(py::init<const fs::path &, Bitmap::FileFormat>(), "path"_a,
            "format"_a = Bitmap::FileFormat::Auto,
            py::call_guard<py::gil_scoped_release>())
        .def(py::init<Stream *, Bitmap::FileFormat>(), "stream"_a,
            "format"_a = Bitmap::FileFormat::Auto,
            py::call_guard<py::gil_scoped_release>())
        .def("write",
            py::overload_cast<Stream *, Bitmap::FileFormat, int>(
                &Bitmap::write, py::const_),
            "stream"_a, "format"_a = Bitmap::FileFormat::Auto, "quality"_a = -1,
            D(Bitmap, write), py::call_guard<py::gil_scoped_release>())
        .def("write",
            py::overload_cast<const fs::path &, Bitmap::FileFormat, int>(
                &Bitmap::write, py::const_),
            "path"_a, "format"_a = Bitmap::FileFormat::Auto, "quality"_a = -1,
            D(Bitmap, write, 2), py::call_guard<py::gil_scoped_release>())
        .def("write_async",
            py::overload_cast<const fs::path &, Bitmap::FileFormat, int>(
                &Bitmap::write_async, py::const_),
            "path"_a, "format"_a = Bitmap::FileFormat::Auto, "quality"_a = -1,
            D(Bitmap, write_async))
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

    /**
     * Tiny wrapper class around py::object to force the Overload Resolution
     * Order of pybind11 to pick any other Bitmap constructors if possible
     * before the one defined below.
     */
    struct PyObjectWrapper {
        PyObjectWrapper(py::object o) : obj(o) {}
        py::object obj;
    };
    py::class_<PyObjectWrapper>(m, "PyObjectWrapper").def(py::init<py::object>());
    py::implicitly_convertible<py::object, PyObjectWrapper>();

    /**
     * Python only constructor for any array type that implements the array
     * interface protocol.
     */
    bitmap.def(py::init([](PyObjectWrapper obj_wrapper, py::object pixel_format_,
                           const std::vector<std::string> &channel_names) {
            py::object obj = obj_wrapper.obj;
            if (!py::hasattr(obj, "__array_interface__"))
                throw py::type_error("Array should define __array_interface__!");

            py::dict interface = obj.attr("__array_interface__");
            auto typestr = interface["typestr"].template cast<std::string>();
            auto shape   = interface["shape"].template cast<py::tuple>();
            auto data    = interface["data"].template cast<py::tuple>();
            void* ptr    = (void*) data[0].cast<size_t>();

            Struct::Type component_format;
            if (typestr == "<f2")
                component_format = Struct::Type::Float16;
            else if (typestr == "<f4")
                component_format = Struct::Type::Float32;
            else if (typestr == "<f8")
                component_format = Struct::Type::Float64;
            else if (typestr == "<i1")
                component_format = Struct::Type::Int8;
            else if (typestr == "<u1")
                component_format = Struct::Type::UInt8;
            else if (typestr == "<i2")
                component_format = Struct::Type::Int16;
            else if (typestr == "<u2")
                component_format = Struct::Type::UInt16;
            else if (typestr == "<i4")
                component_format = Struct::Type::Int32;
            else if (typestr == "<u4")
                component_format = Struct::Type::UInt32;
            else if (typestr == "<i8")
                component_format = Struct::Type::Int64;
            else if (typestr == "<u8")
                component_format = Struct::Type::UInt64;
            else
                throw py::type_error("Invalid component format");

            size_t ndim = shape.size();
            if (ndim != 2 && ndim != 3)
                throw py::type_error("Expected an array of size 2 or 3");

            Bitmap::PixelFormat pixel_format = Bitmap::PixelFormat::Y;
            size_t channel_count = 1;
            if (ndim == 3) {
                channel_count = shape[2].cast<size_t>();
                switch (channel_count) {
                    case 1:  pixel_format = Bitmap::PixelFormat::Y; break;
                    case 2:  pixel_format = Bitmap::PixelFormat::YA; break;
                    case 3:  pixel_format = Bitmap::PixelFormat::RGB; break;
                    case 4:  pixel_format = Bitmap::PixelFormat::RGBA; break;
                    default: pixel_format = Bitmap::PixelFormat::MultiChannel; break;
                }
            }

            if (!pixel_format_.is_none())
                pixel_format = pixel_format_.cast<Bitmap::PixelFormat>();

            obj = py::array::ensure(obj, py::array::c_style);
            Vector2u size(shape[1].cast<size_t>(), shape[0].cast<size_t>());
            auto bitmap = new Bitmap(pixel_format, component_format, size,
                                     channel_count, channel_names);
            memcpy(bitmap->data(), ptr, bitmap->buffer_size());
            return bitmap;
        }),
        "array"_a,
        "pixel_format"_a = py::none(),
        "channel_names"_a = std::vector<std::string>(),
        "Initialize a Bitmap from any array that implements __array_interface__");
}
