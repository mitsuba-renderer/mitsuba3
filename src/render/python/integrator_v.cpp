#include <mitsuba/core/properties.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>
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
    NB_TRAMPOLINE(SamplingIntegrator, 6);

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
        NB_OVERRIDE(render, scene, sensor, seed, spp, develop, evaluate);
    }

    TensorXf render_forward(Scene* scene,
                            void* params,
                            Sensor *sensor,
                            uint32_t seed = 0,
                            uint32_t spp = 0) override {
        NB_OVERRIDE(render_forward, scene, params, sensor, seed, spp);
    }

    void render_backward(Scene* scene,
                         void* params,
                         const TensorXf& grad_in,
                         Sensor* sensor,
                         uint32_t seed = 0,
                         uint32_t spp = 0) override {
        NB_OVERRIDE(render_backward, scene, params, grad_in, sensor, seed, spp);
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const RayDifferential3f &ray,
                                     const Medium *medium,
                                     Float *aovs,
                                     Mask active) const override {
        using PyReturn = std::tuple<Spectrum, Mask, std::vector<Float>>;

        nanobind::detail::ticket nb_ticket(nb_trampoline, "sample", true);
        auto [spec, mask, aovs_] =
            nanobind::cast<PyReturn>(nb_trampoline.base().attr(nb_ticket.key)(
                scene, sampler, ray, medium, active));

        std::copy(aovs_.begin(), aovs_.end(), aovs);
        return { spec, mask };
    }

    std::vector<std::string> aov_names() const override {
        NB_OVERRIDE(aov_names);
    }

    std::string to_string() const override {
        NB_OVERRIDE(to_string);
    }
};

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyAdjointIntegrator : public AdjointIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(AdjointIntegrator, Scene, Sensor, Sampler, ImageBlock)
    NB_TRAMPOLINE(AdjointIntegrator, 4);

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
        NB_OVERRIDE(render, scene, sensor, seed, spp, develop, evaluate);
    }

    void sample(const Scene *scene, const Sensor *sensor, Sampler *sampler,
                ImageBlock *block, ScalarFloat sample_scale) const override {
        NB_OVERRIDE_PURE(sample, scene, sensor, sampler, block, sample_scale);
    }

    std::vector<std::string> aov_names() const override {
        NB_OVERRIDE(aov_names);
    }

    std::string to_string() const override {
        NB_OVERRIDE(to_string);
    }
};

/**
 * \brief Abstract integrator that should **exclusively** be used to trampoline
 * Python AD integrators for primal renderings
 */
template <typename Float, typename Spectrum>
class CppADIntegrator
    : public SamplingIntegrator<Float, Spectrum> {
public:
    ~CppADIntegrator() {}

protected:
    MI_IMPORT_BASE(SamplingIntegrator)

    CppADIntegrator(const Properties &props) : Base(props) {}

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(CppADIntegrator, SamplingIntegrator)

template class CppADIntegrator<MI_VARIANT_FLOAT, MI_VARIANT_SPECTRUM>;

MI_VARIANT class PyADIntegrator : public CppADIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Scene, Sensor, Sampler, Medium, Emitter, EmitterPtr, BSDF, BSDFPtr)
    using Base = CppADIntegrator<Float, Spectrum>;
    NB_TRAMPOLINE(Base, 6);

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
        NB_OVERRIDE(render, scene, sensor, seed, spp, develop, evaluate);
    }

    TensorXf render_forward(Scene* scene,
                            void* params,
                            Sensor *sensor,
                            uint32_t seed = 0,
                            uint32_t spp = 0) override {
        nanobind::detail::ticket nb_ticket(nb_trampoline, "render_forward", false);
        if (nb_ticket.key.is_valid())
            return nanobind::cast<TensorXf>(
                nb_trampoline.base().attr(nb_ticket.key)(
                    scene, *((nb::object *) params), sensor, seed, spp));
        else
            return Base::render_forward(scene, params, sensor, seed, spp);
    }

    void render_backward(Scene* scene,
                         void* params,
                         const TensorXf& grad_in,
                         Sensor* sensor,
                         uint32_t seed = 0,
                         uint32_t spp = 0) override {
        nanobind::detail::ticket nb_ticket(nb_trampoline, "render_backward", false);
        if (nb_ticket.key.is_valid())
            nanobind::cast<void>(nb_trampoline.base().attr(nb_ticket.key)(
                scene, *((nb::object *) params), grad_in, sensor, seed, spp));
        else
            Base::render_backward(scene, params, grad_in, sensor, seed, spp);
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const RayDifferential3f &ray,
                                     const Medium * /* unused */,
                                     Float *aovs,
                                     Mask active) const override {
        nanobind::detail::ticket nb_ticket(nb_trampoline, "sample", true);

        nb::dict kwargs;
        kwargs["keyword"] = "value";
        kwargs["mode"] = drjit::ADMode::Primal;
        kwargs["scene"] = scene;
        kwargs["sampler"] = sampler;
        kwargs["ray"] = ray;
        kwargs["depth"] = 0;
        kwargs["δL"] = nb::none();
        kwargs["δaovs"] = nb::none();
        kwargs["state_in"] = nb::none();
        kwargs["active"] = active;

        using PyReturn = std::tuple<Spectrum, Mask, std::vector<Float>, nb::object>;
        auto [spec, mask, aovs_, _] = nanobind::cast<PyReturn>(
            nb_trampoline.base().attr(nb_ticket.key)(**kwargs));

        std::copy(aovs_.begin(), aovs_.end(), aovs);

        return { spec, mask };
    }

    std::vector<std::string> aov_names() const override {
        NB_OVERRIDE(aov_names);
    }

    std::string to_string() const override {
        NB_OVERRIDE(to_string);
    }

    using Base::m_hide_emitters;
};

MI_PY_EXPORT(Integrator) {
    MI_PY_IMPORT_TYPES()
    using PySamplingIntegrator = PySamplingIntegrator<Float, Spectrum>;
    using PyAdjointIntegrator = PyAdjointIntegrator<Float, Spectrum>;
    using CppADIntegrator = CppADIntegrator<Float, Spectrum>;
    using PyADIntegrator = PyADIntegrator<Float, Spectrum>;
    using Properties = PropertiesV<Float>;

    MI_PY_CLASS(Integrator, Object)
        .def(
            "render",
            [&](Integrator *integrator, Scene *scene, Sensor *sensor,
                uint32_t seed, uint32_t spp, bool develop, bool evaluate) {
                nb::gil_scoped_release release;
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
                nb::gil_scoped_release release;
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
        .def(nb::init<const Properties &>())
        .def(
            "sample",
            [](const SamplingIntegrator *integrator, const Scene *scene,
               Sampler *sampler, const RayDifferential3f &ray,
               const Medium *medium, Mask active) {
                nb::gil_scoped_release release;
                std::vector<Float> aovs(integrator->aov_names().size(), 0.f);
                auto [spec, mask] = integrator->sample(
                    scene, sampler, ray, medium, aovs.data(), active);
                return std::make_tuple(spec, mask, aovs);
            },
            "scene"_a, "sampler"_a, "ray"_a, "medium"_a = nullptr,
            "active"_a = true, D(SamplingIntegrator, sample))
        .def(
            "render_forward",
            [](SamplingIntegrator *integrator, Scene *scene, nb::object* params,
                Sensor* sensor, uint32_t seed, uint32_t spp) {
                nb::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_forward(scene, params, sensor, seed, spp);
            },
            "scene"_a, "params"_a, "sensor"_a, "seed"_a = 0, "spp"_a = 0)
        .def(
            "render_forward",
            [](SamplingIntegrator *integrator, Scene *scene, nb::object* params,
                uint32_t sensor, uint32_t seed, uint32_t spp) {
                nb::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_forward(scene, params, sensor, seed, spp);
            },
            "scene"_a, "params"_a, "sensor"_a = 0, "seed"_a = 0, "spp"_a = 0)
        .def(
            "render_backward",
            [](SamplingIntegrator *integrator, Scene *scene, nb::object* params,
                const TensorXf& grad_in, Sensor* sensor, uint32_t seed,
                uint32_t spp) {
                nb::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_backward(scene, params, grad_in,
                                                   sensor, seed, spp);
            },
            "scene"_a, "params"_a, "grad_in"_a, "sensor"_a, "seed"_a = 0,
            "spp"_a = 0)
        .def(
            "render_backward",
            [](SamplingIntegrator *integrator, Scene *scene, nb::object* params,
                const TensorXf& grad_in, uint32_t sensor, uint32_t seed,
                uint32_t spp) {
                nb::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_backward(scene, params, grad_in,
                                                   sensor, seed, spp);
            },
            "scene"_a, "params"_a, "grad_in"_a, "sensor"_a = 0, "seed"_a = 0,
            "spp"_a = 0)
        .def_rw("hide_emitters", &PySamplingIntegrator::m_hide_emitters);

    MI_PY_REGISTER_OBJECT("register_integrator", Integrator)

    MI_PY_CLASS(MonteCarloIntegrator, SamplingIntegrator);

    nb::class_<CppADIntegrator, SamplingIntegrator, PyADIntegrator>(
        m, "CppADIntegrator")
        .def(nb::init<const Properties &>());

    MI_PY_TRAMPOLINE_CLASS(PyAdjointIntegrator, AdjointIntegrator, Integrator)
        .def(nb::init<const Properties &>())
        .def(
            "render_forward",
            [](AdjointIntegrator *integrator, Scene *scene, nb::object* params,
                Sensor* sensor, uint32_t seed, uint32_t spp) {
                nb::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_forward(scene, params, sensor, seed, spp);
            },
            "scene"_a, "params"_a, "sensor"_a, "seed"_a = 0, "spp"_a = 0)
        .def(
            "render_forward",
            [](AdjointIntegrator *integrator, Scene *scene, nb::object* params,
                uint32_t sensor, uint32_t seed, uint32_t spp) {
                nb::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_forward(scene, params, sensor, seed, spp);
            },
            "scene"_a, "params"_a, "sensor"_a = 0, "seed"_a = 0, "spp"_a = 0)
        .def(
            "render_backward",
            [](AdjointIntegrator *integrator, Scene *scene, nb::object* params,
                const TensorXf& grad_in, Sensor* sensor, uint32_t seed,
                uint32_t spp) {
                nb::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_backward(scene, params, grad_in,
                                                   sensor, seed, spp);
            },
            "scene"_a, "params"_a, "grad_in"_a, "sensor"_a, "seed"_a = 0,
            "spp"_a = 0)
        .def(
            "render_backward",
            [](AdjointIntegrator *integrator, Scene *scene, nb::object* params,
                const TensorXf& grad_in, uint32_t sensor, uint32_t seed,
                uint32_t spp) {
                nb::gil_scoped_release release;
                ScopedSignalHandler sh(integrator);
                return integrator->render_backward(scene, params, grad_in,
                                                   sensor, seed, spp);
            },
            "scene"_a, "params"_a, "grad_in"_a, "sensor"_a = 0, "seed"_a = 0,
            "spp"_a = 0)
        .def_method(AdjointIntegrator, sample, "scene"_a, "sensor"_a,
                    "sampler"_a, "block"_a, "sample_scale"_a);
}
