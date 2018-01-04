#include <mitsuba/render/integrator.h>
#include <mitsuba/python/python.h>


MTS_PY_EXPORT(Integrator) {
    MTS_PY_CLASS(Integrator, Object)
        .mdef(Integrator, render, "scene"_a, "vectorize"_a)
        .mdef(Integrator, cancel);

    MTS_PY_CLASS(SamplingIntegrator, Integrator)
        .def("eval", py::overload_cast<const RayDifferential3f &, RadianceSample3f &>(
                &SamplingIntegrator::eval, py::const_),
                D(SamplingIntegrator, eval), "ray"_a, "rs"_a);
        //.def("eval", enoki::vectorize_wrapper(
                //py::overload_cast<const RayDifferential3fP &, RadianceSample3fP &, MaskP>(
                //&SamplingIntegrator::eval, py::const_)), "ray"_a, "rs"_a, "active"_a = true)

    MTS_PY_CLASS(MonteCarloIntegrator, SamplingIntegrator);
}
