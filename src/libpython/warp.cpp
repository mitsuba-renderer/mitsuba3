#include <mitsuba/core/warp.h>
#include "python.h"

MTS_PY_EXPORT(warp) {
    MTS_PY_IMPORT_MODULE(warp, "mitsuba.core.warp");

    warp.def(
        "square_to_uniform_sphere",
        warp::square_to_uniform_sphere<Point2f>, "sample"_a,
        D(warp, square_to_uniform_sphere));

    warp.def(
        "square_to_uniform_sphere",
        vectorize_wrapper(warp::square_to_uniform_sphere<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_sphere_pdf",
        warp::square_to_uniform_sphere_pdf<true, Vector3f>, "v"_a,
        D(warp, square_to_uniform_sphere_pdf));

    warp.def(
        "square_to_uniform_sphere_pdf",
        vectorize_wrapper(warp::square_to_uniform_sphere_pdf<true, Vector3fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_hemisphere",
        warp::square_to_uniform_hemisphere<Point2f>, "sample"_a,
        D(warp, square_to_uniform_hemisphere));

    warp.def(
        "square_to_uniform_hemisphere",
        vectorize_wrapper(warp::square_to_uniform_hemisphere<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_hemisphere_pdf",
        warp::square_to_uniform_hemisphere_pdf<true, Vector3f>, "v"_a,
        D(warp, square_to_uniform_hemisphere_pdf));

    warp.def(
        "square_to_uniform_hemisphere_pdf",
        vectorize_wrapper(warp::square_to_uniform_hemisphere_pdf<true, Vector3fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_disk",
        warp::square_to_uniform_disk<Point2f>, "v"_a,
        D(warp, square_to_uniform_disk));

    warp.def(
        "square_to_uniform_disk",
        vectorize_wrapper(warp::square_to_uniform_disk<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_disk_pdf",
        warp::square_to_uniform_disk_pdf<true, Point2f>, "v"_a,
        D(warp, square_to_uniform_disk_pdf));

    warp.def(
        "square_to_uniform_disk_pdf",
        vectorize_wrapper(warp::square_to_uniform_disk_pdf<true, Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_disk_concentric",
        warp::square_to_uniform_disk_concentric<Point2f>, "sample"_a,
        D(warp, square_to_uniform_disk_concentric));

    warp.def(
        "square_to_uniform_disk_concentric",
        vectorize_wrapper(warp::square_to_uniform_disk_concentric<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_cosine_hemisphere",
        warp::square_to_cosine_hemisphere<Point2f>, "sample"_a,
        D(warp, square_to_cosine_hemisphere));

    warp.def(
        "square_to_cosine_hemisphere",
        vectorize_wrapper(warp::square_to_cosine_hemisphere<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_cosine_hemisphere_pdf",
        warp::square_to_cosine_hemisphere_pdf<true, Vector3f>, "v"_a,
        D(warp, square_to_cosine_hemisphere_pdf));

    warp.def(
        "square_to_cosine_hemisphere_pdf",
        vectorize_wrapper(warp::square_to_cosine_hemisphere_pdf<true, Vector3fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_cone",
        warp::square_to_uniform_cone<Point2f>, "v"_a, "cosCutoff"_a,
        D(warp, square_to_uniform_cone));

    warp.def(
        "square_to_uniform_cone",
        vectorize_wrapper(warp::square_to_uniform_cone<Point2fP>),
        "sample"_a, "cosCutoff"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_cone_pdf",
        warp::square_to_uniform_cone_pdf<true, Vector3f>, "v"_a, "cosCutoff"_a,
        D(warp, square_to_uniform_cone_pdf));

    warp.def(
        "square_to_uniform_cone_pdf",
        vectorize_wrapper(warp::square_to_uniform_cone_pdf<true, Vector3fP>),
        "sample"_a, "cosCutoff"_a);

    // =======================================================================

    warp.def(
        "square_to_beckmann",
        warp::square_to_beckmann<Point2f>, "v"_a, "alpha"_a,
        D(warp, square_to_beckmann));

    warp.def(
        "square_to_beckmann",
        vectorize_wrapper(warp::square_to_beckmann<Point2fP>),
        "sample"_a, "alpha"_a);

    // =======================================================================

    warp.def(
        "square_to_beckmann_pdf",
        warp::square_to_beckmann_pdf<Vector3f>, "v"_a, "alpha"_a,
        D(warp, square_to_beckmann_pdf));

    warp.def(
        "square_to_beckmann_pdf",
        vectorize_wrapper(warp::square_to_beckmann_pdf<Vector3fP>),
        "sample"_a, "alpha"_a);

    // =======================================================================

    warp.def(
        "square_to_von_mises_fisher",
        warp::square_to_von_mises_fisher<Point2f>, "v"_a, "kappa"_a,
        D(warp, square_to_von_mises_fisher));

    warp.def(
        "square_to_von_mises_fisher",
        vectorize_wrapper(warp::square_to_von_mises_fisher<Point2fP>),
        "sample"_a, "kappa"_a);

    // =======================================================================

    warp.def(
        "square_to_von_mises_fisher_pdf",
        warp::square_to_von_mises_fisher_pdf<Vector3f>, "v"_a, "kappa"_a,
        D(warp, square_to_von_mises_fisher_pdf));

    warp.def(
        "square_to_von_mises_fisher_pdf",
        vectorize_wrapper(warp::square_to_von_mises_fisher_pdf<Vector3fP>),
        "sample"_a, "kappa"_a);

    // =======================================================================

    warp.def(
        "square_to_rough_fiber",
        warp::square_to_rough_fiber<Point3f, Vector3f>, "v"_a, "wi"_a, "tangent"_a, "kappa"_a,
        D(warp, square_to_rough_fiber));

    warp.def(
        "square_to_rough_fiber",
        vectorize_wrapper(warp::square_to_rough_fiber<Point3fP, Vector3fP>),
        "sample"_a, "wi"_a, "tangent"_a, "kappa"_a);

    // =======================================================================

    warp.def(
        "square_to_rough_fiber_pdf",
        warp::square_to_rough_fiber_pdf<Vector3f>, "v"_a, "wi"_a, "tangent"_a, "kappa"_a,
        D(warp, square_to_rough_fiber_pdf));

    warp.def(
        "square_to_rough_fiber_pdf",
        vectorize_wrapper(warp::square_to_rough_fiber_pdf<Vector3fP>),
        "sample"_a, "wi"_a, "tangent"_a, "kappa"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_triangle",
        warp::square_to_uniform_triangle<Point2f>, "v"_a,
        D(warp, square_to_uniform_triangle));

    warp.def(
        "square_to_uniform_triangle",
        vectorize_wrapper(warp::square_to_uniform_triangle<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_triangle_pdf",
        warp::square_to_uniform_triangle_pdf<true, Point2f>, "v"_a,
        D(warp, square_to_uniform_triangle_pdf));

    warp.def("square_to_uniform_triangle_pdf",
        vectorize_wrapper(warp::square_to_uniform_triangle_pdf<true, Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_std_normal",
        warp::square_to_std_normal<Point2f>, "v"_a,
        D(warp, square_to_std_normal));

    warp.def(
        "square_to_std_normal",
        vectorize_wrapper(warp::square_to_std_normal<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_std_normal_pdf",
        warp::square_to_std_normal_pdf<Point2f>, "v"_a,
        D(warp, square_to_std_normal_pdf));

    warp.def(
        "square_to_std_normal_pdf",
        vectorize_wrapper(warp::square_to_std_normal_pdf<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "interval_to_tent",
        warp::interval_to_tent<Float>, "v"_a,
        D(warp, interval_to_tent));

    warp.def(
        "interval_to_tent",
        vectorize_wrapper(warp::interval_to_tent<FloatP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "interval_to_nonuniform_tent",
        warp::interval_to_nonuniform_tent<Float>, "a"_a, "b"_a, "c"_a, "d"_a,
        D(warp, interval_to_nonuniform_tent));

    warp.def(
        "interval_to_nonuniform_tent",
        vectorize_wrapper(warp::interval_to_nonuniform_tent<FloatP>),
        "a"_a, "b"_a, "c"_a, "d"_a);

    // =======================================================================

    warp.def(
        "square_to_tent",
        warp::square_to_tent<Point2f>, "v"_a,
        D(warp, square_to_tent));

    warp.def(
        "square_to_tent",
        vectorize_wrapper(warp::square_to_tent<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_tent_pdf",
        warp::square_to_tent_pdf<Point2f>, "v"_a,
        D(warp, square_to_tent_pdf));

    warp.def(
        "square_to_tent_pdf",
        vectorize_wrapper(warp::square_to_tent_pdf<Point2fP>),
        "sample"_a);
}
