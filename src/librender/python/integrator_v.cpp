#include <mitsuba/render/integrator.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/python/python.h>

#if defined(__APPLE__) || defined(__linux__)
#  define MTS_HANDLE_SIGINT 1
#else
#  define MTS_HANDLE_SIGINT 0
#endif

#if MTS_HANDLE_SIGINT
#include <signal.h>

/// Current signal handler
static std::function<void()> sigint_handler;

/// Previously installed signal handler
static void (*sigint_handler_prev)(int) = nullptr;
#endif

/// Trampoline for derived types implemented in Python
MTS_VARIANT class PySamplingIntegrator : public SamplingIntegrator<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(SamplingIntegrator, Scene, Sampler, Medium, Emitter, EmitterPtr, BSDF, BSDFPtr)

    PySamplingIntegrator(const Properties &props) : SamplingIntegrator(props) { }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const RayDifferential3f &ray,
                                     const Medium *medium,
                                     Float *aovs,
                                     Mask active) const override {
        py::gil_scoped_acquire gil;
        py::function overload = py::get_overload(this, "sample");

        if (overload) {
            using PyReturn = std::tuple<Spectrum, Mask, std::vector<Float>>;
            auto [spec, mask, aovs_]
                = overload(scene, sampler, ray, medium, active).template cast<PyReturn>();

            std::copy(aovs_.begin(), aovs_.end(), aovs);
            return { spec, mask };
        } else {
            Throw("SamplingIntegrator doesn't overload the method \"sample\"");
        }
    }

    std::vector<std::string> aov_names() const override {
        PYBIND11_OVERRIDE(std::vector<std::string>, SamplingIntegrator, aov_names, );
    }

    std::string to_string() const override {
        PYBIND11_OVERRIDE(std::string, SamplingIntegrator, to_string, );
    }
};

MTS_PY_EXPORT(Integrator) {
    MTS_PY_IMPORT_TYPES()
    using PySamplingIntegrator = PySamplingIntegrator<Float, Spectrum>;

    MTS_PY_CLASS(Integrator, Object)
        .def("render",
            [&](Integrator *integrator, Scene *scene, Sensor *sensor) {
                py::gil_scoped_release release;

#if MTS_HANDLE_SIGINT
                // Install new signal handler
                sigint_handler = [integrator]() {
                    integrator->cancel();
                };

                sigint_handler_prev = signal(SIGINT, [](int) {
                    Log(Warn, "Received interrupt signal, winding down..");
                    if (sigint_handler) {
                        sigint_handler();
                        sigint_handler = std::function<void()>();
                        signal(SIGINT, sigint_handler_prev);
                        raise(SIGINT);
                    }
                });
#endif

                bool res = integrator->render(scene, sensor);

#if MTS_HANDLE_SIGINT
                // Restore previous signal handler
                signal(SIGINT, sigint_handler_prev);
#endif

                return res;
            },
            D(Integrator, render), "scene"_a, "sensor"_a)
        .def_method(Integrator, cancel);

    auto integrator =
        py::class_<SamplingIntegrator, PySamplingIntegrator, Integrator,
                    ref<SamplingIntegrator>>(m, "SamplingIntegrator", D(SamplingIntegrator))
            .def(py::init<const Properties&>())
            .def_method(SamplingIntegrator, aov_names)
            .def_method(SamplingIntegrator, should_stop);

    integrator.def(
        "sample",
        [](const SamplingIntegrator *integrator, const Scene *scene, Sampler *sampler,
           const RayDifferential3f &ray, const Medium *medium, Mask active) {
            py::gil_scoped_release release;
            std::vector<Float> aovs(integrator->aov_names().size(), 0.f);
            auto [spec, mask] = integrator->sample(scene, sampler, ray, medium, aovs.data(), active);
            return std::make_tuple(spec, mask, aovs);
        },
        "scene"_a, "sampler"_a, "ray"_a, "medium"_a = nullptr, "active"_a = true,
        D(SamplingIntegrator, sample));

    MTS_PY_REGISTER_OBJECT("register_integrator", Integrator)

    MTS_PY_CLASS(MonteCarloIntegrator, SamplingIntegrator);
}
