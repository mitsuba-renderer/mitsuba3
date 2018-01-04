#include <mitsuba/core/rfilter.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(rfilter) {
    using EBoundaryCondition = ReconstructionFilter::EBoundaryCondition;
    using Resampler = mitsuba::Resampler<Float>;

    auto rfilter = MTS_PY_CLASS(ReconstructionFilter, Object)
        .mdef(ReconstructionFilter, border_size)
        .mdef(ReconstructionFilter, radius)
        .mdef(ReconstructionFilter, eval)
        .def("eval_discretized",
             &ReconstructionFilter::eval_discretized<Float>,
             D(ReconstructionFilter, eval_discretized))
        .def("eval_discretized",
             vectorize_wrapper(
                 &ReconstructionFilter::eval_discretized<FloatP>),
             D(ReconstructionFilter, eval_discretized));

    auto bc = py::enum_<EBoundaryCondition>(rfilter, "EBoundaryCondition")
        .value("EClamp", EBoundaryCondition::EClamp, D(ReconstructionFilter, EBoundaryCondition, EClamp))
        .value("ERepeat", EBoundaryCondition::ERepeat, D(ReconstructionFilter, EBoundaryCondition, ERepeat))
        .value("EMirror", EBoundaryCondition::EMirror, D(ReconstructionFilter, EBoundaryCondition, EMirror))
        .value("EZero", EBoundaryCondition::EZero, D(ReconstructionFilter, EBoundaryCondition, EZero))
        .value("EOne", EBoundaryCondition::EOne, D(ReconstructionFilter, EBoundaryCondition, EOne))
        .export_values();

    auto resampler = py::class_<Resampler>(m, "Resampler", D(Resampler))
        .def(py::init<const ReconstructionFilter *, uint32_t, uint32_t>(),
             "rfilter"_a, "source_res"_a, "target_res"_a,
             D(Resampler, Resampler))
        .mdef(Resampler, source_resolution)
        .mdef(Resampler, target_resolution)
        .mdef(Resampler, boundary_condition)
        .mdef(Resampler, set_boundary_condition)
        .mdef(Resampler, set_clamp)
        .mdef(Resampler, taps)
        .mdef(Resampler, clamp)
        .def("__repr__", &Resampler::to_string)
        .def("resample",
             [](Resampler &resampler, const py::array &source,
                uint32_t source_stride, py::array &target,
                uint32_t target_stride, uint32_t channels) {
                 if (!source.dtype().is(py::dtype::of<Float>()))
                     throw std::runtime_error(
                         "'source' has an incompatible type!");
                 if (!target.dtype().is(py::dtype::of<Float>()))
                     throw std::runtime_error(
                         "'target' has an incompatible type!");
                 if (resampler.source_resolution() * source_stride != (size_t) source.size())
                     throw std::runtime_error(
                         "'source' has an incompatible size!");
                 if (resampler.target_resolution() * target_stride != (size_t) target.size())
                     throw std::runtime_error(
                         "'target' has an incompatible size!");

                 resampler.resample((const Float *) source.data(), source_stride,
                                    (Float *) target.mutable_data(), target_stride,
                                    channels);
             },
             D(Resampler, resample), "self"_a, "source"_a, "source_stride"_a,
             "target_stride"_a, "channels"_a);

    resampler.attr("EBoundaryCondition") = bc;

    m.attr("MTS_FILTER_RESOLUTION") = MTS_FILTER_RESOLUTION;
}
