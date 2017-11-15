#include <mitsuba/render/integrator.h>
#include <mitsuba/python/python.h>


MTS_PY_EXPORT(Integrator) {
    MTS_PY_CLASS(Integrator, Object)
        .def("render_scalar", &Integrator::render<false>,
             "scene"_a, D(Integrator, render))
        .def("render_vector", &Integrator::render<true>,
             "scene"_a, D(Integrator, render))
        .mdef(Integrator, cancel)
        .mdef(Integrator, configure_sampler, "scene"_a, "sampler"_a)
        ;

    MTS_PY_CLASS(SamplingIntegrator, Integrator)
        .def("Li", py::overload_cast<const RayDifferential3f &,
                                     RadianceRecord3f &>(
                &SamplingIntegrator::Li, py::const_), "ray"_a, "r_rec"_a)
        .def("Li", py::overload_cast<const RayDifferential3fP &,
                                     RadianceRecord3fP &,
                                     const mask_t<FloatP> &>(
                &SamplingIntegrator::Li, py::const_), "ray"_a, "r_rec"_a, "active"_a)
        .mdef(SamplingIntegrator, cancel)
        .def("render_block_scalar", &SamplingIntegrator::render_block<false>,
              "scene"_a, "sensor"_a, "sampler"_a, "block"_a,
              "stop"_a, "points"_a, D(SamplingIntegrator, render_block))
        .def("render_block_vector", &SamplingIntegrator::render_block<true>,
              "scene"_a, "sensor"_a, "sampler"_a, "block"_a,
              "stop"_a, "points"_a, D(SamplingIntegrator, render_block))
        ;

    MTS_PY_CLASS(MonteCarloIntegrator, SamplingIntegrator)
        ;
}
