#include <mitsuba/core/properties.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/python/python.h>
#include "signal.h"

#if defined(__APPLE__) || defined(__linux__)
#  define MI_HANDLE_SIGINT 1
#else
#  define MI_HANDLE_SIGINT 0
#endif

#if MI_HANDLE_SIGINT
#include <signal.h>

/// Current signal handler
static std::function<void()> sigint_handler;

/// Previously installed signal handler
static void (*sigint_handler_prev)(int) = nullptr;
#endif

/// RAII helper to catch Ctrl-C keypresses and cancel an ongoing render job
ScopedSignalHandler::ScopedSignalHandler(IntegratorT *integrator) {
#if MI_HANDLE_SIGINT
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
#if MI_HANDLE_SIGINT
    // Restore previous signal handler
    signal(SIGINT, sigint_handler_prev);
#endif
}

/// Trampoline for derived types implemented in Python
MI_VARIANT class PySamplingIntegrator : public SamplingIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(SamplingIntegrator, Scene, Sensor, Sampler, Medium)

    PySamplingIntegrator(const Properties &props) : SamplingIntegrator(props) {
        if constexpr (!dr::is_jit_v<Float>) {
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
        PYBIND11_OVERRIDE(TensorXf, SamplingIntegrator, render,
            scene, sensor, seed, spp, develop, evaluate);
    }

    TensorXf render_forward(Scene* scene,
                            void* params,
                            Sensor *sensor,
                            uint32_t seed = 0,
                            uint32_t spp = 0) override {
        PYBIND11_OVERRIDE(TensorXf, SamplingIntegrator, render_forward,
            scene, params, sensor, seed, spp);
    }

    void render_backward(Scene* scene,
                         void* params,
                         const TensorXf& grad_in,
                         Sensor* sensor,
                         uint32_t seed = 0,
                         uint32_t spp = 0) override {
        PYBIND11_OVERRIDE(void, SamplingIntegrator, render_backward,
            scene, params, grad_in, sensor, seed, spp);
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const RayDifferential3f &ray,
                                     const Medium *medium,
                                     Float *aovs,
                                     Mask active) const override {
        py::gil_scoped_acquire gil;
        py::function sample_override = py::get_override(this, "sample");

        if (sample_override) {
            using PyReturn = std::tuple<Spectrum, Mask, std::vector<Float>>;
            auto [spec, mask, aovs_] =
                sample_override(scene, sampler, ray, medium, active)
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

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyAdjointIntegrator : public AdjointIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(AdjointIntegrator, Scene, Sensor, Sampler, ImageBlock)

    PyAdjointIntegrator(const Properties &props) : AdjointIntegrator(props) {
        if constexpr (!dr::is_jit_v<Float>) {
            Log(Warn, "AdjointIntegrator Python implementations will have "
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
        py::function render_override = py::get_override(this, "render");

        if (render_override)
            return render_override(scene, sensor, seed, spp, develop, evaluate).template cast<TensorXf>();
        else
            return AdjointIntegrator::render(scene, sensor, seed, spp, develop, evaluate);
    }

    void sample(const Scene *scene, const Sensor *sensor, Sampler *sampler,
                ImageBlock *block, ScalarFloat sample_scale) const override {
        py::gil_scoped_acquire gil;
        py::function sample_override = py::get_override(this, "sample");

        if (sample_override)
            sample_override(scene, sensor, sampler, block, sample_scale);
        else
            Throw("AdjointIntegrator doesn't overload the method \"sample\"");
    }

    std::vector<std::string> aov_names() const override {
        PYBIND11_OVERRIDE(std::vector<std::string>, AdjointIntegrator, aov_names, );
    }

    std::string to_string() const override {
        PYBIND11_OVERRIDE(std::string, AdjointIntegrator, to_string, );
    }
};

/**
 * \brief Abstract integrator that should **exclusively** be used to trampoline
 * Python AD integrators for primal renderings
 */
template <typename Float, typename Spectrum>
class CppADIntegrator
    : public SamplingIntegrator<Float, Spectrum> {
    protected:
    MI_IMPORT_BASE(SamplingIntegrator)

    CppADIntegrator(const Properties &props) : Base(props) {}
    virtual ~CppADIntegrator() {}

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(CppADIntegrator, SamplingIntegrator)
MI_INSTANTIATE_CLASS(CppADIntegrator)

MI_VARIANT class PyADIntegrator : public CppADIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Scene, Sensor, Sampler, Medium, Emitter, EmitterPtr, BSDF, BSDFPtr)
    using Base = CppADIntegrator<Float, Spectrum>;

    PyADIntegrator(const Properties &props) : Base(props) {
        if constexpr (!dr::is_jit_v<Float>) {
            Log(Warn, "ADIntegrator Python implementations will have "
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
        PYBIND11_OVERRIDE(TensorXf, Base, render, scene, sensor, seed, spp, develop, evaluate);
    }

    TensorXf render_forward(Scene* scene,
                            void* params,
                            Sensor *sensor,
                            uint32_t seed = 0,
                            uint32_t spp = 0) override {
        py::gil_scoped_acquire gil;

        py::function func = py::get_override(this, "render_forward");
        if (func) {
            auto o = func(scene, *((py::object *) params), sensor, seed, spp);
            return o.template cast<TensorXf>();
        }

        return Base::render_forward(scene, params, sensor, seed, spp);
    }

    void render_backward(Scene* scene,
                         void* params,
                         const TensorXf& grad_in,
                         Sensor* sensor,
                         uint32_t seed = 0,
                         uint32_t spp = 0) override {
        py::gil_scoped_acquire gil;

        py::function func = py::get_override(this, "render_backward");
        if (func) {
            func(scene, *((py::object *) params), grad_in, sensor, seed, spp);
        }

        Base::render_backward(scene, params, grad_in, sensor, seed, spp);
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const RayDifferential3f &ray,
                                     const Medium * /* unused */,
                                     Float *aovs,
                                     Mask active) const override {
        py::gil_scoped_acquire gil;

        py::function sample_override = py::get_override(this, "sample");
        if (sample_override) {
            using PyReturn = std::tuple<Spectrum, Mask, std::vector<Float>, py::object>;
            py::kwargs kwargs = py::dict(
                "mode"_a=drjit::ADMode::Primal,
                "scene"_a=scene,
                "sampler"_a=sampler,
                "ray"_a=ray,
                "depth"_a=0,
                "δL"_a=py::none(),
                "δaovs"_a=py::none(),
                "state_in"_a=py::none(),
                "active"_a=active
            );
            // Third output is the ADIntegrator's state, which we ignore here
            auto [spec, mask, aovs_, _] =
                sample_override(**kwargs).template cast<PyReturn>();
            std::copy(aovs_.begin(), aovs_.end(), aovs);
            return { spec, mask };
        } else {
            Throw("ADIntegrator doesn't overload the method \"sample\"");
        }
    }

    std::vector<std::string> aov_names() const override {
        PYBIND11_OVERRIDE(std::vector<std::string>, Base, aov_names, );
    }

    std::string to_string() const override {
        PYBIND11_OVERRIDE(std::string, Base, to_string, );
    }

    using Base::m_hide_emitters;
};

MI_PY_EXPORT(Integrator) {
    MI_PY_IMPORT_TYPES()
    using PySamplingIntegrator = PySamplingIntegrator<Float, Spectrum>;
    using PyAdjointIntegrator = PyAdjointIntegrator<Float, Spectrum>;
    using CppADIntegrator = CppADIntegrator<Float, Spectrum>;
    using PyADIntegrator = PyADIntegrator<Float, Spectrum>;

    MI_PY_CLASS(Integrator, Object)
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

    MI_PY_TRAMPOLINE_CLASS(PySamplingIntegrator, SamplingIntegrator, Integrator)
        .def(py::init<const Properties &>())
        .def(
            "sample",
            [](const SamplingIntegrator *integrator, const Scene *scene,
               Sampler *sampler, const RayDifferential3f &ray,
               const Medium *medium, Mask active) {
                py::gil_scoped_release release;
                std::vector<Float> aovs(integrator->aov_names().size(), 0.f);
                auto [spec, mask] = integrator->sample(
                    scene, sampler, ray, medium, aovs.data(), active);
                return std::make_tuple(spec, mask, aovs);
            },
            "scene"_a, "sampler"_a, "ray"_a, "medium"_a = nullptr,
            "active"_a = true, D(SamplingIntegrator, sample))
        .def(
            "render_forward",
            [](SamplingIntegrator *integrator, Scene *scene, py::object* params,
                Sensor* sensor, uint32_t seed, uint32_t spp) {
                py::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_forward(scene, params, sensor, seed, spp);
            },
            "scene"_a, "params"_a, "sensor"_a, "seed"_a = 0, "spp"_a = 0)
        .def(
            "render_forward",
            [](SamplingIntegrator *integrator, Scene *scene, py::object* params,
                uint32_t sensor, uint32_t seed, uint32_t spp) {
                py::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_forward(scene, params, sensor, seed, spp);
            },
            "scene"_a, "params"_a, "sensor"_a = 0, "seed"_a = 0, "spp"_a = 0)
        .def(
            "render_backward",
            [](SamplingIntegrator *integrator, Scene *scene, py::object* params,
                const TensorXf& grad_in, Sensor* sensor, uint32_t seed,
                uint32_t spp) {
                py::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_backward(scene, params, grad_in,
                                                   sensor, seed, spp);
            },
            "scene"_a, "params"_a, "grad_in"_a, "sensor"_a, "seed"_a = 0,
            "spp"_a = 0)
        .def(
            "render_backward",
            [](SamplingIntegrator *integrator, Scene *scene, py::object* params,
                const TensorXf& grad_in, uint32_t sensor, uint32_t seed,
                uint32_t spp) {
                py::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_backward(scene, params, grad_in,
                                                   sensor, seed, spp);
            },
            "scene"_a, "params"_a, "grad_in"_a, "sensor"_a = 0, "seed"_a = 0,
            "spp"_a = 0)
        .def_readwrite("hide_emitters", &PySamplingIntegrator::m_hide_emitters);

    MI_PY_REGISTER_OBJECT("register_integrator", Integrator)

    MI_PY_CLASS(MonteCarloIntegrator, SamplingIntegrator);

    py::class_<CppADIntegrator, SamplingIntegrator, ref<CppADIntegrator>,
               PyADIntegrator>(m, "CppADIntegrator")
        .def(py::init<const Properties &>());

    MI_PY_TRAMPOLINE_CLASS(PyAdjointIntegrator, AdjointIntegrator, Integrator)
        .def(py::init<const Properties &>())
        .def(
            "render_forward",
            [](AdjointIntegrator *integrator, Scene *scene, py::object* params,
                Sensor* sensor, uint32_t seed, uint32_t spp) {
                py::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_forward(scene, params, sensor, seed, spp);
            },
            "scene"_a, "params"_a, "sensor"_a, "seed"_a = 0, "spp"_a = 0)
        .def(
            "render_forward",
            [](AdjointIntegrator *integrator, Scene *scene, py::object* params,
                uint32_t sensor, uint32_t seed, uint32_t spp) {
                py::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_forward(scene, params, sensor, seed, spp);
            },
            "scene"_a, "params"_a, "sensor"_a = 0, "seed"_a = 0, "spp"_a = 0)
        .def(
            "render_backward",
            [](AdjointIntegrator *integrator, Scene *scene, py::object* params,
                const TensorXf& grad_in, Sensor* sensor, uint32_t seed,
                uint32_t spp) {
                py::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_backward(scene, params, grad_in,
                                                   sensor, seed, spp);
            },
            "scene"_a, "params"_a, "grad_in"_a, "sensor"_a, "seed"_a = 0,
            "spp"_a = 0)
        .def(
            "render_backward",
            [](AdjointIntegrator *integrator, Scene *scene, py::object* params,
                const TensorXf& grad_in, uint32_t sensor, uint32_t seed,
                uint32_t spp) {
                py::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_backward(scene, params, grad_in,
                                                   sensor, seed, spp);
            },
            "scene"_a, "params"_a, "grad_in"_a, "sensor"_a = 0, "seed"_a = 0,
            "spp"_a = 0)
        .def_method(AdjointIntegrator, sample, "scene"_a, "sensor"_a,
                    "sampler"_a, "block"_a, "sample_scale"_a);
}
