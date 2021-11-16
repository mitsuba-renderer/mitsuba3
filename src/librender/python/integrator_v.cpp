#include <mitsuba/core/properties.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/scatteringintegrator.h>

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
    MTS_IMPORT_TYPES(SamplingIntegrator, Scene, Sensor, Sampler, Medium, Emitter, EmitterPtr, BSDF, BSDFPtr)

    PySamplingIntegrator(const Properties &props) : SamplingIntegrator(props) {
        if constexpr (!ek::is_jit_array_v<Float>) {
            Log(Warn, "Derived SamplingIntegrator defined in Python will have "
                      "terrible performance in scalar_* modes. It is strongly "
                      "recommended to switch to a cuda_* or llvm_* mode");
        }
    }

    TensorXf render(Scene *scene,
                    uint32_t seed,
                    Sensor *sensor,
                    bool develop_film) override {
        py::gil_scoped_acquire gil;
        py::function overload = py::get_overload(this, "render");

        if (overload) {
            return overload(scene, seed, sensor, develop_film).template cast<TensorXf>();
        } else {
            return SamplingIntegrator::render(scene, seed, sensor, develop_film);
        }
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const RayDifferential3f &ray,
                                     const Medium *medium,
                                     Float *aovs,  // this is also implicitlt returned
                                     Mask active) const override {
        py::gil_scoped_acquire gil;
        py::function overload = py::get_overload(this, "sample");  // gives a python function

        if (overload) {
            using PyReturn = std::tuple<Spectrum, Mask, std::vector<Float>>;  // return type
            auto [spec, mask, aovs_]
                = overload(scene, sampler, ray, medium, active).template cast<PyReturn>();  // call it if exist

            std::copy(aovs_.begin(), aovs_.end(), aovs);
            return { spec, mask };
        } else {
            Throw("SamplingIntegrator doesn't overload the method \"sample\"");
        }
    }

    std::vector<std::string> aov_names() const override {
        PYBIND11_OVERRIDE(std::vector<std::string>, SamplingIntegrator, aov_names, );  // auto get overload and call it
    }

    std::string to_string() const override {
        PYBIND11_OVERRIDE(std::string, SamplingIntegrator, to_string, );
    }
};

/// Tramoline class for ScatteringIntegrator
MTS_VARIANT class PyScatteringIntegrator : public ScatteringIntegrator<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(ScatteringIntegrator, Scene, Sensor, Sampler, ImageBlock, Medium, Emitter, EmitterPtr, BSDF, BSDFPtr)

    PyScatteringIntegrator(const Properties &props) : ScatteringIntegrator(props) {}

    TensorXf render(Scene *scene,
                    uint32_t seed,
                    Sensor *sensor,
                    bool develop_film) override {
        py::gil_scoped_acquire gil;
        py::function overload = py::get_overload(this, "render");

        if (overload) {
            return overload(scene, seed, sensor, develop_film).template cast<TensorXf>();
        } else {
            return ScatteringIntegrator::render(scene, seed, sensor, develop_film);
        }
    }

    void sample(const Scene *scene,
                const Sensor *sensor,
                Sampler *sampler,
                ImageBlock *block,
                Mask active) const override {
        py::gil_scoped_acquire gil;
        py::function overload = py::get_overload(this, "sample");  // gives a python function

        if (overload) {
            overload(scene, sensor, sampler, block, active);  // call it if exist
        } else {
            Throw("ScatteringIntegrator doesn't overload the method \"sample\"");
        }
    }

    std::string to_string() const override {
        PYBIND11_OVERRIDE(std::string, ScatteringIntegrator, to_string, );
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////

MTS_PY_EXPORT(Integrator) {
    MTS_PY_IMPORT_TYPES()
    using PySamplingIntegrator = PySamplingIntegrator<Float, Spectrum>;

    MTS_PY_CLASS(Integrator, Object)  // this creates python interface
        .def(
            "render",
            [&](Integrator *integrator,
                Scene *scene,
                uint32_t seed,
                Sensor *sensor,
                bool develop_film,
                uint32_t spp) {
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
                if (spp > 0)
                    sensor->sampler()->set_sample_count(spp);
                auto res = integrator->render(scene, seed, sensor, develop_film);

#if MTS_HANDLE_SIGINT
                // Restore previous signal handler
                signal(SIGINT, sigint_handler_prev);
#endif

                return res;
            },
            D(Integrator, render), "scene"_a, "seed"_a, "sensor"_a,
            "develop_film"_a = true, "spp"_a = 0)

        .def("render",
            [&](Integrator *integrator,
                Scene *scene,
                uint32_t seed,
                uint32_t sensor_idx,
                bool develop_film,
                uint32_t spp) {
                if (sensor_idx >= scene->sensors().size())
                    Throw("Out-of-bound sensor index: %i", sensor_idx);
                Sensor *sensor = scene->sensors()[sensor_idx].get();
                return py::cast(integrator).attr("render")(scene, seed, sensor, develop_film, spp);
            },
            D(Integrator, render), "scene"_a, "seed"_a, "sensor_index"_a = 0,
            "develop_film"_a = true, "spp"_a = 0)
        .def_method(Integrator, cancel)
        .def_method(Integrator, should_stop)
        .def_method(Integrator, aov_names);

        //////////////////////////////////// above: base class integrator

    auto sampling_integrator =
        py::class_<SamplingIntegrator, PySamplingIntegrator, Integrator/*base class*/,
                    ref<SamplingIntegrator>>(m, "SamplingIntegrator", D(SamplingIntegrator))
            .def(py::init<const Properties&>());

    sampling_integrator.def(  // python interface
        // functions the base class doesnt have
        "sample",
        [](const SamplingIntegrator *integrator, const Scene *scene, Sampler *sampler,
           const RayDifferential3f &ray, const Medium *medium, Mask active) {
            py::gil_scoped_release release;
            std::vector<Float> aovs(integrator->aov_names().size(), 0.f);  // aov special case
            auto [spec, mask] = integrator->sample(scene, sampler, ray, medium, aovs.data(), active);
            return std::make_tuple(spec, mask, aovs);
        },
        "scene"_a, "sampler"_a, "ray"_a, "medium"_a = nullptr, "active"_a = true,
        D(SamplingIntegrator, sample));

    MTS_PY_REGISTER_OBJECT("register_integrator", Integrator)  // work same way

    MTS_PY_CLASS(MonteCarloIntegrator, SamplingIntegrator);

    using PyScatteringIntegrator = PyScatteringIntegrator<Float, Spectrum>;
    auto scattering_integrator =
        py::class_<ScatteringIntegrator, PyScatteringIntegrator, Integrator/*base class*/,
                    ref<ScatteringIntegrator>>(m, "ScatteringIntegrator", D(ScatteringIntegrator))
            .def(py::init<const Properties&>());
    scattering_integrator
        // .def(
        //     "sample",
        //     [](const ScatteringIntegrator *integrator, const Scene *scene, const Sensor *sensor,
        //        Sampler *sampler, ImageBlock *block, Mask active) {
        //         py::gil_scoped_release release;
        //         integrator->sample(scene, sensor, sampler, block, active);
        //     },
        //     "scene"_a, "sensor"_a, "sampler"_a, "block"_a, "active"_a = true,
        //     D(ScatteringIntegrator, sample)
        // )
        .def_method(ScatteringIntegrator, sample, "scene"_a, "sensor"_a,
                    "sampler"_a, "block"_a, "active"_a = true);  // can be re-written with .def()
}
