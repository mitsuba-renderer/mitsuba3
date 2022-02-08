#include <mitsuba/core/vector.h>
#include <mitsuba/core/spectrum.h>

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
    PY_TRY_CAST(AdjointIntegrator);
    PY_TRY_CAST(Integrator);

    PY_TRY_CAST(Sampler);

    PY_TRY_CAST(PhaseFunction);
    PY_TRY_CAST(Medium);

    return py::object();
}

// core
MTS_PY_DECLARE(DrJit);
MTS_PY_DECLARE(Object);
MTS_PY_DECLARE(BoundingBox);
MTS_PY_DECLARE(BoundingSphere);
MTS_PY_DECLARE(Frame);
MTS_PY_DECLARE(Ray);
MTS_PY_DECLARE(DiscreteDistribution);
MTS_PY_DECLARE(DiscreteDistribution2D);
MTS_PY_DECLARE(ContinuousDistribution);
MTS_PY_DECLARE(IrregularContinuousDistribution);
MTS_PY_DECLARE(Hierarchical2D);
MTS_PY_DECLARE(Marginal2D);
MTS_PY_DECLARE(math);
MTS_PY_DECLARE(qmc);
MTS_PY_DECLARE(Properties);
MTS_PY_DECLARE(rfilter);
MTS_PY_DECLARE(sample_tea);
MTS_PY_DECLARE(spline);
MTS_PY_DECLARE(Spectrum);
MTS_PY_DECLARE(Transform);
MTS_PY_DECLARE(AnimatedTransform);
MTS_PY_DECLARE(vector);
MTS_PY_DECLARE(warp);
MTS_PY_DECLARE(xml);
MTS_PY_DECLARE(quad);

// render
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
MTS_PY_DECLARE(VolumeGrid);

#define MODULE_NAME MTS_MODULE_NAME(mitsuba, MTS_VARIANT_NAME)

using Caster = py::object(*)(mitsuba::Object *);
Caster cast_object = nullptr;

PYBIND11_MODULE(MODULE_NAME, m) {
    // Temporarily change the module name (for pydoc)
    m.attr("__name__") = "mitsuba";

    MTS_PY_IMPORT_TYPES()

    // Create sub-modules
    py::module math    = create_submodule(m, "math"),
               spline  = create_submodule(m, "spline"),
               warp    = create_submodule(m, "warp"),
               quad    = create_submodule(m, "quad"),
               mueller = create_submodule(m, "mueller");

    math.doc()    = "Mathematical routines, special functions, etc.";
    spline.doc()  = "Functions for evaluating and sampling Catmull-Rom splines";
    warp.doc()    = "Common warping techniques that map from the unit square to other "
                    "domains, such as spheres, hemispheres, etc.";
    quad.doc()    = "Functions for numerical quadrature";
    mueller.doc() = "Routines to manipulate Mueller matrices for polarized rendering.";

    MTS_PY_IMPORT(DrJit);

    m.attr("float_dtype") = std::is_same_v<ScalarFloat, float> ? "f" : "d";
    m.attr("is_monochromatic") = is_monochromatic_v<Spectrum>;
    m.attr("is_rgb") = is_rgb_v<Spectrum>;
    m.attr("is_spectral") = is_spectral_v<Spectrum>;
    m.attr("is_polarized") = is_polarized_v<Spectrum>;

    color_management_static_initialization(dr::is_cuda_array_v<Float>,
                                           dr::is_llvm_array_v<Float>);
    Scene::static_accel_initialization();

    MTS_PY_IMPORT(Object);
    MTS_PY_IMPORT(Ray);
    MTS_PY_IMPORT(BoundingBox);
    MTS_PY_IMPORT(BoundingSphere);
    MTS_PY_IMPORT(Frame);
    MTS_PY_IMPORT(DiscreteDistribution);
    MTS_PY_IMPORT(DiscreteDistribution2D);
    MTS_PY_IMPORT(ContinuousDistribution);
    MTS_PY_IMPORT(IrregularContinuousDistribution);
    MTS_PY_IMPORT_SUBMODULE(math);
    MTS_PY_IMPORT(qmc);
    MTS_PY_IMPORT(Properties);
    MTS_PY_IMPORT(rfilter);
    MTS_PY_IMPORT(sample_tea);
    MTS_PY_IMPORT_SUBMODULE(spline);
    MTS_PY_IMPORT(Spectrum);
    MTS_PY_IMPORT(Transform);
    MTS_PY_IMPORT(AnimatedTransform);
    MTS_PY_IMPORT(Hierarchical2D);
    MTS_PY_IMPORT(Marginal2D);
    MTS_PY_IMPORT(vector);
    MTS_PY_IMPORT_SUBMODULE(quad);
    MTS_PY_IMPORT_SUBMODULE(warp);
    MTS_PY_IMPORT(xml);

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
    MTS_PY_IMPORT(VolumeGrid);

    py::object mitsuba_ext = py::module::import("mitsuba_ext");
    cast_object = (Caster) (void *)((py::capsule) mitsuba_ext.attr("cast_object"));

    /// Register the variant-specific caster with the 'core_ext' module
    auto casters = (std::vector<void *> *) (py::capsule)(mitsuba_ext.attr("casters"));
    casters->push_back((void *) caster);

    /* Increase the reference count of the `mitsuba.core.Object` type to make
       sure libcore doesn't get destroyed before librender */
    py::handle mts_object_type = mitsuba_ext.attr("Object");
    mts_object_type.inc_ref();

    /* Register a cleanup callback function that is invoked when
       the 'mitsuba::Scene' Python type is garbage collected */
    py::cpp_function cleanup_callback(
        [mts_object_type](py::handle weakref) {
            color_management_static_shutdown();
            Scene::static_accel_shutdown();

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

#undef CHANGE_SUBMODULE_NAME
#undef CHANGE_BACK_SUBMODULE_NAME
