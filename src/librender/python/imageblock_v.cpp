#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(ImageBlock) {
    MTS_PY_IMPORT_TYPES(ImageBlock, ReconstructionFilter)
    MTS_PY_CLASS(ImageBlock, Object)
        .def(py::init<const ScalarVector2i &, size_t,
                const ReconstructionFilter *, bool, bool, bool, bool>(),
            "size"_a, "channel_count"_a, "filter"_a = nullptr,
            "warn_negative"_a = true, "warn_invalid"_a = true,
            "border"_a = true, "normalize"_a = false)
        .def("put", py::overload_cast<const ImageBlock *>(&ImageBlock::put),
            D(ImageBlock, put), "block"_a)
        .def("put", vectorize(py::overload_cast<const Point2f &,
            const wavelength_t<Spectrum> &, const Spectrum &, const Float &,
            mask_t<Float>>(&ImageBlock::put)),
            "pos"_a, "wavelengths"_a, "value"_a, "alpha"_a = 1.f, "active"_a = true,
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
        .def_method(ImageBlock, warn_invalid)
        .def_method(ImageBlock, warn_negative)
        .def_method(ImageBlock, set_warn_invalid, "value"_a)
        .def_method(ImageBlock, set_warn_negative, "value"_a)
        .def_method(ImageBlock, border_size)
        .def_method(ImageBlock, channel_count)
        .def("data", py::overload_cast<>(&ImageBlock::data, py::const_), D(ImageBlock, data));
}
