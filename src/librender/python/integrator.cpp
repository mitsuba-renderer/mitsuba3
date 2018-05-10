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
    Log(EWarn, "Received interrupt signal, winding down..");
    (*current_integrator).cancel();
    signal(sig, sigint_handler_prev);
    raise(sig);
}
#endif

MTS_PY_EXPORT(Integrator) {
    /// Integrator (base class).
    MTS_PY_CLASS(Integrator, Object)
        .def("render",
             [&](Integrator &integrator, Scene *scene, bool vectorize) {
                 py::gil_scoped_release release;

#if MTS_HANDLE_SIGINT
                 // Install new signal handler
                 current_integrator  = &integrator;
                 sigint_handler_prev = signal(SIGINT, sigint_handler);
#endif

                 bool res = integrator.render(scene, vectorize);

#if MTS_HANDLE_SIGINT
                 // Restore previous signal handler
                 signal(SIGINT, sigint_handler_prev);
#endif

                 return res;
             },
             D(Integrator, render), "scene"_a, "vectorize"_a)
        .mdef(Integrator, cancel)
        .mdef(Integrator, register_callback, "cb"_a, "period"_a)
        .mdef(Integrator, remove_callback, "index"_a)
        .mdef(Integrator, callback_count)
        .mdef(Integrator, notify, "bitmap"_a, "extra"_a);

    /// SamplingIntegrator.
    MTS_PY_CLASS(SamplingIntegrator, Integrator)
        .def("eval",
             py::overload_cast<const RayDifferential3f &, RadianceSample3f &>(
                 &SamplingIntegrator::eval, py::const_),
             D(SamplingIntegrator, eval), "ray"_a, "rs"_a)
        // TODO: bind this vectorized variant (may need to vectorize manually).
        // .def("eval",
        //      enoki::vectorize_wrapper(
        //          py::overload_cast<const RayDifferential3fP &,
        //                            RadianceSample3fP &, MaskP>(
        //              &SamplingIntegrator::eval, py::const_)),
        //      D(SamplingIntegrator, eval, 2), "ray"_a, "rs"_a, "active"_a =
        //      true)
        .mdef(SamplingIntegrator, should_stop);

    /// MonteCarloIntegrator.
    MTS_PY_CLASS(MonteCarloIntegrator, SamplingIntegrator);
}
