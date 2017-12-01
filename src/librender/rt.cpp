#include <mitsuba/render/rt.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(rt)

// --------------------------------------------------------------------------------------------

template <typename Sampler, typename RayGenerator, typename RTKernel>
std::pair<Float, size_t>
rt_scalar(uint32_t N, const Sampler &sampler, const RayGenerator &ray_generator, const RTKernel &kernel) {
    Timer timer;
    size_t its_count = 0;
    for (uint32_t i = 0; i < N*N; i++) {
        Point2f sample = sampler(i, N);
        Ray3f ray      = ray_generator(sample);
        auto res       = kernel(ray);
        its_count     += count(res.first);
    }
    return std::make_pair(timer.value(), its_count);
}

template <typename Sampler, typename RayGenerator, typename RTKernel>
std::pair<Float, size_t>
rt_packet(uint32_t N, const Sampler &sampler, const RayGenerator &ray_generator, const RTKernel &kernel) {
    Timer timer;
    size_t its_count = 0;
    for (auto pair : range<UInt32P>(N*N)) {
        Point2fP sample = sampler(pair.first, N);
        Ray3fP ray      = ray_generator(sample);
        auto res        = kernel(ray);
        its_count      += count(res.first);
    }
    return std::make_pair(timer.value(), its_count);
}

// --------------------------------------------------------------------------------------------

template <typename UInt32> MTS_INLINE auto sample_morton(UInt32 idx, size_t N) {
    using Float = float_array_t<UInt32>;
    using Vector2u = Vector<UInt32, 2>;
    using Point2f = Point<Float, 2>;

    auto p = enoki::morton_decode<Vector2u>(idx);
    return Point2f(p) * (1.f / N) * 2.f - Point2f(1.f);
}

template <typename UInt32> MTS_INLINE auto sample_independent(UInt32 idx, size_t) {
    using Float = float_array_t<UInt32>;
    using Point2f = Point<Float, 2>;

    return Point2f(sample_tea_float(idx, UInt32(1)),
                   sample_tea_float(idx, UInt32(2)));
}

// --------------------------------------------------------------------------------------------

template <typename Point2f> auto gen_ray_planar(const ShapeKDTree *kdtree) {
    using Point3f = point3_t<Point2f>;
    using Ray3f = Ray<Point3f>;

    BoundingBox3f b = kdtree->bbox();

    return [b](const Point2f &sample) -> Ray3f {
        return Ray3f(Point3f(b.min.x() * (1 - sample.x()) + b.max.x() * sample.x(),
                             b.min.y() * (1 - sample.y()) + b.max.y() * sample.y(),
                             b.min.z()), Vector3f(0, 0, 1));
    };
}

template <typename Point2f> auto gen_ray_sphere(const ShapeKDTree *kdtree) {
    using Point3f = point3_t<Point2f>;
    using Ray3f = Ray<Point3f>;

    BoundingBox3f b = kdtree->bbox();
    Point3f center = b.center();
    Float radius = norm(b.extents()) * 0.5f;

    return [center, radius](const Point2f &sample) -> Ray3f {
        auto v = warp::square_to_uniform_sphere(sample);
        return Ray3f(center + v * radius, -v);
    };
}

// --------------------------------------------------------------------------------------------

template <typename Ray3f, bool IsShadowRay = false> auto kernel_pbrt(const ShapeKDTree *kdtree) {
    using Point = typename Ray3f::Point;
    using Scalar = value_t<Point>;

    return [kdtree] (Ray3f rays) {
        MTS_MAKE_KD_CACHE(cache);
        return kdtree->ray_intersect_pbrt<IsShadowRay>(
            rays, Scalar(0.f), Scalar(math::Infinity),
            (void *) cache, true);
    };
}

template <bool IsShadowRay = false> auto kernel_havran(const ShapeKDTree *kdtree) {
    return [kdtree] (Ray3f rays) {
        MTS_MAKE_KD_CACHE(cache);
        // TODO: should return consistent results even for maxt = +inf
        const Float maxt = norm(kdtree->bbox().extents());
        return kdtree->ray_intersect_havran<IsShadowRay>(rays, 0.f, maxt,
                                                         (void *) cache);
    };
}

template <typename Ray3f, bool IsShadowRay = false> auto kernel_dummy(const ShapeKDTree *kdtree) {
    using Point  = typename Ray3f::Point;
    using Scalar = value_t<Point>;

    return [kdtree] (Ray3f rays) {
        MTS_MAKE_KD_CACHE(cache);
        return kdtree->ray_intersect_dummy(
            rays, Scalar(0.f), Scalar(math::Infinity),
            (void *) cache, true);
    };
}

// --------------------------------------------------------------------------------------------

// pbrt routines

std::pair<Float, size_t> rt_pbrt_planar_morton_scalar(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_morton<uint32_t>, gen_ray_planar<Point2f>(kdtree), kernel_pbrt<Ray3f>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_planar_morton_packet(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_morton<UInt32P>, gen_ray_planar<Point2fP>(kdtree), kernel_pbrt<Ray3fP>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_planar_morton_scalar_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_morton<uint32_t>, gen_ray_planar<Point2f>(kdtree), kernel_pbrt<Ray3f, true>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_planar_morton_packet_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_morton<UInt32P>, gen_ray_planar<Point2fP>(kdtree), kernel_pbrt<Ray3fP, true>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_spherical_morton_scalar(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_morton<uint32_t>, gen_ray_sphere<Point2f>(kdtree), kernel_pbrt<Ray3f>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_spherical_morton_packet(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_morton<UInt32P>, gen_ray_sphere<Point2fP>(kdtree), kernel_pbrt<Ray3fP>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_spherical_morton_scalar_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_morton<uint32_t>, gen_ray_sphere<Point2f>(kdtree), kernel_pbrt<Ray3f, true>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_spherical_morton_packet_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_morton<UInt32P>, gen_ray_sphere<Point2fP>(kdtree), kernel_pbrt<Ray3fP, true>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_planar_independent_scalar(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_independent<uint32_t>, gen_ray_planar<Point2f>(kdtree), kernel_pbrt<Ray3f>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_planar_independent_packet(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_independent<UInt32P>, gen_ray_planar<Point2fP>(kdtree), kernel_pbrt<Ray3fP>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_planar_independent_scalar_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_independent<uint32_t>, gen_ray_planar<Point2f>(kdtree), kernel_pbrt<Ray3f, true>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_planar_independent_packet_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_independent<UInt32P>, gen_ray_planar<Point2fP>(kdtree), kernel_pbrt<Ray3fP, true>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_spherical_independent_scalar(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_independent<uint32_t>, gen_ray_sphere<Point2f>(kdtree), kernel_pbrt<Ray3f>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_spherical_independent_packet(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_independent<UInt32P>, gen_ray_sphere<Point2fP>(kdtree), kernel_pbrt<Ray3fP>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_spherical_independent_scalar_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_independent<uint32_t>, gen_ray_sphere<Point2f>(kdtree), kernel_pbrt<Ray3f, true>(kdtree));
}

std::pair<Float, size_t> rt_pbrt_spherical_independent_packet_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_independent<UInt32P>, gen_ray_sphere<Point2fP>(kdtree), kernel_pbrt<Ray3fP, true>(kdtree));
}

// Havran routines

std::pair<Float, size_t> rt_havran_planar_morton_scalar(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_morton<uint32_t>, gen_ray_planar<Point2f>(kdtree), kernel_havran(kdtree));
}

std::pair<Float, size_t> rt_havran_planar_morton_scalar_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_morton<uint32_t>, gen_ray_planar<Point2f>(kdtree), kernel_havran<true>(kdtree));
}

std::pair<Float, size_t> rt_havran_spherical_morton_scalar(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_morton<uint32_t>, gen_ray_sphere<Point2f>(kdtree), kernel_havran(kdtree));
}

std::pair<Float, size_t> rt_havran_spherical_morton_scalar_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_morton<uint32_t>, gen_ray_sphere<Point2f>(kdtree), kernel_havran<true>(kdtree));
}

std::pair<Float, size_t> rt_havran_planar_independent_scalar(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_independent<uint32_t>, gen_ray_planar<Point2f>(kdtree), kernel_havran(kdtree));
}

std::pair<Float, size_t> rt_havran_planar_independent_scalar_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_independent<uint32_t>, gen_ray_planar<Point2f>(kdtree), kernel_havran<true>(kdtree));
}

std::pair<Float, size_t> rt_havran_spherical_independent_scalar(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_independent<uint32_t>, gen_ray_sphere<Point2f>(kdtree), kernel_havran(kdtree));
}

std::pair<Float, size_t> rt_havran_spherical_independent_scalar_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_independent<uint32_t>, gen_ray_sphere<Point2f>(kdtree), kernel_havran<true>(kdtree));
}

// Dummy routines

std::pair<Float, size_t> rt_dummy_planar_morton_scalar(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_morton<uint32_t>, gen_ray_planar<Point2f>(kdtree), kernel_dummy<Ray3f>(kdtree));
}

std::pair<Float, size_t> rt_dummy_planar_morton_packet(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_morton<UInt32P>, gen_ray_planar<Point2fP>(kdtree), kernel_dummy<Ray3fP>(kdtree));
}

std::pair<Float, size_t> rt_dummy_planar_morton_scalar_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_morton<uint32_t>, gen_ray_planar<Point2f>(kdtree), kernel_dummy<Ray3f, true>(kdtree));
}

std::pair<Float, size_t> rt_dummy_planar_morton_packet_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_morton<UInt32P>, gen_ray_planar<Point2fP>(kdtree), kernel_dummy<Ray3fP, true>(kdtree));
}

std::pair<Float, size_t> rt_dummy_spherical_morton_scalar(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_morton<uint32_t>, gen_ray_sphere<Point2f>(kdtree), kernel_dummy<Ray3f>(kdtree));
}

std::pair<Float, size_t> rt_dummy_spherical_morton_packet(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_morton<UInt32P>, gen_ray_sphere<Point2fP>(kdtree), kernel_dummy<Ray3fP>(kdtree));
}

std::pair<Float, size_t> rt_dummy_spherical_morton_scalar_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_morton<uint32_t>, gen_ray_sphere<Point2f>(kdtree), kernel_dummy<Ray3f, true>(kdtree));
}

std::pair<Float, size_t> rt_dummy_spherical_morton_packet_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_morton<UInt32P>, gen_ray_sphere<Point2fP>(kdtree), kernel_dummy<Ray3fP, true>(kdtree));
}

std::pair<Float, size_t> rt_dummy_planar_independent_scalar(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_independent<uint32_t>, gen_ray_planar<Point2f>(kdtree), kernel_dummy<Ray3f>(kdtree));
}

std::pair<Float, size_t> rt_dummy_planar_independent_packet(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_independent<UInt32P>, gen_ray_planar<Point2fP>(kdtree), kernel_dummy<Ray3fP>(kdtree));
}

std::pair<Float, size_t> rt_dummy_planar_independent_scalar_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_independent<uint32_t>, gen_ray_planar<Point2f>(kdtree), kernel_dummy<Ray3f, true>(kdtree));
}

std::pair<Float, size_t> rt_dummy_planar_independent_packet_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_independent<UInt32P>, gen_ray_planar<Point2fP>(kdtree), kernel_dummy<Ray3fP, true>(kdtree));
}

std::pair<Float, size_t> rt_dummy_spherical_independent_scalar(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_independent<uint32_t>, gen_ray_sphere<Point2f>(kdtree), kernel_dummy<Ray3f>(kdtree));
}

std::pair<Float, size_t> rt_dummy_spherical_independent_packet(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_independent<UInt32P>, gen_ray_sphere<Point2fP>(kdtree), kernel_dummy<Ray3fP>(kdtree));
}

std::pair<Float, size_t> rt_dummy_spherical_independent_scalar_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_scalar(N, sample_independent<uint32_t>, gen_ray_sphere<Point2f>(kdtree), kernel_dummy<Ray3f, true>(kdtree));
}

std::pair<Float, size_t> rt_dummy_spherical_independent_packet_shadow(const ShapeKDTree *kdtree, uint32_t N) {
    return rt_packet(N, sample_independent<UInt32P>, gen_ray_sphere<Point2fP>(kdtree), kernel_dummy<Ray3fP, true>(kdtree));
}

NAMESPACE_END(rt)
NAMESPACE_END(mitsuba)
