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

        // Note that we are not using the py::overload_cast convenience method
        // here, because it seems to be incompatible with overloads that have
        // templated variants.
        .def("put",
             (void (ImageBlock::*)(const ImageBlock *)) &ImageBlock::put,
             D(ImageBlock, put), "block"_a)
        .def("put",
             &ImageBlock::put<Point2f, Spectrumf>,
             D(ImageBlock, put, 2),
             "pos"_a, "spec"_a, "alpha"_a, "unused"_a = true)
        .def("put",
             enoki::vectorize_wrapper(&ImageBlock::put<Point2fP, SpectrumfP>),
             D(ImageBlock, put, 2),
             "pos"_a, "spec"_a, "alpha"_a, "active"_a = true)

        .mdef(ImageBlock, clone)
        .mdef(ImageBlock, copy_to, "copy"_a)
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
        .mdef(ImageBlock, clear)
        .def("__repr__", &ImageBlock::to_string)
        ;
}
