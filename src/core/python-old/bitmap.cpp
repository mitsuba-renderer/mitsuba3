// #include <ostream>

#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(Bitmap) {
    using Float = typename Bitmap::Float;
    MI_IMPORT_CORE_TYPES()
    using ReconstructionFilter = typename Bitmap::ReconstructionFilter;

    auto bitmap = MI_PY_CLASS(Bitmap, Object);

    py::enum_<Bitmap::PixelFormat>(bitmap, "PixelFormat", D(Bitmap, PixelFormat))
        .value("Y",     Bitmap::PixelFormat::Y,     D(Bitmap, PixelFormat, Y))
        .value("YA",    Bitmap::PixelFormat::YA,    D(Bitmap, PixelFormat, YA))
        .value("RGB",   Bitmap::PixelFormat::RGB,   D(Bitmap, PixelFormat, RGB))
        .value("RGBA",  Bitmap::PixelFormat::RGBA,  D(Bitmap, PixelFormat, RGBA))
        .value("RGBW",  Bitmap::PixelFormat::RGBA,  D(Bitmap, PixelFormat, RGBW))
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

    py::enum_<Bitmap::AlphaTransform>(bitmap, "AlphaTransform", D(Bitmap, AlphaTransform))
        .value("Empty",         Bitmap::AlphaTransform::Empty,
                D(Bitmap, AlphaTransform, Empty))
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
            "clamp"_a = std::make_pair(-dr::Infinity<ScalarFloat>,
                                       dr::Infinity<ScalarFloat>),
            "temp"_a = py::none(),
            D(Bitmap, resample)
        )
        .def("resample", py::overload_cast<const ScalarVector2u &, const ReconstructionFilter *,
            const std::pair<FilterBoundaryCondition, FilterBoundaryCondition> &,
            const std::pair<ScalarFloat, ScalarFloat> &>(&Bitmap::resample, py::const_),
            "res"_a, "rfilter"_a = nullptr,
            "bc"_a = std::make_pair(FilterBoundaryCondition::Clamp,
                                    FilterBoundaryCondition::Clamp),
            "clamp"_a = std::make_pair(-dr::Infinity<ScalarFloat>, dr::Infinity<ScalarFloat>),
            D(Bitmap, resample, 2)
        )
        .def("convert",
            [](const Bitmap& b,
               py::object pf, py::object cf, py::object srgb,
               Bitmap::AlphaTransform alpha_transform) {
                   Bitmap::PixelFormat pixel_format = b.pixel_format();
                   if (!pf.is(py::none()))
                       pixel_format = pf.cast<Bitmap::PixelFormat>();
                   Struct::Type component_format = b.component_format();
                   if (!cf.is(py::none()))
                       component_format = cf.cast<Struct::Type>();
                   bool srgb_gamma = b.srgb_gamma();
                   if (!srgb.is(py::none()))
                       srgb_gamma = srgb.cast<bool>();

                   py::gil_scoped_release release;
                   return b.convert(pixel_format, component_format, srgb_gamma,
                                    alpha_transform);
            },
             D(Bitmap, convert),
             "pixel_format"_a = py::none(), "component_format"_a = py::none(),
             "srgb_gamma"_a = py::none(), "alpha_transform"_a = Bitmap::AlphaTransform::Empty)
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

            if (bitmap.channel_count() == 1)
                result["shape"] = py::make_tuple(bitmap.height(), bitmap.width());
            else
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
            else if (typestr.length() == 3 && typestr.substr(1) == "i1")
                component_format = Struct::Type::Int8;
            else if (typestr.length() == 3 && typestr.substr(1) == "u1")
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

            Vector2u size(shape[1].cast<size_t>(), shape[0].cast<size_t>());
            Bitmap* bitmap = new Bitmap(pixel_format, component_format, size,
                                     channel_count, channel_names);

            bool is_contiguous = true;
            if (interface.contains("strides") && !interface["strides"].is_none()) {
                auto strides = interface["strides"].template cast<py::tuple>();

                // Stride information might be given although it's contiguous
                size_t bytes_per_value = bitmap->bytes_per_pixel() / bitmap->channel_count();
                int64_t current_stride = bytes_per_value;
                for (size_t i = ndim; i > 0; --i) {
                    if (current_stride != strides[i - 1].cast<int64_t>()) {
                        is_contiguous = false;
                        break;
                    }
                    current_stride *= shape[i - 1].cast<size_t>();
                }

                // Need to shuffle memory
                if (!is_contiguous) {
                    size_t num_values = size.x() * size.y();
                    if (ndim == 3)
                        num_values *= shape[2].cast<size_t>();

                    std::unique_ptr<size_t[]> shape_cum = std::make_unique<size_t[]>(ndim);
                    shape_cum[ndim - 1] = 1;
                    for (size_t j = ndim - 1; j > 0; --j) {
                        size_t shape_j = shape[j].cast<size_t>();
                        shape_cum[j - 1] = shape_j * shape_cum[j];
                    }

                    for (size_t i = 0; i < num_values; ++i) {
                        unsigned char* src = (unsigned char*) ptr;
                        for (size_t j = ndim; j > 0; --j) {
                            int64_t stride_j = strides[j - 1].cast<int64_t>();
                            size_t shape_j = shape[j - 1].cast<size_t>();
                            int64_t offset_j = ((i / shape_cum[j - 1]) % shape_j) * stride_j;
                            src += offset_j;
                        }
                        unsigned char *dest = (unsigned char *) bitmap->data() + i * bytes_per_value;
                        memcpy(dest, src, bytes_per_value);
                    }
                }
            }

            if (is_contiguous)
                memcpy(bitmap->data(), ptr, bitmap->buffer_size());

            return bitmap;
        }),
        "array"_a,
        "pixel_format"_a = py::none(),
        "channel_names"_a = std::vector<std::string>(),
        "Initialize a Bitmap from any array that implements ``__array_interface__``");

    bitmap.def("_repr_html_", [](const Bitmap &_bitmap) -> py::object {
        if (_bitmap.pixel_format() == Bitmap::PixelFormat::MultiChannel)
            return py::none();

        ref<Bitmap> bitmap = _bitmap.convert(Bitmap::PixelFormat::RGB, Struct::Type::UInt16, true);
        ref<MemoryStream> s = new MemoryStream(bitmap->buffer_size());
        bitmap->write(s, Bitmap::FileFormat::PNG);
        s->seek(0);
        std::unique_ptr<char> tmp(new char[s->size()]);
        s->read((void *) tmp.get(), s->size());

        auto base64 = py::module::import("base64");
        auto bytes = base64.attr("b64encode")(py::bytes(tmp.get(), s->size()));
        std::stringstream s_bytes;
        s_bytes << bytes;

        std::stringstream out;
        out << "<img src=\"data:image/png;base64, ";
        out << s_bytes.str().substr(2, s_bytes.str().size()-3) << "\"";
        out << "width=\"250vm\"";
        out << " />";

        return py::str(out.str());
    });
}
