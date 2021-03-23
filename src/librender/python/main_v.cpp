#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/python/python.h>

#define MODULE_NAME MTS_MODULE_NAME(render, MTS_VARIANT_NAME)

#define PY_TRY_CAST(Type)                                         \
    if (auto tmp = dynamic_cast<Type *>(o); tmp)                  \
        return py::cast(tmp);

/// Helper routine to cast Mitsuba plugins to their underlying interfaces
static py::object caster(Object *o) {
    MTS_PY_IMPORT_TYPES()

    // Try casting, starting from the most precise types
    PY_TRY_CAST(Scene);
    PY_TRY_CAST(Mesh);
    PY_TRY_CAST(Shape);
    PY_TRY_CAST(Texture);
    PY_TRY_CAST(Volume);
    PY_TRY_CAST(ReconstructionFilter);

    PY_TRY_CAST(ProjectiveCamera);
    PY_TRY_CAST(Sensor);

    PY_TRY_CAST(Emitter);
    PY_TRY_CAST(Endpoint);

    PY_TRY_CAST(BSDF);
    PY_TRY_CAST(Film);

    PY_TRY_CAST(MonteCarloIntegrator);
    PY_TRY_CAST(SamplingIntegrator);
    PY_TRY_CAST(Integrator);

    PY_TRY_CAST(Sampler);

    PY_TRY_CAST(PhaseFunction);
    PY_TRY_CAST(Medium);

    return py::object();
}

MTS_PY_DECLARE(BSDFSample);
MTS_PY_DECLARE(BSDF);
MTS_PY_DECLARE(Emitter);
MTS_PY_DECLARE(Endpoint);
MTS_PY_DECLARE(Film);
MTS_PY_DECLARE(fresnel);
MTS_PY_DECLARE(ImageBlock);
MTS_PY_DECLARE(Integrator);
MTS_PY_DECLARE(Interaction);
MTS_PY_DECLARE(SurfaceInteraction);
MTS_PY_DECLARE(MediumInteraction);
MTS_PY_DECLARE(PreliminaryIntersection);
MTS_PY_DECLARE(Medium);
MTS_PY_DECLARE(mueller);
MTS_PY_DECLARE(MicrofacetDistribution);
MTS_PY_DECLARE(PositionSample);
MTS_PY_DECLARE(PhaseFunction);
MTS_PY_DECLARE(DirectionSample);
MTS_PY_DECLARE(Sampler);
MTS_PY_DECLARE(Scene);
MTS_PY_DECLARE(Sensor);
MTS_PY_DECLARE(Shape);
MTS_PY_DECLARE(ShapeKDTree);
MTS_PY_DECLARE(srgb);
MTS_PY_DECLARE(Texture);
MTS_PY_DECLARE(Volume);

PYBIND11_MODULE(MODULE_NAME, m) {
    // Temporarily change the module name (for pydoc)
    m.attr("__name__") = "mitsuba.render";

    using Float = MTS_VARIANT_FLOAT;

    // Create sub-modules
    py::module mueller = create_submodule(m, "mueller");
    mueller.doc() = "Routines to manipulate Mueller matrices for polarized rendering.";

    MTS_PY_IMPORT(Scene);
    MTS_PY_IMPORT(Shape);
    MTS_PY_IMPORT(Medium);
    MTS_PY_IMPORT(Endpoint);
    MTS_PY_IMPORT(Emitter);
    MTS_PY_IMPORT(Interaction);
    MTS_PY_IMPORT(SurfaceInteraction);
    MTS_PY_IMPORT(MediumInteraction);
    MTS_PY_IMPORT(PreliminaryIntersection);
    MTS_PY_IMPORT(PositionSample);
    MTS_PY_IMPORT(DirectionSample);
    MTS_PY_IMPORT(BSDFSample);
    MTS_PY_IMPORT(BSDF);
    MTS_PY_IMPORT(Film);
    MTS_PY_IMPORT(fresnel);
    MTS_PY_IMPORT(ImageBlock);
    MTS_PY_IMPORT(Integrator);
    MTS_PY_IMPORT_SUBMODULE(mueller);
    MTS_PY_IMPORT(MicrofacetDistribution);
    MTS_PY_IMPORT(PhaseFunction);
    MTS_PY_IMPORT(Sampler);
    MTS_PY_IMPORT(Sensor);
    MTS_PY_IMPORT(ShapeKDTree);
    MTS_PY_IMPORT(srgb);
    MTS_PY_IMPORT(Texture);
    MTS_PY_IMPORT(Volume);

    /// Register the variant-specific caster with the 'core_ext' module
    auto casters = (std::vector<void *> *) (py::capsule)(
        py::module::import("mitsuba.core_ext").attr("casters"));
    casters->push_back((void *) caster);

#if defined(MTS_ENABLE_CUDA) || defined(MTS_ENABLE_LLVM)
    if constexpr (ek::is_jit_array_v<Float>) {
        /* Register a cleanup callback function that is invoked when
           the 'mitsuba::Scene' Python type is garbage collected */
        py::cpp_function cleanup_callback(
            [](py::handle weakref) {
                cie_static_shutdown();
                /* The Enoki python module is responsible for cleaning up the
                   JIT state, so jit_shutdown() shouldn't be called here. */
                weakref.dec_ref();
            }
        );

        (void) py::weakref(m.attr("Scene"), cleanup_callback).release();
    }
#endif

    // Change module name back to correct value
    m.attr("__name__") = "mitsuba." ENOKI_TOSTRING(MODULE_NAME);
}
