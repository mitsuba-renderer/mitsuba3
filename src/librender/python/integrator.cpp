#include <mitsuba/core/thread.h>
#include <mitsuba/core/tls.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/integrator.h>

// TODO: move all signal-related mechanisms to libcore?
#if defined(__APPLE__) || defined(__linux__)
// TODO switch this back ON
#  define MTS_HANDLE_SIGINT 0 // 1
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
    MTS_IMPORT_TYPES()
    MTS_IMPORT_OBJECT_TYPES()
    MTS_PY_CHECK_ALIAS(Integrator, m) {
        MTS_PY_CLASS(Integrator, Object)
            .def("render",
                [&](Integrator &integrator, Scene *scene, Sensor *sensor) {
                    py::gil_scoped_release release;

#if MTS_HANDLE_SIGINT
                    // Install new signal handler
                    current_integrator  = &integrator;
                    sigint_handler_prev = signal(SIGINT, sigint_handler);
#endif

                    bool res = integrator.render(scene, sensor);

#if MTS_HANDLE_SIGINT
                    // Restore previous signal handler
                    signal(SIGINT, sigint_handler_prev);
#endif

                    return res;
                },
                D(Integrator, render), "scene"_a, "sensor"_a)
            .def_method(Integrator, cancel);
    }

    MTS_PY_CHECK_ALIAS(SamplingIntegrator, m) {
        MTS_PY_CLASS(SamplingIntegrator, Integrator)
            .def("sample",
                vectorize<Float>(&SamplingIntegrator::sample),
                "scene"_a, "sampler"_a, "ray"_a, "active"_a = true, D(SamplingIntegrator, sample))
            .def_method(SamplingIntegrator, should_stop);
    }

    MTS_PY_CHECK_ALIAS(MonteCarloIntegrator, m){
        MTS_PY_CLASS(MonteCarloIntegrator, SamplingIntegrator);
    }
}
