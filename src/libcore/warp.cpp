#include <mitsuba/core/warp.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(warp)

template MTS_EXPORT_CORE Point2f  square_to_uniform_disk(Point2f);
template MTS_EXPORT_CORE Point2fP square_to_uniform_disk(Point2fP);

template MTS_EXPORT_CORE Point2f  uniform_disk_to_square(Point2f);
template MTS_EXPORT_CORE Point2fP uniform_disk_to_square(Point2fP);

template MTS_EXPORT_CORE Point2f  square_to_uniform_disk_pdf(Point2f);
template MTS_EXPORT_CORE Point2fP square_to_uniform_disk_pdf(Point2fP);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Point2f  square_to_uniform_disk_concentric(Point2f);
template MTS_EXPORT_CORE Point2fP square_to_uniform_disk_concentric(Point2fP);

template MTS_EXPORT_CORE Point2f  uniform_disk_to_square_concentric(Point2f);
template MTS_EXPORT_CORE Point2fP uniform_disk_to_square_concentric(Point2fP);

template MTS_EXPORT_CORE Float  square_to_uniform_disk_concentric_pdf(Point2f);
template MTS_EXPORT_CORE FloatP square_to_uniform_disk_concentric_pdf(Point2fP);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Point2f  square_to_uniform_triangle(Point2f);
template MTS_EXPORT_CORE Point2fP square_to_uniform_triangle(Point2fP);

template MTS_EXPORT_CORE Point2f  uniform_triangle_to_square(Point2f);
template MTS_EXPORT_CORE Point2fP uniform_triangle_to_square(Point2fP);

template MTS_EXPORT_CORE Float  square_to_uniform_triangle_pdf(Point2f);
template MTS_EXPORT_CORE FloatP square_to_uniform_triangle_pdf(Point2fP);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Float  interval_to_tent(Float);
template MTS_EXPORT_CORE FloatP interval_to_tent(FloatP);

template MTS_EXPORT_CORE Float  tent_to_interval(Float);
template MTS_EXPORT_CORE FloatP tent_to_interval(FloatP);

template MTS_EXPORT_CORE Float  interval_to_nonuniform_tent(Float, Float, Float, Float);
template MTS_EXPORT_CORE FloatP interval_to_nonuniform_tent(FloatP, FloatP, FloatP, FloatP);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Point2f  square_to_tent(Point2f);
template MTS_EXPORT_CORE Point2fP square_to_tent(Point2fP);

template MTS_EXPORT_CORE Point2f  tent_to_square(Point2f);
template MTS_EXPORT_CORE Point2fP tent_to_square(Point2fP);

template MTS_EXPORT_CORE Float  square_to_tent_pdf(Point2f);
template MTS_EXPORT_CORE FloatP square_to_tent_pdf(Point2fP);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Vector3f  square_to_uniform_sphere(Point2f);
template MTS_EXPORT_CORE Vector3fP square_to_uniform_sphere(Point2fP);

template MTS_EXPORT_CORE Point2f  uniform_sphere_to_square(Vector3f);
template MTS_EXPORT_CORE Point2fP uniform_sphere_to_square(Vector3fP);

template MTS_EXPORT_CORE Float  square_to_uniform_sphere_pdf<true>(Vector3f);
template MTS_EXPORT_CORE FloatP square_to_uniform_sphere_pdf<true>(Vector3fP);
template MTS_EXPORT_CORE Float  square_to_uniform_sphere_pdf<false>(Vector3f);
template MTS_EXPORT_CORE FloatP square_to_uniform_sphere_pdf<false>(Vector3fP);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Vector3f  square_to_uniform_hemisphere(Point2f);
template MTS_EXPORT_CORE Vector3fP square_to_uniform_hemisphere(Point2fP);

template MTS_EXPORT_CORE Point2f  uniform_hemisphere_to_square(Vector3f);
template MTS_EXPORT_CORE Point2fP uniform_hemisphere_to_square(Vector3fP);

template MTS_EXPORT_CORE Float  square_to_uniform_hemisphere_pdf<true>(Vector3f);
template MTS_EXPORT_CORE FloatP square_to_uniform_hemisphere_pdf<true>(Vector3fP);
template MTS_EXPORT_CORE Float  square_to_uniform_hemisphere_pdf<false>(Vector3f);
template MTS_EXPORT_CORE FloatP square_to_uniform_hemisphere_pdf<false>(Vector3fP);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Vector3f  square_to_cosine_hemisphere(Point2f);
template MTS_EXPORT_CORE Vector3fP square_to_cosine_hemisphere(Point2fP);

template MTS_EXPORT_CORE Point2f  cosine_hemisphere_to_square(Vector3f);
template MTS_EXPORT_CORE Point2fP cosine_hemisphere_to_square(Vector3fP);

template MTS_EXPORT_CORE Float  square_to_cosine_hemisphere_pdf<true>(Vector3f);
template MTS_EXPORT_CORE FloatP square_to_cosine_hemisphere_pdf<true>(Vector3fP);
template MTS_EXPORT_CORE Float  square_to_cosine_hemisphere_pdf<false>(Vector3f);
template MTS_EXPORT_CORE FloatP square_to_cosine_hemisphere_pdf<false>(Vector3fP);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Vector3f  square_to_uniform_cone(Point2f, Float);
template MTS_EXPORT_CORE Vector3fP square_to_uniform_cone(Point2fP, Float);

template MTS_EXPORT_CORE Point2f  uniform_cone_to_square(Vector3f, Float);
template MTS_EXPORT_CORE Point2fP uniform_cone_to_square(Vector3fP, Float);

template MTS_EXPORT_CORE Float  square_to_uniform_cone_pdf<true>(Vector3f , Float);
template MTS_EXPORT_CORE FloatP square_to_uniform_cone_pdf<true>(Vector3fP, Float);
template MTS_EXPORT_CORE Float  square_to_uniform_cone_pdf<false>(Vector3f , Float);
template MTS_EXPORT_CORE FloatP square_to_uniform_cone_pdf<false>(Vector3fP, Float);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Vector3f  square_to_beckmann(Point2f, Float);
template MTS_EXPORT_CORE Vector3fP square_to_beckmann(Point2fP, Float);

template MTS_EXPORT_CORE Point2f  beckmann_to_square(Vector3f, Float);
template MTS_EXPORT_CORE Point2fP beckmann_to_square(Vector3fP, Float);

template MTS_EXPORT_CORE Float  square_to_beckmann_pdf(Vector3f, Float);
template MTS_EXPORT_CORE FloatP square_to_beckmann_pdf(Vector3fP, Float);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Vector3f  square_to_von_mises_fisher(Point2f, Float);
template MTS_EXPORT_CORE Vector3fP square_to_von_mises_fisher(Point2fP, Float);

template MTS_EXPORT_CORE Float  square_to_von_mises_fisher_pdf(Vector3f, Float);
template MTS_EXPORT_CORE FloatP square_to_von_mises_fisher_pdf(Vector3fP, Float);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Vector3f  square_to_rough_fiber(Point3f, Vector3f, Vector3f, Float);
template MTS_EXPORT_CORE Vector3fP square_to_rough_fiber(Point3fP, Vector3fP, Vector3fP, Float);

template MTS_EXPORT_CORE Float  square_to_rough_fiber_pdf(Vector3f, Vector3f, Vector3f, Float);
template MTS_EXPORT_CORE FloatP square_to_rough_fiber_pdf(Vector3fP, Vector3fP, Vector3fP, Float);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Point2f  square_to_std_normal(Point2f);
template MTS_EXPORT_CORE Point2fP square_to_std_normal(Point2fP);

template MTS_EXPORT_CORE Float  square_to_std_normal_pdf(Point2f);
template MTS_EXPORT_CORE FloatP square_to_std_normal_pdf(Point2fP);

// ------------------------------------------------------------------------------------------

NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
