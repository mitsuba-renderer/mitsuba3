#include <mitsuba/python/python.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/imageblock.h>

MTS_PY_EXPORT_VARIANTS(ImageBlock) {

    using Vector2i = typename ImageBlock::Vector2i;
    using Point2f = typename ImageBlock::Point2f;
    using Wavelength = typename ImageBlock::Wavelength;
    using Mask = typename ImageBlock::Mask;
    using ReconstructionFilter = typename ImageBlock::ReconstructionFilter;

    MTS_PY_CLASS(ImageBlock, Object)
        .def(py::init<Bitmap::EPixelFormat, const Vector2i &,
                      const ReconstructionFilter *, size_t, bool, bool, bool>(),
             "fmt"_a, "size"_a, "filter"_a = nullptr, "channels"_a = 0,
             "warn"_a = true, "border"_a = true, "normalize"_a = false)
        .def("put", py::overload_cast<const ImageBlock *>(&ImageBlock::put),
             D(ImageBlock, put), "block"_a)
        .def("put", py::overload_cast<const Point2f &,
             const Wavelength &, const Spectrum &, const Float &, Mask>(&ImageBlock::put),
             "pos"_a, "wavelengths"_a, "value"_a, "alpha"_a, "active"_a = true,
             D(ImageBlock, put, 2))
        .def("put", [](ImageBlock &ib, const Point2f &pos,
                       const std::vector<Float> &data, bool mask) {
                 if (data.size() != ib.channel_count())
                     throw std::runtime_error("Incompatible channel count!");
                 ib.put(pos, data.data(), mask);
             }, "pos"_a, "data"_a, "active"_a = true)
        // TODO vectorize_wrapper on put with FloatP
        .mdef(ImageBlock, clear)
        .mdef(ImageBlock, set_offset, "offset"_a)
        .mdef(ImageBlock, offset)
        .mdef(ImageBlock, size)
        .mdef(ImageBlock, width)
        .mdef(ImageBlock, height)
        .mdef(ImageBlock, warns)
        .mdef(ImageBlock, set_warn, "warn"_a)
        .mdef(ImageBlock, border_size)
        .mdef(ImageBlock, channel_count)
        .mdef(ImageBlock, pixel_format)
        .def("bitmap", py::overload_cast<>(&ImageBlock::bitmap))
        ;
}
