#include <mitsuba/core/tls.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/python/python.h>

// TODO: move all signal-related mechanisms to libcore?
#define MTS_INSTALL_HANDLER defined(__APPLE__) || defined(__linux__)

#if MTS_INSTALL_HANDLER
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

#include <chrono>
#include <thread>

MTS_PY_EXPORT(Integrator) {
    MTS_PY_CLASS(Integrator, Object)
        .def("render", [&](Integrator &integrator, Scene *scene, bool vectorize) {
                #if MTS_INSTALL_HANDLER
                // Install new signal handler
                current_integrator = &integrator;
                sigint_handler_prev = signal(SIGINT, sigint_handler);
                #endif

                bool res = integrator.render(scene, vectorize);

                #if MTS_INSTALL_HANDLER
                // Restore previous signal handler
                signal(SIGINT, sigint_handler_prev);
                #endif

                return res;
             },
             D(Integrator, render), "scene"_a, "vectorize"_a)
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

#undef MTS_INSTALL_HANDLER
