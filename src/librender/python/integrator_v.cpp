#include <mitsuba/core/properties.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/python/python.h>
#include "signal.h"

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

/// RAII helper to catch Ctrl-C keypresses and cancel an ongoing render job
ScopedSignalHandler::ScopedSignalHandler(IntegratorT *integrator) {
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
#else
    (void) integrator;
#endif
}

ScopedSignalHandler::~ScopedSignalHandler() {
#if MTS_HANDLE_SIGINT
    // Restore previous signal handler
    signal(SIGINT, sigint_handler_prev);
#endif
}

/// Trampoline for derived types implemented in Python
MTS_VARIANT class PySamplingIntegrator : public SamplingIntegrator<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(SamplingIntegrator, Scene, Sensor, Sampler, Medium, Emitter, EmitterPtr, BSDF, BSDFPtr)

    PySamplingIntegrator(const Properties &props) : SamplingIntegrator(props) {
        if constexpr (!dr::is_jit_array_v<Float>) {
            Log(Warn, "SamplingIntegrator Python implementations will have "
                      "terrible performance in scalar_* modes. It is strongly "
                      "recommended to switch to a cuda_* or llvm_* mode");
        }
    }

    TensorXf render(Scene *scene,
                    Sensor *sensor,
                    uint32_t seed,
                    uint32_t spp,
                    bool develop,
                    bool evaluate) override {
        py::gil_scoped_acquire gil;
        py::function overload = py::get_overload(this, "render");

        if (overload) {
            return overload(scene, sensor, seed, spp, develop, evaluate).template cast<TensorXf>();
        } else {
            return SamplingIntegrator::render(scene, sensor, seed, spp, develop, evaluate);
        }
    }

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
            auto [spec, mask, aovs_] =
                overload(scene, sampler, ray, medium, active)
                    .template cast<PyReturn>();

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
        .def(
            "render",
            [&](Integrator *integrator, Scene *scene, Sensor *sensor,
                uint32_t seed, uint32_t spp, bool develop, bool evaluate) {
                py::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render(scene, sensor, seed, spp, develop,
                                          evaluate);
            },
            D(Integrator, render), "scene"_a, "sensor"_a, "seed"_a = 0,
            "spp"_a = 0, "develop"_a = true, "evaluate"_a = true)

        .def(
            "render",
            [&](Integrator *integrator, Scene *scene, uint32_t sensor,
                uint32_t seed, uint32_t spp, bool develop, bool evaluate) {
                py::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render(scene, sensor, seed, spp,
                                          develop, evaluate);
            },
            D(Integrator, render, 2), "scene"_a, "sensor"_a = 0,
            "seed"_a = 0, "spp"_a = 0, "develop"_a = true, "evaluate"_a = true)
        .def_method(Integrator, cancel)
        .def_method(Integrator, should_stop)
        .def_method(Integrator, aov_names);

    auto sampling_integrator =
        py::class_<SamplingIntegrator, PySamplingIntegrator, Integrator,
                    ref<SamplingIntegrator>>(m, "SamplingIntegrator", D(SamplingIntegrator))
            .def(py::init<const Properties&>());

    sampling_integrator.def(
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

    MTS_PY_CLASS(AdjointIntegrator, Integrator)
        .def_method(AdjointIntegrator, sample, "scene"_a, "sensor"_a,
                    "sampler"_a, "block"_a, "sample_scale"_a);
}
