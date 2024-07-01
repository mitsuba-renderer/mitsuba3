#include <mitsuba/core/rfilter.h>
#include <mitsuba/python/python.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>

MI_PY_EXPORT(rfilter) {
    nb::enum_<FilterBoundaryCondition>(m, "FilterBoundaryCondition", D(FilterBoundaryCondition))
        .value("Clamp", FilterBoundaryCondition::Clamp, D(FilterBoundaryCondition, Clamp))
        .value("Repeat", FilterBoundaryCondition::Repeat, D(FilterBoundaryCondition, Repeat))
        .value("Mirror", FilterBoundaryCondition::Mirror, D(FilterBoundaryCondition, Mirror))
        .value("Zero", FilterBoundaryCondition::Zero, D(FilterBoundaryCondition, Zero))
        .value("One", FilterBoundaryCondition::One, D(FilterBoundaryCondition, One));

    using Resampler = mitsuba::Resampler<float>;
    using ArrayType = nb::ndarray<float,  nb::c_contig, nb::device::cpu>;

    nb::class_<Resampler>(m, "Resampler", D(Resampler))
        .def(nb::init<const ReconstructionFilter<float, Color<float, 3>> *, uint32_t, uint32_t>(),
            "rfilter"_a, "source_res"_a, "target_res"_a,
            D(Resampler, Resampler))
        .def_method(Resampler, source_resolution)
        .def_method(Resampler, target_resolution)
        .def_method(Resampler, boundary_condition)
        .def_method(Resampler, set_boundary_condition)
        .def_method(Resampler, set_clamp)
        .def_method(Resampler, taps)
        .def_method(Resampler, clamp)
        .def("__repr__", &Resampler::to_string)
        .def("resample",
            [](Resampler &resampler, const ArrayType &source,
                uint32_t source_stride, ArrayType &target,
                uint32_t target_stride, uint32_t channels) {
                if (resampler.source_resolution() * source_stride != (size_t) source.size())
                    throw std::runtime_error(
                        "'source' has an incompatible size!");
                if (resampler.target_resolution() * target_stride != (size_t) target.size())
                    throw std::runtime_error(
                        "'target' has an incompatible size!");

                resampler.resample((const float *) source.data(), source_stride,
                                    (float *) target.data(), target_stride,
                                    channels);
            },
            D(Resampler, resample), "source"_a, "source_stride"_a, "target"_a,
            "target_stride"_a, "channels"_a);

    m.attr("MI_FILTER_RESOLUTION") = MI_FILTER_RESOLUTION;
}
