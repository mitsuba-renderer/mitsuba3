#include <mitsuba/render/rtbench.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/random.h>
#include <enoki/morton.h>

#if !defined(MTS_USE_EMBREE)

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(rtbench)

// --------------------------------------------------------------------------------------------

template <typename Sampler, typename RayGenerator, typename RTKernel>
std::pair<Float, size_t>
run_scalar(uint32_t N, const Sampler &sampler, const RayGenerator &ray_generator, const RTKernel &kernel) {
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
run_packet(uint32_t N, const Sampler &sampler, const RayGenerator &ray_generator, const RTKernel &kernel) {
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

template <typename UInt32,
          typename Float = float_array_t<UInt32>,
          typename Point2f = Point<Float, 2>>
MTS_INLINE Point2f sample_morton(UInt32 idx, size_t N) {
    using Vector2u = Vector<UInt32, 2>;
    auto p = enoki::morton_decode<Vector2u>(idx);
    return Point2f(p) * (1.f / N) * 2.f - Point2f(1.f);
}

template <typename UInt32,
          typename Float = float_array_t<UInt32>,
          typename Point2f = Point<Float, 2>>
MTS_INLINE Point2f sample_independent(UInt32 idx, size_t) {
    return { sample_tea_float(idx, UInt32(1)),
             sample_tea_float(idx, UInt32(2)) };
}

// --------------------------------------------------------------------------------------------

template <typename Point2> auto gen_ray_planar(const Scene *scene) {
    using Point3 = point3_t<Point2>;
    using Ray3 = Ray<Point3>;

    BoundingBox3f b = scene->bbox();

    return [b](const Point2 &sample) -> Ray3 {
        return Ray3(Point3(b.min.x() * (1 - sample.x()) + b.max.x() * sample.x(),
                             b.min.y() * (1 - sample.y()) + b.max.y() * sample.y(),
                             b.min.z()), Vector3f(0, 0, 1), 0, Spectrumf());
    };
}

template <typename Point2> auto gen_ray_sphere(const Scene *scene) {
    using Point3 = point3_t<Point2>;
    using Ray3 = Ray<Point3>;

    BoundingBox3f b = scene->bbox();
    Point3 center = b.center();
    Float radius = norm(b.extents()) * .5f;

    return [center, radius](const Point2 &sample) -> Ray3 {
        auto v = warp::square_to_uniform_sphere(sample);
        return Ray3(center + v * radius, -v, 0, Spectrumf());
    };
}

// --------------------------------------------------------------------------------------------

template <typename Ray3, bool ShadowRay = false> auto kernel(const Scene *scene) {
    auto kdtree = scene->kdtree();
    return [kdtree](const Ray3 &rays) {
        typename Ray3::Value cache[MTS_KD_INTERSECTION_CACHE_SIZE];
        return kdtree->ray_intersect<ShadowRay>(rays, cache);
    };
}

template <typename Ray3, bool ShadowRay = false> auto kernel_naive(const Scene *scene) {
    auto kdtree = scene->kdtree();
    return [kdtree](const Ray3 &rays) {
        typename Ray3::Value cache[MTS_KD_INTERSECTION_CACHE_SIZE];
        return kdtree->ray_intersect_naive<ShadowRay>(rays, cache);
    };
}

// --------------------------------------------------------------------------------------------

// pbrt routines

std::pair<Float, size_t> planar_morton_scalar(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_morton<uint32_t>, gen_ray_planar<Point2f>(scene), kernel<Ray3f>(scene));
}

std::pair<Float, size_t> planar_morton_packet(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_morton<UInt32P>, gen_ray_planar<Point2fP>(scene), kernel<Ray3fP>(scene));
}

std::pair<Float, size_t> planar_morton_scalar_shadow(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_morton<uint32_t>, gen_ray_planar<Point2f>(scene), kernel<Ray3f, true>(scene));
}

std::pair<Float, size_t> planar_morton_packet_shadow(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_morton<UInt32P>, gen_ray_planar<Point2fP>(scene), kernel<Ray3fP, true>(scene));
}

std::pair<Float, size_t> spherical_morton_scalar(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_morton<uint32_t>, gen_ray_sphere<Point2f>(scene), kernel<Ray3f>(scene));
}

std::pair<Float, size_t> spherical_morton_packet(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_morton<UInt32P>, gen_ray_sphere<Point2fP>(scene), kernel<Ray3fP>(scene));
}

std::pair<Float, size_t> spherical_morton_scalar_shadow(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_morton<uint32_t>, gen_ray_sphere<Point2f>(scene), kernel<Ray3f, true>(scene));
}

std::pair<Float, size_t> spherical_morton_packet_shadow(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_morton<UInt32P>, gen_ray_sphere<Point2fP>(scene), kernel<Ray3fP, true>(scene));
}

std::pair<Float, size_t> planar_independent_scalar(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_independent<uint32_t>, gen_ray_planar<Point2f>(scene), kernel<Ray3f>(scene));
}

std::pair<Float, size_t> planar_independent_packet(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_independent<UInt32P>, gen_ray_planar<Point2fP>(scene), kernel<Ray3fP>(scene));
}

std::pair<Float, size_t> planar_independent_scalar_shadow(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_independent<uint32_t>, gen_ray_planar<Point2f>(scene), kernel<Ray3f, true>(scene));
}

std::pair<Float, size_t> planar_independent_packet_shadow(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_independent<UInt32P>, gen_ray_planar<Point2fP>(scene), kernel<Ray3fP, true>(scene));
}

std::pair<Float, size_t> spherical_independent_scalar(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_independent<uint32_t>, gen_ray_sphere<Point2f>(scene), kernel<Ray3f>(scene));
}

std::pair<Float, size_t> spherical_independent_packet(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_independent<UInt32P>, gen_ray_sphere<Point2fP>(scene), kernel<Ray3fP>(scene));
}

std::pair<Float, size_t> spherical_independent_scalar_shadow(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_independent<uint32_t>, gen_ray_sphere<Point2f>(scene), kernel<Ray3f, true>(scene));
}

std::pair<Float, size_t> spherical_independent_packet_shadow(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_independent<UInt32P>, gen_ray_sphere<Point2fP>(scene), kernel<Ray3fP, true>(scene));
}

// Dummy routines

std::pair<Float, size_t> naive_planar_morton_scalar(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_morton<uint32_t>, gen_ray_planar<Point2f>(scene), kernel_naive<Ray3f>(scene));
}

std::pair<Float, size_t> naive_planar_morton_packet(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_morton<UInt32P>, gen_ray_planar<Point2fP>(scene), kernel_naive<Ray3fP>(scene));
}

std::pair<Float, size_t> naive_planar_morton_scalar_shadow(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_morton<uint32_t>, gen_ray_planar<Point2f>(scene), kernel_naive<Ray3f, true>(scene));
}

std::pair<Float, size_t> naive_planar_morton_packet_shadow(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_morton<UInt32P>, gen_ray_planar<Point2fP>(scene), kernel_naive<Ray3fP, true>(scene));
}

std::pair<Float, size_t> naive_spherical_morton_scalar(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_morton<uint32_t>, gen_ray_sphere<Point2f>(scene), kernel_naive<Ray3f>(scene));
}

std::pair<Float, size_t> naive_spherical_morton_packet(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_morton<UInt32P>, gen_ray_sphere<Point2fP>(scene), kernel_naive<Ray3fP>(scene));
}

std::pair<Float, size_t> naive_spherical_morton_scalar_shadow(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_morton<uint32_t>, gen_ray_sphere<Point2f>(scene), kernel_naive<Ray3f, true>(scene));
}

std::pair<Float, size_t> naive_spherical_morton_packet_shadow(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_morton<UInt32P>, gen_ray_sphere<Point2fP>(scene), kernel_naive<Ray3fP, true>(scene));
}

std::pair<Float, size_t> naive_planar_independent_scalar(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_independent<uint32_t>, gen_ray_planar<Point2f>(scene), kernel_naive<Ray3f>(scene));
}

std::pair<Float, size_t> naive_planar_independent_packet(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_independent<UInt32P>, gen_ray_planar<Point2fP>(scene), kernel_naive<Ray3fP>(scene));
}

std::pair<Float, size_t> naive_planar_independent_scalar_shadow(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_independent<uint32_t>, gen_ray_planar<Point2f>(scene), kernel_naive<Ray3f, true>(scene));
}

std::pair<Float, size_t> naive_planar_independent_packet_shadow(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_independent<UInt32P>, gen_ray_planar<Point2fP>(scene), kernel_naive<Ray3fP, true>(scene));
}

std::pair<Float, size_t> naive_spherical_independent_scalar(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_independent<uint32_t>, gen_ray_sphere<Point2f>(scene), kernel_naive<Ray3f>(scene));
}

std::pair<Float, size_t> naive_spherical_independent_packet(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_independent<UInt32P>, gen_ray_sphere<Point2fP>(scene), kernel_naive<Ray3fP>(scene));
}

std::pair<Float, size_t> naive_spherical_independent_scalar_shadow(const Scene *scene, uint32_t N) {
    return run_scalar(N, sample_independent<uint32_t>, gen_ray_sphere<Point2f>(scene), kernel_naive<Ray3f, true>(scene));
}

std::pair<Float, size_t> naive_spherical_independent_packet_shadow(const Scene *scene, uint32_t N) {
    return run_packet(N, sample_independent<UInt32P>, gen_ray_sphere<Point2fP>(scene), kernel_naive<Ray3fP, true>(scene));
}

NAMESPACE_END(rtbench)
NAMESPACE_END(mitsuba)

#endif
