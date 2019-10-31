#include <mitsuba/core/thread.h>
#include <mitsuba/core/tls.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/integrator.h>

// TODO: move all signal-related mechanisms to libcore?
#if defined(__APPLE__) || defined(__linux__)
#  define MTS_HANDLE_SIGINT 1
#else
#  define MTS_HANDLE_SIGINT 0
#endif

#if MTS_HANDLE_SIGINT
#include <signal.h>

/// Integrator currently in use (has to be global)
ThreadLocal<Integrator *> current_integrator;
/// Previously installed signal handler
static void (*sigint_handler_prev)(int) = nullptr;
/// Custom signal handler definition
static void sigint_handler(int sig) {
    Log(Warn, "Received interrupt signal, winding down..");
    (*current_integrator).cancel();
    signal(sig, sigint_handler_prev);
    raise(sig);
}
#endif

MTS_PY_EXPORT(Integrator) {
    /// Integrator (base class).
    MTS_PY_CLASS(Integrator, Object)
        .def("render",
             [&](Integrator &integrator, Scene *scene) {
                 py::gil_scoped_release release;

#if MTS_HANDLE_SIGINT
                 // Install new signal handler
                 current_integrator  = &integrator;
                 sigint_handler_prev = signal(SIGINT, sigint_handler);
#endif

                 bool res = integrator.render(scene);

#if MTS_HANDLE_SIGINT
                 // Restore previous signal handler
                 signal(SIGINT, sigint_handler_prev);
#endif

                 return res;
             },
             D(Integrator, render), "scene"_a)
        .def_method(Integrator, cancel)
        .def_method(Integrator, register_callback, "cb"_a, "period"_a)
        .def_method(Integrator, remove_callback, "index"_a)
        .def_method(Integrator, callback_count)
        .def_method(Integrator, notify, "bitmap"_a, "extra"_a);

    /// SamplingIntegrator.
    MTS_PY_CLASS(SamplingIntegrator, Integrator)
        .def("eval",
             py::overload_cast<const RayDifferential3f &, RadianceSample3f &, bool>(
                 &SamplingIntegrator::eval, py::const_),
             D(SamplingIntegrator, eval), "ray"_a, "rs"_a, "unused"_a = true)
#if 0
         .def("eval",
              enoki::vectorize_wrapper(
                  py::overload_cast<const RayDifferential3fP &,
                                    RadianceSample3fP &, MaskP>(
                      &SamplingIntegrator::eval, py::const_)),
              "ray"_a, "rs"_a, "active"_a = true)
#endif
#if defined(MTS_ENABLE_AUTODIFF)
        .def("eval",
             py::overload_cast<const RayDifferential3fD &, RadianceSample3fD &, MaskD>(
                 &SamplingIntegrator::eval, py::const_),
             "ray"_a, "rs"_a, "active"_a = true)
#endif
        .def_method(SamplingIntegrator, should_stop);

    /// MonteCarloIntegrator.
    MTS_PY_CLASS(MonteCarloIntegrator, SamplingIntegrator);

    /// PolarizedMonteCarloIntegrator.
    MTS_PY_CLASS(PolarizedMonteCarloIntegrator, SamplingIntegrator)
        .def("eval_pol",
             py::overload_cast<const RayDifferential3f &, RadianceSample3f &>(
                 &PolarizedMonteCarloIntegrator::eval_pol, py::const_),
             D(PolarizedMonteCarloIntegrator, eval_pol), "ray"_a, "rs"_a);
}
