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

// core
MI_PY_DECLARE(DrJit);
MI_PY_DECLARE(Object);
MI_PY_DECLARE(BoundingBox);
MI_PY_DECLARE(BoundingSphere);
MI_PY_DECLARE(Frame);
MI_PY_DECLARE(Ray);
MI_PY_DECLARE(DiscreteDistribution);
MI_PY_DECLARE(DiscreteDistribution2D);
MI_PY_DECLARE(ContinuousDistribution);
MI_PY_DECLARE(IrregularContinuousDistribution);
MI_PY_DECLARE(Hierarchical2D);
MI_PY_DECLARE(Marginal2D);
MI_PY_DECLARE(math);
MI_PY_DECLARE(qmc);
MI_PY_DECLARE(Properties);
MI_PY_DECLARE(rfilter);
MI_PY_DECLARE(sample_tea);
MI_PY_DECLARE(spline);
MI_PY_DECLARE(Spectrum);
MI_PY_DECLARE(Transform);
// MI_PY_DECLARE(AnimatedTransform);
MI_PY_DECLARE(vector);
MI_PY_DECLARE(warp);
MI_PY_DECLARE(xml);
MI_PY_DECLARE(quad);

// render
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
MI_PY_DECLARE(MicroflakeDistribution);
#if defined(MI_ENABLE_CUDA)
MI_PY_DECLARE(OptixDenoiser);
#endif // defined(MI_ENABLE_CUDA)
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

#define MODULE_NAME MI_MODULE_NAME(mitsuba, MI_VARIANT_NAME)

using Caster = py::object(*)(mitsuba::Object *);
Caster cast_object = nullptr;

PYBIND11_MODULE(MODULE_NAME, m) {
    // Temporarily change the module name (for pydoc)
    m.attr("__name__") = "mitsuba." DRJIT_TOSTRING(MI_VARIANT_NAME);

    MI_PY_IMPORT_TYPES()

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

    MI_PY_IMPORT(DrJit);

    m.attr("float_dtype") = std::is_same_v<ScalarFloat, float> ? "f" : "d";
    m.attr("is_monochromatic") = is_monochromatic_v<Spectrum>;
    m.attr("is_rgb") = is_rgb_v<Spectrum>;
    m.attr("is_spectral") = is_spectral_v<Spectrum>;
    m.attr("is_polarized") = is_polarized_v<Spectrum>;

    color_management_static_initialization(dr::is_cuda_v<Float>,
                                           dr::is_llvm_v<Float>);
    Scene::static_accel_initialization();

    MI_PY_IMPORT(Object);
    MI_PY_IMPORT(Ray);
    MI_PY_IMPORT(BoundingBox);
    MI_PY_IMPORT(BoundingSphere);
    MI_PY_IMPORT(Frame);
    MI_PY_IMPORT(DiscreteDistribution);
    MI_PY_IMPORT(DiscreteDistribution2D);
    MI_PY_IMPORT(ContinuousDistribution);
    MI_PY_IMPORT(IrregularContinuousDistribution);
    MI_PY_IMPORT_SUBMODULE(math);
    MI_PY_IMPORT(qmc);
    MI_PY_IMPORT(Properties);
    MI_PY_IMPORT(rfilter);
    MI_PY_IMPORT(sample_tea);
    MI_PY_IMPORT_SUBMODULE(spline);
    MI_PY_IMPORT(Spectrum);
    MI_PY_IMPORT(Transform);
    // MI_PY_IMPORT(AnimatedTransform);
    MI_PY_IMPORT(Hierarchical2D);
    MI_PY_IMPORT(Marginal2D);
    MI_PY_IMPORT(vector);
    MI_PY_IMPORT_SUBMODULE(quad);
    MI_PY_IMPORT_SUBMODULE(warp);
    MI_PY_IMPORT(xml);

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
    MI_PY_IMPORT(MicroflakeDistribution);
#if defined(MI_ENABLE_CUDA)
    MI_PY_IMPORT(OptixDenoiser);
#endif // defined(MI_ENABLE_CUDA)
    MI_PY_IMPORT(PhaseFunction);
    MI_PY_IMPORT(Sampler);
    MI_PY_IMPORT(Sensor);
    MI_PY_IMPORT(ShapeKDTree);
    MI_PY_IMPORT(srgb);
    MI_PY_IMPORT(Texture);
    MI_PY_IMPORT(Volume);
    MI_PY_IMPORT(VolumeGrid);

    py::object mitsuba_ext = py::module::import("mitsuba.mitsuba_ext");
    cast_object = (Caster) (void *)((py::capsule) mitsuba_ext.attr("cast_object"));

    /// Register the variant-specific caster with the 'core_ext' module
    auto casters = (std::vector<void *> *) (py::capsule)(mitsuba_ext.attr("casters"));
    casters->push_back((void *) caster);

    /* Register a cleanup callback function that is invoked when
       the 'mitsuba::Scene' Python type is garbage collected */
    py::cpp_function cleanup_callback(
        [](py::handle weakref) {
            color_management_static_shutdown();
            Scene::static_accel_shutdown();

            /* The DrJit python module is responsible for cleaning up the
               JIT state, so jit_shutdown() shouldn't be called here. */
            weakref.dec_ref();
        }
    );

    (void) py::weakref(m.attr("Scene"), cleanup_callback).release();

    // Change module name back to correct value
    m.attr("__name__") = "mitsuba." DRJIT_TOSTRING(MODULE_NAME);
}

#undef CHANGE_SUBMODULE_NAME
#undef CHANGE_BACK_SUBMODULE_NAME
