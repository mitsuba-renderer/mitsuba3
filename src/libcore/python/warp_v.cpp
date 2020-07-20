#include <mitsuba/core/warp.h>
#include <enoki/stl.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(warp) {
    MTS_PY_IMPORT_TYPES()

    m.def("square_to_uniform_disk",
          vectorize(warp::square_to_uniform_disk<Float>),
          "sample"_a, D(warp, square_to_uniform_disk));

    m.def("uniform_disk_to_square",
          vectorize(warp::uniform_disk_to_square<Float>),
          "p"_a, D(warp, uniform_disk_to_square));

    m.def("square_to_uniform_disk_pdf",
          vectorize(warp::square_to_uniform_disk_pdf<true, Float>),
          "p"_a, D(warp, square_to_uniform_disk_pdf));

    m.def("uniform_disk_to_square_concentric",
          vectorize(warp::uniform_disk_to_square_concentric<Float>),
          "p"_a, D(warp, uniform_disk_to_square_concentric));
    m.def("square_to_uniform_disk_concentric",
          vectorize(warp::square_to_uniform_disk_concentric<Float>),
          "sample"_a, D(warp, square_to_uniform_disk_concentric));

    m.def("square_to_uniform_square_concentric",
          vectorize(warp::square_to_uniform_square_concentric<Float>),
          "sample"_a, D(warp, square_to_uniform_square_concentric));

    m.def("square_to_uniform_disk_concentric_pdf",
          vectorize(warp::square_to_uniform_disk_concentric_pdf<true, Float>),
          "p"_a, D(warp, square_to_uniform_disk_concentric_pdf));

    m.def("square_to_uniform_triangle",
          vectorize(warp::square_to_uniform_triangle<Float>),
          "sample"_a, D(warp, square_to_uniform_triangle));

    m.def("uniform_triangle_to_square",
          vectorize(warp::uniform_triangle_to_square<Float>),
          "p"_a, D(warp, uniform_triangle_to_square));

    m.def("interval_to_tent",
          warp::interval_to_tent<Float>,
          "sample"_a, D(warp, interval_to_tent));

    m.def("tent_to_interval",
          warp::tent_to_interval<Float>,
          "value"_a, D(warp, tent_to_interval));

    m.def("interval_to_nonuniform_tent",
          warp::interval_to_nonuniform_tent<Float>,
          "a"_a, "b"_a, "c"_a, "d"_a, D(warp, interval_to_nonuniform_tent));

    m.def("square_to_tent",
          vectorize(warp::square_to_tent<Float>),
          "sample"_a, D(warp, square_to_tent));

    m.def("tent_to_square",
          vectorize(warp::tent_to_square<Float>),
          "value"_a, D(warp, tent_to_square));

    m.def("square_to_tent_pdf",
          vectorize(warp::square_to_tent_pdf<Float>),
          "v"_a, D(warp, square_to_tent_pdf));

    m.def("square_to_uniform_triangle_pdf",
          vectorize(warp::square_to_uniform_triangle_pdf<true, Float>),
          "p"_a, D(warp, square_to_uniform_triangle_pdf));

    m.def("square_to_uniform_sphere",
          vectorize(warp::square_to_uniform_sphere<Float>),
          "sample"_a, D(warp, square_to_uniform_sphere));

    m.def("uniform_sphere_to_square",
          vectorize(warp::uniform_sphere_to_square<Float>),
          "sample"_a, D(warp, uniform_sphere_to_square));

    m.def("square_to_uniform_sphere_pdf",
          vectorize(warp::square_to_uniform_sphere_pdf<true, Float>),
          "v"_a, D(warp, square_to_uniform_sphere_pdf));

    m.def("square_to_uniform_hemisphere",
          vectorize(warp::square_to_uniform_hemisphere<Float>),
          "sample"_a, D(warp, square_to_uniform_hemisphere));

    m.def("uniform_hemisphere_to_square",
          vectorize(warp::uniform_hemisphere_to_square<Float>),
          "v"_a, D(warp, uniform_hemisphere_to_square));

    m.def("square_to_uniform_hemisphere_pdf",
          vectorize(warp::square_to_uniform_hemisphere_pdf<true, Float>),
          "v"_a, D(warp, square_to_uniform_hemisphere_pdf));

    m.def("square_to_cosine_hemisphere",
          vectorize(warp::square_to_cosine_hemisphere<Float>),
          "sample"_a, D(warp, square_to_cosine_hemisphere));

    m.def("cosine_hemisphere_to_square",
          vectorize(warp::cosine_hemisphere_to_square<Float>),
          "v"_a, D(warp, cosine_hemisphere_to_square));

    m.def("square_to_cosine_hemisphere_pdf",
          vectorize(warp::square_to_cosine_hemisphere_pdf<true, Float>),
          "v"_a, D(warp, square_to_cosine_hemisphere_pdf));

    m.def("square_to_uniform_cone",
          vectorize(warp::square_to_uniform_cone<Float>),
          "v"_a, "cos_cutoff"_a, D(warp, square_to_uniform_cone));

    m.def("uniform_cone_to_square",
          vectorize(warp::uniform_cone_to_square<Float>),
          "v"_a, "cos_cutoff"_a, D(warp, uniform_cone_to_square));

    m.def("square_to_uniform_cone_pdf",
          vectorize(warp::square_to_uniform_cone_pdf<true, Float>),
          "v"_a, "cos_cutoff"_a, D(warp, square_to_uniform_cone_pdf));

    m.def("square_to_beckmann",
          vectorize(warp::square_to_beckmann<Float>),
          "sample"_a, "alpha"_a, D(warp, square_to_beckmann));

    m.def("beckmann_to_square",
          vectorize(warp::beckmann_to_square<Float>),
          "v"_a, "alpha"_a, D(warp, beckmann_to_square));

    m.def("square_to_beckmann_pdf",
          vectorize(warp::square_to_beckmann_pdf<Float>),
          "v"_a, "alpha"_a, D(warp, square_to_beckmann_pdf));

    m.def("square_to_von_mises_fisher",
          vectorize(warp::square_to_von_mises_fisher<Float>),
          "sample"_a, "kappa"_a, D(warp, square_to_von_mises_fisher));

    m.def("von_mises_fisher_to_square",
          vectorize(warp::von_mises_fisher_to_square<Float>),
          "v"_a, "kappa"_a, D(warp, von_mises_fisher_to_square));

    m.def("square_to_von_mises_fisher_pdf",
          vectorize(warp::square_to_von_mises_fisher_pdf<Float>),
          "v"_a, "kappa"_a, D(warp, square_to_von_mises_fisher_pdf));

    m.def("square_to_rough_fiber",
          vectorize(warp::square_to_rough_fiber<Float>),
          "sample"_a, "wi"_a, "tangent"_a, "kappa"_a,
          D(warp, square_to_rough_fiber));

    m.def("square_to_rough_fiber_pdf",
          vectorize(warp::square_to_rough_fiber_pdf<Float>),
          "v"_a, "wi"_a, "tangent"_a, "kappa"_a,
          D(warp, square_to_rough_fiber_pdf));

    m.def("square_to_std_normal",
          vectorize(warp::square_to_std_normal<Float>),
          "v"_a, D(warp, square_to_std_normal));

    m.def("square_to_std_normal_pdf",
          vectorize(warp::square_to_std_normal_pdf<Float>),
          "v"_a, D(warp, square_to_std_normal_pdf));

    m.def("interval_to_linear", vectorize(warp::interval_to_linear<Float>),
          "v0"_a, "v1"_a, "sample"_a, D(warp, interval_to_linear));

    m.def("linear_to_interval", vectorize(warp::linear_to_interval<Float>),
          "v0"_a, "v1"_a, "sample"_a, D(warp, linear_to_interval));

    m.def("square_to_bilinear", vectorize(warp::square_to_bilinear<Float>),
          "v00"_a, "v10"_a, "v01"_a, "v11"_a, "sample"_a,
          D(warp, square_to_bilinear));

    m.def("square_to_bilinear_pdf", vectorize(warp::square_to_bilinear_pdf<Float>),
          "v00"_a, "v10"_a, "v01"_a, "v11"_a, "sample"_a,
          D(warp, square_to_bilinear_pdf));

    m.def("bilinear_to_square", vectorize(warp::bilinear_to_square<Float>),
          "v00"_a, "v10"_a, "v01"_a, "v11"_a, "sample"_a,
          D(warp, bilinear_to_square));
}
