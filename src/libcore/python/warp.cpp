#include <mitsuba/core/warp.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(warp) {
    MTS_PY_IMPORT_MODULE(warp, "mitsuba.core.warp");

    warp.def(
        "square_to_uniform_disk",
        warp::square_to_uniform_disk<Point2f>,
        "sample"_a, D(warp, square_to_uniform_disk));

    warp.def(
        "square_to_uniform_disk",
        vectorize_wrapper(warp::square_to_uniform_disk<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "uniform_disk_to_square",
        warp::uniform_disk_to_square<Point2f>,
        "p"_a, D(warp, uniform_disk_to_square));

    warp.def(
        "uniform_disk_to_square",
        vectorize_wrapper(warp::uniform_disk_to_square<Point2fP>),
        "p"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_disk_pdf",
        warp::square_to_uniform_disk_pdf<true, Point2f>,
        "p"_a, D(warp, square_to_uniform_disk_pdf));

    warp.def(
        "square_to_uniform_disk_pdf",
        vectorize_wrapper(warp::square_to_uniform_disk_pdf<true, Point2fP>),
        "p"_a);

    // =======================================================================

    warp.def(
        "uniform_disk_to_square_concentric",
        warp::uniform_disk_to_square_concentric<Point2f>,
        "p"_a, D(warp, uniform_disk_to_square_concentric));

    warp.def(
        "uniform_disk_to_square_concentric",
        vectorize_wrapper(warp::uniform_disk_to_square_concentric<Point2fP>),
        "p"_a);

    // =======================================================================
    warp.def(
        "square_to_uniform_disk_concentric",
        warp::square_to_uniform_disk_concentric<Point2f>,
        "sample"_a, D(warp, square_to_uniform_disk_concentric));

    warp.def(
        "square_to_uniform_disk_concentric",
        vectorize_wrapper(warp::square_to_uniform_disk_concentric<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_disk_concentric_pdf",
        warp::square_to_uniform_disk_concentric_pdf<true, Point2f>,
        "p"_a, D(warp, square_to_uniform_disk_concentric_pdf));

    warp.def(
        "square_to_uniform_disk_concentric_pdf",
        vectorize_wrapper(warp::square_to_uniform_disk_concentric_pdf<true, Point2fP>),
        "p"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_triangle",
        warp::square_to_uniform_triangle<Point2f>,
        "sample"_a, D(warp, square_to_uniform_triangle));

    warp.def(
        "square_to_uniform_triangle",
        vectorize_wrapper(warp::square_to_uniform_triangle<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "uniform_triangle_to_square",
        warp::uniform_triangle_to_square<Point2f>,
        "p"_a, D(warp, uniform_triangle_to_square));

    warp.def(
        "uniform_triangle_to_square",
        vectorize_wrapper(warp::uniform_triangle_to_square<Point2fP>),
        "p"_a);

    // =======================================================================

    warp.def(
        "interval_to_tent",
        warp::interval_to_tent<Float>,
        "sample"_a, D(warp, interval_to_tent));

    warp.def(
        "interval_to_tent",
        vectorize_wrapper(warp::interval_to_tent<FloatP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "tent_to_interval",
        warp::tent_to_interval<Float>,
        "value"_a, D(warp, tent_to_interval));

    warp.def(
        "tent_to_interval",
        vectorize_wrapper(warp::tent_to_interval<FloatP>),
        "value"_a);

    // =======================================================================

    warp.def(
        "interval_to_nonuniform_tent",
        warp::interval_to_nonuniform_tent<Float>,
        "a"_a, "b"_a, "c"_a, "d"_a, D(warp, interval_to_nonuniform_tent));

    warp.def(
        "interval_to_nonuniform_tent",
        vectorize_wrapper(warp::interval_to_nonuniform_tent<FloatP>),
        "a"_a, "b"_a, "c"_a, "d"_a);

    // =======================================================================

    warp.def(
        "square_to_tent",
        warp::square_to_tent<Point2f>,
        "sample"_a, D(warp, square_to_tent));

    warp.def(
        "square_to_tent",
        vectorize_wrapper(warp::square_to_tent<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "tent_to_square",
        warp::tent_to_square<Point2f>,
        "value"_a, D(warp, tent_to_square));

    warp.def(
        "tent_to_square",
        vectorize_wrapper(warp::tent_to_square<Point2fP>),
        "value"_a);

    // =======================================================================

    warp.def(
        "square_to_tent_pdf",
        warp::square_to_tent_pdf<Point2f>,
        "v"_a, D(warp, square_to_tent_pdf));

    warp.def(
        "square_to_tent_pdf",
        vectorize_wrapper(warp::square_to_tent_pdf<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_triangle_pdf",
        warp::square_to_uniform_triangle_pdf<true, Point2f>,
        "p"_a, D(warp, square_to_uniform_triangle_pdf));

    warp.def("square_to_uniform_triangle_pdf",
        vectorize_wrapper(warp::square_to_uniform_triangle_pdf<true, Point2fP>),
        "p"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_sphere",
        warp::square_to_uniform_sphere<Point2f>,
        "sample"_a, D(warp, square_to_uniform_sphere));

    warp.def(
        "square_to_uniform_sphere",
        vectorize_wrapper(warp::square_to_uniform_sphere<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "uniform_sphere_to_square",
        warp::uniform_sphere_to_square<Vector3f>,
        "sample"_a, D(warp, uniform_sphere_to_square));

    warp.def(
        "uniform_sphere_to_square",
        vectorize_wrapper(warp::uniform_sphere_to_square<Vector3fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_sphere_pdf",
        warp::square_to_uniform_sphere_pdf<true, Vector3f>,
        "v"_a, D(warp, square_to_uniform_sphere_pdf));

    warp.def(
        "square_to_uniform_sphere_pdf",
        vectorize_wrapper(warp::square_to_uniform_sphere_pdf<true, Vector3fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_hemisphere",
        warp::square_to_uniform_hemisphere<Point2f>,
        "sample"_a, D(warp, square_to_uniform_hemisphere));

    warp.def(
        "square_to_uniform_hemisphere",
        vectorize_wrapper(warp::square_to_uniform_hemisphere<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "uniform_hemisphere_to_square",
        warp::uniform_hemisphere_to_square<Vector3f>,
        "v"_a, D(warp, uniform_hemisphere_to_square));

    warp.def(
        "uniform_hemisphere_to_square",
        vectorize_wrapper(warp::uniform_hemisphere_to_square<Vector3fP>),
        "v"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_hemisphere_pdf",
        warp::square_to_uniform_hemisphere_pdf<true, Vector3f>,
        "v"_a, D(warp, square_to_uniform_hemisphere_pdf));

    warp.def(
        "square_to_uniform_hemisphere_pdf",
        vectorize_wrapper(warp::square_to_uniform_hemisphere_pdf<true, Vector3fP>),
        "v"_a);

    // =======================================================================

    warp.def(
        "square_to_cosine_hemisphere",
        warp::square_to_cosine_hemisphere<Point2f>,
        "sample"_a, D(warp, square_to_cosine_hemisphere));

    warp.def(
        "square_to_cosine_hemisphere",
        vectorize_wrapper(warp::square_to_cosine_hemisphere<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "cosine_hemisphere_to_square",
        warp::cosine_hemisphere_to_square<Vector3f>,
        "v"_a, D(warp, cosine_hemisphere_to_square));

    warp.def(
        "cosine_hemisphere_to_square",
        vectorize_wrapper(warp::cosine_hemisphere_to_square<Vector3fP>),
        "v"_a);

    // =======================================================================

    warp.def(
        "square_to_cosine_hemisphere_pdf",
        warp::square_to_cosine_hemisphere_pdf<true, Vector3f>,
        "v"_a, D(warp, square_to_cosine_hemisphere_pdf));

    warp.def(
        "square_to_cosine_hemisphere_pdf",
        vectorize_wrapper(warp::square_to_cosine_hemisphere_pdf<true, Vector3fP>),
        "v"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_cone",
        warp::square_to_uniform_cone<Point2f>,
        "v"_a, "cos_cutoff"_a, D(warp, square_to_uniform_cone));

    warp.def(
        "square_to_uniform_cone",
        vectorize_wrapper(warp::square_to_uniform_cone<Point2fP>),
        "sample"_a, "cos_cutoff"_a);

    // =======================================================================

    warp.def(
        "uniform_cone_to_square",
        warp::uniform_cone_to_square<Vector3f>,
        "v"_a, "cos_cutoff"_a, D(warp, uniform_cone_to_square));

    warp.def(
        "uniform_cone_to_square",
        vectorize_wrapper(warp::uniform_cone_to_square<Vector3fP>),
        "v"_a, "cos_cutoff"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_cone_pdf",
        warp::square_to_uniform_cone_pdf<true, Vector3f>,
        "v"_a, "cos_cutoff"_a, D(warp, square_to_uniform_cone_pdf));

    warp.def(
        "square_to_uniform_cone_pdf",
        vectorize_wrapper(warp::square_to_uniform_cone_pdf<true, Vector3fP>),
        "v"_a, "cos_cutoff"_a);

    // =======================================================================

    warp.def(
        "square_to_beckmann",
        warp::square_to_beckmann<Point2f>,
        "sample"_a, "alpha"_a, D(warp, square_to_beckmann));

    warp.def(
        "square_to_beckmann",
        vectorize_wrapper(warp::square_to_beckmann<Point2fP>),
        "sample"_a, "alpha"_a);

    // =======================================================================

    warp.def(
        "beckmann_to_square",
        warp::beckmann_to_square<Vector3f>,
        "v"_a, "alpha"_a, D(warp, beckmann_to_square));

    warp.def(
        "beckmann_to_square",
        vectorize_wrapper(warp::beckmann_to_square<Vector3fP>),
        "v"_a, "alpha"_a);

    // =======================================================================

    warp.def(
        "square_to_beckmann_pdf",
        warp::square_to_beckmann_pdf<Vector3f>,
        "v"_a, "alpha"_a, D(warp, square_to_beckmann_pdf));

    warp.def(
        "square_to_beckmann_pdf",
        vectorize_wrapper(warp::square_to_beckmann_pdf<Vector3fP>),
        "v"_a, "alpha"_a);

    // =======================================================================

    warp.def(
        "square_to_von_mises_fisher",
        warp::square_to_von_mises_fisher<Point2f>,
        "sample"_a, "kappa"_a, D(warp, square_to_von_mises_fisher));

    warp.def(
        "square_to_von_mises_fisher",
        vectorize_wrapper(warp::square_to_von_mises_fisher<Point2fP>),
        "sample"_a, "kappa"_a);

    // =======================================================================

    warp.def(
        "von_mises_fisher_to_square",
        warp::von_mises_fisher_to_square<Vector3f>,
        "v"_a, "kappa"_a, D(warp, von_mises_fisher_to_square));

    warp.def(
        "von_mises_fisher_to_square",
        vectorize_wrapper(warp::von_mises_fisher_to_square<Vector3fP>),
        "v"_a, "kappa"_a);

    // =======================================================================

    warp.def(
        "square_to_von_mises_fisher_pdf",
        warp::square_to_von_mises_fisher_pdf<Vector3f>,
        "v"_a, "kappa"_a, D(warp, square_to_von_mises_fisher_pdf));

    warp.def(
        "square_to_von_mises_fisher_pdf",
        vectorize_wrapper(warp::square_to_von_mises_fisher_pdf<Vector3fP>),
        "v"_a, "kappa"_a);

    // =======================================================================

    warp.def(
        "square_to_rough_fiber",
        warp::square_to_rough_fiber<Point3f, Vector3f>,
        "sample"_a, "wi"_a, "tangent"_a, "kappa"_a,
        D(warp, square_to_rough_fiber));

    warp.def(
        "square_to_rough_fiber",
        vectorize_wrapper(warp::square_to_rough_fiber<Point3fP, Vector3fP>),
        "sample"_a, "wi"_a, "tangent"_a, "kappa"_a);

    // =======================================================================

    warp.def(
        "square_to_rough_fiber_pdf",
        warp::square_to_rough_fiber_pdf<Vector3f>,
        "v"_a, "wi"_a, "tangent"_a, "kappa"_a,
        D(warp, square_to_rough_fiber_pdf));

    warp.def(
        "square_to_rough_fiber_pdf",
        vectorize_wrapper(warp::square_to_rough_fiber_pdf<Vector3fP>),
        "sample"_a, "wi"_a, "tangent"_a, "kappa"_a);

    // =======================================================================

    warp.def(
        "square_to_std_normal",
        warp::square_to_std_normal<Point2f>,
        "v"_a, D(warp, square_to_std_normal));

    warp.def(
        "square_to_std_normal",
        vectorize_wrapper(warp::square_to_std_normal<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_std_normal_pdf",
        warp::square_to_std_normal_pdf<Point2f>,
        "v"_a, D(warp, square_to_std_normal_pdf));

    warp.def(
        "square_to_std_normal_pdf",
        vectorize_wrapper(warp::square_to_std_normal_pdf<Point2fP>),
        "sample"_a);
}
