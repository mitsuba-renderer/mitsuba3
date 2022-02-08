#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/volume.h>

#include <mitsuba/python/python.h>

#define MODULE_NAME MI_MODULE_NAME(render, MI_VARIANT_NAME)

#define PY_TRY_CAST(Type)                                         \
    if (auto tmp = dynamic_cast<Type *>(o); tmp)                  \
        return py::cast(tmp);

/// Helper routine to cast Mitsuba plugins to their underlying interfaces
static py::object caster(Object *o) {
    MI_PY_IMPORT_TYPES()

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
    PY_TRY_CAST(AdjointIntegrator);
    PY_TRY_CAST(Integrator);

    PY_TRY_CAST(Sampler);

    PY_TRY_CAST(PhaseFunction);
    PY_TRY_CAST(Medium);

    return py::object();
}

MI_PY_DECLARE(BSDFSample);
MI_PY_DECLARE(BSDF);
MI_PY_DECLARE(Emitter);
MI_PY_DECLARE(Endpoint);
MI_PY_DECLARE(Film);
MI_PY_DECLARE(fresnel);
MI_PY_DECLARE(ImageBlock);
MI_PY_DECLARE(Integrator);
MI_PY_DECLARE(Interaction);
MI_PY_DECLARE(SurfaceInteraction);
MI_PY_DECLARE(MediumInteraction);
MI_PY_DECLARE(PreliminaryIntersection);
MI_PY_DECLARE(Medium);
MI_PY_DECLARE(mueller);
MI_PY_DECLARE(MicrofacetDistribution);
MI_PY_DECLARE(PositionSample);
MI_PY_DECLARE(PhaseFunction);
MI_PY_DECLARE(DirectionSample);
MI_PY_DECLARE(Sampler);
MI_PY_DECLARE(Scene);
MI_PY_DECLARE(Sensor);
MI_PY_DECLARE(Shape);
MI_PY_DECLARE(ShapeKDTree);
MI_PY_DECLARE(srgb);
MI_PY_DECLARE(Texture);
MI_PY_DECLARE(Volume);
MI_PY_DECLARE(VolumeGrid);

PYBIND11_MODULE(MODULE_NAME, m) {
    // Temporarily change the module name (for pydoc)
    m.attr("__name__") = "mitsuba.render";

    using Float = MI_VARIANT_FLOAT;
    using Spectrum = MI_VARIANT_SPECTRUM;

    Scene<Float, Spectrum>::static_accel_initialization();

    // Create sub-modules
    py::module mueller = create_submodule(m, "mueller");
    mueller.doc() = "Routines to manipulate Mueller matrices for polarized rendering.";

    MI_PY_IMPORT(Scene);
    MI_PY_IMPORT(Shape);
    MI_PY_IMPORT(Medium);
    MI_PY_IMPORT(Endpoint);
    MI_PY_IMPORT(Emitter);
    MI_PY_IMPORT(Interaction);
    MI_PY_IMPORT(SurfaceInteraction);
    MI_PY_IMPORT(MediumInteraction);
    MI_PY_IMPORT(PreliminaryIntersection);
    MI_PY_IMPORT(PositionSample);
    MI_PY_IMPORT(DirectionSample);
    MI_PY_IMPORT(BSDFSample);
    MI_PY_IMPORT(BSDF);
    MI_PY_IMPORT(Film);
    MI_PY_IMPORT(fresnel);
    MI_PY_IMPORT(ImageBlock);
    MI_PY_IMPORT(Integrator);
    MI_PY_IMPORT_SUBMODULE(mueller);
    MI_PY_IMPORT(MicrofacetDistribution);
    MI_PY_IMPORT(PhaseFunction);
    MI_PY_IMPORT(Sampler);
    MI_PY_IMPORT(Sensor);
    MI_PY_IMPORT(ShapeKDTree);
    MI_PY_IMPORT(srgb);
    MI_PY_IMPORT(Texture);
    MI_PY_IMPORT(Volume);
    MI_PY_IMPORT(VolumeGrid);

    auto mts_core = py::module::import("mitsuba.core_ext");

    /// Register the variant-specific caster with the 'core_ext' module
    auto casters = (std::vector<void *> *) (py::capsule)(mts_core.attr("casters"));
    casters->push_back((void *) caster);

    /* Increase the reference count of the `mitsuba.core.Object` type to make
        sure libcore doesn't get destroyed before librender */
    py::handle mts_object_type = mts_core.attr("Object");
    mts_object_type.inc_ref();

    /* Register a cleanup callback function that is invoked when
        the 'mitsuba::Scene' Python type is garbage collected */
    py::cpp_function cleanup_callback(
        [mts_object_type](py::handle weakref) {
            color_management_static_shutdown();
            Scene<Float, Spectrum>::static_accel_shutdown();

            /* The DrJit python module is responsible for cleaning up the
                JIT state, so jit_shutdown() shouldn't be called here. */
            weakref.dec_ref();

            /* Decrease the reference count of the `mitsuba.core.Object` type as
                the libcore can now be destroyed. Somehow the reference counter
                needs to be decremented twice for this to work properly. */
            mts_object_type.dec_ref();
            mts_object_type.dec_ref();
        }
    );

    (void) py::weakref(m.attr("Scene"), cleanup_callback).release();

    // Change module name back to correct value
    m.attr("__name__") = "mitsuba." DRJIT_TOSTRING(MODULE_NAME);
}
