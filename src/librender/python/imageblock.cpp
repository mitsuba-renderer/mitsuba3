#include <mitsuba/python/python.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/rfilter.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/render/scene.h>

MTS_PY_EXPORT(ImageBlock) {
    MTS_PY_CLASS(ImageBlock, Object)
        .def(py::init<Bitmap::EPixelFormat, const Vector2i &,
                      const ReconstructionFilter *, size_t, bool>(),
             "fmt"_a, "size"_a, "filter"_a = nullptr, "channels"_a = 0,
             "warn"_a = true)

        .def("put",
             (void (ImageBlock::*)(const ImageBlock *)) &ImageBlock::put,
             "block"_a)
        .def("put",
             &ImageBlock::put<Point2f, Spectrumf>,
             "pos"_a, "spec"_a, "alpha"_a)
        .def("put",
             &ImageBlock::put<Point2fP, SpectrumfP>,
             "pos"_a, "spec"_a, "alpha"_a)
        .def("put",
             (bool (ImageBlock::*)(const Point2f &, const Float *)) &ImageBlock::put,
             "pos"_a, "value"_a)
        .def("put",
             (bool (ImageBlock::*)(const Point2fP &, const FloatP *)) &ImageBlock::put,
             "pos"_a, "value"_a)

        .mdef(ImageBlock, clone)
        .mdef(ImageBlock, copy_to, "copy"_a)
        .mdef(ImageBlock, set_offset, "offset"_a)
        .mdef(ImageBlock, offset)
        .mdef(ImageBlock, set_size, "size"_a)
        .mdef(ImageBlock, size)
        .mdef(ImageBlock, width)
        .mdef(ImageBlock, height)
        .mdef(ImageBlock, warns)
        .mdef(ImageBlock, set_warn, "warn"_a)
        .mdef(ImageBlock, border_size)
        .mdef(ImageBlock, channel_count)
        .mdef(ImageBlock, pixel_format)
        .def("bitmap", py::overload_cast<>(&ImageBlock::bitmap))
        .mdef(ImageBlock, clear)
        .def("__repr__", &ImageBlock::to_string)
        ;
}
