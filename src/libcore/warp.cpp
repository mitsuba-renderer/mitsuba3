#include <mitsuba/core/warp.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(warp)

template MTS_EXPORT_CORE Point2f  square_to_uniform_disk(const Point2f &);
template MTS_EXPORT_CORE Point2fP square_to_uniform_disk(const Point2fP &);

template MTS_EXPORT_CORE Point2f  uniform_disk_to_square(const Point2f &);
template MTS_EXPORT_CORE Point2fP uniform_disk_to_square(const Point2fP &);

template MTS_EXPORT_CORE Point2f  square_to_uniform_disk_pdf(const Point2f &);
template MTS_EXPORT_CORE Point2fP square_to_uniform_disk_pdf(const Point2fP &);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Point2f  square_to_uniform_disk_concentric(const Point2f &);
template MTS_EXPORT_CORE Point2fP square_to_uniform_disk_concentric(const Point2fP &);

template MTS_EXPORT_CORE Point2f  uniform_disk_to_square_concentric(const Point2f &);
template MTS_EXPORT_CORE Point2fP uniform_disk_to_square_concentric(const Point2fP &);

template MTS_EXPORT_CORE Float  square_to_uniform_disk_concentric_pdf(const Point2f &);
template MTS_EXPORT_CORE FloatP square_to_uniform_disk_concentric_pdf(const Point2fP &);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Point2f  square_to_uniform_triangle(const Point2f &);
template MTS_EXPORT_CORE Point2fP square_to_uniform_triangle(const Point2fP &);

template MTS_EXPORT_CORE Point2f  uniform_triangle_to_square(const Point2f &);
template MTS_EXPORT_CORE Point2fP uniform_triangle_to_square(const Point2fP &);

template MTS_EXPORT_CORE Float  square_to_uniform_triangle_pdf(const Point2f &);
template MTS_EXPORT_CORE FloatP square_to_uniform_triangle_pdf(const Point2fP &);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Float  interval_to_tent(const Float &);
template MTS_EXPORT_CORE FloatP interval_to_tent(const FloatP &);

template MTS_EXPORT_CORE Float  tent_to_interval(const Float &);
template MTS_EXPORT_CORE FloatP tent_to_interval(const FloatP &);

template MTS_EXPORT_CORE Float  interval_to_nonuniform_tent(const Float &, const Float &, const Float &, const Float &);
template MTS_EXPORT_CORE FloatP interval_to_nonuniform_tent(const FloatP &, const FloatP &, const FloatP &, const FloatP &);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Point2f  square_to_tent(const Point2f &);
template MTS_EXPORT_CORE Point2fP square_to_tent(const Point2fP &);

template MTS_EXPORT_CORE Point2f  tent_to_square(const Point2f &);
template MTS_EXPORT_CORE Point2fP tent_to_square(const Point2fP &);

template MTS_EXPORT_CORE Float  square_to_tent_pdf(const Point2f &);
template MTS_EXPORT_CORE FloatP square_to_tent_pdf(const Point2fP &);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Vector3f  square_to_uniform_sphere(const Point2f &);
template MTS_EXPORT_CORE Vector3fP square_to_uniform_sphere(const Point2fP &);

template MTS_EXPORT_CORE Point2f  uniform_sphere_to_square(const Vector3f &);
template MTS_EXPORT_CORE Point2fP uniform_sphere_to_square(const Vector3fP &);

template MTS_EXPORT_CORE Float  square_to_uniform_sphere_pdf<true>(const Vector3f &);
template MTS_EXPORT_CORE FloatP square_to_uniform_sphere_pdf<true>(const Vector3fP &);
template MTS_EXPORT_CORE Float  square_to_uniform_sphere_pdf<false>(const Vector3f &);
template MTS_EXPORT_CORE FloatP square_to_uniform_sphere_pdf<false>(const Vector3fP &);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Vector3f  square_to_uniform_hemisphere(const Point2f &);
template MTS_EXPORT_CORE Vector3fP square_to_uniform_hemisphere(const Point2fP &);

template MTS_EXPORT_CORE Point2f  uniform_hemisphere_to_square(const Vector3f &);
template MTS_EXPORT_CORE Point2fP uniform_hemisphere_to_square(const Vector3fP &);

template MTS_EXPORT_CORE Float  square_to_uniform_hemisphere_pdf<true>(const Vector3f &);
template MTS_EXPORT_CORE FloatP square_to_uniform_hemisphere_pdf<true>(const Vector3fP &);
template MTS_EXPORT_CORE Float  square_to_uniform_hemisphere_pdf<false>(const Vector3f &);
template MTS_EXPORT_CORE FloatP square_to_uniform_hemisphere_pdf<false>(const Vector3fP &);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Vector3f  square_to_cosine_hemisphere(const Point2f &);
template MTS_EXPORT_CORE Vector3fP square_to_cosine_hemisphere(const Point2fP &);

template MTS_EXPORT_CORE Point2f  cosine_hemisphere_to_square(const Vector3f &);
template MTS_EXPORT_CORE Point2fP cosine_hemisphere_to_square(const Vector3fP &);

template MTS_EXPORT_CORE Float  square_to_cosine_hemisphere_pdf<true>(const Vector3f &);
template MTS_EXPORT_CORE FloatP square_to_cosine_hemisphere_pdf<true>(const Vector3fP &);
template MTS_EXPORT_CORE Float  square_to_cosine_hemisphere_pdf<false>(const Vector3f &);
template MTS_EXPORT_CORE FloatP square_to_cosine_hemisphere_pdf<false>(const Vector3fP &);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Vector3f  square_to_uniform_cone(const Point2f &, const Float &);
template MTS_EXPORT_CORE Vector3fP square_to_uniform_cone(const Point2fP &, const FloatP &);

template MTS_EXPORT_CORE Point2f  uniform_cone_to_square(const Vector3f &, const Float &);
template MTS_EXPORT_CORE Point2fP uniform_cone_to_square(const Vector3fP &, const FloatP &);

template MTS_EXPORT_CORE Float  square_to_uniform_cone_pdf<true>(const Vector3f & , const Float &);
template MTS_EXPORT_CORE FloatP square_to_uniform_cone_pdf<true>(const Vector3fP &, const FloatP &);
template MTS_EXPORT_CORE Float  square_to_uniform_cone_pdf<false>(const Vector3f & , const Float &);
template MTS_EXPORT_CORE FloatP square_to_uniform_cone_pdf<false>(const Vector3fP &, const FloatP &);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Vector3f  square_to_beckmann(const Point2f &, const Float &);
template MTS_EXPORT_CORE Vector3fP square_to_beckmann(const Point2fP &, const FloatP &);

template MTS_EXPORT_CORE Point2f  beckmann_to_square(const Vector3f &, const Float &);
template MTS_EXPORT_CORE Point2fP beckmann_to_square(const Vector3fP &, const FloatP &);

template MTS_EXPORT_CORE Float  square_to_beckmann_pdf(const Vector3f &, const Float &);
template MTS_EXPORT_CORE FloatP square_to_beckmann_pdf(const Vector3fP &, const FloatP &);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Vector3f  square_to_von_mises_fisher(const Point2f &, Float);
template MTS_EXPORT_CORE Vector3fP square_to_von_mises_fisher(const Point2fP &, Float);

template MTS_EXPORT_CORE Float  square_to_von_mises_fisher_pdf(const Vector3f &, Float);
template MTS_EXPORT_CORE FloatP square_to_von_mises_fisher_pdf(const Vector3fP &, Float);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Vector3f  square_to_rough_fiber(const Point3f &, const Vector3f &, const Vector3f &, Float);
template MTS_EXPORT_CORE Vector3fP square_to_rough_fiber(const Point3fP &, const Vector3fP &, const Vector3fP &, Float);

template MTS_EXPORT_CORE Float  square_to_rough_fiber_pdf(const Vector3f &, const Vector3f &, const Vector3f &, Float);
template MTS_EXPORT_CORE FloatP square_to_rough_fiber_pdf(const Vector3fP &, const Vector3fP &, const Vector3fP &, Float);

// ------------------------------------------------------------------------------------------

template MTS_EXPORT_CORE Point2f  square_to_std_normal(const Point2f &);
template MTS_EXPORT_CORE Point2fP square_to_std_normal(const Point2fP &);

template MTS_EXPORT_CORE Float  square_to_std_normal_pdf(const Point2f &);
template MTS_EXPORT_CORE FloatP square_to_std_normal_pdf(const Point2fP &);

NAMESPACE_END(warp)
NAMESPACE_END(mitsuba)
