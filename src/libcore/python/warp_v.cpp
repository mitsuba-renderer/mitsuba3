#include <mitsuba/python/python.h>
#include <mitsuba/core/warp.h>
#include <enoki/stl.h>

MTS_PY_EXPORT(warp) {
    MTS_PY_IMPORT_TYPES()

    // Create dedicated submodule
    auto warp = m.def_submodule("warp", "Common warping techniques that map from the unit square "
                                        "to other domains, such as spheres, hemispheres, etc.");

    warp.def(
        "square_to_uniform_disk",
        vectorize(warp::square_to_uniform_disk<Float>),
        "sample"_a, D(warp, square_to_uniform_disk));

    warp.def(
        "uniform_disk_to_square",
        vectorize(warp::uniform_disk_to_square<Float>),
        "p"_a, D(warp, uniform_disk_to_square));

    warp.def(
        "square_to_uniform_disk_pdf",
        vectorize(warp::square_to_uniform_disk_pdf<true, Float>),
        "p"_a, D(warp, square_to_uniform_disk_pdf));

    warp.def(
        "uniform_disk_to_square_concentric",
        vectorize(warp::uniform_disk_to_square_concentric<Float>),
        "p"_a, D(warp, uniform_disk_to_square_concentric));
    warp.def(
        "square_to_uniform_disk_concentric",
        vectorize(warp::square_to_uniform_disk_concentric<Float>),
        "sample"_a, D(warp, square_to_uniform_disk_concentric));

    warp.def(
        "square_to_uniform_square_concentric",
        vectorize(warp::square_to_uniform_square_concentric<Float>),
        "sample"_a, D(warp, square_to_uniform_square_concentric));

    warp.def(
        "square_to_uniform_disk_concentric_pdf",
        vectorize(warp::square_to_uniform_disk_concentric_pdf<true, Float>),
        "p"_a, D(warp, square_to_uniform_disk_concentric_pdf));

    warp.def(
        "square_to_uniform_triangle",
        vectorize(warp::square_to_uniform_triangle<Float>),
        "sample"_a, D(warp, square_to_uniform_triangle));

    warp.def(
        "uniform_triangle_to_square",
        vectorize(warp::uniform_triangle_to_square<Float>),
        "p"_a, D(warp, uniform_triangle_to_square));

    warp.def(
        "interval_to_tent",
        warp::interval_to_tent<Float>,
        "sample"_a, D(warp, interval_to_tent));

    warp.def(
        "tent_to_interval",
        warp::tent_to_interval<Float>,
        "value"_a, D(warp, tent_to_interval));

    warp.def(
        "interval_to_nonuniform_tent",
        warp::interval_to_nonuniform_tent<Float>,
        "a"_a, "b"_a, "c"_a, "d"_a, D(warp, interval_to_nonuniform_tent));

    warp.def(
        "square_to_tent",
        vectorize(warp::square_to_tent<Float>),
        "sample"_a, D(warp, square_to_tent));

    warp.def(
        "tent_to_square",
        vectorize(warp::tent_to_square<Float>),
        "value"_a, D(warp, tent_to_square));

    warp.def(
        "square_to_tent_pdf",
        vectorize(warp::square_to_tent_pdf<Float>),
        "v"_a, D(warp, square_to_tent_pdf));

    warp.def(
        "square_to_uniform_triangle_pdf",
        vectorize(warp::square_to_uniform_triangle_pdf<true, Float>),
        "p"_a, D(warp, square_to_uniform_triangle_pdf));

    warp.def(
        "square_to_uniform_sphere",
        vectorize(warp::square_to_uniform_sphere<Float>),
        "sample"_a, D(warp, square_to_uniform_sphere));

    warp.def(
        "uniform_sphere_to_square",
        vectorize(warp::uniform_sphere_to_square<Float>),
        "sample"_a, D(warp, uniform_sphere_to_square));

    warp.def(
        "square_to_uniform_sphere_pdf",
        vectorize(warp::square_to_uniform_sphere_pdf<true, Float>),
        "v"_a, D(warp, square_to_uniform_sphere_pdf));

    warp.def(
        "square_to_uniform_hemisphere",
        vectorize(warp::square_to_uniform_hemisphere<Float>),
        "sample"_a, D(warp, square_to_uniform_hemisphere));

    warp.def(
        "uniform_hemisphere_to_square",
        vectorize(warp::uniform_hemisphere_to_square<Float>),
        "v"_a, D(warp, uniform_hemisphere_to_square));

    warp.def(
        "square_to_uniform_hemisphere_pdf",
        vectorize(warp::square_to_uniform_hemisphere_pdf<true, Float>),
        "v"_a, D(warp, square_to_uniform_hemisphere_pdf));

    warp.def(
        "square_to_cosine_hemisphere",
        vectorize(warp::square_to_cosine_hemisphere<Float>),
        "sample"_a, D(warp, square_to_cosine_hemisphere));

    warp.def(
        "cosine_hemisphere_to_square",
        vectorize(warp::cosine_hemisphere_to_square<Float>),
        "v"_a, D(warp, cosine_hemisphere_to_square));

    warp.def(
        "square_to_cosine_hemisphere_pdf",
        vectorize(warp::square_to_cosine_hemisphere_pdf<true, Float>),
        "v"_a, D(warp, square_to_cosine_hemisphere_pdf));

    warp.def(
        "square_to_uniform_cone",
        vectorize(warp::square_to_uniform_cone<Float>),
        "v"_a, "cos_cutoff"_a, D(warp, square_to_uniform_cone));

    warp.def(
        "uniform_cone_to_square",
        vectorize(warp::uniform_cone_to_square<Float>),
        "v"_a, "cos_cutoff"_a, D(warp, uniform_cone_to_square));

    warp.def(
        "square_to_uniform_cone_pdf",
        vectorize(warp::square_to_uniform_cone_pdf<true, Float>),
        "v"_a, "cos_cutoff"_a, D(warp, square_to_uniform_cone_pdf));

    warp.def(
        "square_to_beckmann",
        vectorize(warp::square_to_beckmann<Float>),
        "sample"_a, "alpha"_a, D(warp, square_to_beckmann));

    warp.def(
        "beckmann_to_square",
        vectorize(warp::beckmann_to_square<Float>),
        "v"_a, "alpha"_a, D(warp, beckmann_to_square));

    warp.def(
        "square_to_beckmann_pdf",
        vectorize(warp::square_to_beckmann_pdf<Float>),
        "v"_a, "alpha"_a, D(warp, square_to_beckmann_pdf));

    warp.def(
        "square_to_von_mises_fisher",
        vectorize(warp::square_to_von_mises_fisher<Float>),
        "sample"_a, "kappa"_a, D(warp, square_to_von_mises_fisher));

    warp.def(
        "von_mises_fisher_to_square",
        vectorize(warp::von_mises_fisher_to_square<Float>),
        "v"_a, "kappa"_a, D(warp, von_mises_fisher_to_square));

    warp.def(
        "square_to_von_mises_fisher_pdf",
        vectorize(warp::square_to_von_mises_fisher_pdf<Float>),
        "v"_a, "kappa"_a, D(warp, square_to_von_mises_fisher_pdf));

    warp.def(
        "square_to_rough_fiber",
        vectorize(warp::square_to_rough_fiber<Float>),
        "sample"_a, "wi"_a, "tangent"_a, "kappa"_a,
        D(warp, square_to_rough_fiber));

    warp.def(
        "square_to_rough_fiber_pdf",
        vectorize(warp::square_to_rough_fiber_pdf<Float>),
        "v"_a, "wi"_a, "tangent"_a, "kappa"_a,
        D(warp, square_to_rough_fiber_pdf));

    warp.def(
        "square_to_std_normal",
        vectorize(warp::square_to_std_normal<Float>),
        "v"_a, D(warp, square_to_std_normal));

    warp.def(
        "square_to_std_normal_pdf",
        vectorize(warp::square_to_std_normal_pdf<Float>),
        "v"_a, D(warp, square_to_std_normal_pdf));
}
