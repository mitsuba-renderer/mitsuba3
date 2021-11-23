#include <mitsuba/core/vector.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/python/python.h>

MTS_PY_DECLARE(Enoki);
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

#define MODULE_NAME MTS_MODULE_NAME(core, MTS_VARIANT_NAME)

using Caster = py::object(*)(mitsuba::Object *);
Caster cast_object = nullptr;

PYBIND11_MODULE(MODULE_NAME, m) {
    MTS_PY_IMPORT_TYPES()

    // Temporarily change the module name (for pydoc)
    m.attr("__name__") = "mitsuba.core";

    // Create sub-modules
    py::module math   = create_submodule(m, "math"),
               spline = create_submodule(m, "spline"),
               warp   = create_submodule(m, "warp"),
               quad   = create_submodule(m, "quad");

    math.doc()   = "Mathematical routines, special functions, etc.";
    spline.doc() = "Functions for evaluating and sampling Catmull-Rom splines";
    warp.doc()   = "Common warping techniques that map from the unit square to other "
                   "domains, such as spheres, hemispheres, etc.";
    quad.doc()   = "Functions for numerical quadrature";

    MTS_PY_IMPORT(Enoki);

    m.attr("float_dtype") = std::is_same_v<ScalarFloat, float> ? "f" : "d";
    m.attr("is_monochromatic") = is_monochromatic_v<Spectrum>;
    m.attr("is_rgb") = is_rgb_v<Spectrum>;
    m.attr("is_spectral") = is_spectral_v<Spectrum>;
    m.attr("is_polarized") = is_polarized_v<Spectrum>;

    color_management_static_initialization(ek::is_cuda_array_v<Float>,
                                           ek::is_llvm_array_v<Float>);

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

    py::object core_ext = py::module::import("mitsuba.core_ext");
    cast_object = (Caster) (void *)((py::capsule) core_ext.attr("cast_object"));

    // Change module name back to correct value
    m.attr("__name__") = "mitsuba." ENOKI_TOSTRING(MODULE_NAME);
}

#undef CHANGE_SUBMODULE_NAME
#undef CHANGE_BACK_SUBMODULE_NAME
