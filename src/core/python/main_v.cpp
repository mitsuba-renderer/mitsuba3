#include <mitsuba/core/vector.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/python/python.h>

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
MI_PY_DECLARE(AnimatedTransform);
MI_PY_DECLARE(vector);
MI_PY_DECLARE(warp);
MI_PY_DECLARE(xml);
MI_PY_DECLARE(quad);

#define MODULE_NAME MI_MODULE_NAME(core, MI_VARIANT_NAME)

using Caster = py::object(*)(mitsuba::Object *);
Caster cast_object = nullptr;

PYBIND11_MODULE(MODULE_NAME, m) {
    MI_PY_IMPORT_TYPES()

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

    MI_PY_IMPORT(DrJit);

    m.attr("float_dtype") = std::is_same_v<ScalarFloat, float> ? "f" : "d";
    m.attr("is_monochromatic") = is_monochromatic_v<Spectrum>;
    m.attr("is_rgb") = is_rgb_v<Spectrum>;
    m.attr("is_spectral") = is_spectral_v<Spectrum>;
    m.attr("is_polarized") = is_polarized_v<Spectrum>;

    color_management_static_initialization(dr::is_cuda_array_v<Float>,
                                           dr::is_llvm_array_v<Float>);

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
    MI_PY_IMPORT(AnimatedTransform);
    MI_PY_IMPORT(Hierarchical2D);
    MI_PY_IMPORT(Marginal2D);
    MI_PY_IMPORT(vector);
    MI_PY_IMPORT_SUBMODULE(quad);
    MI_PY_IMPORT_SUBMODULE(warp);
    MI_PY_IMPORT(xml);

    py::object core_ext = py::module::import("mitsuba.core_ext");
    cast_object = (Caster) (void *)((py::capsule) core_ext.attr("cast_object"));

    // Change module name back to correct value
    m.attr("__name__") = "mitsuba." DRJIT_TOSTRING(MODULE_NAME);
}

#undef CHANGE_SUBMODULE_NAME
#undef CHANGE_BACK_SUBMODULE_NAME
