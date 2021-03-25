#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

#include <enoki/packet.h>

#if defined(MTS_ENABLE_CUDA)
    #include "optix/cylinder.cuh"
#endif

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-cylinder:

Cylinder (:monosp:`cylinder`)
----------------------------------------------------

.. pluginparameters::


 * - p0
   - |point|
   - Object-space starting point of the cylinder's centerline.
     (Default: (0, 0, 0))
 * - p1
   - |point|
   - Object-space endpoint of the cylinder's centerline (Default: (0, 0, 1))
 * - radius
   - |float|
   - Radius of the cylinder in object-space units (Default: 1)
 * - flip_normals
   - |bool|
   -  Is the cylinder inverted, i.e. should the normal vectors
      be flipped? (Default: |false|, i.e. the normals point outside)
 * - to_world
   - |transform|
   - Specifies an optional linear object-to-world transformation. Note that non-uniform scales are
     not permitted! (Default: none, i.e. object space = world space)

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/shape_cylinder_onesided.jpg
   :caption: Cylinder with the default one-sided shading
.. subfigure:: ../../resources/data/docs/images/render/shape_cylinder_twosided.jpg
   :caption: Cylinder with two-sided shading
.. subfigend::
   :label: fig-cylinder

This shape plugin describes a simple cylinder intersection primitive.
It should always be preferred over approximations modeled using
triangles. Note that the cylinder does not have endcaps -- also,
its normals point outward, which means that the inside will be treated
as fully absorbing by most material models. If this is not
desirable, consider using the :ref:`twosided <bsdf-twosided>` plugin.

A simple example for instantiating a cylinder, whose interior is visible:

.. code-block:: xml

    <shape type="cylinder">
        <float name="radius" value="0.3"/>
        <bsdf type="twosided">
            <bsdf type="diffuse"/>
        </bsdf>
    </shape>
 */

template <typename Float, typename Spectrum>
class Cylinder final : public Shape<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Shape, m_to_world, m_to_object, set_children,
                    get_children_string, parameters_grad_enabled)
    MTS_IMPORT_TYPES()

    using typename Base::ScalarIndex;
    using typename Base::ScalarSize;

    Cylinder(const Properties &props) : Base(props) {
        /// Are the sphere normals pointing inwards? default: no
        m_flip_normals = props.bool_("flip_normals", false);

        // Update the to_world transform if face points and radius are also provided
        float radius = props.float_("radius", 1.f);
        ScalarPoint3f p0 = props.point3f("p0", ScalarPoint3f(0.f, 0.f, 0.f)),
                      p1 = props.point3f("p1", ScalarPoint3f(0.f, 0.f, 1.f));

        m_to_world = m_to_world * ScalarTransform4f::translate(p0) *
                                  ScalarTransform4f::to_frame(ScalarFrame3f(p1 - p0)) *
                                  ScalarTransform4f::scale(ScalarVector3f(radius, radius, 1.f));

        update();
        set_children();
    }

    void update() {
         // Extract center and radius from to_world matrix (25 iterations for numerical accuracy)
        auto [S, Q, T] = transform_decompose(m_to_world.matrix, 25);

        if (ek::abs(S[0][1]) > 1e-6f || ek::abs(S[0][2]) > 1e-6f || ek::abs(S[1][0]) > 1e-6f ||
            ek::abs(S[1][2]) > 1e-6f || ek::abs(S[2][0]) > 1e-6f || ek::abs(S[2][1]) > 1e-6f)
            Log(Warn, "'to_world' transform shouldn't contain any shearing!");

        if (!(ek::abs(S[0][0] - S[1][1]) < 1e-6f))
            Log(Warn, "'to_world' transform shouldn't contain non-uniform scaling along the X and Y axes!");

        m_radius = S[0][0];
        m_length = S[2][2];

        if (m_radius <= 0.f) {
            m_radius = ek::abs(m_radius);
            m_flip_normals = !m_flip_normals;
        }

        // Reconstruct the to_world transform with uniform scaling and no shear
        m_to_world = ek::transform_compose<ScalarMatrix4f>(ScalarMatrix3f(1.f), Q, T);
        m_to_object = m_to_world.inverse();

        m_inv_surface_area = ek::rcp(surface_area());
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarVector3f x1 = m_to_world * ScalarVector3f(m_radius, 0.f, 0.f),
                       x2 = m_to_world * ScalarVector3f(0.f, m_radius, 0.f),
                       x  = ek::sqrt(ek::sqr(x1) + ek::sqr(x2));

        ScalarPoint3f p0 = m_to_world * ScalarPoint3f(0.f, 0.f, 0.f),
                      p1 = m_to_world * ScalarPoint3f(0.f, 0.f, m_length);

        /* To bound the cylinder, it is sufficient to find the
           smallest box containing the two circles at the endpoints. */
        return ScalarBoundingBox3f(ek::min(p0 - x, p1 - x), ek::max(p0 + x, p1 + x));
    }

    ScalarBoundingBox3f bbox(ScalarIndex /*index*/, const ScalarBoundingBox3f &clip) const override {
        using FloatP8         = ek::Packet<ScalarFloat, 8>;
        using MaskP8          = ek::mask_t<FloatP8>;
        using Point3fP8       = Point<FloatP8, 3>;
        using Vector3fP8      = Vector<FloatP8, 3>;
        using BoundingBox3fP8 = BoundingBox<Point3fP8>;

        ScalarPoint3f cyl_p = m_to_world.transform_affine(ScalarPoint3f(0.f, 0.f, 0.f));
        ScalarVector3f cyl_d =
            m_to_world.transform_affine(ScalarVector3f(0.f, 0.f, m_length));

        // Compute a base bounding box
        ScalarBoundingBox3f bbox(this->bbox());
        bbox.clip(clip);

        /* Now forget about the cylinder ends and intersect an infinite
           cylinder with each bounding box face, then compute a bounding
           box of the resulting ellipses. */
        Point3fP8 face_p = ek::zero<Point3fP8>();
        Vector3fP8 face_n = ek::zero<Vector3fP8>();

        for (size_t i = 0; i < 3; ++i) {
            face_p.entry(i,  i * 2 + 0) = bbox.min.entry(i);
            face_p.entry(i,  i * 2 + 1) = bbox.max.entry(i);
            face_n.entry(i,  i * 2 + 0) = -1.f;
            face_n.entry(i,  i * 2 + 1) = 1.f;
        }

        // Project the cylinder direction onto the plane
        FloatP8 dp   = ek::dot(cyl_d, face_n);
        MaskP8 valid = ek::neq(dp, 0.f);

        // Compute semimajor/minor axes of ellipse
        Vector3fP8 v1 = ek::fnmadd(face_n, dp, cyl_d);
        FloatP8 v1_n2 = ek::squared_norm(v1);
        v1 = ek::select(ek::neq(v1_n2, 0.f), v1 * ek::rsqrt(v1_n2),
                    coordinate_system(face_n).first);
        Vector3fP8 v2 = ek::cross(face_n, v1);

        // Compute length of axes
        v1 *= m_radius / ek::abs(dp);
        v2 *= m_radius;

        // Compute center of ellipse
        FloatP8 t = ek::dot(face_n, face_p - cyl_p) / dp;
        Point3fP8 center = ek::fmadd(Vector3fP8(cyl_d), t, Vector3fP8(cyl_p));
        center[neq(face_n, 0.f)] = face_p;

        // Compute ellipse minima and maxima
        Vector3fP8 x = ek::sqrt(ek::sqr(v1) + ek::sqr(v2));
        BoundingBox3fP8 ellipse_bounds(center - x, center + x);
        MaskP8 ellipse_overlap = valid && bbox.overlaps(ellipse_bounds);
        ellipse_bounds.clip(bbox);

        return ScalarBoundingBox3f(
            hmin_inner(select(ellipse_overlap, ellipse_bounds.min,
                              Point3fP8(ek::Infinity<ScalarFloat>))),
            hmax_inner(select(ellipse_overlap, ellipse_bounds.max,
                              Point3fP8(-ek::Infinity<ScalarFloat>))));
    }

    ScalarFloat surface_area() const override {
        return 2.f * ek::Pi<ScalarFloat> * m_radius * m_length;
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        auto [sin_theta, cos_theta] = ek::sincos(2.f * ek::Pi<Float> * sample.y());

        Point3f p(cos_theta * m_radius,
                  sin_theta * m_radius,
                  sample.x() * m_length);

        Normal3f n(cos_theta, sin_theta, 0.f);

        if (m_flip_normals)
            n *= -1;

        PositionSample3f ps;
        ps.p     = m_to_world.transform_affine(p);
        ps.n     = ek::normalize(n);
        ps.pdf   = m_inv_surface_area;
        ps.time  = time;
        ps.delta = false;
        return ps;
    }

    Float pdf_position(const PositionSample3f & /*ps*/, Mask active) const override {
        MTS_MASK_ARGUMENT(active);
        return m_inv_surface_area;
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    template <typename FloatX, typename Ray3fX>
    std::pair<FloatX, Point<FloatX, 2>>
    ray_intersect_preliminary_impl(const Ray3fX &ray_,
                                   ek::mask_t<FloatX> active) const {
        MTS_MASK_ARGUMENT(active);

        using Double = std::conditional_t<ek::is_cuda_array_v<FloatX> ||
                                              ek::is_diff_array_v<Float>,
                                          FloatX, ek::float64_array_t<FloatX>>;

        Ray3fX ray = m_to_object.transform_affine(ray_);
        Double mint = Double(ray.mint),
               maxt = Double(ray.maxt);

        Double ox = Double(ray.o.x()),
               oy = Double(ray.o.y()),
               oz = Double(ray.o.z()),
               dx = Double(ray.d.x()),
               dy = Double(ray.d.y()),
               dz = Double(ray.d.z());

        ek::scalar_t<Double> radius = ek::scalar_t<Double>(m_radius),
                             length = ek::scalar_t<Double>(m_length);

        Double A = ek::sqr(dx) + ek::sqr(dy),
               B = ek::scalar_t<Double>(2.f) * (dx * ox + dy * oy),
               C = ek::sqr(ox) + ek::sqr(oy) - ek::sqr(radius);

        auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

        // Cylinder doesn't intersect with the segment on the ray
        ek::mask_t<FloatX> out_bounds = !(near_t <= maxt && far_t >= mint); // NaN-aware conditionals

        Double z_pos_near = oz + dz*near_t,
               z_pos_far  = oz + dz*far_t;

        // Cylinder fully contains the segment of the ray
        ek::mask_t<FloatX> in_bounds = near_t < mint && far_t > maxt;

        active &= solution_found && !out_bounds && !in_bounds &&
                  ((z_pos_near >= Double(0.0) && z_pos_near <= length && near_t >= mint) ||
                   (z_pos_far  >= Double(0.0) && z_pos_far <= length  && far_t <= maxt));

        FloatX t =
            ek::select(active,
                       ek::select(z_pos_near >= Double(0.0) && z_pos_near <= length &&
                                      near_t >= mint,
                                  FloatX(near_t), FloatX(far_t)),
                       ek::Infinity<FloatX>);

        return { t, ek::zero<Point<FloatX, 2>>() };
    }

    template <typename FloatX, typename Ray3fX>
    ek::mask_t<FloatX> ray_test_impl(const Ray3fX &ray_,
                                     ek::mask_t<FloatX> active) const {
        MTS_MASK_ARGUMENT(active);

        using Double = std::conditional_t<ek::is_cuda_array_v<FloatX> ||
                                              ek::is_diff_array_v<Float>,
                                          FloatX, ek::float64_array_t<FloatX>>;

        Ray3fX ray = m_to_object.transform_affine(ray_);
        Double mint = Double(ray.mint);
        Double maxt = Double(ray.maxt);

        Double ox = Double(ray.o.x()),
               oy = Double(ray.o.y()),
               oz = Double(ray.o.z()),
               dx = Double(ray.d.x()),
               dy = Double(ray.d.y()),
               dz = Double(ray.d.z());

        ek::scalar_t<Double> radius = ek::scalar_t<Double>(m_radius),
                         length = ek::scalar_t<Double>(m_length);

        Double A = ek::sqr(dx) + ek::sqr(dy),
               B = ek::scalar_t<Double>(2.f) * (dx * ox + dy * oy),
               C = ek::sqr(ox) + ek::sqr(oy) - ek::sqr(radius);

        auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

        // Cylinder doesn't intersect with the segment on the ray
        ek::mask_t<FloatX> out_bounds = !(near_t <= maxt && far_t >= mint); // NaN-aware conditionals

        Double z_pos_near = oz + dz * near_t,
               z_pos_far  = oz + dz * far_t;

        // Cylinder fully contains the segment of the ray
        ek::mask_t<FloatX> in_bounds = near_t < mint && far_t > maxt;

        ek::mask_t<FloatX> valid_intersection =
            active && solution_found && !out_bounds && !in_bounds &&
            ((z_pos_near >= Double(0.0) && z_pos_near <= length && near_t >= mint) ||
             (z_pos_far >= Double(0.0) && z_pos_far <= length && far_t <= maxt));

        return valid_intersection;
    }

    MTS_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     PreliminaryIntersection3f pi,
                                                     uint32_t hit_flags,
                                                     Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        bool differentiable = ek::grad_enabled(ray) || parameters_grad_enabled();

        // Recompute ray intersection to get differentiable prim_uv and t
        if (differentiable && !has_flag(hit_flags, HitComputeFlags::NonDifferentiable))
            pi = ray_intersect_preliminary(ray, active);

        active &= pi.is_valid();

        // TODO handle sticky derivatives
        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();
        si.t = ek::select(active, pi.t, ek::Infinity<Float>);

        si.p = ray(pi.t);

        Vector3f local = m_to_object.transform_affine(si.p);

        Float phi = ek::atan2(local.y(), local.x());
        ek::masked(phi, phi < 0.f) += 2.f * ek::Pi<Float>;

        si.uv = Point2f(phi * ek::InvTwoPi<Float>, local.z() / m_length);

        Vector3f dp_du = 2.f * ek::Pi<Float> * Vector3f(-local.y(), local.x(), 0.f);
        Vector3f dp_dv = Vector3f(0.f, 0.f, m_length);
        si.dp_du = m_to_world.transform_affine(dp_du);
        si.dp_dv = m_to_world.transform_affine(dp_dv);
        si.n = Normal3f(ek::normalize(ek::cross(si.dp_du, si.dp_dv)));

        /* Mitigate roundoff error issues by a normal shift of the computed
           intersection point */
        si.p += si.n * (m_radius - ek::norm(ek::head<2>(local)));

        if (m_flip_normals)
            si.n *= -1.f;

        si.sh_frame.n = si.n;

        if (has_flag(hit_flags, HitComputeFlags::dNSdUV) ||
            has_flag(hit_flags, HitComputeFlags::dNGdUV)) {
            si.dn_du = si.dp_du / (m_radius * (m_flip_normals ? -1.f : 1.f));
            si.dn_dv = Vector3f(0.f);
        }

        si.shape    = this;
        si.instance = nullptr;

        return si;
    }

    //! @}
    // =============================================================

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/) override {
        update();
        Base::parameters_changed();
#if defined(MTS_ENABLE_CUDA)
        optix_prepare_geometry();
#endif
    }

#if defined(MTS_ENABLE_CUDA)
    using Base::m_optix_data_ptr;

    void optix_prepare_geometry() override {
        if constexpr (ek::is_cuda_array_v<Float>) {
            if (!m_optix_data_ptr)
                m_optix_data_ptr
                    = jit_malloc(AllocType::Device, sizeof(OptixCylinderData));

            OptixCylinderData data = { bbox(),  m_to_object,
                                       (float) m_length, (float) m_radius };

            jit_memcpy(JitBackend::CUDA, m_optix_data_ptr, &data,
                       sizeof(OptixCylinderData));
        }
    }
#endif

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Cylinder[" << std::endl
            << "  to_world = " << string::indent(m_to_world, 13) << "," << std::endl
            << "  radius = "  << m_radius << "," << std::endl
            << "  length = "  << m_length << "," << std::endl
            << "  surface_area = " << surface_area() << "," << std::endl
            << "  " << string::indent(get_children_string()) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ScalarFloat m_radius, m_length;
    ScalarFloat m_inv_surface_area;
    bool m_flip_normals;
};

MTS_IMPLEMENT_CLASS_VARIANT(Cylinder, Shape)
MTS_EXPORT_PLUGIN(Cylinder, "Cylinder intersection primitive");
NAMESPACE_END(mitsuba)
