#include <mitsuba/core/rfilter.h>
#include <mitsuba/python/python.h>


MTS_PY_EXPORT_VARIANTS(rfilter) {
    MTS_PY_CHECK_ALIAS(FilterBoundaryCondition, m) {
        py::enum_<FilterBoundaryCondition>(m, "FilterBoundaryCondition", D(FilterBoundaryCondition))
            .value("Clamp", FilterBoundaryCondition::Clamp, D(FilterBoundaryCondition, Clamp))
            .value("Repeat", FilterBoundaryCondition::Repeat, D(FilterBoundaryCondition, Repeat))
            .value("Mirror", FilterBoundaryCondition::Mirror, D(FilterBoundaryCondition, Mirror))
            .value("Zero", FilterBoundaryCondition::Zero, D(FilterBoundaryCondition, Zero))
            .value("One", FilterBoundaryCondition::One, D(FilterBoundaryCondition, One))
            .export_values();
    }

    using Resampler = mitsuba::Resampler<float>;
    MTS_PY_CHECK_ALIAS(Resampler, m) {
        py::class_<Resampler>(m, "Resampler", D(Resampler))
            .def(py::init<const ReconstructionFilter<float, void> *, uint32_t, uint32_t>(),
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
                [](Resampler &resampler, const py::array &source,
                    uint32_t source_stride, py::array &target,
                    uint32_t target_stride, uint32_t channels) {
                    if (!source.dtype().is(py::dtype::of<float>()))
                        throw std::runtime_error(
                            "'source' has an incompatible type!");
                    if (!target.dtype().is(py::dtype::of<float>()))
                        throw std::runtime_error(
                            "'target' has an incompatible type!");
                    if (resampler.source_resolution() * source_stride != (size_t) source.size())
                        throw std::runtime_error(
                            "'source' has an incompatible size!");
                    if (resampler.target_resolution() * target_stride != (size_t) target.size())
                        throw std::runtime_error(
                            "'target' has an incompatible size!");

                    resampler.resample((const float *) source.data(), source_stride,
                                        (float *) target.mutable_data(), target_stride,
                                        channels);
                },
                D(Resampler, resample), "self"_a, "source"_a, "source_stride"_a,
                "target_stride"_a, "channels"_a);
    }

    using ReconstructionFilter = mitsuba::ReconstructionFilter<Float, Spectrum>;
    using ReconstructionFilterP = mitsuba::ReconstructionFilter<FloatP, SpectrumP>;

    MTS_PY_CHECK_ALIAS(ReconstructionFilter, m) {
        MTS_PY_CLASS(ReconstructionFilter, Object)
            .def_method(ReconstructionFilter, border_size)
            .def_method(ReconstructionFilter, radius)
            .def("eval",
                vectorize<Float>(&ReconstructionFilterP::eval),
                D(ReconstructionFilter, eval), "x"_a)
            .def("eval_discretized",
                vectorize<Float>(&ReconstructionFilterP::template eval_discretized<FloatP>),
                D(ReconstructionFilter, eval_discretized), "x"_a, "active"_a = true)
            ;
    }

    m.attr("MTS_FILTER_RESOLUTION") = MTS_FILTER_RESOLUTION;
}
