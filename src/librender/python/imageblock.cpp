#include <mitsuba/python/python.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/imageblock.h>

MTS_PY_EXPORT(ImageBlock) {
    MTS_IMPORT_TYPES()
    MTS_IMPORT_OBJECT_TYPES()
    using ImageBlockP = mitsuba::ImageBlock<FloatP, SpectrumP>;

    MTS_PY_CHECK_ALIAS(ImageBlock, m) {
        MTS_PY_CLASS(ImageBlock, Object)
            .def(py::init<PixelFormat, const ScalarVector2i &, const ReconstructionFilter *,
                 size_t, bool, bool, bool>(),
                "fmt"_a, "size"_a, "filter"_a = nullptr, "channels"_a = 0,
                "warn"_a = true, "border"_a = true, "normalize"_a = false)
            .def("put", py::overload_cast<const ImageBlock *>(&ImageBlock::put),
                D(ImageBlock, put), "block"_a)
            .def("put", vectorize<Float>(py::overload_cast<const Point<FloatP, 2> &,
                const wavelength_t<SpectrumP> &, const SpectrumP &, const FloatP &,
                mask_t<FloatP>>(&ImageBlockP::put)),
                "pos"_a, "wavelengths"_a, "value"_a, "alpha"_a, "active"_a = true,
                D(ImageBlock, put, 2))
            .def("put",
                [](ImageBlock &ib, const Point2f &pos,
                   const std::vector<Float> &data, Mask mask) {
                    if (data.size() != ib.channel_count())
                            throw std::runtime_error("Incompatible channel count!");
                    ib.put(pos, data.data(), mask);
                }, "pos"_a, "data"_a, "active"_a = true)
            .def_method(ImageBlock, clear)
            .def_method(ImageBlock, set_offset, "offset"_a)
            .def_method(ImageBlock, offset)
            .def_method(ImageBlock, size)
            .def_method(ImageBlock, width)
            .def_method(ImageBlock, height)
            .def_method(ImageBlock, warns)
            .def_method(ImageBlock, set_warn, "warn"_a)
            .def_method(ImageBlock, border_size)
            .def_method(ImageBlock, channel_count)
            .def_method(ImageBlock, pixel_format)
            .def("bitmap", py::overload_cast<>(&ImageBlock::bitmap));
    }
}
