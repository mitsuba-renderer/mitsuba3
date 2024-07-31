#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/python/python.h>
#include <string>
#include <drjit/python.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>

struct fp16 {
    unsigned char d[2];
};
static_assert(sizeof(fp16) == 2);

namespace nanobind {
    template <> struct ndarray_traits<fp16> {
        static constexpr bool is_complex = false;
        static constexpr bool is_float   = true;
        static constexpr bool is_bool    = false;
        static constexpr bool is_int     = false;
        static constexpr bool is_signed  = true;
    };
}

using ContigCpuNdArray = nb::ndarray<nb::device::cpu, nb::c_contig>;

void from_cpu_dlpack(Bitmap *b, ContigCpuNdArray data,
                     nb::object pixel_format_,
                     const std::vector<std::string> &channel_names) {
        using Float = typename Bitmap::Float;
        MI_IMPORT_CORE_TYPES()

        if (data.ndim() != 2 && data.ndim() != 3)
            throw nb::type_error("Invalid num of dimensions. Expected two or three!");

        size_t shape[3];
        for (size_t i = 0; i < 2; ++i)
            shape[i] = data.shape(i);
        shape[2] = data.ndim() == 3 ? data.shape(2) : 1;

        auto dtype = data.dtype();
        Struct::Type component_format;
        if (dtype == nb::dtype<fp16>())
            component_format = Struct::Type::Float16;
        else if (dtype == nb::dtype<float>())
            component_format = Struct::Type::Float32;
        else if (dtype == nb::dtype<double>())
            component_format = Struct::Type::Float64;
        else if (dtype == nb::dtype<int8_t>())
            component_format = Struct::Type::Int8;
        else if (dtype == nb::dtype<uint8_t>())
            component_format = Struct::Type::UInt8;
        else if (dtype == nb::dtype<int16_t>())
            component_format = Struct::Type::Int16;
        else if (dtype == nb::dtype<uint16_t>())
            component_format = Struct::Type::UInt16;
        else if (dtype == nb::dtype<int32_t>())
            component_format = Struct::Type::Int32;
        else if (dtype == nb::dtype<uint32_t>())
            component_format = Struct::Type::UInt32;
        else if (dtype == nb::dtype<int64_t>())
            component_format = Struct::Type::Int64;
        else if (dtype == nb::dtype<uint64_t>())
            component_format = Struct::Type::UInt64;
        else
            throw nb::type_error("Invalid component format");

        Bitmap::PixelFormat pixel_format = Bitmap::PixelFormat::Y;
        size_t channel_count             = 1;
        channel_count                    = shape[2];
        switch (channel_count) {
            case 1:
                pixel_format = Bitmap::PixelFormat::Y;
                break;
            case 2:
                pixel_format = Bitmap::PixelFormat::YA;
                break;
            case 3:
                pixel_format = Bitmap::PixelFormat::RGB;
                break;
            case 4:
                pixel_format = Bitmap::PixelFormat::RGBA;
                break;
            default:
                pixel_format = Bitmap::PixelFormat::MultiChannel;
                break;
        }

        if (!pixel_format_.is_none())
            pixel_format = nb::cast<Bitmap::PixelFormat>(pixel_format_);

        ScalarVector2u size(shape[1], shape[0]);
        new (b) Bitmap(pixel_format, component_format, size, channel_count,
                       channel_names);

        memcpy(b->data(), data.data(), b->buffer_size());
}

MI_PY_EXPORT(Bitmap) {
    using Float = typename Bitmap::Float;
    MI_IMPORT_CORE_TYPES()
    using ReconstructionFilter = typename Bitmap::ReconstructionFilter;

    auto bitmap = MI_PY_CLASS(Bitmap, Object);

    nb::enum_<Bitmap::PixelFormat>(bitmap, "PixelFormat", D(Bitmap, PixelFormat))
        .value("Y",     Bitmap::PixelFormat::Y,     D(Bitmap, PixelFormat, Y))
        .value("YA",    Bitmap::PixelFormat::YA,    D(Bitmap, PixelFormat, YA))
        .value("RGB",   Bitmap::PixelFormat::RGB,   D(Bitmap, PixelFormat, RGB))
        .value("RGBA",  Bitmap::PixelFormat::RGBA,  D(Bitmap, PixelFormat, RGBA))
        .value("RGBW",  Bitmap::PixelFormat::RGBA,  D(Bitmap, PixelFormat, RGBW))
        .value("RGBAW", Bitmap::PixelFormat::RGBAW, D(Bitmap, PixelFormat, RGBAW))
        .value("XYZ",   Bitmap::PixelFormat::XYZ,   D(Bitmap, PixelFormat, XYZ))
        .value("XYZA",  Bitmap::PixelFormat::XYZA,  D(Bitmap, PixelFormat, XYZA))
        .value("MultiChannel", Bitmap::PixelFormat::MultiChannel, D(Bitmap, PixelFormat, MultiChannel));

    nb::enum_<Bitmap::FileFormat>(bitmap, "FileFormat", D(Bitmap, FileFormat))
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

    nb::enum_<Bitmap::AlphaTransform>(bitmap, "AlphaTransform", D(Bitmap, AlphaTransform))
        .value("Empty",         Bitmap::AlphaTransform::Empty,
                D(Bitmap, AlphaTransform, Empty))
        .value("Premultiply",   Bitmap::AlphaTransform::Premultiply,
                D(Bitmap, AlphaTransform, Premultiply))
        .value("Unpremultiply", Bitmap::AlphaTransform::Unpremultiply,
                D(Bitmap, AlphaTransform, Unpremultiply));

    bitmap
        .def(nb::init<Bitmap::PixelFormat, Struct::Type, const Vector2u &, size_t, std::vector<std::string>>(),
             "pixel_format"_a, "component_format"_a, "size"_a, "channel_count"_a = 0, "channel_names"_a = std::vector<std::string>(),
             D(Bitmap, Bitmap))
        .def(nb::init<const Bitmap &>())
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
        .def("metadata", [](const Bitmap& b) {
                return PropertiesV<Float>(b.metadata());
            }, D(Bitmap, metadata), 
            nb::sig("def metadata(self) -> mitsuba.scalar_rgb.Properties"))
        .def("resample", nb::overload_cast<Bitmap *, const ReconstructionFilter *,
            const std::pair<FilterBoundaryCondition, FilterBoundaryCondition> &,
            const std::pair<ScalarFloat, ScalarFloat> &, Bitmap *>(&Bitmap::resample, nb::const_),
            "target"_a, "rfilter"_a = nb::none(),
            "bc"_a = std::make_pair(FilterBoundaryCondition::Clamp,
                                    FilterBoundaryCondition::Clamp),
            "clamp"_a = std::make_pair(-dr::Infinity<ScalarFloat>,
                                       dr::Infinity<ScalarFloat>),
            "temp"_a = nb::none(),
            D(Bitmap, resample)
        )
        .def("resample", nb::overload_cast<const ScalarVector2u &, const ReconstructionFilter *,
            const std::pair<FilterBoundaryCondition, FilterBoundaryCondition> &,
            const std::pair<ScalarFloat, ScalarFloat> &>(&Bitmap::resample, nb::const_),
            "res"_a, "rfilter"_a = nullptr,
            "bc"_a = std::make_pair(FilterBoundaryCondition::Clamp,
                                    FilterBoundaryCondition::Clamp),
            "clamp"_a = std::make_pair(-dr::Infinity<ScalarFloat>, dr::Infinity<ScalarFloat>),
            D(Bitmap, resample, 2)
        )
        .def("convert",
            [](const Bitmap& b,
               nb::object pf, nb::object cf, nb::object srgb,
               Bitmap::AlphaTransform alpha_transform) {
                   Bitmap::PixelFormat pixel_format = b.pixel_format();
                   if (!pf.is(nb::none()))
                       pixel_format = nb::cast<Bitmap::PixelFormat>(pf);
                   Struct::Type component_format = b.component_format();
                   if (!cf.is(nb::none()))
                       component_format = nb::cast<Struct::Type>(cf);
                   bool srgb_gamma = b.srgb_gamma();
                   if (!srgb.is(nb::none()))
                       srgb_gamma = nb::cast<bool>(srgb);

                   nb::gil_scoped_release release;
                   return b.convert(pixel_format, component_format,
                                               srgb_gamma, alpha_transform);
            },
             D(Bitmap, convert),
             "pixel_format"_a = nb::none(), "component_format"_a = nb::none(),
             "srgb_gamma"_a = nb::none(), "alpha_transform"_a = Bitmap::AlphaTransform::Empty)
        .def("convert", nb::overload_cast<Bitmap *>(&Bitmap::convert, nb::const_),
             D(Bitmap, convert, 2), "target"_a,
             nb::call_guard<nb::gil_scoped_release>())
        .def("accumulate",
             nb::overload_cast<const Bitmap *, ScalarPoint2i,
                               ScalarPoint2i, ScalarVector2i>(&Bitmap::accumulate),
             D(Bitmap, accumulate),
             "bitmap"_a, "source_offset"_a, "target_offset"_a, "size"_a)
        .def("accumulate",
             nb::overload_cast<const Bitmap *, const ScalarPoint2i &>(&Bitmap::accumulate),
             D(Bitmap, accumulate, 2),
             "bitmap"_a, "target_offset"_a)
        .def("accumulate", nb::overload_cast<const Bitmap *>(
                &Bitmap::accumulate), D(Bitmap, accumulate, 3),
             "bitmap"_a)
        .def_method(Bitmap, vflip)
        .def("struct_",
             nb::overload_cast<>(&Bitmap::struct_, nb::const_),
             D(Bitmap, struct))
        .def(nb::self == nb::self)
        .def(nb::self != nb::self);

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

    bitmap
        .def(nb::init<const fs::path &, Bitmap::FileFormat>(), "path"_a,
             "format"_a = Bitmap::FileFormat::Auto,
             nb::call_guard<nb::gil_scoped_release>())
        .def(nb::init<Stream *, Bitmap::FileFormat>(), "stream"_a,
             "format"_a = Bitmap::FileFormat::Auto,
             nb::call_guard<nb::gil_scoped_release>())
        .def("write",
             nb::overload_cast<Stream *, Bitmap::FileFormat, int>(
                 &Bitmap::write, nb::const_),
             "stream"_a, "format"_a = Bitmap::FileFormat::Auto,
             "quality"_a = -1, D(Bitmap, write),
             nb::call_guard<nb::gil_scoped_release>())
        .def("write",
             nb::overload_cast<const fs::path &, Bitmap::FileFormat, int>(
                 &Bitmap::write, nb::const_),
             "path"_a, "format"_a = Bitmap::FileFormat::Auto, "quality"_a = -1,
             D(Bitmap, write, 2), nb::call_guard<nb::gil_scoped_release>())
        .def("write_async",
             nb::overload_cast<const fs::path &, Bitmap::FileFormat, int>(
                 &Bitmap::write_async, nb::const_),
             "path"_a, "format"_a = Bitmap::FileFormat::Auto, "quality"_a = -1,
             D(Bitmap, write_async))
        .def("split", &Bitmap::split, D(Bitmap, split))
        .def_static("detect_file_format", &Bitmap::detect_file_format,
                    D(Bitmap, detect_file_format))
        .def(
            "__dlpack__",
            [](Bitmap &bitmap, nb::object /*stream*/) {
                Struct::Type component_format = bitmap.component_format();
                nb::dlpack::dtype dtype;
                switch (component_format) {
                    case Struct::Type::UInt8:
                        dtype = nb::dtype<uint8_t>();
                        break;
                    case Struct::Type::UInt16:
                        dtype = nb::dtype<uint16_t>();
                        break;
                    case Struct::Type::UInt32:
                        dtype = nb::dtype<uint32_t>();
                        break;
                    case Struct::Type::UInt64:
                        dtype = nb::dtype<uint64_t>();
                        break;
                    case Struct::Type::Int8:
                        dtype = nb::dtype<int8_t>();
                        break;
                    case Struct::Type::Int16:
                        dtype = nb::dtype<int16_t>();
                        break;
                    case Struct::Type::Int32:
                        dtype = nb::dtype<int32_t>();
                        break;
                    case Struct::Type::Int64:
                        dtype = nb::dtype<int64_t>();
                        break;
                    case Struct::Type::Float16:
                        dtype = nb::dtype<fp16>();
                        break;
                    case Struct::Type::Float32:
                        dtype = nb::dtype<float>();
                        break;
                    case Struct::Type::Float64:
                        dtype = nb::dtype<double>();
                        break;
                    default:
                        throw nb::type_error("Invalid component format");
                }

                nb::make_tuple(bitmap.height(), bitmap.width(),
                               bitmap.channel_count());

                return nb::ndarray<>(
                    bitmap.data(),
                    { bitmap.height(), bitmap.width(), bitmap.channel_count() },
                    nb::handle(), {}, dtype, nb::device::cpu::value, 0);
            },
            "stream"_a = nb::none(), "Interface for the DLPack protocol.",
            nb::rv_policy::reference_internal)
        .def(
            "__dlpack_device__",
            [](Bitmap & /*bitmap*/) {
                return nb::make_tuple(nb::device::cpu::value, 0);
            },
            "Interface for the DLPack protocol.")
        .def_prop_ro("__array_interface__", [](Bitmap &bitmap) -> nb::object {
            if (bitmap.struct_()->size() == 0)
                return nb::none();
            auto field = bitmap.struct_()->operator[](0);
            nb::dict result;

            if (bitmap.channel_count() == 1)
                result["shape"] = nb::make_tuple(bitmap.height(), bitmap.width());
            else
                result["shape"] = nb::make_tuple(bitmap.height(), bitmap.width(),
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
                result["typestr"] = nb::bytes(code.c_str());
            #endif
            result["data"] = nb::make_tuple(size_t(bitmap.uint8_data()), false);
            result["version"] = 3;
            return nb::object(result);
        });

    /**
     * Python only constructor for any CPU array type
     */
    bitmap.def(
        "__init__",
        [](Bitmap *b, ContigCpuNdArray data,
           nb::object pixel_format_,
           const std::vector<std::string> &channel_names) {
            from_cpu_dlpack(b, data, pixel_format_, channel_names);
        },
        "array"_a, "pixel_format"_a = nb::none(),
        "channel_names"_a = std::vector<std::string>(),
        "Initialize a Bitmap from any array that implements the buffer or "
        "DLPack protocol.");

    /**
     * Python only constructor for Dr.Jit tensor types
     */
    bitmap.def(
        "__init__",
        [](Bitmap *b, nb::handle_t<dr::ArrayBase> h, nb::object pixel_format_,
           const std::vector<std::string> &channel_names) {
            nb::handle type = h.type();
            dr::ArrayMeta m = nb::type_supplement<dr::ArraySupplement>(type);

            if (!m.is_tensor)
                throw nb::type_error("This constructor is only supported with "
                                     "Dr.Jit Tensor types!");

            // Use Numpy conversion to get migration to the CPU
            ContigCpuNdArray data = nb::cast<ContigCpuNdArray>(h.attr("numpy")());
            from_cpu_dlpack(b, data, pixel_format_, channel_names);
        },
        "array"_a, "pixel_format"_a = nb::none(),
        "channel_names"_a = std::vector<std::string>(),
        "Initialize a Bitmap from any array that implements the buffer or "
        "DLPack protocol.");

    bitmap.def("_repr_html_", [](const Bitmap &_bitmap) -> nb::object {
        if (_bitmap.pixel_format() == Bitmap::PixelFormat::MultiChannel)
            return nb::none();

        ref<Bitmap> bitmap = _bitmap.convert(Bitmap::PixelFormat::RGB,
                                             Struct::Type::UInt16, true);
        ref<MemoryStream> s = new MemoryStream(bitmap->buffer_size());
        bitmap->write(s, Bitmap::FileFormat::PNG);
        s->seek(0);
        std::unique_ptr<char> tmp(new char[s->size()]);
        s->read((void *) tmp.get(), s->size());

        auto base64 = nb::module_::import_("base64");
        auto bytes = base64.attr("b64encode")(nb::bytes(tmp.get(), s->size()));
        std::stringstream s_bytes;
        s_bytes << nb::str(bytes).c_str();

        std::stringstream out;
        out << "<img src=\"data:image/png;base64, ";
        out << s_bytes.str().substr(2, s_bytes.str().size() - 3) << "\"";
        out << "width=\"250vm\"";
        out << " />";

        return nb::str(out.str().c_str());
    });
}
