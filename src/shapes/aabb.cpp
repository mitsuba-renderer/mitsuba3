#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

#if defined(MI_ENABLE_CUDA)
    #include "optix/aabb.cuh"
#endif

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-aabb:

Axis-aligned cube (:monosp:`aabb`)
-------------------------------------------------

This shape plugin describes a simple axis-aligned cube shape.

TODO: documentation.
 */

template <typename Float, typename Spectrum>
class AxisAlignedBox final : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_to_world, m_to_object, m_is_instance, initialize,
                   mark_dirty, get_children_string, parameters_grad_enabled)
    MI_IMPORT_TYPES(ShapePtr)

    using typename Base::ScalarSize;

    AxisAlignedBox(const Properties &props) : Base(props) {
        /// Are the box's normals pointing inwards? default: no
        m_flip_normals = props.get<bool>("flip_normals", false);

        update();
        initialize();
    }

    void update() {
        // TODO: ensure that there is only translation and scaling in m_to_world

        m_bbox = BoundingBox3f(
            m_to_world.scalar() * ScalarPoint3f(0.f),
            m_to_world.scalar() * ScalarPoint3f(1.f)
        );

        m_inv_surface_area = dr::rcp(surface_area());

        dr::make_opaque(m_bbox);
        mark_dirty();
    }

    ScalarBoundingBox3f bbox() const override {
        return m_bbox.scalar();
    }

    Float surface_area() const override {
        return m_bbox.value().surface_area();
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        NotImplementedError("sample_position");
#if 0
        Point3f local = warp::square_to_uniform_sphere(sample);

        PositionSample3f ps = dr::zeros<PositionSample3f>();
        ps.p = dr::fmadd(local, m_radius.value(), m_center.value());
        ps.n = local;

        if (m_flip_normals)
            ps.n = -ps.n;

        ps.time = time;
        ps.delta = m_radius.value() == 0.f;
        ps.pdf = m_inv_surface_area;
        ps.uv = sample;

        return ps;
#endif
    }

    Float pdf_position(const PositionSample3f & /*ps*/, Mask active) const override {
        MI_MASK_ARGUMENT(active);
        return m_inv_surface_area;
    }

    DirectionSample3f sample_direction(const Interaction3f &it, const Point2f &sample,
                                       Mask active) const override {
        MI_MASK_ARGUMENT(active);

        NotImplementedError("sample_direction");
#if 0
        DirectionSample3f result = dr::zeros<DirectionSample3f>();

        Vector3f dc_v = m_center.value() - it.p;
        Float dc_2 = dr::squared_norm(dc_v);

        Float radius_adj = m_radius.value() * (m_flip_normals ?
                                               (1.f + math::RayEpsilon<Float>) :
                                               (1.f - math::RayEpsilon<Float>));
        Mask outside_mask = active && dc_2 > dr::sqr(radius_adj);
        if (likely(dr::any_or<true>(outside_mask))) {
            Float inv_dc            = dr::rsqrt(dc_2),
                  sin_theta_max     = m_radius.value() * inv_dc,
                  sin_theta_max_2   = dr::sqr(sin_theta_max),
                  inv_sin_theta_max = dr::rcp(sin_theta_max),
                  cos_theta_max     = dr::safe_sqrt(1.f - sin_theta_max_2);

            /* Fall back to a Taylor series expansion for small angles, where
               the standard approach suffers from severe cancellation errors */
            Float sin_theta_2 = dr::select(sin_theta_max_2 > 0.00068523f, /* sin^2(1.5 deg) */
                                       1.f - dr::sqr(dr::fmadd(cos_theta_max - 1.f, sample.x(), 1.f)),
                                       sin_theta_max_2 * sample.x()),
                  cos_theta = dr::safe_sqrt(1.f - sin_theta_2);

            // Based on https://www.akalin.com/sampling-visible-sphere
            Float cos_alpha = sin_theta_2 * inv_sin_theta_max +
                              cos_theta * dr::safe_sqrt(dr::fnmadd(sin_theta_2, dr::sqr(inv_sin_theta_max), 1.f)),
                  sin_alpha = dr::safe_sqrt(dr::fnmadd(cos_alpha, cos_alpha, 1.f));

            auto [sin_phi, cos_phi] = dr::sincos(sample.y() * (2.f * dr::Pi<Float>));

            Vector3f d = Frame3f(dc_v * -inv_dc).to_world(Vector3f(
                cos_phi * sin_alpha,
                sin_phi * sin_alpha,
                cos_alpha));

            DirectionSample3f ds = dr::zeros<DirectionSample3f>();
            ds.p        = dr::fmadd(d, m_radius.value(), m_center.value());
            ds.n        = d;
            ds.d        = ds.p - it.p;

            Float dist2 = dr::squared_norm(ds.d);
            ds.dist     = dr::sqrt(dist2);
            ds.d        = ds.d / ds.dist;
            ds.pdf      = warp::square_to_uniform_cone_pdf(dr::zeros<Vector3f>(), cos_theta_max);
            dr::masked(ds.pdf, dr::eq(ds.dist, 0.f)) = 0.f;

            dr::masked(result, outside_mask) = ds;
        }

        Mask inside_mask = dr::andnot(active, outside_mask);
        if (unlikely(dr::any_or<true>(inside_mask))) {
            Vector3f d = warp::square_to_uniform_sphere(sample);
            DirectionSample3f ds = dr::zeros<DirectionSample3f>();
            ds.p        = dr::fmadd(d, m_radius.value(), m_center.value());
            ds.n        = d;
            ds.d        = ds.p - it.p;

            Float dist2 = dr::squared_norm(ds.d);
            ds.dist     = dr::sqrt(dist2);
            ds.d        = ds.d / ds.dist;
            ds.pdf      = m_inv_surface_area * dist2 / dr::abs_dot(ds.d, ds.n);

            dr::masked(result, inside_mask) = ds;
        }

        result.time = it.time;
        result.delta = m_radius.value() == 0.f;

        if (m_flip_normals)
            result.n = -result.n;

        return result;
    #endif
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        MI_MASK_ARGUMENT(active);

        NotImplementedError("sample_direction");
#if 0
        // Sine of the angle of the cone containing the sphere as seen from 'it.p'.
        Float sin_alpha = m_radius.value() * dr::rcp(dr::norm(m_center.value() - it.p)),
              cos_alpha = dr::safe_sqrt(1.f - sin_alpha * sin_alpha);

        return dr::select(sin_alpha < dr::OneMinusEpsilon<Float>,
            // Reference point lies outside the sphere
            warp::square_to_uniform_cone_pdf(dr::zeros<Vector3f>(), cos_alpha),
            m_inv_surface_area * dr::sqr(ds.dist) / dr::abs_dot(ds.d, ds.n)
        );
#endif
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    template <typename FloatP, typename Ray3fP>
    std::tuple<FloatP, Point<FloatP, 2>, dr::uint32_array_t<FloatP>,
               dr::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray,
                                   dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);
        using Value = std::conditional_t<dr::is_cuda_v<FloatP> ||
                                              dr::is_diff_v<Float>,
                                          dr::float32_array_t<FloatP>,
                                          dr::float64_array_t<FloatP>>;

        const auto &bbox = m_bbox.value();
        auto [hit, mint, maxt] = bbox.ray_intersect(ray);
        dr::mask_t<Value> starts_outside = mint > 0.f;
        Value t = dr::select(starts_outside, mint, maxt);
        hit &= active && (t <= ray.maxt) && (t > math::RayEpsilon<Value>);
        t = dr::select(hit, t, dr::Infinity<Value>);

        // TODO: UVs, shape index, instance index?

        return { t, dr::zeros<Point<FloatP, 2>>(), ((uint32_t) -1), 0 };
    }

    template <typename FloatP, typename Ray3fP>
    dr::mask_t<FloatP> ray_test_impl(const Ray3fP &ray,
                                     dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);
        using Value =
            std::conditional_t<dr::is_cuda_v<FloatP> || dr::is_diff_v<Float>,
                               dr::float32_array_t<FloatP>,
                               dr::float64_array_t<FloatP>>;
        using Mask = dr::mask_t<Value>;

        auto [hit, mint, maxt] = m_bbox.value().ray_intersect(ray);
        Mask starts_outside    = mint > 0.f;
        Value t                = dr::select(starts_outside, mint, maxt);
        return active && hit && (t <= ray.maxt) &&
               (t > math::RayEpsilon<Value>);
    }

    MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     uint32_t recursion_depth,
                                                     Mask active) const override {
        // using ShapePtr = dr::replace_scalar_t<Float, const Shape *>;
        MI_MASK_ARGUMENT(active);
        // TODO
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.t = pi.t;
        si.time = ray.time;
        si.wavelengths = ray.wavelengths;
        si.p = ray(si.t);

        // Normal vector: assuming axis-aligned bbox, figure
        // out the normal direction based on the relative position
        // of the intersection point to the bbox's center.
        const auto &bbox = m_bbox.value();
        Point3f p_local = (si.p - bbox.center()) / bbox.extents();
        // The axis with the largest local coordinate (magnitude)
        // is the axis of the normal vector.
        Point3f p_local_abs = dr::abs(p_local);
        Float vmax = dr::max(p_local_abs);
        Normal3f n(dr::eq(p_local_abs.x(), vmax), dr::eq(p_local_abs.y(), vmax),
                   dr::eq(p_local_abs.z(), vmax));
        Mask hit = pi.is_valid();
        // Normal always points to the outside of the bbox, independently
        // of the ray direction.
        n = dr::normalize(dr::sign(p_local) * n);
        si.n = dr::select(hit, n, -ray.d);

        si.shape = dr::select(hit, dr::opaque<ShapePtr>(this), dr::zeros<ShapePtr>());
        si.uv = 0.f;  // TODO: proper UVs
        si.sh_frame.n = si.n;
        if (has_flag(ray_flags, RayFlags::ShadingFrame))
            si.initialize_sh_frame();
        si.wi = dr::select(hit, si.to_local(-ray.d), -ray.d);
        return si;

#if 0
        // Early exit when tracing isn't necessary
        if (!m_is_instance && recursion_depth > 0)
            return dr::zeros<SurfaceInteraction3f>();

        // Recompute ray intersection to get differentiable t
        Float t = pi.t;
        if constexpr (dr::is_diff_v<Float>)
            t = dr::replace_grad(t, ray_intersect_preliminary(ray, active).t);

        // TODO handle RayFlags::FollowShape and RayFlags::DetachShape

        // Fields requirement dependencies
        bool need_dn_duv = has_flag(ray_flags, RayFlags::dNSdUV) ||
                           has_flag(ray_flags, RayFlags::dNGdUV);
        bool need_dp_duv = has_flag(ray_flags, RayFlags::dPdUV) || need_dn_duv;
        bool need_uv     = has_flag(ray_flags, RayFlags::UV) || need_dp_duv;

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.t = dr::select(active, t, dr::Infinity<Float>);

        si.sh_frame.n = dr::normalize(ray(t) - m_center.value());

        // Re-project onto the sphere to improve accuracy
        si.p = dr::fmadd(si.sh_frame.n, m_radius.value(), m_center.value());

        if (likely(need_uv)) {
            Vector3f local = m_to_object.value().transform_affine(si.p);

            Float rd_2  = dr::sqr(local.x()) + dr::sqr(local.y()),
                  theta = unit_angle_z(local),
                  phi   = dr::atan2(local.y(), local.x());

            dr::masked(phi, phi < 0.f) += 2.f * dr::Pi<Float>;

            si.uv = Point2f(phi * dr::InvTwoPi<Float>, theta * dr::InvPi<Float>);
            if (likely(need_dp_duv)) {
                si.dp_du = Vector3f(-local.y(), local.x(), 0.f);

                Float rd      = dr::sqrt(rd_2),
                      inv_rd  = dr::rcp(rd),
                      cos_phi = local.x() * inv_rd,
                      sin_phi = local.y() * inv_rd;

                si.dp_dv = Vector3f(local.z() * cos_phi,
                                    local.z() * sin_phi,
                                    -rd);

                Mask singularity_mask = active && dr::eq(rd, 0.f);
                if (unlikely(dr::any_or<true>(singularity_mask)))
                    si.dp_dv[singularity_mask] = Vector3f(1.f, 0.f, 0.f);

                si.dp_du = m_to_world.value() * si.dp_du * (2.f * dr::Pi<Float>);
                si.dp_dv = m_to_world.value() * si.dp_dv * dr::Pi<Float>;
            }
        }

        if (m_flip_normals)
            si.sh_frame.n = -si.sh_frame.n;

        si.n = si.sh_frame.n;

        if (need_dn_duv) {
            Float inv_radius =
                (m_flip_normals ? -1.f : 1.f) * dr::rcp(m_radius.value());
            si.dn_du = si.dp_du * inv_radius;
            si.dn_dv = si.dp_dv * inv_radius;
        }

        si.shape    = this;
        si.instance = nullptr;

        if (unlikely(has_flag(ray_flags, RayFlags::BoundaryTest)))
            si.boundary_test = dr::abs(dr::dot(si.sh_frame.n, -ray.d));

        return si;
#endif
    }

    //! @}
    // =============================================================

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("to_world", *m_to_world.ptr(), +ParamFlags::NonDifferentiable);
        Base::traverse(callback);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "to_world")) {
            // Update the scalar value of the matrix
            m_to_world = m_to_world.value();
            update();
        }
        Base::parameters_changed();
    }

#if defined(MI_ENABLE_CUDA)
    using Base::m_optix_data_ptr;

    void optix_prepare_geometry() override {
        if constexpr (dr::is_cuda_v<Float>) {
            NotImplementedError("optix_prepare_geometry");
            if (!m_optix_data_ptr)
                m_optix_data_ptr = jit_malloc(AllocType::Device, sizeof(OptixAABBData));

            OptixAABBData data = { bbox() };
            jit_memcpy(JitBackend::CUDA, m_optix_data_ptr, &data,
                       sizeof(OptixAABBData));
        }
    }
#endif

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "AxisAlignedBox[" << std::endl
            << "  bbox = " << string::indent(m_bbox, 13) << "," << std::endl
            << "  to_world = " << string::indent(m_to_world, 13) << "," << std::endl
            << "  surface_area = " << surface_area() << "," << std::endl
            << "  " << string::indent(get_children_string()) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    /// Axis-aligned bounding box in world space
    field<BoundingBox3f, ScalarBoundingBox3f> m_bbox;
    Float m_inv_surface_area;
    bool m_flip_normals;
};

MI_IMPLEMENT_CLASS_VARIANT(AxisAlignedBox, Shape)
MI_EXPORT_PLUGIN(AxisAlignedBox, "AxisAlignedBox intersection primitive");
NAMESPACE_END(mitsuba)
