#include <mitsuba/python/python.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/rfilter.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/render/scene.h>

MTS_PY_EXPORT(ImageBlock) {
    MTS_PY_CLASS(ImageBlock, Object)
        .def(py::init<Bitmap::EPixelFormat, const Vector2i &,
                      const ReconstructionFilter *, size_t, bool, bool, bool, bool>(),
             "fmt"_a, "size"_a, "filter"_a = nullptr, "channels"_a = 0,
             "warn"_a = true, "monochrome"_a = false, "border"_a = true, "normalize"_a = false)

        .def("put", (void (ImageBlock::*)(const ImageBlock *)) &ImageBlock::put,
             D(ImageBlock, put), "block"_a)

        .def("put", &ImageBlock::put<Point2f, Spectrumf>, D(ImageBlock, put, 2),
             "pos"_a, "wavelengths"_a, "value"_a, "alpha"_a, "unused"_a = true)

        .def("put", enoki::vectorize_wrapper(&ImageBlock::put<Point2fP, SpectrumfP>),
             "pos"_a, "wavelengths"_a, "value"_a, "alpha"_a, "active"_a = true)

        .def("put", [](ImageBlock &ib, const Point2f &pos,
                       const std::vector<Float> &data, bool mask) {
                 if (data.size() != ib.channel_count())
                     throw std::runtime_error("Incompatible channel count!");
                 ib.put(pos, data.data(), mask);
             }, "pos"_a, "data"_a, "unused"_a = true)

#if defined(MTS_ENABLE_AUTODIFF)
        .def("put", [](ImageBlock &ib, const Point2fD &pos,
                       const std::vector<FloatD> &data, MaskD mask) {
                 if (data.size() != ib.channel_count())
                     throw std::runtime_error("Incompatible channel count!");
                 ib.put(pos, data.data(), mask);
              }, "pos"_a, "data"_a, "unused"_a = true)

        .def("put", &ImageBlock::put<Point2fD, SpectrumfD>,
             "pos"_a, "wavelengths"_a, "value"_a, "alpha"_a, "active"_a = true)
#endif

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
#if defined(MTS_ENABLE_AUTODIFF)
        .def("bitmap_d", py::overload_cast<>(&ImageBlock::bitmap_d))
#endif
        .mdef(ImageBlock, clear)
#if defined(MTS_ENABLE_AUTODIFF)
        .mdef(ImageBlock, clear_d)
#endif
        ;
}
