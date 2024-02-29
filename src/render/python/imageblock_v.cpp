#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/vector.h>

MI_PY_EXPORT(ImageBlock) {
    MI_PY_IMPORT_TYPES(ImageBlock, ReconstructionFilter)
    MI_PY_CLASS(ImageBlock, Object)
        .def(nb::init<const ScalarVector2u &, const ScalarPoint2i &, uint32_t,
                      const ReconstructionFilter *, bool, bool, bool,
                      bool, bool, bool>(),
             "size"_a, "offset"_a, "channel_count"_a, "rfilter"_a = nullptr,
             "border"_a = std::is_scalar_v<Float>, "normalize"_a = false,
             "coalesce"_a = dr::is_jit_v<Float>, "compensate"_a = false,
             "warn_negative"_a = std::is_scalar_v<Float>,
             "warn_invalid"_a  = std::is_scalar_v<Float>)
        .def(nb::init<const TensorXf &, const ScalarPoint2i &,
                      const ReconstructionFilter *, bool, bool, bool,
                      bool, bool, bool>(),
             "tensor"_a, "offset"_a = ScalarPoint2i(0), "rfilter"_a = nullptr,
             "border"_a = std::is_scalar_v<Float>, "normalize"_a = false,
             "coalesce"_a = dr::is_jit_v<Float>, "compensate"_a = false,
             "warn_negative"_a = std::is_scalar_v<Float>,
             "warn_invalid"_a  = std::is_scalar_v<Float>)
        .def("put_block", &ImageBlock::put_block, D(ImageBlock, put_block),
             "block"_a)
        .def("put",
             nb::overload_cast<const Point2f &, const wavelength_t<Spectrum> &,
                               const Spectrum &, Float, Float,
                               dr::mask_t<Float>>(&ImageBlock::put),
             "pos"_a, "wavelengths"_a, "value"_a, "alpha"_a = 1.f,
             "weight"_a = 1, "active"_a = true, D(ImageBlock, put, 2))
        .def("put",
             [](ImageBlock &ib, const Point2f &pos,
                const std::vector<Float> &values, Mask active) {
                 if (values.size() != ib.channel_count())
                     throw std::runtime_error("Incompatible channel count!");
                 ib.put(pos, values.data(), active);
             }, "pos"_a, "values"_a, "active"_a = true)
        .def("read",
             [](ImageBlock &ib, const Point2f &pos, Mask active) {
                 std::vector<Float> values(ib.channel_count());
                 ib.read(pos, values.data(), active);
                 return values;
             }, "pos"_a, "active"_a = true)
        .def_method(ImageBlock, clear)
        .def_method(ImageBlock, offset)
        .def_method(ImageBlock, set_offset, "offset"_a)
        .def_method(ImageBlock, size)
        .def_method(ImageBlock, set_size, "size"_a)
        .def_method(ImageBlock, coalesce)
        .def_method(ImageBlock, set_coalesce)
        .def_method(ImageBlock, compensate)
        .def_method(ImageBlock, set_compensate)
        .def_method(ImageBlock, width)
        .def_method(ImageBlock, height)
        .def_method(ImageBlock, rfilter)
        .def_method(ImageBlock, normalize)
        .def_method(ImageBlock, set_normalize)
        .def_method(ImageBlock, warn_invalid)
        .def_method(ImageBlock, warn_negative)
        .def_method(ImageBlock, set_warn_invalid, "value"_a)
        .def_method(ImageBlock, set_warn_negative, "value"_a)
        .def_method(ImageBlock, border_size)
        .def_method(ImageBlock, has_border)
        .def_method(ImageBlock, channel_count)
        .def("tensor", nb::overload_cast<>(&ImageBlock::tensor),
             nb::rv_policy::reference_internal,
             D(ImageBlock, tensor));
}
